//! Direct sound path parameters applied to a multichannel signal (upstream `DirectEffect`): distance attenuation,
//! air absorption, directivity, occlusion and transmission, as one smoothed gain and one EQ per channel.

use std::ops::{BitOr, BitOrAssign};

use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::bands::NUM_BANDS;
use crate::effects::{EqEffect, GainEffect};

/// Parameters of the direct sound path from a source to the listener (upstream `DirectSoundPath`), as computed by
/// the direct simulator (stage 5.1) or by the caller.
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub struct DirectSoundPath {
    /// Gain from distance attenuation.
    pub distance_attenuation: f32,
    /// Gain per band from air absorption.
    pub air_absorption: [f32; NUM_BANDS],
    /// Propagation delay, in seconds. Not used by [`DirectEffect`].
    pub delay: f32,
    /// Fraction of the source that is visible (1: not occluded).
    pub occlusion: f32,
    /// Fraction of occluded sound transmitted through geometry, per band.
    pub transmission: [f32; NUM_BANDS],
    /// Gain from the source's directivity.
    pub directivity: f32,
}

/// Which parts of a [`DirectSoundPath`] a [`DirectEffect`] applies (upstream `DirectEffectFlags`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct DirectEffectFlags(u32);

impl DirectEffectFlags {
    pub const NONE: Self = Self(0);
    pub const DISTANCE_ATTENUATION: Self = Self(1 << 0);
    pub const AIR_ABSORPTION: Self = Self(1 << 1);
    pub const DIRECTIVITY: Self = Self(1 << 2);
    pub const OCCLUSION: Self = Self(1 << 3);
    pub const TRANSMISSION: Self = Self(1 << 4);

    /// Flags from their bits, as in `phonon.h` (unknown bits are kept and ignored).
    pub const fn from_bits(bits: u32) -> Self {
        Self(bits)
    }

    pub const fn bits(self) -> u32 {
        self.0
    }

    /// Whether every flag of `other` is set.
    pub const fn contains(self, other: Self) -> bool {
        self.0 & other.0 == other.0
    }
}

impl BitOr for DirectEffectFlags {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self {
        Self(self.0 | rhs.0)
    }
}

impl BitOrAssign for DirectEffectFlags {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

/// How transmission through occluding geometry is applied (upstream `TransmissionType`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub enum TransmissionType {
    /// The average of the band transmission coefficients scales the overall gain.
    FreqIndependent,
    /// Each band's transmission coefficient scales that band's EQ gain.
    #[default]
    FreqDependent,
}

/// Parameters of [`DirectEffect::apply`] (upstream `DirectEffectParams`).
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub struct DirectEffectParams {
    pub direct_path: DirectSoundPath,
    pub flags: DirectEffectFlags,
    pub transmission_type: TransmissionType,
}

/// Applies a [`DirectSoundPath`] to every channel of a signal.
///
/// Unlike the other effects, it can run in place ([`apply_in_place`](Self::apply_in_place)), as `phonon.h` allows.
#[derive(Debug, Clone)]
pub struct DirectEffect {
    eq_effects: Vec<EqEffect>,
    gain_effects: Vec<GainEffect>,
}

impl DirectEffect {
    /// An effect for signals of `num_channels` channels.
    pub fn new(audio_settings: &AudioSettings, num_channels: usize) -> Self {
        Self {
            eq_effects: (0..num_channels)
                .map(|_| EqEffect::new(audio_settings))
                .collect(),
            gain_effects: (0..num_channels)
                .map(|_| GainEffect::new(audio_settings))
                .collect(),
        }
    }

    pub fn num_channels(&self) -> usize {
        self.gain_effects.len()
    }

    pub fn reset(&mut self) {
        for eq in &mut self.eq_effects {
            eq.reset();
        }
        for gain in &mut self.gain_effects {
            gain.reset();
        }
    }

    /// Applies `params` to `input`, writing `out`.
    ///
    /// # Panics
    ///
    /// If either buffer does not have the effect's channel count or does not hold one frame.
    pub fn apply(
        &mut self,
        params: &DirectEffectParams,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(input.num_channels(), self.num_channels(), "input channels");
        assert_eq!(input.num_samples(), out.num_samples(), "sample count");
        assert_eq!(out.num_channels(), self.num_channels(), "output channels");
        out.as_mut_slice().copy_from_slice(input.as_slice());
        self.apply_in_place(params, out)
    }

    /// [`apply`](Self::apply) with the same buffer as input and output.
    ///
    /// Upstream filters each channel from the input into the output and then scales the output in place; doing both
    /// in place gives the same result.
    pub fn apply_in_place(
        &mut self,
        params: &DirectEffectParams,
        buffer: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(
            buffer.num_channels(),
            self.num_channels(),
            "buffer channels"
        );

        let (gain, eq_coeffs) = Self::calculate_gain_and_eq(
            &params.direct_path,
            params.flags,
            params.transmission_type,
        );
        let apply_eq = applies_eq(params.flags, params.transmission_type);

        for (i, (eq, gain_effect)) in self
            .eq_effects
            .iter_mut()
            .zip(&mut self.gain_effects)
            .enumerate()
        {
            let channel = buffer.channel_mut(i);
            if apply_eq {
                eq.process(&eq_coeffs, channel);
            }
            gain_effect.process(gain, channel);
        }

        AudioEffectState::TailComplete
    }

