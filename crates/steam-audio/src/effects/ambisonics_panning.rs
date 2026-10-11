//! Decoding of Ambisonics to a speaker layout (upstream `AmbisonicsPanningEffect`), by t-design Ambisonic panning as
//! described in F. Zotter, M. Frank, "All-Round Ambisonic Panning and Decoding", JAES 2012: the sound field is sampled
//! at 24 virtual speakers, and each virtual speaker is panned onto the layout.

use super::PanningEffect;
use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::math::{DynamicMatrix, PI, Vector3};
use crate::sh;
use crate::speaker_layout::SpeakerLayout;

/// Renders an Ambisonics buffer to a speaker layout with one precomputed matrix.
#[derive(Debug, Clone)]
pub struct AmbisonicsPanningEffect {
    speaker_layout: SpeakerLayout,
    max_order: usize,
    frame_size: usize,
    /// `num_speakers × num_coeffs(max_order)`.
    ambisonics_to_speakers: DynamicMatrix,
    /// `num_coeffs(max_order) × frame_size`. Rows above the order of the last input keep earlier values (risk R15).
    ambisonics_vectors: DynamicMatrix,
    /// `num_speakers × frame_size`.
    speakers_vectors: DynamicMatrix,
}

impl AmbisonicsPanningEffect {
    /// Number of virtual speakers (upstream `kNumVirtualSpeakers`).
    pub const NUM_VIRTUAL_SPEAKERS: usize = 24;

    /// The virtual speaker directions: a spherical 7-design of 24 points, from
    /// <http://neilsloane.com/sphdesigns/dim3/des.3.24.7.txt> (upstream `kVirtualSpeakers`). The Ambisonics HRTFs of
    /// [`HrtfDatabase`](crate::hrtf::HrtfDatabase) are sampled at them too.
    #[allow(clippy::excessive_precision)] // Upstream's literals, kept verbatim.
    pub const VIRTUAL_SPEAKERS: [Vector3; Self::NUM_VIRTUAL_SPEAKERS] = {
        const A: f32 = 0.8662468181078205913835980;
        const B: f32 = 0.4225186537611115291185464;
        const C: f32 = 0.2666354015167047203315344;
        [
            Vector3::new(A, B, C),
            Vector3::new(A, -B, -C),
            Vector3::new(A, C, -B),
            Vector3::new(A, -C, B),
            Vector3::new(-A, B, -C),
            Vector3::new(-A, -B, C),
            Vector3::new(-A, C, B),
            Vector3::new(-A, -C, -B),
            Vector3::new(C, A, B),
            Vector3::new(-C, A, -B),
            Vector3::new(-B, A, C),
            Vector3::new(B, A, -C),
            Vector3::new(-C, -A, B),
            Vector3::new(C, -A, -B),
            Vector3::new(B, -A, C),
            Vector3::new(-B, -A, -C),
            Vector3::new(B, C, A),
            Vector3::new(-B, -C, A),
            Vector3::new(C, -B, A),
            Vector3::new(-C, B, A),
            Vector3::new(B, -C, -A),
            Vector3::new(-B, C, -A),
            Vector3::new(C, B, -A),
            Vector3::new(-C, -B, -A),
        ]
    };

    /// Decodes up to `max_order` to `speaker_layout`.
    pub fn new(
        audio_settings: &AudioSettings,
        speaker_layout: &SpeakerLayout,
        max_order: usize,
    ) -> Self {
        let num_coeffs = sh::num_coeffs_for_order(max_order);
        let num_speakers = speaker_layout.num_speakers();
        let num_virtual = Self::NUM_VIRTUAL_SPEAKERS;

        let mut ambisonics_to_virtual_speakers = DynamicMatrix::new(num_virtual, num_coeffs);
        let mut i = 0;
        for l in 0..=max_order {
            for m in -(l as i32)..=l as i32 {
                for (j, speaker) in Self::VIRTUAL_SPEAKERS.iter().enumerate() {
                    ambisonics_to_virtual_speakers[(j, i)] =
                        sh::evaluate(l, m, speaker.normalize_or_zero());
                }
                i += 1;
            }
        }

        let mut virtual_speakers_to_speakers = DynamicMatrix::new(num_speakers, num_virtual);
        for (i, &speaker) in Self::VIRTUAL_SPEAKERS.iter().enumerate() {
            for j in 0..num_speakers {
                virtual_speakers_to_speakers[(j, i)] = (4.0 * PI / num_virtual as f32)
                    * PanningEffect::panning_weight(speaker, speaker_layout, j);
            }
        }

        let mut ambisonics_to_speakers = DynamicMatrix::new(num_speakers, num_coeffs);
        DynamicMatrix::multiply(
            &virtual_speakers_to_speakers,
            &ambisonics_to_virtual_speakers,
            &mut ambisonics_to_speakers,
        );

        Self {
            speaker_layout: speaker_layout.clone(),
            max_order,
            frame_size: audio_settings.frame_size,
            ambisonics_to_speakers,
            ambisonics_vectors: DynamicMatrix::new(num_coeffs, audio_settings.frame_size),
            speakers_vectors: DynamicMatrix::new(num_speakers, audio_settings.frame_size),
        }
    }

