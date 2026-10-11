//! Uniformly partitioned overlap-save convolution with long IRs (upstream `overlap_save_convolution_effect`), used for
//! reflections and reverb.
//!
//! An IR is split into blocks of one frame, each zero-padded to two frames and transformed ([`OverlapSavePartitioner`]
//! into an [`OverlapSaveFir`]). Every frame, the effect transforms the last two input frames and multiply-accumulates
//! the spectra of the recent inputs with the IR blocks; the second half of the inverse transform is the output. When
//! the IR changes, the outputs with the old and new IR are crossfaded over one frame. Effects can also accumulate
//! their spectra into an [`OverlapSaveConvolutionMixer`], which needs only one inverse transform per channel.
//!
//! Upstream passes new IRs through a `TripleBuffer` or a pointer plus an "updated" flag, and swaps them with the IR
//! it holds. Here a new IR is passed as `Some(&mut fir)`: the effect takes its contents and leaves the previous IR in
//! `fir`.

use std::mem;

use super::tail_state;
use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::fft::RealFft;
use crate::math::next_pow2;
use crate::{Complex, array_math};

/// A partitioned IR spectrum: for each channel, one spectrum per block of one frame.
#[derive(Debug, Clone, PartialEq)]
pub struct OverlapSaveFir {
    num_channels: usize,
    num_blocks: usize,
    num_spectrum_samples: usize,
    data: Vec<Complex>,
}

impl OverlapSaveFir {
    /// A silent FIR with room for `ir_size` samples per channel.
    pub fn new(num_channels: usize, ir_size: usize, frame_size: usize) -> Self {
        let num_blocks = OverlapSaveConvolutionEffect::num_blocks(frame_size, ir_size);
        let num_spectrum_samples = next_pow2(2 * frame_size) / 2 + 1;
        Self {
            num_channels,
            num_blocks,
            num_spectrum_samples,
            data: vec![Complex::default(); num_channels * num_blocks * num_spectrum_samples],
        }
    }

    pub fn num_channels(&self) -> usize {
        self.num_channels
    }

    pub fn num_blocks(&self) -> usize {
        self.num_blocks
    }

    pub fn num_spectrum_samples(&self) -> usize {
        self.num_spectrum_samples
    }

    /// Spectrum of block `block` of channel `channel`. Panics if either is out of range.
    pub fn block(&self, channel: usize, block: usize) -> &[Complex] {
        let start = self.offset(channel, block);
        &self.data[start..start + self.num_spectrum_samples]
    }

    /// Spectrum of block `block` of channel `channel`. Panics if either is out of range.
    pub fn block_mut(&mut self, channel: usize, block: usize) -> &mut [Complex] {
        let start = self.offset(channel, block);
        &mut self.data[start..start + self.num_spectrum_samples]
    }

    /// Silences every block.
    pub fn reset(&mut self) {
        self.data.fill(Complex::default());
    }

    /// Copies as many channels, blocks and bins of `src` as both FIRs have (upstream `OverlapSaveFIR::copy`).
    pub fn copy_from(&mut self, src: &Self) {
        let n = self.num_spectrum_samples.min(src.num_spectrum_samples);
        for i in 0..self.num_channels.min(src.num_channels) {
            for j in 0..self.num_blocks.min(src.num_blocks) {
                self.block_mut(i, j)[..n].copy_from_slice(&src.block(i, j)[..n]);
            }
        }
    }

    fn offset(&self, channel: usize, block: usize) -> usize {
        assert!(channel < self.num_channels, "channel index out of range");
        assert!(block < self.num_blocks, "block index out of range");
        (channel * self.num_blocks + block) * self.num_spectrum_samples
    }
}

/// Splits IRs into the blocks of an [`OverlapSaveFir`].
#[derive(Debug, Clone)]
pub struct OverlapSavePartitioner {
    frame_size: usize,
    fft: RealFft,
    temp_ir_block: Vec<f32>,
}

