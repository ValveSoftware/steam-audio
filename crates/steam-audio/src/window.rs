//! Standard window functions (upstream `window_function.h`).
//!
//! Each function fills the whole slice. Like upstream, a window of length 1 divides 0 by 0 and yields NaN.

use crate::math::PI;

/// Rectangular window: all ones.
pub fn rectangular(window: &mut [f32]) {
    window.fill(1.0);
}

/// Bartlett (triangular) window.
pub fn bartlett(window: &mut [f32]) {
    let half = (window.len() as i32 - 1) as f32 / 2.0;
    for (i, w) in window.iter_mut().enumerate() {
        *w = 1.0 - ((i as f32 - half) / half).abs();
    }
}

/// Hann window.
pub fn hann(window: &mut [f32]) {
    let m = (window.len() as i32 - 1) as f32;
    for (i, w) in window.iter_mut().enumerate() {
        *w = 0.5 * (1.0 - ((2.0 * PI * i as f32) / m).cos());
    }
}

/// Hamming window.
pub fn hamming(window: &mut [f32]) {
    let m = (window.len() as i32 - 1) as f32;
    for (i, w) in window.iter_mut().enumerate() {
        *w = 0.54 - 0.46 * ((2.0 * PI * i as f32) / m).cos();
    }
}

/// Blackman window.
pub fn blackman(window: &mut [f32]) {
    let m = (window.len() as i32 - 1) as f32;
    for (i, w) in window.iter_mut().enumerate() {
        let i = i as f32;
        *w = 0.42 - 0.5 * ((2.0 * PI * i) / m).cos() + 0.08 * ((4.0 * PI * i) / m).cos();
    }
}

/// Blackman-Harris window.
pub fn blackman_harris(window: &mut [f32]) {
    let m = (window.len() as i32 - 1) as f32;
    for (i, w) in window.iter_mut().enumerate() {
        let i = i as f32;
        *w = 0.35875 - 0.48829 * ((2.0 * PI * i) / m).cos() + 0.14128 * ((4.0 * PI * i) / m).cos()
            - 0.01168 * ((6.0 * PI * i) / m).cos();
    }
}

/// Tukey window for overlap-add with `overlap_size` samples of overlap between frames of `size` samples.
///
/// `window` must hold `size + overlap_size` samples: a raised-cosine fade-in over the first `overlap_size`
/// samples, ones up to `size`, and a raised-cosine fade-out over the last `overlap_size` samples. The regions are
/// written in that order, so when `overlap_size > size` the fade-out overwrites part of the fade-in, as upstream.
/// An `overlap_size` of 1 yields NaN at the fades (0 / 0, as upstream).
///
/// # Panics
///
/// If `window.len() != size + overlap_size`.
pub fn tukey(size: usize, overlap_size: usize, window: &mut [f32]) {
    let n = size + overlap_size;
    assert_eq!(
        window.len(),
        n,
        "Tukey window length must be size + overlap_size"
    );
    let k = (overlap_size as i32 - 1) as f32;

    for (i, w) in window.iter_mut().enumerate().take(overlap_size) {
        *w = 0.5 * (1.0 + (PI * (i as f32 / k - 1.0)).cos());
    }
    for w in window.iter_mut().take(size).skip(overlap_size) {
        *w = 1.0;
    }
    for (i, w) in window.iter_mut().enumerate().skip(size) {
        *w = 0.5 * (1.0 + (PI * (1.0 - (n - i - 1) as f32 / k)).cos());
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn symmetric_windows_peak_at_the_centre() {
        let mut w = [0.0; 9];
        for f in [bartlett, hann, hamming, blackman, blackman_harris] {
            f(&mut w);
            for i in 0..9 {
                assert!((w[i] - w[8 - i]).abs() < 1e-6);
            }
            assert!(w[4] > 0.999);
        }
    }
}
