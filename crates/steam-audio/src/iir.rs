//! Biquad IIR filters (upstream `iir.h`: `IIR2`, `IIR2Filterer`, and the `IIR`/`IIRFilterer` wrappers).
//!
//! Upstream's `IIR` and `IIRFilterer` wrap a biquad and an 8th-order cascade, and switch between them on two global
//! flags that nothing ever sets, so they always behave as the biquad. The port has only the biquad (see
//! `docs/DEVIATIONS.md`).
//!
//! Coefficient formulas follow the Audio EQ Cookbook, with upstream's exact mix of `f32` and `f64` arithmetic.

use crate::bands::{HIGH_CUTOFF_FREQUENCIES, LOW_CUTOFF_FREQUENCIES, NUM_BANDS};
use crate::math::{PI, PI_F64};

/// Q used by the designs that do not take one (`0.707f`).
const DEFAULT_Q: f32 = 0.707;

/// Added to every input sample before filtering (upstream's guard against denormals).
const EPSILON: f32 = 1e-9;

/// Coefficients of a biquad `y[n] = b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]` (upstream `IIR2`).
///
/// The default value has all coefficients zero and filters everything to silence.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Iir {
    pub a1: f32,
    pub a2: f32,
    pub b0: f32,
    pub b1: f32,
    pub b2: f32,
}

/// Divides every coefficient by `a0` in double precision, as upstream's `float /= double` does.
fn normalize_f64(a1: f32, a2: f32, b0: f32, b1: f32, b2: f32, a0: f64) -> Iir {
    let div = |x: f32| (f64::from(x) / a0) as f32;
    Iir {
        a1: div(a1),
        a2: div(a2),
        b0: div(b0),
        b1: div(b1),
        b2: div(b2),
    }
}

fn normalize_f32(a1: f32, a2: f32, b0: f32, b1: f32, b2: f32, a0: f32) -> Iir {
    Iir {
        a1: a1 / a0,
        a2: a2 / a0,
        b0: b0 / a0,
        b1: b1 / a0,
        b2: b2 / a0,
    }
}

impl Iir {
    /// Magnitude response at angular frequency `frequency` (radians per sample).
    pub fn spectrum(&self, frequency: f32) -> f32 {
        let Self { a1, a2, b0, b1, b2 } = *self;
        let numerator = b0 * b0
            + b1 * b1
            + b2 * b2
            + 2.0 * (b0 * b1 + b1 * b2) * frequency.cos()
            + 2.0 * b0 * b2 * (2.0 * frequency).cos();
        let denominator = 1.0
            + a1 * a1
            + a2 * a2
            + 2.0 * (a1 + a1 * a2) * frequency.cos()
            + 2.0 * a2 * (2.0 * frequency).cos();
        (numerator / denominator).sqrt()
    }

    /// Low-pass filter with Q = 0.707.
    pub fn low_pass(cutoff: f32, sampling_rate: u32) -> Self {
        Self::low_pass_q(cutoff, DEFAULT_Q, sampling_rate)
    }

    /// Low-pass filter. Computed in `f64`, like upstream.
    pub fn low_pass_q(cutoff: f32, q: f32, sampling_rate: u32) -> Self {
        let w0 = 2.0 * PI_F64 * f64::from(cutoff) / f64::from(sampling_rate);
        let cw0 = w0.cos();
        let sw0 = w0.sin();
        let alpha = sw0 / (2.0 * f64::from(q));

        let a0 = 1.0 + alpha;
        normalize_f64(
            (-2.0 * cw0) as f32,
            (1.0 - alpha) as f32,
            ((1.0 - cw0) / 2.0) as f32,
            (1.0 - cw0) as f32,
            ((1.0 - cw0) / 2.0) as f32,
            a0,
        )
    }

    /// High-pass filter with Q = 0.707.
    pub fn high_pass(cutoff: f32, sampling_rate: u32) -> Self {
        Self::high_pass_q(cutoff, DEFAULT_Q, sampling_rate)
    }

    /// High-pass filter.
    pub fn high_pass_q(cutoff: f32, q: f32, sampling_rate: u32) -> Self {
        let w0 = 2.0 * PI * cutoff / sampling_rate as f32;
        let cw0 = w0.cos();
        let sw0 = w0.sin();
        let alpha = sw0 / (2.0 * q);

        let a0 = 1.0 + alpha;
        normalize_f32(
            -2.0 * cw0,
            1.0 - alpha,
            (1.0 + cw0) / 2.0,
            -(1.0 + cw0),
            (1.0 + cw0) / 2.0,
            a0,
        )
    }

