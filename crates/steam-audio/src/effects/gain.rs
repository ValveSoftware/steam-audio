//! Smoothed gain of a mono signal (upstream `GainEffect`).

use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};

/// Number of frames over which a gain change is spread: each frame moves a quarter of the remaining way.
const NUM_INTERPOLATION_FRAMES: usize = 4;

/// Applies a gain to a mono signal. After the first frame, the gain approaches each new target exponentially (a
/// quarter of the way per frame), ramping linearly within each frame.
#[derive(Debug, Clone)]
pub struct GainEffect {
    frame_size: usize,
    prev_gain: f32,
    first_frame: bool,
}

impl GainEffect {
    pub fn new(audio_settings: &AudioSettings) -> Self {
        Self {
            frame_size: audio_settings.frame_size,
            prev_gain: 0.0,
            first_frame: true,
        }
    }

    pub fn reset(&mut self) {
        self.prev_gain = 0.0;
        self.first_frame = true;
    }

    /// Scales the mono `input` by `gain` into `out`.
    ///
    /// # Panics
    ///
    /// If either buffer is not mono or does not hold one frame.
    pub fn apply(
        &mut self,
        gain: f32,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(input.num_channels(), 1, "input must be mono");
        assert_eq!(input.num_samples(), self.frame_size, "frame size");
        assert_eq!(out.num_channels(), 1, "output must be mono");
        assert_eq!(out.num_samples(), self.frame_size, "frame size");
        out.channel_mut(0).copy_from_slice(input.channel(0));
        self.process(gain, out.channel_mut(0));
        AudioEffectState::TailComplete
    }

    /// [`apply`](Self::apply) with the same buffer as input and output.
    pub fn apply_in_place(
        &mut self,
        gain: f32,
        buffer: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(buffer.num_channels(), 1, "buffer must be mono");
        assert_eq!(buffer.num_samples(), self.frame_size, "frame size");
        self.process(gain, buffer.channel_mut(0));
        AudioEffectState::TailComplete
    }

    /// Scales one frame in place. Scaling a copy of the input gives the same result as upstream's out-of-place loop.
    pub(crate) fn process(&mut self, gain: f32, samples: &mut [f32]) {
        assert_eq!(samples.len(), self.frame_size, "frame size");

        if self.first_frame {
            for s in samples {
                *s *= gain;
            }
            self.prev_gain = gain;
            self.first_frame = false;
        } else {
            let target_gain =
                self.prev_gain + (1.0 / NUM_INTERPOLATION_FRAMES as f32) * (gain - self.prev_gain);

            let mut cur_gain = self.prev_gain;
            let d_gain = (target_gain - self.prev_gain) / self.frame_size as f32;

            for s in samples {
                *s *= cur_gain;
                cur_gain += d_gain;
            }

            self.prev_gain = target_gain;
        }
    }

    /// Applies the last gain again (upstream `tailApply`).
    pub fn tail_apply(
        &mut self,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        self.apply(self.prev_gain, input, out)
    }

    /// The gain effect has no tail: outputs silence.
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        out.make_silent();
        AudioEffectState::TailComplete
    }

    pub fn num_tail_samples_remaining(&self) -> usize {
        0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn approaches_target_by_a_quarter_per_frame() {
        let mut effect = GainEffect::new(&AudioSettings::new(48000, 4));
        let input = AudioBuffer::from_planar(1, 4, vec![1.0; 4]);
        let mut out = AudioBuffer::new(1, 4);

        effect.apply(2.0, &input, &mut out);
        assert_eq!(out.channel(0), &[2.0; 4]);

        // Target 2 + (6 - 2) / 4 = 3, ramping by 1/4 per sample from 2.
        effect.apply(6.0, &input, &mut out);
        assert_eq!(out.channel(0), &[2.0, 2.25, 2.5, 2.75]);

        let mut buffer = input.clone();
        effect.apply_in_place(6.0, &mut buffer);
        assert_eq!(buffer.channel(0), &[3.0, 3.1875, 3.375, 3.5625]);
    }
}
