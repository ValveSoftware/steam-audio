//! Discrete Fourier transforms (upstream `FFT`, PFFFT backend `pffft_fft.cpp`).
//!
//! Sizes are rounded up to the next power of two; callers zero-pad their signals to the rounded size. Forward
//! transforms are unscaled and inverse transforms are scaled by `1 / n`, so a forward transform followed by an
//! inverse transform reproduces the input. The transforms are computed by `realfft`/`rustfft`, which round
//! differently from PFFFT (risk R1 in `docs/PORTING_PLAN.md`).
//!
//! Like upstream, each transform owns its scratch buffers, so applying it takes `&mut self` and never allocates.

use std::fmt;
use std::sync::Arc;

use realfft::{ComplexToReal, RealFftPlanner, RealToComplex};
use rustfft::FftPlanner;

use crate::Complex;
use crate::math::next_pow2;

/// Fourier transform of real-valued signals (upstream `FFT(n, FFTDomain::Real)`).
///
/// The spectrum has `n / 2 + 1` bins (conjugate-even form): bin 0 is DC and the last bin is Nyquist, both with a
/// zero imaginary part.
#[derive(Clone)]
pub struct RealFft {
    num_real_samples: usize,
    forward: Arc<dyn RealToComplex<f32>>,
    inverse: Arc<dyn ComplexToReal<f32>>,
    signal: Vec<f32>,
    spectrum: Vec<Complex>,
    scratch: Vec<Complex>,
}

impl RealFft {
    /// A transform for signals of `size` samples, rounded up to the next power of two. Panics if `size` is 0.
    pub fn new(size: usize) -> Self {
        assert!(size > 0, "FFT size must be positive");
        let num_real_samples = next_pow2(size);

        let mut planner = RealFftPlanner::new();
        let forward = planner.plan_fft_forward(num_real_samples);
        let inverse = planner.plan_fft_inverse(num_real_samples);
        let scratch_len = forward.get_scratch_len().max(inverse.get_scratch_len());

        Self {
            num_real_samples,
            signal: forward.make_input_vec(),
            spectrum: inverse.make_input_vec(),
            scratch: vec![Complex::default(); scratch_len],
            forward,
            inverse,
        }
    }

    /// Size of the time-domain signal: the requested size rounded up to a power of two.
    pub fn num_real_samples(&self) -> usize {
        self.num_real_samples
    }

    /// Number of spectrum bins: `num_real_samples / 2 + 1`.
    pub fn num_complex_samples(&self) -> usize {
        self.num_real_samples / 2 + 1
    }

    /// Transforms a real signal of [`num_real_samples`](Self::num_real_samples) samples into its spectrum of
    /// [`num_complex_samples`](Self::num_complex_samples) bins. Panics on a length mismatch.
    pub fn forward(&mut self, signal: &[f32], spectrum: &mut [Complex]) {
        assert_eq!(signal.len(), self.num_real_samples, "signal length");
        assert_eq!(
            spectrum.len(),
            self.num_complex_samples(),
            "spectrum length"
        );

        self.signal.copy_from_slice(signal);
        self.forward
            .process_with_scratch(&mut self.signal, spectrum, &mut self.scratch)
            .expect("buffer lengths are checked above");
    }

    /// Transforms a spectrum of [`num_complex_samples`](Self::num_complex_samples) bins back into a real signal,
    /// scaled by `1 / num_real_samples`. The imaginary parts of the DC and Nyquist bins are ignored, as upstream.
    /// Panics on a length mismatch.
    pub fn inverse(&mut self, spectrum: &[Complex], signal: &mut [f32]) {
        assert_eq!(
            spectrum.len(),
            self.num_complex_samples(),
            "spectrum length"
        );
        assert_eq!(signal.len(), self.num_real_samples, "signal length");

        self.spectrum.copy_from_slice(spectrum);
        let last = self.spectrum.len() - 1;
        self.spectrum[0].im = 0.0;
        self.spectrum[last].im = 0.0;

        self.inverse
            .process_with_scratch(&mut self.spectrum, signal, &mut self.scratch)
            .expect("buffer lengths are checked and DC/Nyquist are real");

        let scale = 1.0 / self.num_real_samples as f32;
        for x in signal {
            *x *= scale;
        }
    }
}

