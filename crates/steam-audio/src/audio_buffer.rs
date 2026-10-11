//! Deinterleaved multi-channel audio buffers (upstream `AudioBuffer`) and Ambisonics format conversion.
//!
//! [`AudioBuffer`] is generic over its storage, a contiguous planar array (`channels × samples`, channel-major):
//! an owned `Vec<f32>` by default, or a borrowed slice for the views [`AudioBufferRef`] and [`AudioBufferMut`].
//! Views replace upstream's non-owning constructors (external data arrays and the single-channel view of another
//! buffer) and never allocate.

use std::ops::{Index, IndexMut};

use crate::array_math;

/// Whether an effect still has a tail to output after its input has stopped.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum AudioEffectState {
    TailRemaining,
    TailComplete,
}

/// Ambisonics channel ordering and normalization. N3D is used internally for everything.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum AmbisonicsType {
    /// ACN ordering, N3D normalization.
    N3d,
    /// ACN ordering, SN3D normalization.
    Sn3d,
    /// Furse-Malham ordering and normalization (orders 0-3 only).
    FuMa,
}

/// Sampling rate and frame size shared by the effects of one audio pipeline.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct AudioSettings {
    pub sampling_rate: u32,
    pub frame_size: usize,
}

impl AudioSettings {
    pub fn new(sampling_rate: u32, frame_size: usize) -> Self {
        Self {
            sampling_rate,
            frame_size,
        }
    }
}

/// A deinterleaved buffer of audio: `num_channels` channels of `num_samples` samples each, stored channel-major
/// in one contiguous array.
///
/// `buffer[i]` is channel `i` as a slice.
#[derive(Debug, Clone, PartialEq)]
pub struct AudioBuffer<S = Vec<f32>> {
    num_channels: usize,
    num_samples: usize,
    data: S,
}

/// A borrowed, read-only view of planar audio data.
pub type AudioBufferRef<'a> = AudioBuffer<&'a [f32]>;

/// A borrowed, writable view of planar audio data.
pub type AudioBufferMut<'a> = AudioBuffer<&'a mut [f32]>;

impl AudioBuffer {
    /// Creates a silent buffer that owns its data.
    pub fn new(num_channels: usize, num_samples: usize) -> Self {
        Self {
            num_channels,
            num_samples,
            data: vec![0.0; num_channels * num_samples],
        }
    }
}

impl<S: AsRef<[f32]>> AudioBuffer<S> {
    /// Wraps planar data (`channels × samples`, channel-major).
    ///
    /// Panics if `data` does not hold exactly `num_channels * num_samples` samples.
    pub fn from_planar(num_channels: usize, num_samples: usize, data: S) -> Self {
        assert_eq!(
            data.as_ref().len(),
            num_channels * num_samples,
            "planar data length must be num_channels * num_samples"
        );
        Self {
            num_channels,
            num_samples,
            data,
        }
    }

    pub fn num_channels(&self) -> usize {
        self.num_channels
    }

    pub fn num_samples(&self) -> usize {
        self.num_samples
    }

    /// All samples, channel-major.
    pub fn as_slice(&self) -> &[f32] {
        self.data.as_ref()
    }

    /// Returns the underlying storage.
    pub fn into_inner(self) -> S {
        self.data
    }

    /// Channel `i`. Panics if `i >= num_channels`.
    pub fn channel(&self, i: usize) -> &[f32] {
        assert!(i < self.num_channels, "channel index out of range");
        &self.data.as_ref()[i * self.num_samples..(i + 1) * self.num_samples]
    }

    /// Iterates over the channels in order.
    pub fn channels(&self) -> impl ExactSizeIterator<Item = &[f32]> {
        (0..self.num_channels).map(|i| self.channel(i))
    }

    /// Borrows the whole buffer as a view.
    pub fn view(&self) -> AudioBufferRef<'_> {
        AudioBuffer {
            num_channels: self.num_channels,
            num_samples: self.num_samples,
            data: self.data.as_ref(),
        }
    }

    /// Borrows channel `i` as a mono buffer (upstream `AudioBuffer(other, channel)`).
    pub fn channel_view(&self, i: usize) -> AudioBufferRef<'_> {
        AudioBuffer {
            num_channels: 1,
            num_samples: self.num_samples,
            data: self.channel(i),
        }
    }

    /// Interleaves the buffer into `out` (upstream `read`).
    ///
    /// Panics if `out` does not hold exactly `num_channels * num_samples` samples.
    pub fn interleave(&self, out: &mut [f32]) {
        assert_eq!(out.len(), self.data.as_ref().len(), "interleaved length");
        if self.num_channels == 0 {
            return;
        }
        for (i, frame) in out.chunks_exact_mut(self.num_channels).enumerate() {
            for (j, o) in frame.iter_mut().enumerate() {
                *o = self.channel(j)[i];
            }
        }
    }
}

