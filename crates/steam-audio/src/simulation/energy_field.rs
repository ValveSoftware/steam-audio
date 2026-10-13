//! Energy fields (upstream `energy_field.{h,cpp}`): the energy arriving at a listener over time, per Ambisonics channel
//! and frequency band, in bins of [`EnergyField::BIN_DURATION`]. Reflection simulation produces them and the
//! [`Reconstructor`](super::Reconstructor) turns them into impulse responses.

use crate::array_math;
use crate::bands::NUM_BANDS;
use crate::sh::num_coeffs_for_order;

/// A `channels × bands × bins` array of energies (upstream `EnergyField`). The default value is empty.
///
/// Upstream's `swap` is [`std::mem::swap`]. Serialization comes with the rest of stage 6's serialization.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct EnergyField {
    num_channels: usize,
    num_bins: usize,
    data: Vec<f32>,
}

impl EnergyField {
    /// Duration of one bin, in seconds.
    pub const BIN_DURATION: f32 = 1e-2;

    /// A silent field of `ceil(duration / BIN_DURATION)` bins for an Ambisonics `order`.
    pub fn new(duration: f32, order: usize) -> Self {
        let num_bins = (duration / Self::BIN_DURATION).ceil() as usize;
        Self::with_size(num_coeffs_for_order(order), num_bins)
    }

    /// A silent field with the given dimensions.
    pub fn with_size(num_channels: usize, num_bins: usize) -> Self {
        Self {
            num_channels,
            num_bins,
            data: vec![0.0; num_channels * NUM_BANDS * num_bins],
        }
    }

    pub fn num_channels(&self) -> usize {
        self.num_channels
    }

    pub fn num_bins(&self) -> usize {
        self.num_bins
    }

    /// All energies, channel-major, then band, then bin (upstream `flatData`).
    pub fn data(&self) -> &[f32] {
        &self.data
    }

    /// All energies, channel-major, then band, then bin (upstream `flatData`).
    pub fn data_mut(&mut self) -> &mut [f32] {
        &mut self.data
    }

    /// The bins of `band` in `channel` (upstream `field[channel][band]`). Panics if either is out of range.
    pub fn band(&self, channel: usize, band: usize) -> &[f32] {
        let start = self.offset(channel, band);
        &self.data[start..start + self.num_bins]
    }

    /// The bins of `band` in `channel` (upstream `field[channel][band]`). Panics if either is out of range.
    pub fn band_mut(&mut self, channel: usize, band: usize) -> &mut [f32] {
        let start = self.offset(channel, band);
        &mut self.data[start..start + self.num_bins]
    }

    /// Zeroes every energy.
    pub fn reset(&mut self) {
        self.data.fill(0.0);
    }

    /// Copies as many channels and bins of `other` as both fields have; the rest is left as it is.
    pub fn copy_from(&mut self, other: &Self) {
        let n = self.num_bins.min(other.num_bins);
        for i in 0..self.num_channels.min(other.num_channels) {
            for j in 0..NUM_BANDS {
                self.band_mut(i, j)[..n].copy_from_slice(&other.band(i, j)[..n]);
            }
        }
    }

    /// `out = in1 + in2` over the channels and bins all three have; the rest of `out` is left as it is.
    pub fn add(in1: &Self, in2: &Self, out: &mut Self) {
        let num_channels = in1.num_channels.min(in2.num_channels).min(out.num_channels);
        let n = in1.num_bins.min(in2.num_bins).min(out.num_bins);
        for i in 0..num_channels {
            for j in 0..NUM_BANDS {
                array_math::add(
                    &in1.band(i, j)[..n],
                    &in2.band(i, j)[..n],
                    &mut out.band_mut(i, j)[..n],
                );
            }
        }
    }

    /// `out = input * scalar` over the channels and bins both have; the rest of `out` is left as it is.
    pub fn scale(input: &Self, scalar: f32, out: &mut Self) {
        let (num_channels, n) = common_size(input, out);
        for i in 0..num_channels {
            for j in 0..NUM_BANDS {
                array_math::scale(&input.band(i, j)[..n], scalar, &mut out.band_mut(i, j)[..n]);
            }
        }
    }

    /// `out += input * scalar` over the channels and bins both have.
    pub fn scale_accumulate(input: &Self, scalar: f32, out: &mut Self) {
        let (num_channels, n) = common_size(input, out);
        for i in 0..num_channels {
            for j in 0..NUM_BANDS {
                array_math::scale_accumulate(
                    &input.band(i, j)[..n],
                    scalar,
                    &mut out.band_mut(i, j)[..n],
                );
            }
        }
    }

    fn offset(&self, channel: usize, band: usize) -> usize {
        assert!(channel < self.num_channels, "channel index out of range");
        assert!(band < NUM_BANDS, "band index out of range");
        (channel * NUM_BANDS + band) * self.num_bins
    }
}

fn common_size(a: &EnergyField, b: &EnergyField) -> (usize, usize) {
    (
        a.num_channels.min(b.num_channels),
        a.num_bins.min(b.num_bins),
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sizes() {
        let field = EnergyField::new(0.0123, 2);
        assert_eq!((field.num_channels(), field.num_bins()), (9, 2));
        assert_eq!(field.data().len(), 9 * NUM_BANDS * 2);
        assert_eq!(EnergyField::new(0.0, 0).num_bins(), 0);
    }

    #[test]
    fn operations_use_the_common_size() {
        let mut a = EnergyField::with_size(2, 3);
        a.data_mut().fill(1.0);
        let mut out = EnergyField::with_size(1, 4);
        out.data_mut().fill(5.0);
        EnergyField::scale_accumulate(&a, 2.0, &mut out);
        assert_eq!(out.band(0, 1), [7.0, 7.0, 7.0, 5.0]);
    }
}