    /// Band-pass filter centred on the geometric mean of the cutoffs.
    pub fn band_pass(low_cutoff: f32, high_cutoff: f32, sampling_rate: u32) -> Self {
        let cutoff = (low_cutoff * high_cutoff).sqrt();
        let q_inverse = (high_cutoff - low_cutoff) / cutoff;
        let w0 = 2.0 * PI * cutoff / sampling_rate as f32;
        let cw0 = w0.cos();
        let sw0 = w0.sin();
        let alpha = sw0 * q_inverse / 2.0;

        let a0 = 1.0 + alpha;
        normalize_f32(-2.0 * cw0, 1.0 - alpha, alpha, 0.0, -alpha, a0)
    }

    /// Low-shelf filter with Q = 0.707: scales frequencies below `cutoff` by `gain`.
    pub fn low_shelf(cutoff: f32, gain: f32, sampling_rate: u32) -> Self {
        Self::low_shelf_q(cutoff, DEFAULT_Q, gain, sampling_rate)
    }

    /// Low-shelf filter.
    pub fn low_shelf_q(cutoff: f32, q: f32, gain: f32, sampling_rate: u32) -> Self {
        let w0 = 2.0 * PI * cutoff / sampling_rate as f32;
        let cw0 = w0.cos();
        let sw0 = w0.sin();
        let alpha = sw0 / (2.0 * q);
        let a = gain.sqrt();
        let two_sqrt_a_alpha = 2.0 * a.sqrt() * alpha;

        let a0 = (a + 1.0) + (a - 1.0) * cw0 + two_sqrt_a_alpha;
        normalize_f32(
            -2.0 * ((a - 1.0) + (a + 1.0) * cw0),
            (a + 1.0) + (a - 1.0) * cw0 - two_sqrt_a_alpha,
            a * ((a + 1.0) - (a - 1.0) * cw0 + two_sqrt_a_alpha),
            2.0 * a * ((a - 1.0) - (a + 1.0) * cw0),
            a * ((a + 1.0) - (a - 1.0) * cw0 - two_sqrt_a_alpha),
            a0,
        )
    }

    /// High-shelf filter with Q = 0.707: scales frequencies above `cutoff` by `gain`.
    pub fn high_shelf(cutoff: f32, gain: f32, sampling_rate: u32) -> Self {
        Self::high_shelf_q(cutoff, DEFAULT_Q, gain, sampling_rate)
    }

    /// High-shelf filter. Computed in `f64` (except the square roots of the gain), like upstream.
    pub fn high_shelf_q(cutoff: f32, q: f32, gain: f32, sampling_rate: u32) -> Self {
        let w0 = 2.0 * PI_F64 * f64::from(cutoff) / f64::from(sampling_rate);
        let cw0 = w0.cos();
        let sw0 = w0.sin();
        let alpha = sw0 / (2.0 * f64::from(q));
        // Upstream calls `sqrt` on a float, which resolves to the float overload.
        let sqrt_gain = gain.sqrt();
        let a = f64::from(sqrt_gain);
        let two_sqrt_a_alpha = 2.0 * f64::from(sqrt_gain.sqrt()) * alpha;

        let a0 = (a + 1.0) - (a - 1.0) * cw0 + two_sqrt_a_alpha;
        normalize_f64(
            (2.0 * ((a - 1.0) - (a + 1.0) * cw0)) as f32,
            ((a + 1.0) - (a - 1.0) * cw0 - two_sqrt_a_alpha) as f32,
            (a * ((a + 1.0) + (a - 1.0) * cw0 + two_sqrt_a_alpha)) as f32,
            (-2.0 * a * ((a - 1.0) + (a + 1.0) * cw0)) as f32,
            (a * ((a + 1.0) + (a - 1.0) * cw0 - two_sqrt_a_alpha)) as f32,
            a0,
        )
    }