impl OverlapSavePartitioner {
    /// Panics if `frame_size` is 0.
    pub fn new(frame_size: usize) -> Self {
        let fft = RealFft::new(2 * frame_size);
        Self {
            frame_size,
            temp_ir_block: vec![0.0; fft.num_real_samples()],
            fft,
        }
    }

    /// Replaces `fft_ir` with the first `num_samples` samples of the first `num_channels` channels of `ir` (each count
    /// limited to what `ir` and `fft_ir` hold). The upstream `ImpulseResponse` is a planar `channels × samples` array,
    /// so `ir` is an [`AudioBuffer`].
    ///
    /// Like upstream, a partial last block is copied over the previous block's samples without clearing the rest of
    /// the frame, so the stale samples (possibly from a previous call) remain part of that block.
    ///
    /// Panics if `fft_ir` was created for a different frame size.
    pub fn partition(
        &mut self,
        ir: &AudioBuffer<impl AsRef<[f32]>>,
        num_channels: usize,
        num_samples: usize,
        fft_ir: &mut OverlapSaveFir,
    ) {
        fft_ir.reset();

        let f = self.frame_size;
        let num_channels = num_channels
            .min(ir.num_channels())
            .min(fft_ir.num_channels());

        for i in 0..num_channels {
            let channel = ir.channel(i);
            let mut num_samples_left = num_samples.min(ir.num_samples());
            for j in 0..fft_ir.num_blocks() {
                let n = f.min(num_samples_left);
                num_samples_left -= n;

                if n == 0 {
                    break;
                }

                self.temp_ir_block[..n].copy_from_slice(&channel[j * f..j * f + n]);
                self.fft
                    .forward(&self.temp_ir_block, fft_ir.block_mut(i, j));
            }
        }
    }
}

/// Channel count and IR length of an [`OverlapSaveConvolutionEffect`] or [`OverlapSaveConvolutionMixer`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct OverlapSaveConvolutionSettings {
    pub num_channels: usize,
    pub ir_size: usize,
}

/// Convolves a mono input with a multi-channel partitioned IR.
#[derive(Debug, Clone)]
pub struct OverlapSaveConvolutionEffect {
    frame_size: usize,
    ir_size: usize,
    num_channels: usize,
    fft: RealFft,
    dry_block: Vec<f32>,
    fft_dry_blocks: Vec<Vec<Complex>>,
    dry_block_index: usize,
    fft_wet: Vec<Vec<Complex>>,
    prev_fft_wet: Vec<Vec<Complex>>,
    wet: Vec<Vec<f32>>,
    prev_wet: Vec<Vec<f32>>,
    /// Signed like upstream: every [`tail`](Self::tail) decrements it, also past zero.
    num_tail_blocks_remaining: isize,
    prev_fft_ir: OverlapSaveFir,
}

impl OverlapSaveConvolutionEffect {
    /// Panics if the frame size is 0.
    pub fn new(audio_settings: &AudioSettings, settings: &OverlapSaveConvolutionSettings) -> Self {
        let frame_size = audio_settings.frame_size;
        let num_channels = settings.num_channels;
        let fft = RealFft::new(2 * frame_size);
        let n = fft.num_real_samples();
        let m = fft.num_complex_samples();
        let num_blocks = Self::num_blocks(frame_size, settings.ir_size);

        Self {
            frame_size,
            ir_size: settings.ir_size,
            num_channels,
            dry_block: vec![0.0; n],
            fft_dry_blocks: vec![vec![Complex::default(); m]; num_blocks],
            dry_block_index: 0,
            fft_wet: vec![vec![Complex::default(); m]; num_channels],
            prev_fft_wet: vec![vec![Complex::default(); m]; num_channels],
            wet: vec![vec![0.0; n]; num_channels],
            prev_wet: vec![vec![0.0; n]; num_channels],
            num_tail_blocks_remaining: 0,
            prev_fft_ir: OverlapSaveFir::new(num_channels, settings.ir_size, frame_size),
            fft,
        }
    }