    /// Does nothing, like upstream: in particular, the matrix rows kept from earlier inputs (risk R15) stay.
    pub fn reset(&mut self) {}

    /// Decodes one frame of order-`order` Ambisonics to one channel per speaker.
    ///
    /// Like upstream (risk R15), applying a lower order than an earlier frame mixes in that frame's higher-order
    /// channels, which the effect still holds.
    ///
    /// # Panics
    ///
    /// If `order` exceeds the maximum order, the input does not have `num_coeffs_for_order(order)` channels, the
    /// output does not have one channel per speaker, or either is not one frame long.
    pub fn apply(
        &mut self,
        order: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert!(order <= self.max_order, "order exceeds the maximum order");
        assert_eq!(
            input.num_channels(),
            sh::num_coeffs_for_order(order),
            "input channels must match the order"
        );
        self.process(input, out)
    }

    /// [`apply`](Self::apply) for an input of any order up to the maximum, given by its channel count. Upstream's
    /// `apply` ignores its order parameter (other than in assertions), and `AmbisonicsDecodeEffect` passes it a buffer
    /// of the maximum order.
    pub(crate) fn process(
        &mut self,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(
            input.num_samples(),
            self.frame_size,
            "input must be one frame long"
        );
        assert_eq!(
            out.num_samples(),
            self.frame_size,
            "output must be one frame long"
        );
        assert!(
            input.num_channels() <= self.ambisonics_vectors.rows(),
            "input order exceeds the maximum order"
        );
        assert_eq!(
            out.num_channels(),
            self.speaker_layout.num_speakers(),
            "one output channel per speaker"
        );

        out.make_silent();

        for k in 0..self.frame_size {
            for i in 0..input.num_channels() {
                self.ambisonics_vectors[(i, k)] = input.channel(i)[k];
            }
        }

        DynamicMatrix::multiply(
            &self.ambisonics_to_speakers,
            &self.ambisonics_vectors,
            &mut self.speakers_vectors,
        );

        for i in 0..out.num_channels() {
            for (k, o) in out.channel_mut(i).iter_mut().enumerate() {
                *o = self.speakers_vectors[(i, k)];
            }
        }

        AudioEffectState::TailComplete
    }

    /// Panning has no tail: outputs silence.
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
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn virtual_speakers_are_unit_vectors() {
        for speaker in AmbisonicsPanningEffect::VIRTUAL_SPEAKERS {
            assert!((speaker.length() - 1.0).abs() < 1e-6);
        }
    }

    /// The zeroth-order channel is omnidirectional, so a mono layout receives it with gain 4π/24 · 24 · Y00 = √(4π).
    #[test]
    fn mono_receives_the_omni_channel() {
        let layout = SpeakerLayout::new(crate::speaker_layout::SpeakerLayoutType::Mono);
        let mut effect = AmbisonicsPanningEffect::new(&AudioSettings::new(48000, 3), &layout, 1);
        let input = AudioBuffer::from_planar(
            4,
            3,
            vec![1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        );
        let mut out = AudioBuffer::new(1, 3);
        effect.apply(1, &input, &mut out);
        let gain = (4.0 * std::f32::consts::PI).sqrt();
        for (k, &o) in out.channel(0).iter().enumerate() {
            assert!((o - gain * (k + 1) as f32).abs() < 1e-4, "{o}");
        }
    }
}
