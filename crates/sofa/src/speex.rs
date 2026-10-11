//! The Speex resampler as libmysofa builds it (upstream `resampler/speex_resampler.c`): floating point, quality 10
//! only, one channel, unit strides.
//!
//! Only what `mysofa_resample` reaches is ported: initialisation, `reset_mem`, `skip_zeros` and `process_float`.
//! The rate is never changed after initialisation, so `update_filter` runs once on a resampler that has not started
//! and the "magic samples" that only a filter change creates stay 0.
//!
//! Copyright (C) 2007-2008 Jean-Marc Valin, (C) 2008 Thorvald Natvig, BSD 3-Clause (see `LICENSE-libmysofa`).

use std::f64::consts::PI;

use crate::Error;

/// `RESAMPLER_ERR_ALLOC_FAILED`: also what `update_filter` reports when the filter does not fit.
const ERR_ALLOC_FAILED: i32 = 1;
/// `RESAMPLER_ERR_INVALID_ARG`.
const ERR_INVALID_ARG: i32 = 3;

/// `quality_map[0]`, quality 10.
const BASE_LENGTH: u32 = 256;
const OVERSAMPLE: u32 = 32;
const BANDWIDTH: f32 = 0.975;
/// `st->buffer_size`.
const BUFFER_SIZE: u32 = 160;
/// `INT_MAX / sizeof(spx_word16_t)`, computed in `size_t`.
const MAX_FLOATS: u64 = 2_147_483_647 / 4;

/// The port's bound on the filter length, reached when downsampling by more than 64:1 (`docs/DEVIATIONS.md`).
pub(crate) const MAX_FILTER_LENGTH: u32 = 1 << 14;

/// `kaiser12_table`.
#[allow(clippy::excessive_precision, clippy::unreadable_literal)]
const KAISER12: [f64; 68] = [
    0.99859849,
    1.00000000,
    0.99859849,
    0.99440475,
    0.98745105,
    0.97779076,
    0.96549770,
    0.95066529,
    0.93340547,
    0.91384741,
    0.89213598,
    0.86843014,
    0.84290116,
    0.81573067,
    0.78710866,
    0.75723148,
    0.72629970,
    0.69451601,
    0.66208321,
    0.62920216,
    0.59606986,
    0.56287762,
    0.52980938,
    0.49704014,
    0.46473455,
    0.43304576,
    0.40211431,
    0.37206735,
    0.34301800,
    0.31506490,
    0.28829195,
    0.26276832,
    0.23854851,
    0.21567274,
    0.19416736,
    0.17404546,
    0.15530766,
    0.13794294,
    0.12192957,
    0.10723616,
    0.09382272,
    0.08164178,
    0.07063950,
    0.06075685,
    0.05193064,
    0.04409466,
    0.03718069,
    0.03111947,
    0.02584161,
    0.02127838,
    0.01736250,
    0.01402878,
    0.01121463,
    0.00886058,
    0.00691064,
    0.00531256,
    0.00401805,
    0.00298291,
    0.00216702,
    0.00153438,
    0.00105297,
    0.00069463,
    0.00043489,
    0.00025272,
    0.00013031,
    0.0000527734,
    0.00001000,
    0.00000000,
];
/// `KAISER12->oversample`.
const KAISER12_OVERSAMPLE: f32 = 64.0;

/// `compute_func`: cubic interpolation of the Kaiser window. `frac` and its powers are `float`, the weights `double`.
#[allow(clippy::unreadable_literal)]
fn compute_func(x: f32) -> f64 {
    let y = x * KAISER12_OVERSAMPLE;
    let ind = f64::from(y).floor() as i32;
    let frac = y - ind as f32;
    let f = f64::from(frac);
    let f2 = f64::from(frac * frac);
    let f3 = f64::from(frac * frac * frac);
    let interp3 = -0.1666666667 * f + 0.1666666667 * f3;
    let interp2 = f + 0.5 * f2 - 0.5 * f3;
    let interp0 = -0.3333333333 * f + 0.5 * f2 - 0.1666666667 * f3;
    let interp1 = 1.0 - interp3 - interp2 - interp0;
    let t = &KAISER12[ind as usize..];
    interp0 * t[0] + interp1 * t[1] + interp2 * t[2] + interp3 * t[3]
}