    /// Number of one-frame blocks needed for an IR of `ir_size` samples (computed in `f32`, as upstream).
    pub fn num_blocks(frame_size: usize, ir_size: usize) -> usize {
        (ir_size as f32 / frame_size as f32).ceil() as usize
    }

    /// Samples of tail left to output (0 once [`tail`](Self::tail) has been called past the end of the tail).
    pub fn num_tail_samples_remaining(&self) -> usize {
        self.num_tail_blocks_remaining.max(0) as usize * self.frame_size
    }

    /// Clears the input history and the tail, and silences the current IR: the output stays silent until a new IR
    /// is passed.
    pub fn reset(&mut self) {
        self.dry_block.fill(0.0);
        for block in &mut self.fft_dry_blocks {
            block.fill(Complex::default());
        }
        self.dry_block_index = 0;
        self.num_tail_blocks_remaining = 0;
        self.prev_fft_ir.reset();
    }

    /// Convolves one frame of the mono `input` and writes the first `num_channels` channels of the result to `out`
    /// (the other channels are silenced).
    ///
    /// `new_ir`, if given, becomes the current IR (the output crossfades to it over this frame) and receives the
    /// previous IR in exchange.
    ///
    /// # Panics
    ///
    /// If the input is not mono, the input and output lengths differ from the frame size, the output does not have
    /// the effect's channel count, `num_channels` exceeds it, or `new_ir` has fewer channels or blocks than needed.
    pub fn apply(
        &mut self,
        new_ir: Option<&mut OverlapSaveFir>,
        num_channels: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(
            out.num_samples(),
            self.frame_size,
            "output must be one frame long"
        );
        assert_eq!(out.num_channels(), self.num_channels, "output channels");

        let crossfade = self.convolve(new_ir, num_channels, input);
        let f = self.frame_size;

        for i in 0..num_channels {
            self.fft.inverse(&self.fft_wet[i], &mut self.wet[i]);
        }

        if crossfade {
            for i in 0..num_channels {
                self.fft
                    .inverse(&self.prev_fft_wet[i], &mut self.prev_wet[i]);
                crossfade_frame(&self.prev_wet[i][f..2 * f], &mut self.wet[i][f..2 * f]);
            }
        }

        out.make_silent();

        for i in 0..num_channels {
            out.channel_mut(i).copy_from_slice(&self.wet[i][f..2 * f]);
        }

        self.state()
    }

    /// Like [`apply`](Self::apply), but accumulates the result into `mixer` instead of an output buffer.
    ///
    /// Panics as [`apply`](Self::apply), or if the effect has fewer channels than the mixer.
    pub fn apply_to_mixer(
        &mut self,
        new_ir: Option<&mut OverlapSaveFir>,
        num_channels: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        mixer: &mut OverlapSaveConvolutionMixer,
    ) -> AudioEffectState {
        let crossfade = self.convolve(new_ir, num_channels, input);
        mixer.mix(
            &self.fft_wet,
            crossfade.then_some(self.prev_fft_wet.as_slice()),
        );
        self.state()
    }

    /// Outputs the next frame of the tail, as if the input were silent, to the first channels of `out`.
    ///
    /// Like upstream, the tail is one frame shorter than the IR, and leaves out the most recent input frame's
    /// convolution with the IR's leading blocks.
    ///
    /// Panics if `out` has more channels than the effect or is not one frame long.
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert!(
            out.num_channels() <= self.num_channels,
            "too many output channels"
        );
        assert_eq!(
            out.num_samples(),
            self.frame_size,
            "output must be one frame long"
        );

        out.make_silent();

        self.tail_spectra();

        let f = self.frame_size;
        for i in 0..out.num_channels() {
            self.fft.inverse(&self.fft_wet[i], &mut self.wet[i]);
            out.channel_mut(i).copy_from_slice(&self.wet[i][f..2 * f]);
        }

