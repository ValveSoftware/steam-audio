//! Overlap-add convolution with a short, frequently changing IR (upstream `OverlapAddConvolutionEffect`). The binaural
//! effects use it to convolve with a new HRTF every frame.
//!
//! Each frame is extended backwards by a quarter frame, Tukey-windowed (so consecutive frames crossfade as the IR
//! changes), transformed, multiplied by the IR spectrum and transformed back; the part of the result past the frame is
//! kept and added to the next frame.

use super::tail_state;
use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::fft::RealFft;
use crate::{Complex, array_math, window};

/// Channel count and IR length of an [`OverlapAddConvolutionEffect`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct OverlapAddConvolutionSettings {
    pub num_channels: usize,
    pub ir_size: usize,
}

/// Convolves one input (or one input per channel) with one IR per output channel, given as spectra of
/// [`ir_spectrum_size`](Self::ir_spectrum_size) bins.
#[derive(Debug, Clone)]
pub struct OverlapAddConvolutionEffect {
    num_channels: usize,
    frame_size: usize,
    window: Vec<f32>,
    fft: RealFft,
    windowed_dry: Vec<f32>,
    fft_windowed_dry: Vec<Complex>,
    dry: Vec<Vec<f32>>,
    fft_wet: Vec<Vec<Complex>>,
    wet: Vec<Vec<f32>>,
    overlap: Vec<Vec<f32>>,
    num_tail_samples_remaining: usize,
}

impl OverlapAddConvolutionEffect {
    /// Panics if the frame size is 0.
    pub fn new(audio_settings: &AudioSettings, settings: &OverlapAddConvolutionSettings) -> Self {
        let frame_size = audio_settings.frame_size;
        assert!(frame_size > 0, "frame size must be positive");
        let num_channels = settings.num_channels;
        let overlap_size = frame_size / 4;
        let window_size = frame_size + overlap_size;

        let mut window = vec![0.0; window_size];
        window::tukey(frame_size, overlap_size, &mut window);

        let fft = RealFft::new(window_size + settings.ir_size - 1);
        let n = fft.num_real_samples();
        let m = fft.num_complex_samples();

        Self {
            num_channels,
            frame_size,
            window,
            windowed_dry: vec![0.0; n],
            fft_windowed_dry: vec![Complex::default(); m],
            dry: vec![vec![0.0; window_size]; num_channels],
            fft_wet: vec![vec![Complex::default(); m]; num_channels],
            wet: vec![vec![0.0; n]; num_channels],
            overlap: vec![vec![0.0; n - frame_size]; num_channels],
            num_tail_samples_remaining: 0,
            fft,
        }
    }

    /// Length of the convolved block (the FFT size).
    pub fn wet_audio_size(&self) -> usize {
        self.fft.num_real_samples()
    }

    /// Number of bins each IR spectrum passed to [`apply`](Self::apply) must have.
    pub fn ir_spectrum_size(&self) -> usize {
        self.fft.num_complex_samples()
    }

    pub fn num_tail_samples_remaining(&self) -> usize {
        self.num_tail_samples_remaining
    }

    /// Clears the input history and the pending tail.
    pub fn reset(&mut self) {
        for d in &mut self.dry {
            d.fill(0.0);
        }
        for o in &mut self.overlap {
            o.fill(0.0);
        }
        self.num_tail_samples_remaining = 0;
    }