/// `sinc`: a windowed sinc, computed in `double` and rounded to `float`.
fn sinc(cutoff: f32, x: f32, n: u32) -> f32 {
    let xx = x * cutoff;
    let ax = f64::from(x).abs();
    if ax < 1e-6 {
        return cutoff;
    }
    if ax > 0.5 * f64::from(n) {
        return 0.0;
    }
    let pxx = PI * f64::from(xx);
    let window = compute_func((2.0 * f64::from(x) / f64::from(n)).abs() as f32);
    (f64::from(cutoff) * pxx.sin() / pxx * window) as f32
}

/// `cubic_coef`: `float` except for the last coefficient, which is computed in `double`.
fn cubic_coef(frac: f32) -> [f32; 4] {
    let i0 = -0.16667 * frac + 0.16667 * frac * frac * frac;
    let i1 = frac + 0.5 * frac * frac - 0.5 * frac * frac * frac;
    let i3 = -0.33333 * frac + 0.5 * frac * frac - 0.16667 * frac * frac * frac;
    let i2 = (1.0 - f64::from(i0) - f64::from(i1) - f64::from(i3)) as f32;
    [i0, i1, i2, i3]
}

/// `_muldiv`: `value * mul / div` without overflowing 32 bits, or `None`.
///
/// `UINT32_MAX` is not defined where libmysofa compiles this file, so the fallback `4294967296U` (2^32, an
/// `unsigned long long`) is used, and the bound checks are done in 64 bits while the products stay 32-bit.
fn muldiv(value: u32, mul: u32, div: u32) -> Option<u32> {
    const UINT32_MAX: u64 = 4_294_967_296;
    let major = value / div;
    let remainder = value % div;
    if u64::from(remainder) > UINT32_MAX / u64::from(mul)
        || u64::from(major) > UINT32_MAX / u64::from(mul)
        || u64::from(major.wrapping_mul(mul))
            > UINT32_MAX - u64::from(remainder.wrapping_mul(mul) / div)
    {
        return None;
    }
    Some((remainder.wrapping_mul(mul) / div).wrapping_add(major.wrapping_mul(mul)))
}

/// `_gcd`.
fn gcd(mut a: u32, mut b: u32) -> u32 {
    while b != 0 {
        (a, b) = (b, a % b);
    }
    a
}

/// A one-channel quality-10 resampler (`SpeexResamplerState`).
pub(crate) struct Resampler {
    den_rate: u32,
    int_advance: i32,
    frac_advance: i32,
    oversample: u32,
    filt_len: u32,
    /// `resampler_basic_direct_double` rather than `resampler_basic_interpolate_double`.
    direct: bool,
    sinc_table: Vec<f32>,
    /// `filt_len - 1` samples of history followed by up to `BUFFER_SIZE` input samples.
    mem: Vec<f32>,
    last_sample: i32,
    samp_frac_num: u32,
}