    /// Peaking filter: scales frequencies between the cutoffs by `gain`.
    pub fn peaking(low_cutoff: f32, high_cutoff: f32, gain: f32, sampling_rate: u32) -> Self {
        let cutoff = (low_cutoff * high_cutoff).sqrt();
        let q_inverse = (high_cutoff - low_cutoff) / cutoff;
        let w0 = 2.0 * PI * cutoff / sampling_rate as f32;
        let cw0 = w0.cos();
        let sw0 = w0.sin();
        let alpha = sw0 * q_inverse / 2.0;
        let a = gain.sqrt();

        let a0 = 1.0 + alpha / a;
        normalize_f32(
            -2.0 * cw0,
            1.0 - alpha / a,
            1.0 + alpha * a,
            -2.0 * cw0,
            1.0 - alpha * a,
            a0,
        )
    }

    /// One filter per band (see [`crate::bands`]): low-pass, band-pass, high-pass.
    pub fn band_filters(sampling_rate: u32) -> [Self; NUM_BANDS] {
        std::array::from_fn(|i| {
            if i == 0 {
                Self::low_pass(HIGH_CUTOFF_FREQUENCIES[i], sampling_rate)
            } else if i == NUM_BANDS - 1 {
                Self::high_pass(LOW_CUTOFF_FREQUENCIES[i], sampling_rate)
            } else {
                Self::band_pass(
                    LOW_CUTOFF_FREQUENCIES[i],
                    HIGH_CUTOFF_FREQUENCIES[i],
                    sampling_rate,
                )
            }
        })
    }
}

/// Samples per step of the block recurrence.
const BLOCK: usize = 8;

/// Filter state carried across frames (upstream `IIR2Filterer`).
///
/// Buffers are filtered [`BLOCK`] = 8 samples per step: each output of a step is a fixed linear combination of the
/// step's inputs and the previous two inputs and outputs. This is upstream's AVX algorithm, which it uses whenever the
/// CPU has AVX; on other CPUs upstream steps 4 samples at a time, which rounds differently (see
/// `docs/DEVIATIONS.md`). Samples past the last whole step are filtered one at a time.
///
/// If the filter changes between frames, the caller must crossfade. The default value has zero coefficients and
/// outputs silence.
#[derive(Debug, Clone, Default)]
pub struct IirFilterer {
    filter: Iir,
    xm1: f32,
    xm2: f32,
    ym1: f32,
    ym2: f32,
    /// Row `r`, column `j`: weight of input `x[BLOCK - 1 - r]` (rows `0..BLOCK`), then of `x[-1]`, `x[-2]`, `y[-1]`,
    /// `y[-2]`, in output `y[j]` of a step (upstream `mCoeffs8`).
    coeffs: [[f32; BLOCK]; BLOCK + 4],
}

impl IirFilterer {
    /// A filterer for `filter`, with zero state.
    pub fn new(filter: Iir) -> Self {
        let mut f = Self::default();
        f.set_filter(filter);
        f
    }

    /// Zeroes the past inputs and outputs.
    pub fn reset(&mut self) {
        self.xm1 = 0.0;
        self.xm2 = 0.0;
        self.ym1 = 0.0;
        self.ym2 = 0.0;
    }

    /// Copies the past inputs and outputs (not the filter) from `other`.
    pub fn copy_state(&mut self, other: &Self) {
        self.xm1 = other.xm1;
        self.xm2 = other.xm2;
        self.ym1 = other.ym1;
        self.ym2 = other.ym2;
    }

    /// The filter being applied.
    pub fn filter(&self) -> Iir {
        self.filter
    }

    /// Replaces the filter, keeping the state.
    pub fn set_filter(&mut self, filter: Iir) {
        self.filter = filter;
        let Iir { a1, a2, b0, b1, b2 } = filter;
        let c = &mut self.coeffs;
        *c = [[0.0; BLOCK]; BLOCK + 4];

        // Direct terms: output j takes b0, b1, b2 from inputs j, j-1, j-2 (rows BLOCK-1-j and below; rows BLOCK and
        // BLOCK+1 hold x[-1] and x[-2]).
        for j in 0..BLOCK {
            c[BLOCK - 1 - j][j] = b0;
            c[BLOCK - j][j] = b1;
            c[BLOCK + 1 - j][j] = b2;
        }
        // Past outputs feed the first two outputs directly. Upstream sets these after the b terms of column 1; the
        // assignment order does not matter since the slots are distinct.
        c[BLOCK + 2][0] = -a1;
        c[BLOCK + 3][0] = -a2;
        c[BLOCK + 2][1] = -a2;

        // Fold in the recursion on outputs within the step.
        for row in c.iter_mut() {
            row[1] += -a1 * row[0];
            for j in 2..BLOCK {
                row[j] += -a1 * row[j - 1] + -a2 * row[j - 2];
            }
        }
    }