        self.state()
    }

    /// Like [`tail`](Self::tail), but accumulates the result into `mixer`.
    ///
    /// Panics if the effect has fewer channels than the mixer.
    pub fn tail_to_mixer(&mut self, mixer: &mut OverlapSaveConvolutionMixer) -> AudioEffectState {
        self.tail_spectra();
        mixer.mix(&self.fft_wet, None);
        self.state()
    }

    /// Transforms the newest input frame and computes the wet spectra (upstream's private `apply(params, in)`).
    /// Returns whether the IR changed, in which case `prev_fft_wet` holds the spectra with the previous IR.
    fn convolve(
        &mut self,
        new_ir: Option<&mut OverlapSaveFir>,
        num_channels: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
    ) -> bool {
        let f = self.frame_size;
        assert_eq!(input.num_channels(), 1, "input must be mono");
        assert_eq!(input.num_samples(), f, "input must be one frame long");
        assert!(num_channels <= self.num_channels, "too many channels");

        self.dry_block.copy_within(f..2 * f, 0);
        self.dry_block[f..2 * f].copy_from_slice(input.channel(0));

        self.dry_block_index = match self.dry_block_index {
            0 => self.fft_dry_blocks.len() - 1,
            i => i - 1,
        };

        self.fft.forward(
            &self.dry_block,
            &mut self.fft_dry_blocks[self.dry_block_index],
        );

        let num_blocks = Self::num_blocks(f, self.ir_size);
        let blocks = Blocks {
            dry: &self.fft_dry_blocks,
            index: self.dry_block_index,
        };

        let crossfade = new_ir.is_some();
        match new_ir {
            Some(new_ir) => {
                blocks.convolve(new_ir, num_channels, 0, num_blocks, &mut self.fft_wet);
                blocks.convolve(
                    &self.prev_fft_ir,
                    num_channels,
                    0,
                    num_blocks,
                    &mut self.prev_fft_wet,
                );
                mem::swap(&mut self.prev_fft_ir, new_ir);
            }
            None => {
                blocks.convolve(
                    &self.prev_fft_ir,
                    num_channels,
                    0,
                    num_blocks,
                    &mut self.fft_wet,
                );
            }
        }

        self.num_tail_blocks_remaining = num_blocks as isize - 1;

        crossfade
    }

    /// Computes the wet spectra of the next tail frame, for all channels.
    fn tail_spectra(&mut self) {
        let num_blocks = Self::num_blocks(self.frame_size, self.ir_size);
        let remaining = self.num_tail_blocks_remaining;
        let blocks = Blocks {
            dry: &self.fft_dry_blocks,
            index: self.dry_block_index,
        };

        let count = remaining.max(0) as usize;
        blocks.convolve(
            &self.prev_fft_ir,
            self.num_channels,
            (num_blocks as isize - remaining) as usize,
            count,
            &mut self.fft_wet,
        );

        self.num_tail_blocks_remaining -= 1;
    }

    fn state(&self) -> AudioEffectState {
        tail_state(self.num_tail_blocks_remaining > 0)
    }
}

/// The ring of input spectra, newest at `index`.
struct Blocks<'a> {
    dry: &'a [Vec<Complex>],
    index: usize,
}

impl Blocks<'_> {
    /// Zeroes `wet`, then accumulates, for the first `num_channels` channels, the `j`-th newest input spectrum times
    /// IR block `offset + j`, for `j` in `0..count`.
    fn convolve(
        &self,
        ir: &OverlapSaveFir,
        num_channels: usize,
        offset: usize,
        count: usize,
        wet: &mut [Vec<Complex>],
    ) {
        for w in wet.iter_mut() {
            w.fill(Complex::default());
        }

        for (i, w) in wet.iter_mut().enumerate().take(num_channels) {
            for j in 0..count {
                let dry = &self.dry[(self.index + j) % self.dry.len()];
                array_math::multiply_accumulate_complex(dry, ir.block(i, offset + j), w);
            }
        }
    }
}

/// `wet[j] = (1 − w) · prev[j] + w · wet[j]` with `w = j / len`.
fn crossfade_frame(prev: &[f32], wet: &mut [f32]) {
    let len = wet.len() as f32;
    for (j, (w, &p)) in wet.iter_mut().zip(prev).enumerate() {
        let weight = j as f32 / len;
        *w = (1.0 - weight) * p + weight * *w;
    }
}

