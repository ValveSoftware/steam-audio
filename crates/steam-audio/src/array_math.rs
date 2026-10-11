//! Element-wise operations on real and complex arrays (upstream `ArrayMath`, generic non-IPP path).
//!
//! Upstream hand-vectorizes most of these with SSE/NEON. Every operation here is element-wise (except the
//! sequential `max` reductions), so plain loops produce the same per-element arithmetic and the compiler is free to
//! vectorize them. The one place where upstream's SIMD body and scalar tail disagree, [`threshold`], is reproduced
//! exactly.
//!
//! All functions take the length from `out` (or the accumulator) and panic if the inputs are shorter.

use crate::Complex;
use crate::math::cpp_max;

/// Number of leading elements upstream processes in 4-wide SIMD (`size & ~3`).
const fn simd_len(len: usize) -> usize {
    len & !3
}

/// `out[i] = a[i] + b[i]`.
pub fn add(a: &[f32], b: &[f32], out: &mut [f32]) {
    let n = out.len();
    for ((o, &x), &y) in out.iter_mut().zip(&a[..n]).zip(&b[..n]) {
        *o = x + y;
    }
}

/// `out[i] = a[i] + b[i]`.
pub fn add_complex(a: &[Complex], b: &[Complex], out: &mut [Complex]) {
    let n = out.len();
    for ((o, &x), &y) in out.iter_mut().zip(&a[..n]).zip(&b[..n]) {
        *o = x + y;
    }
}

/// `out[i] = a[i] * b[i]`.
pub fn multiply(a: &[f32], b: &[f32], out: &mut [f32]) {
    let n = out.len();
    for ((o, &x), &y) in out.iter_mut().zip(&a[..n]).zip(&b[..n]) {
        *o = x * y;
    }
}

/// `out[i] = a[i] * b[i]` (complex product `(ac − bd) + (ad + bc)i`).
pub fn multiply_complex(a: &[Complex], b: &[Complex], out: &mut [Complex]) {
    let n = out.len();
    for ((o, &x), &y) in out.iter_mut().zip(&a[..n]).zip(&b[..n]) {
        *o = x * y;
    }
}

/// `accum[i] += a[i] * b[i]`.
pub fn multiply_accumulate(a: &[f32], b: &[f32], accum: &mut [f32]) {
    let n = accum.len();
    for ((acc, &x), &y) in accum.iter_mut().zip(&a[..n]).zip(&b[..n]) {
        *acc += x * y;
    }
}

/// `accum[i] += a[i] * b[i]`.
pub fn multiply_accumulate_complex(a: &[Complex], b: &[Complex], accum: &mut [Complex]) {
    let n = accum.len();
    for ((acc, &x), &y) in accum.iter_mut().zip(&a[..n]).zip(&b[..n]) {
        *acc += x * y;
    }
}

/// `out[i] = input[i] * scalar`.
pub fn scale(input: &[f32], scalar: f32, out: &mut [f32]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o = x * scalar;
    }
}

/// `out[i] = input[i] * scalar`.
pub fn scale_complex(input: &[Complex], scalar: f32, out: &mut [Complex]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o = x * scalar;
    }
}

/// `out[i] += scalar * input[i]`.
pub fn scale_accumulate(input: &[f32], scalar: f32, out: &mut [f32]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o += scalar * x;
    }
}

/// `out[i] = input[i] + constant`.
pub fn add_constant(input: &[f32], constant: f32, out: &mut [f32]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o = x + constant;
    }
}

/// Largest value, folding with `std::max` semantics from the first element. Panics on an empty slice.
pub fn max(input: &[f32]) -> f32 {
    input[1..].iter().fold(input[0], |m, &x| cpp_max(m, x))
}

/// Largest value and the index of its first occurrence. Panics on an empty slice.
pub fn max_index(input: &[f32]) -> (f32, usize) {
    let mut best = (input[0], 0);
    for (i, &x) in input.iter().enumerate().skip(1) {
        if x > best.0 {
            best = (x, i);
        }
    }
    best
}

/// `out[i] = max(input[i], min_value)`.
///
/// Upstream computes the first `len & !3` elements with `_mm_max_ps(x, min)`, which returns `min` when `x` is NaN
/// or equal to `min` (so `-0.0` becomes `+0.0` for `min = 0`), and the remaining tail with `std::max(x, min)`,
/// which keeps NaN and `-0.0`. Both behaviours are reproduced.
pub fn threshold(input: &[f32], min_value: f32, out: &mut [f32]) {
    let n = out.len();
    let body = simd_len(n);
    for (o, &x) in out[..body].iter_mut().zip(&input[..body]) {
        *o = if x > min_value { x } else { min_value };
    }
    for (o, &x) in out[body..].iter_mut().zip(&input[body..n]) {
        *o = cpp_max(x, min_value);
    }
}

/// `out[i] = ln(input[i])`.
pub fn log(input: &[f32], out: &mut [f32]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o = x.ln();
    }
}

/// `out[i] = e^input[i]`.
pub fn exp(input: &[f32], out: &mut [f32]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o = x.exp();
    }
}

/// `out[i] = e^input[i]` for complex input: `e^re · (cos im + i sin im)`.
pub fn exp_complex(input: &[Complex], out: &mut [Complex]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o = x.exp();
    }
}

/// `out[i] = |input[i]|`.
pub fn magnitude(input: &[Complex], out: &mut [f32]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o = x.norm();
    }
}

/// `out[i] = arg(input[i])`, in `(-π, π]`.
pub fn phase(input: &[Complex], out: &mut [f32]) {
    let n = out.len();
    for (o, &x) in out.iter_mut().zip(&input[..n]) {
        *o = x.arg();
    }
}

/// `out[i] = magnitude[i] · (cos phase[i] + i sin phase[i])`.
pub fn polar_to_cartesian(magnitude: &[f32], phase: &[f32], out: &mut [Complex]) {
    let n = out.len();
    for ((o, &r), &theta) in out.iter_mut().zip(&magnitude[..n]).zip(&phase[..n]) {
        *o = Complex::from_polar(r, theta);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn threshold_body_and_tail_differ_like_upstream() {
        let input = [f32::NAN, -0.0, 1.0, -1.0, f32::NAN, -0.0];
        let mut out = [9.0; 6];
        threshold(&input, 0.0, &mut out);
        // SIMD body (first 4): NaN -> 0, -0.0 -> +0.0.
        assert_eq!(out[0], 0.0);
        assert!(out[1].is_sign_positive());
        assert_eq!(out[2..4], [1.0, 0.0]);
        // Scalar tail: NaN and -0.0 pass through.
        assert!(out[4].is_nan());
        assert!(out[5].is_sign_negative());
    }

    #[test]
    fn max_index_keeps_first() {
        assert_eq!(max_index(&[1.0, 3.0, 3.0, 2.0]), (3.0, 1));
        assert_eq!(max(&[1.0, 3.0, 2.0]), 3.0);
    }
}