impl<S: AsRef<[f32]> + AsMut<[f32]>> AudioBuffer<S> {
    /// All samples, channel-major.
    pub fn as_mut_slice(&mut self) -> &mut [f32] {
        self.data.as_mut()
    }

    /// Channel `i`. Panics if `i >= num_channels`.
    pub fn channel_mut(&mut self, i: usize) -> &mut [f32] {
        assert!(i < self.num_channels, "channel index out of range");
        let n = self.num_samples;
        &mut self.data.as_mut()[i * n..(i + 1) * n]
    }

    /// Borrows the whole buffer as a writable view.
    pub fn view_mut(&mut self) -> AudioBufferMut<'_> {
        AudioBuffer {
            num_channels: self.num_channels,
            num_samples: self.num_samples,
            data: self.data.as_mut(),
        }
    }

    /// Borrows channel `i` as a writable mono buffer (upstream `AudioBuffer(other, channel)`).
    pub fn channel_view_mut(&mut self, i: usize) -> AudioBufferMut<'_> {
        let num_samples = self.num_samples;
        AudioBuffer {
            num_channels: 1,
            num_samples,
            data: self.channel_mut(i),
        }
    }

    /// Sets every sample to zero.
    pub fn make_silent(&mut self) {
        self.data.as_mut().fill(0.0);
    }

    /// Deinterleaves `input` into the buffer (upstream `write`).
    ///
    /// Panics if `input` does not hold exactly `num_channels * num_samples` samples.
    pub fn deinterleave(&mut self, input: &[f32]) {
        assert_eq!(input.len(), self.data.as_ref().len(), "interleaved length");
        if self.num_channels == 0 {
            return;
        }
        for (i, frame) in input.chunks_exact(self.num_channels).enumerate() {
            for (j, &x) in frame.iter().enumerate() {
                self.channel_mut(j)[i] = x;
            }
        }
    }

    /// Multiplies every sample by `volume`.
    pub fn scale(&mut self, volume: f32) {
        for x in self.data.as_mut() {
            *x *= volume;
        }
    }
}

impl<S: AsRef<[f32]>> Index<usize> for AudioBuffer<S> {
    type Output = [f32];

    fn index(&self, i: usize) -> &[f32] {
        self.channel(i)
    }
}

impl<S: AsRef<[f32]> + AsMut<[f32]>> IndexMut<usize> for AudioBuffer<S> {
    fn index_mut(&mut self, i: usize) -> &mut [f32] {
        self.channel_mut(i)
    }
}

/// Adds `input` to `out`. Panics if the shapes differ.
pub fn mix(
    input: &AudioBuffer<impl AsRef<[f32]>>,
    out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
) {
    assert_eq!(input.num_channels, out.num_channels, "channel count");
    assert_eq!(input.num_samples, out.num_samples, "sample count");

    for (o, &x) in out.as_mut_slice().iter_mut().zip(input.as_slice()) {
        *o += x;
    }
}

/// Downmixes `input` to the mono buffer `out`: channels are summed and multiplied by `1 / num_channels`.
///
/// Panics if `out` is not mono, the sample counts differ or `input` has no channels.
pub fn downmix(
    input: &AudioBuffer<impl AsRef<[f32]>>,
    out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
) {
    assert_eq!(input.num_samples, out.num_samples, "sample count");
    assert_eq!(out.num_channels, 1, "downmix output must be mono");

    let mono = out.channel_mut(0);
    mono.copy_from_slice(input.channel(0));
    for channel in input.channels().skip(1) {
        for (o, &x) in mono.iter_mut().zip(channel) {
            *o += x;
        }
    }

    let gain = 1.0 / input.num_channels as f32;
    for o in mono {
        *o *= gain;
    }
}

/// FuMa channel index for each ACN channel index.
const FUMA_FOR_ACN_INDEX: [usize; 16] = [0, 2, 3, 1, 8, 6, 4, 5, 7, 15, 13, 11, 9, 10, 12, 14];

/// ACN channel index for each FuMa channel index.
const ACN_FOR_FUMA_INDEX: [usize; 16] = [0, 3, 1, 2, 6, 7, 5, 8, 4, 12, 13, 11, 14, 10, 15, 9];