/// Sums the spectra of several [`OverlapSaveConvolutionEffect`]s, so that each frame needs one inverse transform per
/// channel instead of one per effect and channel.
#[derive(Debug, Clone)]
pub struct OverlapSaveConvolutionMixer {
    frame_size: usize,
    num_channels: usize,
    fft: RealFft,
    fft_wet: Vec<Vec<Complex>>,
    prev_fft_wet: Vec<Vec<Complex>>,
    wet: Vec<Vec<f32>>,
    prev_wet: Vec<Vec<f32>>,
}

impl OverlapSaveConvolutionMixer {
    /// Panics if the frame size is 0. `settings.ir_size` is unused.
    pub fn new(audio_settings: &AudioSettings, settings: &OverlapSaveConvolutionSettings) -> Self {
        let frame_size = audio_settings.frame_size;
        let num_channels = settings.num_channels;
        let fft = RealFft::new(2 * frame_size);
        let n = fft.num_real_samples();
        let m = fft.num_complex_samples();

        Self {
            frame_size,
            num_channels,
            fft_wet: vec![vec![Complex::default(); m]; num_channels],
            prev_fft_wet: vec![vec![Complex::default(); m]; num_channels],
            wet: vec![vec![0.0; n]; num_channels],
            prev_wet: vec![vec![0.0; n]; num_channels],
            fft,
        }
    }

    /// Clears the accumulated spectra.
    pub fn reset(&mut self) {
        for s in self.fft_wet.iter_mut().chain(&mut self.prev_fft_wet) {
            s.fill(Complex::default());
        }
        for s in self.wet.iter_mut().chain(&mut self.prev_wet) {
            s.fill(0.0);
        }
    }

    /// Outputs the first `num_channels` channels of the mix to `out` (silencing the rest), crossfading from the mix
    /// with the effects' previous IRs, then clears the mix.
    ///
    /// Panics if `num_channels` exceeds the mixer's or `out`'s channel count, or `out` is not one frame long.
    pub fn apply(
        &mut self,
        num_channels: usize,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) {
        assert!(num_channels <= self.num_channels, "too many channels");
        assert_eq!(
            out.num_samples(),
            self.frame_size,
            "output must be one frame long"
        );

        out.make_silent();

        let f = self.frame_size;
        for i in 0..num_channels {
            self.fft.inverse(&self.fft_wet[i], &mut self.wet[i]);
            self.fft
                .inverse(&self.prev_fft_wet[i], &mut self.prev_wet[i]);

            crossfade_frame(&self.prev_wet[i][f..2 * f], &mut self.wet[i][f..2 * f]);

            out.channel_mut(i).copy_from_slice(&self.wet[i][f..2 * f]);
        }

        self.reset();
    }

    /// Adds an effect's wet spectra, and its spectra with the previous IR (`fft_wet` again if the IR did not change).
    fn mix(&mut self, fft_wet: &[Vec<Complex>], prev_fft_wet: Option<&[Vec<Complex>]>) {
        let prev_fft_wet = prev_fft_wet.unwrap_or(fft_wet);
        for i in 0..self.num_channels {
            add_assign(&mut self.fft_wet[i], &fft_wet[i]);
            add_assign(&mut self.prev_fft_wet[i], &prev_fft_wet[i]);
        }
    }
}