    /// Convolves one frame. Output channel `i` is convolved with `fft_ir[i]`. With `multiple_inputs` and a
    /// multi-channel input, input channel `i` feeds output channel `i`; otherwise input channel 0 feeds every output
    /// channel.
    ///
    /// # Panics
    ///
    /// If the input or output is not one frame long, the input has neither 1 nor `num_channels` channels, the output
    /// does not have `num_channels` channels, or `fft_ir` has fewer than `num_channels` spectra of
    /// [`ir_spectrum_size`](Self::ir_spectrum_size) bins.
    pub fn apply(
        &mut self,
        fft_ir: &[&[Complex]],
        multiple_inputs: bool,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        let f = self.frame_size;
        assert_eq!(input.num_samples(), f, "input must be one frame long");
        assert_eq!(out.num_samples(), f, "output must be one frame long");
        assert!(
            input.num_channels() == 1 || input.num_channels() == self.num_channels,
            "input must have 1 or num_channels channels"
        );
        assert_eq!(out.num_channels(), self.num_channels, "output channels");

        assert!(
            fft_ir.len() >= self.num_channels,
            "one IR spectrum per channel"
        );

        let q = f / 4;
        let window_size = self.window.len();

        if input.num_channels() > 1 && multiple_inputs {
            for (i, (dry, wet)) in self.dry.iter_mut().zip(&mut self.fft_wet).enumerate() {
                dry.copy_within(f..f + q, 0);
                dry[q..].copy_from_slice(input.channel(i));

                array_math::multiply(dry, &self.window, &mut self.windowed_dry[..window_size]);
                self.fft
                    .forward(&self.windowed_dry, &mut self.fft_windowed_dry);
                array_math::multiply_complex(&self.fft_windowed_dry, fft_ir[i], wet);
            }
        } else {
            // Every channel keeps the history, so switching between the two modes has no discontinuity.
            for dry in &mut self.dry {
                dry.copy_within(f..f + q, 0);
                dry[q..].copy_from_slice(input.channel(0));
            }

            array_math::multiply(
                &self.dry[0],
                &self.window,
                &mut self.windowed_dry[..window_size],
            );
            self.fft
                .forward(&self.windowed_dry, &mut self.fft_windowed_dry);

            for (wet, ir) in self.fft_wet.iter_mut().zip(fft_ir) {
                array_math::multiply_complex(&self.fft_windowed_dry, ir, wet);
            }
        }

        for i in 0..self.num_channels {
            let wet = &mut self.wet[i];
            let overlap = &mut self.overlap[i];
            self.fft.inverse(&self.fft_wet[i], wet);

            for (w, &o) in wet.iter_mut().zip(overlap.iter()) {
                *w += o;
            }

            overlap.copy_from_slice(&wet[f..]);
            out.channel_mut(i).copy_from_slice(&wet[..f]);
        }

        self.num_tail_samples_remaining = self.overlap_size();
        tail_state(self.num_tail_samples_remaining > 0)
    }

    /// Outputs the next frame of the pending tail, as if the input were silent.
    ///
    /// Like upstream, this does not consume the tail: a later [`apply`](Self::apply) adds the whole pending tail to
    /// its output again (call [`reset`](Self::reset) first to avoid that).
    ///
    /// # Panics
    ///
    /// If the output does not have `num_channels` channels of one frame.
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(out.num_channels(), self.num_channels, "output channels");
        assert_eq!(
            out.num_samples(),
            self.frame_size,
            "output must be one frame long"
        );

        out.make_silent();

        let overlap_size = self.overlap_size();
        let start = overlap_size - self.num_tail_samples_remaining;
        let end = (start + self.frame_size).min(overlap_size);
        let n = end - start;

        for (i, overlap) in self.overlap.iter().enumerate() {
            out.channel_mut(i)[..n].copy_from_slice(&overlap[start..end]);
        }

        self.num_tail_samples_remaining -= n;
        tail_state(self.num_tail_samples_remaining > 0)
    }

    fn overlap_size(&self) -> usize {
        self.fft.num_real_samples() - self.frame_size
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn signal(n: usize, seed: f32) -> Vec<f32> {
        (0..n).map(|i| (i as f32 * seed).sin()).collect()
    }

    /// The fade-out of one Tukey-windowed frame and the fade-in of the next sum to one, so with a fixed IR the effect
    /// is a linear convolution delayed by a quarter frame.
    #[test]
    fn fixed_ir_is_delayed_linear_convolution() {
        let f = 32;
        let delay = f / 4;
        let irs = [signal(45, 0.37), signal(45, 1.13)];
        let audio = AudioSettings::new(48000, f);
        let mut effect = OverlapAddConvolutionEffect::new(
            &audio,
            &OverlapAddConvolutionSettings {
                num_channels: 2,
                ir_size: 45,
            },
        );

        let mut fft = RealFft::new(effect.wet_audio_size());
        let spectra: Vec<Vec<Complex>> = irs
            .iter()
            .map(|h| {
                let mut padded = vec![0.0; effect.wet_audio_size()];
                padded[..h.len()].copy_from_slice(h);
                let mut s = vec![Complex::default(); effect.ir_spectrum_size()];
                fft.forward(&padded, &mut s);
                s
            })
            .collect();
        let spectra: Vec<&[Complex]> = spectra.iter().map(Vec::as_slice).collect();

        let num_frames = 6;
        let x = signal(num_frames * f, 0.21);
        let mut y = vec![Vec::new(); 2];
        let mut out = AudioBuffer::new(2, f);
        for frame in x.chunks(f) {
            let input = AudioBuffer::from_planar(1, f, frame);
            effect.apply(&spectra, false, &input, &mut out);
            for (c, y) in y.iter_mut().enumerate() {
                y.extend_from_slice(out.channel(c));
            }
        }

        for (h, y) in irs.iter().zip(&y) {
            for (n, &actual) in y.iter().enumerate() {
                let expected: f32 = (0..h.len())
                    .filter(|&k| n >= delay + k)
                    .map(|k| h[k] * x[n - delay - k])
                    .sum();
                assert!(
                    (actual - expected).abs() < 1e-4,
                    "sample {n}: {actual} vs {expected}"
                );
            }
        }
    }
}