/// SN3D → FuMa gain for each FuMa channel index.
fn fuma_for_sn3d_factor() -> [f32; 16] {
    let two_over_sqrt3 = 2.0 / 3.0f32.sqrt();
    let sqrt_45_over_32 = (45.0f32 / 32.0).sqrt();
    let three_over_sqrt5 = 3.0 / 5.0f32.sqrt();
    let sqrt_8_over_5 = (8.0f32 / 5.0).sqrt();
    [
        1.0 / 2.0f32.sqrt(),
        1.0,
        1.0,
        1.0,
        1.0,
        two_over_sqrt3,
        two_over_sqrt3,
        two_over_sqrt3,
        two_over_sqrt3,
        1.0,
        sqrt_45_over_32,
        sqrt_45_over_32,
        three_over_sqrt5,
        three_over_sqrt5,
        sqrt_8_over_5,
        sqrt_8_over_5,
    ]
}

/// Converts `input` from one Ambisonics format to another, reordering channels and rescaling them.
///
/// Conversions between N3D and SN3D work for any order; conversions to or from FuMa are defined for orders 0-3
/// (up to 16 channels). Panics if the shapes differ, or if a FuMa conversion has more than 16 channels.
pub fn convert_ambisonics(
    in_type: AmbisonicsType,
    out_type: AmbisonicsType,
    input: &AudioBuffer<impl AsRef<[f32]>>,
    out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
) {
    use AmbisonicsType::{FuMa, N3d, Sn3d};

    assert_eq!(input.num_channels, out.num_channels, "channel count");
    assert_eq!(input.num_samples, out.num_samples, "sample count");

    let fuma_factor = fuma_for_sn3d_factor();

    for in_index in 0..input.num_channels {
        let out_index = match (in_type, out_type) {
            (N3d | Sn3d, FuMa) => FUMA_FOR_ACN_INDEX[in_index],
            (FuMa, N3d | Sn3d) => ACN_FOR_FUMA_INDEX[in_index],
            _ => in_index,
        };

        let acn_in_index = if in_type != FuMa {
            in_index
        } else {
            ACN_FOR_FUMA_INDEX[in_index]
        };
        let l = (acn_in_index as f32).sqrt().floor() as i32;
        let two_l_plus_one = 2.0 * l as f32 + 1.0;

        let factor = match (in_type, out_type) {
            (N3d, Sn3d) => 1.0 / two_l_plus_one.sqrt(),
            (N3d, FuMa) => (1.0 / two_l_plus_one.sqrt()) * fuma_factor[out_index],
            (Sn3d, N3d) => two_l_plus_one.sqrt(),
            (Sn3d, FuMa) => fuma_factor[out_index],
            (FuMa, Sn3d) => 1.0 / fuma_factor[in_index],
            (FuMa, N3d) => two_l_plus_one.sqrt() * (1.0 / fuma_factor[in_index]),
            _ => 1.0,
        };

        array_math::scale(input.channel(in_index), factor, out.channel_mut(out_index));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn interleave_round_trip() {
        let buffer = AudioBuffer::from_planar(2, 3, vec![1.0, 2.0, 3.0, 4.0, 5.0, 6.0]);
        let mut interleaved = [0.0; 6];
        buffer.interleave(&mut interleaved);
        assert_eq!(interleaved, [1.0, 4.0, 2.0, 5.0, 3.0, 6.0]);

        let mut back = AudioBuffer::new(2, 3);
        back.deinterleave(&interleaved);
        assert_eq!(back, buffer);
    }

    #[test]
    fn channel_views_alias_the_parent() {
        let mut buffer = AudioBuffer::new(3, 4);
        buffer.channel_view_mut(1).scale(0.0);
        buffer.channel_view_mut(2)[0].fill(2.0);
        assert_eq!(buffer[2], [2.0; 4]);
        assert_eq!(buffer.channel_view(2).as_slice(), &[2.0; 4]);
        assert_eq!(buffer.channel_view(2).num_channels(), 1);

        buffer.make_silent();
        assert!(buffer.as_slice().iter().all(|&x| x == 0.0));
    }

    #[test]
    fn views_wrap_borrowed_data() {
        let mut data = [1.0, 2.0, 3.0, 4.0];
        let mut view = AudioBufferMut::from_planar(2, 2, &mut data[..]);
        view.scale(2.0);
        assert_eq!(view[1], [6.0, 8.0]);
        assert_eq!(data, [2.0, 4.0, 6.0, 8.0]);
    }

    #[test]
    fn zero_samples() {
        let buffer = AudioBuffer::new(2, 0);
        assert_eq!(buffer.channels().len(), 2);
        assert!(buffer[1].is_empty());
        buffer.interleave(&mut []);
    }

    #[test]
    #[should_panic(expected = "planar data length")]
    fn from_planar_checks_length() {
        AudioBuffer::from_planar(2, 3, vec![0.0; 5]);
    }

    #[test]
    #[should_panic(expected = "mono")]
    fn downmix_requires_mono_output() {
        downmix(&AudioBuffer::new(2, 4), &mut AudioBuffer::new(2, 4));
    }
}