/// `accum[i] = x[i] + accum[i]` (upstream `ArrayMath::add(x, accum, accum)`).
fn add_assign(accum: &mut [Complex], x: &[Complex]) {
    for (a, &x) in accum.iter_mut().zip(x) {
        *a = x + *a;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn signal(n: usize, seed: f32) -> Vec<f32> {
        (0..n).map(|i| (i as f32 * seed).sin()).collect()
    }

    fn settings(num_channels: usize, ir_size: usize) -> OverlapSaveConvolutionSettings {
        OverlapSaveConvolutionSettings {
            num_channels,
            ir_size,
        }
    }

    #[test]
    fn num_blocks_rounds_up() {
        assert_eq!(OverlapSaveConvolutionEffect::num_blocks(64, 1), 1);
        assert_eq!(OverlapSaveConvolutionEffect::num_blocks(64, 64), 1);
        assert_eq!(OverlapSaveConvolutionEffect::num_blocks(64, 65), 2);
        assert_eq!(OverlapSaveFir::new(2, 150, 64).num_spectrum_samples(), 65);
    }

    /// With a whole number of blocks the partitioned IR is exact, and after the crossfade from the initial silent IR
    /// the effect is a linear convolution.
    #[test]
    fn fixed_ir_is_linear_convolution() {
        let f = 16;
        let ir_size = 48;
        let ir_data = [signal(ir_size, 0.43), signal(ir_size, 0.91)].concat();
        let ir = AudioBuffer::from_planar(2, ir_size, ir_data);

        let mut fir = OverlapSaveFir::new(2, ir_size, f);
        OverlapSavePartitioner::new(f).partition(&ir, 2, ir_size, &mut fir);

        let audio = AudioSettings::new(48000, f);
        let mut effect = OverlapSaveConvolutionEffect::new(&audio, &settings(2, ir_size));
        let mut out = AudioBuffer::new(2, f);

        let silence = AudioBuffer::new(1, f);
        effect.apply(Some(&mut fir), 2, &silence, &mut out);
        assert!(out.as_slice().iter().all(|&s| s == 0.0));
        assert!(
            fir.data.iter().all(|&c| c == Complex::default()),
            "previous IR handed back"
        );

        let num_frames = 7;
        let x = signal(num_frames * f, 0.17);
        let mut y = vec![Vec::new(); 2];
        for frame in x.chunks(f) {
            let input = AudioBuffer::from_planar(1, f, frame);
            effect.apply(None, 2, &input, &mut out);
            for (c, y) in y.iter_mut().enumerate() {
                y.extend_from_slice(out.channel(c));
            }
        }

        for (c, y) in y.iter().enumerate() {
            let h = ir.channel(c);
            for (n, &actual) in y.iter().enumerate() {
                let expected: f32 = (0..=n.min(ir_size - 1)).map(|k| h[k] * x[n - k]).sum();
                assert!(
                    (actual - expected).abs() < 1e-4,
                    "channel {c}, sample {n}: {actual} vs {expected}"
                );
            }
        }
    }

    #[test]
    fn copy_from_copies_the_overlap() {
        let mut a = OverlapSaveFir::new(1, 10, 4);
        for (i, c) in a.data.iter_mut().enumerate() {
            *c = Complex::new(i as f32, 1.0);
        }
        let mut b = OverlapSaveFir::new(2, 30, 4);
        b.copy_from(&a);
        assert_eq!(b.block(0, 0), a.block(0, 0));
        assert_eq!(b.block(0, 2), a.block(0, 2));
        assert!(
            b.block(0, 3)
                .iter()
                .chain(b.block(1, 0))
                .all(|&c| c == Complex::default())
        );
    }

    #[test]
    fn tail_counter_stops_at_zero() {
        let audio = AudioSettings::new(48000, 8);
        let mut effect = OverlapSaveConvolutionEffect::new(&audio, &settings(1, 20));
        let mut out = AudioBuffer::new(1, 8);
        let input = AudioBuffer::new(1, 8);
        assert_eq!(
            effect.apply(None, 1, &input, &mut out),
            AudioEffectState::TailRemaining
        );
        assert_eq!(effect.num_tail_samples_remaining(), 16);
        assert_eq!(effect.tail(&mut out), AudioEffectState::TailRemaining);
        assert_eq!(effect.tail(&mut out), AudioEffectState::TailComplete);
        assert_eq!(effect.tail(&mut out), AudioEffectState::TailComplete);
        assert_eq!(effect.num_tail_samples_remaining(), 0);
    }
}
