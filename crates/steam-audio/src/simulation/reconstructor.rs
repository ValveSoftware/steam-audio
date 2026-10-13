//! Reconstruction of impulse responses from energy fields (upstream `reconstructor.{h,cpp}`).
//!
//! For each channel and band, every bin of the energy field shapes a stretch of white noise (Gaussian bumps around the
//! bin centres, or a linear ramp between bins), scaled by air absorption at the distance sound travels in that time.
//! The bands are then band-pass filtered and summed, and the result is optionally multiplied by a distance attenuation
//! correction curve.

use crate::air_absorption::AirAbsorptionModel;
use crate::bands::NUM_BANDS;
use crate::iir::{Iir, IirFilterer};
use crate::math::PI;
use crate::propagation_medium::SPEED_OF_SOUND;
use crate::random::Mt19937;
use crate::sh::num_coeffs_for_order;

use super::{EnergyField, ImpulseResponse};

/// Energies whose magnitude is below this are treated as silence.
const ENERGY_THRESHOLD: f32 = 1e-7;
/// Variance (in s²) of the Gaussian bump of each bin.
const MIN_VARIANCE: f32 = 1e-5;

/// How a bin's energy shapes the noise (upstream `ReconstructionType`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ReconstructionType {
    /// A Gaussian bump centred on each bin, scaled by the bin's energy.
    Gaussian,
    /// An envelope ramping linearly from the previous bin's energy to this one's.
    Linear,
}

/// Settings of one [`Reconstructor::reconstruct`] call.
#[derive(Debug, Clone, Copy)]
pub struct ReconstructionParams<'a> {
    pub kind: ReconstructionType,
    /// Seconds of IR to reconstruct (limited to the IR's length).
    pub duration: f32,
    /// Ambisonics order to reconstruct (limited to the channels of the energy field and the IR).
    pub order: usize,
    pub air_absorption: &'a AirAbsorptionModel,
    /// Multiplies the reconstructed IR, if given; must hold at least as many samples as are reconstructed.
    pub distance_attenuation_correction: Option<&'a [f32]>,
}

/// Turns energy fields into impulse responses (upstream `Reconstructor`).
///
/// Upstream's constructor also takes a maximum order, which it never uses. Upstream reconstructs several IRs per call;
/// that is the same as calling [`reconstruct`](Self::reconstruct) for each in turn.
#[derive(Debug, Clone)]
pub struct Reconstructor {
    sampling_rate: u32,
    /// The noise of every band. Upstream draws each band's noise from a new default-seeded `std::mt19937`, so all
    /// bands get the same noise and one row is enough.
    white_noise: Vec<f32>,
    /// Per band, the shaped noise. It persists between calls: upstream skips silent Gaussian bins and bins past the
    /// end of a short energy field without clearing them, so their samples from an earlier channel or call are
    /// filtered again (risk R23).
    band_irs: [Vec<f32>; NUM_BANDS],
    filters: [IirFilterer; NUM_BANDS],
}

impl Reconstructor {
    /// A reconstructor for IRs of up to `max_duration` seconds at `sampling_rate`.
    pub fn new(max_duration: f32, sampling_rate: u32) -> Self {
        let num_samples = (max_duration * sampling_rate as f32).ceil() as usize;

        let mut rng = Mt19937::default();
        let white_noise = (0..num_samples)
            .map(|_| rng.uniform_f32(-1.0, 1.0))
            .collect();

        Self {
            sampling_rate,
            white_noise,
            band_irs: std::array::from_fn(|_| vec![0.0; num_samples]),
            filters: Iir::band_filters(sampling_rate).map(IirFilterer::new),
        }
    }