impl fmt::Debug for RealFft {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("RealFft")
            .field("num_real_samples", &self.num_real_samples)
            .finish_non_exhaustive()
    }
}

/// Fourier transform of complex-valued signals (upstream `FFT(n, FFTDomain::Complex)`).
///
/// Signal and spectrum both have `n` samples.
#[derive(Clone)]
pub struct ComplexFft {
    num_samples: usize,
    forward: Arc<dyn rustfft::Fft<f32>>,
    inverse: Arc<dyn rustfft::Fft<f32>>,
    scratch: Vec<Complex>,
}

impl ComplexFft {
    /// A transform for signals of `size` samples, rounded up to the next power of two. Panics if `size` is 0.
    pub fn new(size: usize) -> Self {
        assert!(size > 0, "FFT size must be positive");
        let num_samples = next_pow2(size);

        let mut planner = FftPlanner::new();
        let forward = planner.plan_fft_forward(num_samples);
        let inverse = planner.plan_fft_inverse(num_samples);
        let scratch_len = forward
            .get_inplace_scratch_len()
            .max(inverse.get_inplace_scratch_len());

        Self {
            num_samples,
            forward,
            inverse,
            scratch: vec![Complex::default(); scratch_len],
        }
    }

    /// Size of the signal and of the spectrum: the requested size rounded up to a power of two (upstream
    /// `numRealSamples` and `numComplexSamples`, which are equal in the complex domain).
    pub fn num_samples(&self) -> usize {
        self.num_samples
    }

    /// Transforms a complex signal into its spectrum. Panics on a length mismatch.
    pub fn forward(&mut self, signal: &[Complex], spectrum: &mut [Complex]) {
        assert_eq!(signal.len(), self.num_samples, "signal length");
        spectrum.copy_from_slice(signal);
        self.forward
            .process_with_scratch(spectrum, &mut self.scratch);
    }

    /// Transforms a spectrum back into a complex signal, scaled by `1 / num_samples`. Panics on a length mismatch.
    pub fn inverse(&mut self, spectrum: &[Complex], signal: &mut [Complex]) {
        assert_eq!(spectrum.len(), self.num_samples, "spectrum length");
        signal.copy_from_slice(spectrum);
        self.inverse.process_with_scratch(signal, &mut self.scratch);

        let scale = 1.0 / self.num_samples as f32;
        for x in signal {
            x.re *= scale;
            x.im *= scale;
        }
    }
}

impl fmt::Debug for ComplexFft {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("ComplexFft")
            .field("num_samples", &self.num_samples)
            .finish_non_exhaustive()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_send<T: Send>() {}

    #[test]
    fn transforms_are_send() {
        assert_send::<RealFft>();
        assert_send::<ComplexFft>();
    }

    #[test]
    fn sizes_round_up_to_powers_of_two() {
        let real = RealFft::new(20);
        assert_eq!(real.num_real_samples(), 32);
        assert_eq!(real.num_complex_samples(), 17);
        assert_eq!(ComplexFft::new(100).num_samples(), 128);
    }

    #[test]
    fn small_sizes_round_trip() {
        // PFFFT rejects real sizes below 32 and complex sizes below 16; they work here.
        for size in [1, 2, 4, 8, 16] {
            let mut real = RealFft::new(size);
            let signal: Vec<f32> = (0..size).map(|i| i as f32 + 1.0).collect();
            let mut spectrum = vec![Complex::default(); real.num_complex_samples()];
            let mut round_trip = vec![0.0; size];
            real.forward(&signal, &mut spectrum);
            real.inverse(&spectrum, &mut round_trip);
            for (a, e) in round_trip.iter().zip(&signal) {
                assert!((a - e).abs() <= 1e-5 * e.abs(), "real {size}: {a} vs {e}");
            }

            let mut complex = ComplexFft::new(size);
            let signal: Vec<Complex> = (0..size)
                .map(|i| Complex::new(i as f32 + 1.0, -(i as f32)))
                .collect();
            let mut spectrum = vec![Complex::default(); size];
            let mut round_trip = vec![Complex::default(); size];
            complex.forward(&signal, &mut spectrum);
            complex.inverse(&spectrum, &mut round_trip);
            for (a, e) in round_trip.iter().zip(&signal) {
                assert!(
                    (a - e).norm() <= 1e-5 * e.norm(),
                    "complex {size}: {a} vs {e}"
                );
            }
        }
    }
}