    /// The direct effect has no tail: outputs silence.
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        out.make_silent();
        AudioEffectState::TailComplete
    }

    pub fn num_tail_samples_remaining(&self) -> usize {
        0
    }

    /// The overall gain and the band gains (normalized to a maximum of 1 when an EQ is applied) that
    /// [`apply`](Self::apply) uses for `direct_path` (upstream `calculateGainAndEQ`).
    pub fn calculate_gain_and_eq(
        direct_path: &DirectSoundPath,
        flags: DirectEffectFlags,
        transmission_type: TransmissionType,
    ) -> (f32, [f32; NUM_BANDS]) {
        // Distance attenuation.
        let mut overall_gain = if flags.contains(DirectEffectFlags::DISTANCE_ATTENUATION) {
            direct_path.distance_attenuation
        } else {
            1.0
        };

        // Air absorption.
        let mut eq_coeffs = if flags.contains(DirectEffectFlags::AIR_ABSORPTION) {
            direct_path.air_absorption
        } else {
            [1.0; NUM_BANDS]
        };

        // Directivity.
        if flags.contains(DirectEffectFlags::DIRECTIVITY) {
            overall_gain *= direct_path.directivity;
        }

        // Occlusion and transmission.
        if flags.contains(DirectEffectFlags::OCCLUSION) {
            let occlusion = direct_path.occlusion;
            if flags.contains(DirectEffectFlags::TRANSMISSION) {
                match transmission_type {
                    TransmissionType::FreqIndependent => {
                        // The average transmission coefficient, weighted by the occluded fraction.
                        let mut average_transmission_factor = 0.0;
                        for t in direct_path.transmission {
                            average_transmission_factor += t;
                        }
                        average_transmission_factor /= NUM_BANDS as f32;

                        overall_gain *= occlusion + (1.0 - occlusion) * average_transmission_factor;
                    }
                    TransmissionType::FreqDependent => {
                        for (c, t) in eq_coeffs.iter_mut().zip(direct_path.transmission) {
                            *c *= occlusion + (1.0 - occlusion) * t;
                        }
                    }
                }
            } else {
                overall_gain *= occlusion;
            }
        }

        if applies_eq(flags, transmission_type) {
            // Moving the largest band gain into the overall gain keeps EQ changes smooth.
            EqEffect::normalize_gains(&mut eq_coeffs, &mut overall_gain);
        }

        (overall_gain, eq_coeffs)
    }
}

/// Whether the EQ runs: for air absorption, or for frequency-dependent transmission (even without occlusion).
fn applies_eq(flags: DirectEffectFlags, transmission_type: TransmissionType) -> bool {
    flags.contains(DirectEffectFlags::AIR_ABSORPTION)
        || (flags.contains(DirectEffectFlags::TRANSMISSION)
            && transmission_type == TransmissionType::FreqDependent)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn path() -> DirectSoundPath {
        DirectSoundPath {
            distance_attenuation: 0.5,
            air_absorption: [0.9, 0.6, 0.3],
            delay: 0.0,
            occlusion: 0.25,
            transmission: [0.4, 0.2, 0.1],
            directivity: 0.8,
        }
    }

    #[test]
    fn gain_only_without_eq() {
        let flags = DirectEffectFlags::DISTANCE_ATTENUATION | DirectEffectFlags::DIRECTIVITY;
        let (gain, eq) =
            DirectEffect::calculate_gain_and_eq(&path(), flags, TransmissionType::FreqDependent);
        assert_eq!(gain, 0.5 * 0.8);
        assert_eq!(eq, [1.0; NUM_BANDS]);
    }

    #[test]
    fn frequency_independent_transmission() {
        let flags = DirectEffectFlags::OCCLUSION | DirectEffectFlags::TRANSMISSION;
        let (gain, eq) =
            DirectEffect::calculate_gain_and_eq(&path(), flags, TransmissionType::FreqIndependent);
        let average = (0.4f32 + 0.2 + 0.1) / 3.0;
        assert_eq!(gain, 0.25 + 0.75 * average);
        assert_eq!(eq, [1.0; NUM_BANDS]);
    }

    #[test]
    fn in_place_matches_out_of_place() {
        let settings = AudioSettings::new(44100, 37);
        let params = DirectEffectParams {
            direct_path: path(),
            flags: DirectEffectFlags::from_bits(0b11111),
            transmission_type: TransmissionType::FreqDependent,
        };
        let input = AudioBuffer::from_planar(
            2,
            37,
            (0..74).map(|i| (i as f32 * 0.7).sin()).collect::<Vec<_>>(),
        );

        let mut a = DirectEffect::new(&settings, 2);
        let mut b = DirectEffect::new(&settings, 2);
        let mut out = AudioBuffer::new(2, 37);
        let mut buffer = input.clone();
        for _ in 0..3 {
            a.apply(&params, &input, &mut out);
            buffer.as_mut_slice().copy_from_slice(input.as_slice());
            b.apply_in_place(&params, &mut buffer);
            assert_eq!(out, buffer);
        }
    }
}