    /// Reconstructs `ir` from `energy_field`. Every sample of `ir` is overwritten; channels and samples beyond those
    /// reconstructed are zeroed.
    ///
    /// # Panics
    ///
    /// If more samples are reconstructed than the reconstructor's maximum duration holds, or than the correction
    /// curve holds.
    pub fn reconstruct(
        &mut self,
        energy_field: &EnergyField,
        ir: &mut ImpulseResponse,
        params: &ReconstructionParams<'_>,
    ) {
        let rate = self.sampling_rate as f32;

        let num_channels = energy_field
            .num_channels()
            .min(ir.num_channels())
            .min(num_coeffs_for_order(params.order));
        let num_samples_per_bin = (EnergyField::BIN_DURATION * rate).ceil() as usize;
        let num_samples = ir
            .num_samples()
            .min((params.duration * rate).ceil() as usize);
        let num_bins = energy_field
            .num_bins()
            .min((num_samples as f32 / num_samples_per_bin as f32).ceil() as usize);
        assert!(
            num_samples <= self.white_noise.len(),
            "IR longer than the reconstructor's maximum duration"
        );

        ir.reset();

        // sqrtf(4.0f * Math::kPi), with upstream's truncated pi.
        let sqrt_4pi = (4.0 * PI).sqrt();
        let normalized = |channel: usize, band: usize, bin: usize| {
            energy_field.band(channel, band)[bin]
                / (energy_field.band(0, band)[bin] * sqrt_4pi).sqrt()
        };
        let audible = |channel: usize, band: usize, bin: usize| {
            energy_field.band(channel, band)[bin].abs() >= ENERGY_THRESHOLD
                && energy_field.band(0, band)[bin].abs() >= ENERGY_THRESHOLD
        };

        for channel in 0..num_channels {
            for band in 0..NUM_BANDS {
                let band_ir = &mut self.band_irs[band];
                for bin in 0..num_bins {
                    let start = bin * num_samples_per_bin;
                    let num_bin_samples = num_samples_per_bin.min(num_samples - start);
                    let samples = &mut band_ir[start..start + num_bin_samples];
                    let noise = &self.white_noise[start..start + num_bin_samples];
                    let mut normalization = 1.0f32;

                    match params.kind {
                        ReconstructionType::Linear => {
                            let energy = if audible(channel, band, bin) {
                                normalized(channel, band, bin)
                            } else {
                                0.0
                            };
                            let prev_energy = if bin == 0 {
                                energy
                            } else if audible(channel, band, bin - 1) {
                                normalized(channel, band, bin - 1)
                            } else {
                                0.0
                            };

                            for (k, (s, &n)) in samples.iter_mut().zip(noise).enumerate() {
                                let weight = k as f32 / num_samples_per_bin as f32;
                                let sample_energy = (1.0 - weight) * prev_energy + weight * energy;
                                *s = sample_energy * n;
                            }
                        }
                        ReconstructionType::Gaussian => {
                            if !audible(channel, band, bin) {
                                continue;
                            }

                            let t_mean = ((bin as f32 + 0.5) * num_samples_per_bin as f32) / rate;
                            let t = start as f32 / rate;
                            let dt = 1.0 / rate;
                            let mut g =
                                (-((t - t_mean) * (t - t_mean)) / (2.0 * MIN_VARIANCE)).exp();
                            let mut dg =
                                (-(dt * ((2.0 * (t - t_mean)) + dt)) / (2.0 * MIN_VARIANCE)).exp();
                            let ddg = (-(dt * dt) / MIN_VARIANCE).exp();

                            for (s, &n) in samples.iter_mut().zip(noise) {
                                *s = g * n;
                                g *= dg;
                                dg *= ddg;
                            }

                            // Upstream asserts (in debug builds only) that this is finite.
                            normalization = normalized(channel, band, bin);
                        }
                    }

                    let distance = 0.5
                        * SPEED_OF_SOUND
                        * ((bin as f32 + 0.5) * num_samples_per_bin as f32 * (1.0 / rate));
                    normalization *= params.air_absorption.evaluate(distance, band);

                    for s in samples.iter_mut() {
                        *s *= normalization;
                    }
                }
            }

            for filter in &mut self.filters {
                filter.reset();
            }

            let out = &mut ir.channel_mut(channel)[..num_samples];
            self.filters[0].apply(&self.band_irs[0][..num_samples], out);
            for band in 1..NUM_BANDS {
                let band_ir = &mut self.band_irs[band][..num_samples];
                self.filters[band].apply_in_place(band_ir);
                for (o, &b) in out.iter_mut().zip(band_ir.iter()) {
                    *o += b;
                }
            }

            if let Some(curve) = params.distance_attenuation_correction {
                for (o, &c) in out.iter_mut().zip(&curve[..num_samples]) {
                    *o *= c;
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn params(kind: ReconstructionType, air: &AirAbsorptionModel) -> ReconstructionParams<'_> {
        ReconstructionParams {
            kind,
            duration: 0.1,
            order: 1,
            air_absorption: air,
            distance_attenuation_correction: None,
        }
    }

    #[test]
    fn silence_gives_silence() {
        let air = AirAbsorptionModel::default();
        let mut reconstructor = Reconstructor::new(0.1, 48000);
        let field = EnergyField::new(0.1, 1);
        let mut ir = ImpulseResponse::new(0.1, 1, 48000);
        ir.data_mut().fill(1.0);
        reconstructor.reconstruct(&field, &mut ir, &params(ReconstructionType::Linear, &air));
        assert!(ir.data().iter().all(|&s| s.abs() < 1e-6));
    }

    #[test]
    fn energy_lands_in_its_bin() {
        let air = AirAbsorptionModel::default();
        let mut reconstructor = Reconstructor::new(0.1, 48000);
        let mut field = EnergyField::new(0.1, 0);
        for band in 0..NUM_BANDS {
            field.band_mut(0, band)[5] = 1.0;
        }
        let mut ir = ImpulseResponse::new(0.1, 0, 48000);
        reconstructor.reconstruct(&field, &mut ir, &params(ReconstructionType::Gaussian, &air));

        let energy =
            |range: std::ops::Range<usize>| ir.channel(0)[range].iter().map(|s| s * s).sum::<f32>();
        assert!(energy(2400..2880) > 100.0 * energy(0..2000));
    }
}