    /// Filters one sample.
    pub fn apply_sample(&mut self, input: f32) -> f32 {
        let f = &self.filter;
        let x = input + EPSILON;
        let y = f.b0 * x + f.b1 * self.xm1 + f.b2 * self.xm2 - f.a1 * self.ym1 - f.a2 * self.ym2;
        self.xm2 = self.xm1;
        self.xm1 = x;
        self.ym2 = self.ym1;
        self.ym1 = y;
        y
    }

    /// Filters `input` into `output`.
    ///
    /// # Panics
    ///
    /// If the lengths differ.
    pub fn apply(&mut self, input: &[f32], output: &mut [f32]) {
        assert_eq!(
            input.len(),
            output.len(),
            "IIR input and output lengths differ"
        );
        output.copy_from_slice(input);
        self.apply_in_place(output);
    }

    /// Filters `buffer` in place.
    pub fn apply_in_place(&mut self, buffer: &mut [f32]) {
        let c = &self.coeffs;
        let simd_size = buffer.len() / BLOCK * BLOCK;

        let mut xm1 = self.xm1;
        let mut xm2 = self.xm2;
        let mut ym1 = self.ym1;
        let mut ym2 = self.ym2;

        // Upstream keeps the last raw inputs here, without the epsilon the recurrence adds.
        if simd_size > 0 {
            self.xm1 = buffer[simd_size - 1];
            self.xm2 = buffer[simd_size - 2];
        }

        for block in buffer[..simd_size].chunks_exact_mut(BLOCK) {
            let x: [f32; BLOCK] = std::array::from_fn(|k| block[k] + EPSILON);
            for (j, out) in block.iter_mut().enumerate() {
                let mut y = c[0][j] * x[BLOCK - 1];
                for r in 1..BLOCK {
                    y += c[r][j] * x[BLOCK - 1 - r];
                }
                y += c[BLOCK][j] * xm1;
                y += c[BLOCK + 1][j] * xm2;
                y += c[BLOCK + 2][j] * ym1;
                y += c[BLOCK + 3][j] * ym2;
                *out = y;
            }
            xm1 = x[BLOCK - 1];
            xm2 = x[BLOCK - 2];
            ym1 = block[BLOCK - 1];
            ym2 = block[BLOCK - 2];
        }

        if simd_size > 0 {
            self.ym1 = ym1;
            self.ym2 = ym2;
        }

        for sample in &mut buffer[simd_size..] {
            *sample = self.apply_sample(*sample);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Upstream `IirFilter.test.cpp`.
    #[test]
    fn iir_filter_is_applied_correctly() {
        let filter = Iir {
            a1: 2.0,
            a2: 3.0,
            b0: 4.0,
            b1: 5.0,
            b2: 6.0,
        };
        let mut filterer = IirFilterer::new(filter);
        let dry = [1.0, 2.0, 3.0, 4.0, 5.0];
        let mut wet = [0.0; 5];
        filterer.apply(&dry, &mut wet);
        for (w, e) in wet.iter().zip([4.0, 5.0, 6.0, 16.0, 8.0]) {
            assert!((w - e).abs() <= 1e-5 * e, "{w} != {e}");
        }
    }

    #[test]
    fn default_filterer_outputs_silence() {
        let mut f = IirFilterer::default();
        let mut buf = [1.0; 13];
        f.apply_in_place(&mut buf);
        assert_eq!(buf, [0.0; 13]);
    }

    #[test]
    fn blocks_match_the_scalar_recurrence() {
        let filter = Iir::band_pass(800.0, 8000.0, 44100);
        let mut block = IirFilterer::new(filter);
        let mut scalar = block.clone();
        let x: Vec<f32> = (0..64)
            .map(|i| ((i * 37) % 11) as f32 / 5.0 - 1.0)
            .collect();
        let mut y = vec![0.0; 64];
        block.apply(&x, &mut y);
        for (&xi, &yi) in x.iter().zip(&y) {
            let s = scalar.apply_sample(xi);
            assert!((s - yi).abs() < 1e-5, "{s} != {yi}");
        }
    }
}
