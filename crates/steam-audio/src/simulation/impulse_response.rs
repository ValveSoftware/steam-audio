//! Ambisonics impulse responses (upstream `impulse_response.{h,cpp}`), reconstructed from energy fields and
//! partitioned for overlap-save convolution.

use crate::array_math;
use crate::sh::num_coeffs_for_order;

/// A planar `channels × samples` impulse response (upstream `ImpulseResponse`). The default value is empty.
///
/// Upstream's `swap` is [`std::mem::swap`].
#[derive(Debug, Clone, Default, PartialEq)]
pub struct ImpulseResponse {
    num_channels: usize,
    num_samples: usize,
    data: Vec<f32>,
}

impl ImpulseResponse {
    /// A silent IR of `ceil(duration * sampling_rate)` samples for an Ambisonics `order`.
    pub fn new(duration: f32, order: usize, sampling_rate: u32) -> Self {
        let num_samples = (duration * sampling_rate as f32).ceil() as usize;
        Self::with_size(num_coeffs_for_order(order), num_samples)
    }

    /// A silent IR with the given dimensions.
    pub fn with_size(num_channels: usize, num_samples: usize) -> Self {
        Self {
            num_channels,
            num_samples,
            data: vec![0.0; num_channels * num_samples],
        }
    }

    pub fn num_channels(&self) -> usize {
        self.num_channels
    }

    pub fn num_samples(&self) -> usize {
        self.num_samples
    }

    /// All samples, channel after channel.
    pub fn data(&self) -> &[f32] {
        &self.data
    }

    /// All samples, channel after channel.
    pub fn data_mut(&mut self) -> &mut [f32] {
        &mut self.data
    }

    /// Channel `i` (upstream `ir[i]`). Panics if `i` is out of range.
    pub fn channel(&self, i: usize) -> &[f32] {
        assert!(i < self.num_channels, "channel index out of range");
        &self.data[i * self.num_samples..(i + 1) * self.num_samples]
    }

    /// Channel `i` (upstream `ir[i]`). Panics if `i` is out of range.
    pub fn channel_mut(&mut self, i: usize) -> &mut [f32] {
        assert!(i < self.num_channels, "channel index out of range");
        &mut self.data[i * self.num_samples..(i + 1) * self.num_samples]
    }

    /// Zeroes every sample.
    pub fn reset(&mut self) {
        self.data.fill(0.0);
    }

    /// Copies as many channels and samples of `src` as both IRs have; the rest is left as it is (upstream
    /// `ImpulseResponse::copy(src, dst)`).
    pub fn copy_from(&mut self, src: &Self) {
        let (num_channels, n) = common_size(src, self);
        for i in 0..num_channels {
            self.channel_mut(i)[..n].copy_from_slice(&src.channel(i)[..n]);
        }
    }

    /// `out = in1 + in2` over the channels and samples all three have; the rest of `out` is left as it is.
    pub fn add(in1: &Self, in2: &Self, out: &mut Self) {
        let num_channels = in1.num_channels.min(in2.num_channels).min(out.num_channels);
        let n = in1.num_samples.min(in2.num_samples).min(out.num_samples);
        for i in 0..num_channels {
            array_math::add(
                &in1.channel(i)[..n],
                &in2.channel(i)[..n],
                &mut out.channel_mut(i)[..n],
            );
        }
    }

    /// `out = input * scalar` over the channels and samples both have; the rest of `out` is left as it is.
    pub fn scale(input: &Self, scalar: f32, out: &mut Self) {
        let (num_channels, n) = common_size(input, out);
        for i in 0..num_channels {
            array_math::scale(&input.channel(i)[..n], scalar, &mut out.channel_mut(i)[..n]);
        }
    }

    /// `out += input * scalar` over the channels and samples both have.
    pub fn scale_accumulate(input: &Self, scalar: f32, out: &mut Self) {
        let (num_channels, n) = common_size(input, out);
        for i in 0..num_channels {
            array_math::scale_accumulate(
                &input.channel(i)[..n],
                scalar,
                &mut out.channel_mut(i)[..n],
            );
        }
    }
}

fn common_size(a: &ImpulseResponse, b: &ImpulseResponse) -> (usize, usize) {
    (
        a.num_channels.min(b.num_channels),
        a.num_samples.min(b.num_samples),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sizes() {
        let ir = ImpulseResponse::new(0.00625, 1, 48000);
        assert_eq!((ir.num_channels(), ir.num_samples()), (4, 300));
        assert_eq!(ImpulseResponse::new(0.0, 3, 48000).num_samples(), 0);
    }

    #[test]
    fn copy_keeps_the_rest() {
        let mut src = ImpulseResponse::with_size(1, 2);
        src.data_mut().fill(1.0);
        let mut dst = ImpulseResponse::with_size(2, 3);
        dst.data_mut().fill(2.0);
        dst.copy_from(&src);
        assert_eq!(dst.data(), [1.0, 1.0, 2.0, 2.0, 2.0, 2.0]);
    }
}