impl Resampler {
    /// `speex_resampler_init(1, in_rate, out_rate, 10, &err)`.
    ///
    /// Fails with the Speex error code, or with [`Error::ResourceLimit`] when the filter would be longer than
    /// [`MAX_FILTER_LENGTH`].
    pub(crate) fn new(in_rate: u32, out_rate: u32) -> Result<Self, Error> {
        if in_rate == 0 || out_rate == 0 {
            return Err(Error::Resampler(ERR_INVALID_ARG));
        }

        // `speex_resampler_set_rate_frac`.
        let fact = gcd(in_rate, out_rate);
        let num_rate = in_rate / fact;
        let den_rate = out_rate / fact;

        // `update_filter`.
        let mut oversample = OVERSAMPLE;
        let mut filt_len = BASE_LENGTH;
        let cutoff;
        if num_rate > den_rate {
            // Downsampling.
            cutoff = BANDWIDTH * den_rate as f32 / num_rate as f32;
            filt_len =
                muldiv(filt_len, num_rate, den_rate).ok_or(Error::Resampler(ERR_ALLOC_FAILED))?;
            // Round up to a multiple of 8.
            filt_len = (filt_len.wrapping_sub(1) & !0x7).wrapping_add(8);
            for factor in [2, 4, 8, 16] {
                if den_rate.wrapping_mul(factor) < num_rate {
                    oversample >>= 1;
                }
            }
            oversample = oversample.max(1);
        } else {
            cutoff = BANDWIDTH;
        }

        let direct = filt_len.wrapping_mul(den_rate)
            <= filt_len.wrapping_mul(oversample).wrapping_add(8)
            && MAX_FLOATS / u64::from(den_rate) >= u64::from(filt_len);
        if !direct && (MAX_FLOATS - 8) / u64::from(oversample) < u64::from(filt_len) {
            return Err(Error::Resampler(ERR_ALLOC_FAILED));
        }
        if MAX_FLOATS < u64::from(filt_len) - 1 + u64::from(BUFFER_SIZE) {
            return Err(Error::Resampler(ERR_ALLOC_FAILED));
        }
        if filt_len > MAX_FILTER_LENGTH {
            return Err(Error::ResourceLimit);
        }

        let sinc_table = if direct {
            let half = (filt_len / 2) as i32;
            (0..den_rate)
                .flat_map(|i| {
                    (0..filt_len as i32).map(move |j| {
                        let x = (j - half + 1) as f32 - i as f32 / den_rate as f32;
                        sinc(cutoff, x, filt_len)
                    })
                })
                .collect()
        } else {
            let half = (filt_len / 2) as f32;
            (-4..(oversample * filt_len + 4) as i32)
                .map(|i| sinc(cutoff, i as f32 / oversample as f32 - half, filt_len))
                .collect()
        };

        Ok(Self {
            den_rate,
            int_advance: (num_rate / den_rate) as i32,
            frac_advance: (num_rate % den_rate) as i32,
            oversample,
            filt_len,
            direct,
            sinc_table,
            mem: vec![0.0; (filt_len - 1 + BUFFER_SIZE) as usize],
            last_sample: 0,
            samp_frac_num: 0,
        })
    }

    /// `speex_resampler_reset_mem`.
    pub(crate) fn reset_mem(&mut self) {
        self.last_sample = 0;
        self.samp_frac_num = 0;
        self.mem[..(self.filt_len - 1) as usize].fill(0.0);
    }

    /// `speex_resampler_skip_zeros`.
    pub(crate) fn skip_zeros(&mut self) {
        self.last_sample = (self.filt_len / 2) as i32;
    }

    /// `speex_resampler_process_float`: returns the number of input samples consumed and output samples written.
    pub(crate) fn process(&mut self, input: &[f32], output: &mut [f32]) -> (usize, usize) {
        let filt_offs = (self.filt_len - 1) as usize;
        let xlen = self.mem.len() - filt_offs;
        let mut in_pos = 0;
        let mut out_pos = 0;
        while in_pos < input.len() && out_pos < output.len() {
            let ichunk = (input.len() - in_pos).min(xlen);
            self.mem[filt_offs..filt_offs + ichunk]
                .copy_from_slice(&input[in_pos..in_pos + ichunk]);
            let (consumed, produced) = self.process_native(ichunk, &mut output[out_pos..]);
            in_pos += consumed;
            out_pos += produced;
        }
        (in_pos, out_pos)
    }

    /// `speex_resampler_process_native`.
    fn process_native(&mut self, in_len: usize, out: &mut [f32]) -> (usize, usize) {
        let produced = if self.direct {
            self.direct_double(in_len, out)
        } else {
            self.interpolate_double(in_len, out)
        };
        let consumed = (self.last_sample.max(0) as usize).min(in_len);
        self.last_sample -= consumed as i32;
        let n = (self.filt_len - 1) as usize;
        self.mem.copy_within(consumed..consumed + n, 0);
        (consumed, produced)
    }

    /// Advances the position by one output sample.
    fn advance(&self, last_sample: &mut i32, samp_frac_num: &mut u32) {
        *last_sample += self.int_advance;
        *samp_frac_num = samp_frac_num.wrapping_add(self.frac_advance as u32);
        if *samp_frac_num >= self.den_rate {
            *samp_frac_num -= self.den_rate;
            *last_sample += 1;
        }
    }

