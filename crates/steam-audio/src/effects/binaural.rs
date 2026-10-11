//! Binaural rendering of a mono (or stereo) source from a direction (upstream `BinauralEffect`): the HRTF for the
//! direction is looked up in an [`HrtfDatabase`] and convolved with the input by an [`OverlapAddConvolutionEffect`].

use super::OverlapAddConvolutionEffect;
use super::overlap_add::OverlapAddConvolutionSettings;
use crate::Complex;
use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::hrtf::{HrtfDatabase, HrtfInterpolation, HrtfPhaseType, NUM_EARS};
use crate::math::Vector3;

/// Per-frame parameters of a [`BinauralEffect`] (upstream `BinauralEffectParams`).
#[derive(Debug)]
pub struct BinauralParams<'a> {
    /// Direction of the source relative to the listener (need not be normalized).
    pub direction: Vector3,
    pub interpolation: HrtfInterpolation,
    /// 1 applies the HRTF fully; below 1, a stereo input is crossfed and the HRTF is mixed with an unspatialized one
    /// (see [`HrtfPhaseType`]).
    pub spatial_blend: f32,
    pub phase_type: HrtfPhaseType,
    /// The HRTF to use. The effect can be created with one database and applied with another; if the HRIR length
    /// differs, the convolution is rebuilt (allocating, and dropping the tail).
    pub hrtf: &'a mut HrtfDatabase,
    /// Receives the delay in seconds of the peak of each ear's HRIR.
    pub peak_delays: Option<&'a mut [f32; NUM_EARS]>,
}

impl<'a> BinauralParams<'a> {
    /// Nearest-neighbour lookup, full spatial blend, no peak delays.
    pub fn new(direction: Vector3, hrtf: &'a mut HrtfDatabase) -> Self {
        Self {
            direction,
            interpolation: HrtfInterpolation::NearestNeighbor,
            spatial_blend: 1.0,
            phase_type: HrtfPhaseType::None,
            hrtf,
            peak_delays: None,
        }
    }
}

/// Where the spectra of the current frame are: in the database, or synthesized into the effect's scratch.
enum Spectra {
    Database(usize),
    Interpolated,
}

/// Applies an HRTF to a mono audio buffer (or a stereo one, for sources with extent) from a given direction.
#[derive(Debug, Clone)]
pub struct BinauralEffect {
    sampling_rate: u32,
    frame_size: usize,
    hrir_size: usize,
    overlap_add: OverlapAddConvolutionEffect,
    /// `NUM_EARS × num_spectrum_samples`.
    interpolated_hrtf: Vec<Complex>,
    partial_downmixed: AudioBuffer,
}

impl BinauralEffect {
    /// Sizes the effect for `hrtf`.
    pub fn new(audio_settings: &AudioSettings, hrtf: &HrtfDatabase) -> Self {
        let (overlap_add, interpolated_hrtf) = Self::build(audio_settings, hrtf);
        Self {
            sampling_rate: audio_settings.sampling_rate,
            frame_size: audio_settings.frame_size,
            hrir_size: hrtf.num_samples(),
            overlap_add,
            interpolated_hrtf,
            partial_downmixed: AudioBuffer::new(2, audio_settings.frame_size),
        }
    }

    fn build(
        audio_settings: &AudioSettings,
        hrtf: &HrtfDatabase,
    ) -> (OverlapAddConvolutionEffect, Vec<Complex>) {
        let overlap_add = OverlapAddConvolutionEffect::new(
            audio_settings,
            &OverlapAddConvolutionSettings {
                num_channels: NUM_EARS,
                ir_size: hrtf.num_samples(),
            },
        );
        let interpolated = vec![Complex::default(); NUM_EARS * hrtf.num_spectrum_samples()];
        (overlap_add, interpolated)
    }

    pub fn reset(&mut self) {
        self.overlap_add.reset();
    }

    pub fn num_tail_samples_remaining(&self) -> usize {
        self.overlap_add.num_tail_samples_remaining()
    }

    /// Spatializes one frame. A mono input is convolved with the HRTF of the direction; a stereo input is first
    /// crossfed according to the spatial blend, then each channel is convolved with its ear's HRTF.
    ///
    /// # Panics
    ///
    /// If the input is not 1 or 2 channels, the output is not stereo, or either is not one frame long.
    pub fn apply(
        &mut self,
        params: BinauralParams<'_>,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(input.num_samples(), out.num_samples(), "sample count");
        assert!(
            input.num_channels() == 1 || input.num_channels() == 2,
            "input must be mono or stereo"
        );
        assert_eq!(out.num_channels(), NUM_EARS, "output must be stereo");

        let BinauralParams {
            direction,
            interpolation,
            spatial_blend,
            phase_type,
            hrtf,
            peak_delays,
        } = params;

        if self.hrir_size != hrtf.num_samples() {
            let audio_settings = AudioSettings::new(self.sampling_rate, self.frame_size);
            (self.overlap_add, self.interpolated_hrtf) = Self::build(&audio_settings, hrtf);
            self.hrir_size = hrtf.num_samples();
        }

        out.make_silent();

        let bins = hrtf.num_spectrum_samples();
        let enable_spatial_blend = input.num_channels() == 2 && spatial_blend < 1.0;

        // Upstream also computes the blended HRTF for a mono input with a spatial blend below 1, then ignores it.
        let (spectra, peak_delay_in_samples) = match interpolation {
            HrtfInterpolation::NearestNeighbor => {
                let index = hrtf.nearest_index(direction);
                if enable_spatial_blend {
                    let (left, right) = self.interpolated_hrtf.split_at_mut(bins);
                    hrtf.blended_hrtf(index, direction, spatial_blend, phase_type, [left, right]);
                    (Spectra::Interpolated, hrtf.peak_delays(index))
                } else {
                    (Spectra::Database(index), hrtf.peak_delays(index))
                }
            }
            HrtfInterpolation::Bilinear => {
                let (left, right) = self.interpolated_hrtf.split_at_mut(bins);
                let peaks =
                    hrtf.interpolated_hrtf(direction, spatial_blend, phase_type, [left, right]);
                (Spectra::Interpolated, peaks)
            }
        };

        let fft_ir: [&[Complex]; NUM_EARS] = match spectra {
            Spectra::Database(index) => hrtf.hrtf(index),
            Spectra::Interpolated => {
                let (left, right) = self.interpolated_hrtf.split_at(bins);
                [left, right]
            }
        };

        let state = if input.num_channels() == 2 {
            let direct = 1.0 - 0.5 * spatial_blend;
            let cross = 0.5 * spatial_blend;
            let (left, right) = self
                .partial_downmixed
                .as_mut_slice()
                .split_at_mut(self.frame_size);
            for (i, (l, r)) in left.iter_mut().zip(right.iter_mut()).enumerate() {
                let (a, b) = (input.channel(0)[i], input.channel(1)[i]);
                *l = direct * a + cross * b;
                *r = direct * b + cross * a;
            }

            self.overlap_add
                .apply(&fft_ir, spatial_blend < 1.0, &self.partial_downmixed, out)
        } else {
            self.overlap_add.apply(&fft_ir, false, input, out)
        };

        if let Some(peak_delays) = peak_delays {
            for (delay, &samples) in peak_delays.iter_mut().zip(&peak_delay_in_samples) {
                *delay = samples as f32 / self.sampling_rate as f32;
            }
        }

        state
    }

    /// Outputs the next frame of the convolution tail.
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        self.overlap_add.tail(out)
    }
}