    /// `resampler_basic_direct_double`: `float` products accumulated in four `double` lanes.
    fn direct_double(&mut self, in_len: usize, out: &mut [f32]) -> usize {
        let n = self.filt_len as usize;
        let mut last_sample = self.last_sample;
        let mut samp_frac_num = self.samp_frac_num;
        let mut out_sample = 0;
        while !(last_sample >= in_len as i32 || out_sample >= out.len()) {
            let start = samp_frac_num.wrapping_mul(self.filt_len) as usize;
            let sinct = &self.sinc_table[start..start + n];
            let iptr = &self.mem[last_sample as usize..last_sample as usize + n];
            let mut accum = [0.0f64; 4];
            for (s, x) in sinct.chunks_exact(4).zip(iptr.chunks_exact(4)) {
                for ((a, &s), &x) in accum.iter_mut().zip(s).zip(x) {
                    *a += f64::from(s * x);
                }
            }
            let sum = accum[0] + accum[1] + accum[2] + accum[3];
            out[out_sample] = sum as f32;
            out_sample += 1;
            self.advance(&mut last_sample, &mut samp_frac_num);
        }
        self.last_sample = last_sample;
        self.samp_frac_num = samp_frac_num;
        out_sample
    }

    /// `resampler_basic_interpolate_double`: four phases of the oversampled table, mixed with cubic weights.
    fn interpolate_double(&mut self, in_len: usize, out: &mut [f32]) -> usize {
        let n = self.filt_len as usize;
        let oversample = self.oversample as usize;
        let mut last_sample = self.last_sample;
        let mut samp_frac_num = self.samp_frac_num;
        let mut out_sample = 0;
        while !(last_sample >= in_len as i32 || out_sample >= out.len()) {
            let iptr = &self.mem[last_sample as usize..last_sample as usize + n];
            let phase = samp_frac_num.wrapping_mul(self.oversample);
            let offset = (phase / self.den_rate) as usize;
            let frac = (phase % self.den_rate) as f32 / self.den_rate as f32;

            let mut accum = [0.0f64; 4];
            for (j, &x) in iptr.iter().enumerate() {
                let base = 4 + (j + 1) * oversample - offset - 2;
                let taps = &self.sinc_table[base..base + 4];
                for (a, &tap) in accum.iter_mut().zip(taps) {
                    *a += f64::from(x * tap);
                }
            }
            let interp = cubic_coef(frac);
            let sum = f64::from(interp[0]) * accum[0]
                + f64::from(interp[1]) * accum[1]
                + f64::from(interp[2]) * accum[2]
                + f64::from(interp[3]) * accum[3];
            out[out_sample] = sum as f32;
            out_sample += 1;
            self.advance(&mut last_sample, &mut samp_frac_num);
        }
        self.last_sample = last_sample;
        self.samp_frac_num = samp_frac_num;
        out_sample
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn muldiv_matches_exact_product_or_overflows() {
        assert_eq!(muldiv(256, 160, 147), Some(278));
        assert_eq!(muldiv(256, 2, 1), Some(512));
        assert_eq!(muldiv(256, 286_331_153, 3200), None);
    }

    #[test]
    fn filter_choice() {
        // 44.1 → 48 kHz and 48 → 44.1 kHz interpolate, 96 → 48 kHz uses a direct table.
        let up = Resampler::new(44_100, 48_000).unwrap();
        assert!(!up.direct);
        assert_eq!(up.filt_len, 256);
        let down = Resampler::new(48_000, 44_100).unwrap();
        assert!(!down.direct);
        assert_eq!(down.filt_len, 280);
        let half = Resampler::new(96_000, 48_000).unwrap();
        assert!(half.direct);
        assert_eq!(half.filt_len, 512);
    }

    #[test]
    fn errors() {
        assert!(matches!(
            Resampler::new(0, 48_000),
            Err(Error::Resampler(3))
        ));
        assert!(matches!(
            Resampler::new(u32::MAX, 48_000),
            Err(Error::Resampler(1))
        ));
        assert!(matches!(
            Resampler::new(48_000 * 65, 48_000),
            Err(Error::ResourceLimit)
        ));
    }
}
