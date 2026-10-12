//! Three-band equalizer of a mono signal (upstream `EQEffect`).

use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::bands::{HIGH_CUTOFF_FREQUENCIES, LOW_CUTOFF_FREQUENCIES, NUM_BANDS};
use crate::iir::{Iir, IirFilterer};
use crate::math::cpp_max;

/// Smallest gain [`EqEffect::normalize_gains`] leaves in a band (−24 dB), for a good filter response.
const MIN_EQ_GAIN: f32 = 0.0625;

/// Applies a gain per frequency band (see [`crate::bands`]) with a low-shelf, a peaking and a high-shelf filter in
/// series. When the gains change, the frame is crossfaded linearly from the old filters to the new ones.
#[derive(Debug, Clone)]
pub struct EqEffect {
    sampling_rate: u32,
    frame_size: usize,
    /// Two filter sets per band: the current gains and the previous ones, for crossfading.
    filters: [[IirFilterer; 2]; NUM_BANDS],
    temp: Vec<f32>,
    prev_gains: [f32; NUM_BANDS],
    current: usize,
    first_frame: bool,
}

impl EqEffect {
    pub fn new(audio_settings: &AudioSettings) -> Self {
        let mut effect = Self {
            sampling_rate: audio_settings.sampling_rate,
            frame_size: audio_settings.frame_size,
            filters: Default::default(),
            temp: vec![0.0; audio_settings.frame_size],
            prev_gains: [1.0; NUM_BANDS],
            current: 0,
            first_frame: true,
        };
        effect.reset();
        effect
    }

    /// Sets every gain back to 1 and forgets the previous gains. Like upstream, the filter states are kept.
    pub fn reset(&mut self) {
        self.prev_gains = [1.0; NUM_BANDS];

        self.set_filter_gains(0, self.prev_gains);
        self.set_filter_gains(1, self.prev_gains);

        self.current = 0;

        self.first_frame = true;
    }

    /// Filters the mono `input` with the band `gains` into `out`.
    ///
    /// # Panics
    ///
    /// If either buffer is not mono or does not hold one frame.
    pub fn apply(
        &mut self,
        gains: &[f32; NUM_BANDS],
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(input.num_channels(), 1, "input must be mono");
        assert_eq!(input.num_samples(), self.frame_size, "frame size");
        assert_eq!(out.num_channels(), 1, "output must be mono");
        out.channel_mut(0).copy_from_slice(input.channel(0));
        self.process(gains, out.channel_mut(0));
        AudioEffectState::TailComplete
    }

    /// [`apply`](Self::apply) with the same buffer as input and output.
    pub fn apply_in_place(
        &mut self,
        gains: &[f32; NUM_BANDS],
        buffer: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(buffer.num_channels(), 1, "buffer must be mono");
        self.process(gains, buffer.channel_mut(0));
        AudioEffectState::TailComplete
    }

    /// Filters one frame in place. Upstream filters `in` into `out` (and into a scratch buffer when crossfading);
    /// every filter reads its whole input before writing, so filtering a copy gives the same result.
    pub(crate) fn process(&mut self, gains: &[f32; NUM_BANDS], samples: &mut [f32]) {
        assert_eq!(samples.len(), self.frame_size, "frame size");

        if self.first_frame {
            self.prev_gains = *gains;
            self.set_filter_gains(self.current, *gains);
            self.first_frame = false;
        }

        let gains_changed = self.prev_gains.iter().zip(gains).any(|(p, g)| p != g);

        if gains_changed {
            let previous = self.current;
            self.current = 1 - self.current;

            self.set_filter_gains(self.current, *gains);

            for band in &mut self.filters {
                let [a, b] = band;
                if self.current == 1 {
                    b.copy_state(a);
                } else {
                    a.copy_state(b);
                }
            }

            self.temp.copy_from_slice(samples);
            apply_filter_cascade(&mut self.filters, previous, &mut self.temp);
            apply_filter_cascade(&mut self.filters, self.current, samples);

            let frame_size = self.frame_size as f32;
            for (i, (o, &t)) in samples.iter_mut().zip(&self.temp).enumerate() {
                let weight = i as f32 / frame_size;
                *o = weight * *o + (1.0 - weight) * t;
            }

            self.prev_gains = *gains;
        } else {
            apply_filter_cascade(&mut self.filters, self.current, samples);
        }
    }

    /// Applies the last gains again (upstream `tailApply`).
    pub fn tail_apply(
        &mut self,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        let gains = self.prev_gains;
        self.apply(&gains, input, out)
    }

    /// The EQ effect has no tail: outputs silence.
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

    /// Divides the band gains by their maximum and moves that maximum into `overall_gain`, then raises every band
    /// gain to at least 0.0625 (−24 dB). If no gain is at least `f32::MIN_POSITIVE`, the band gains become 1 and the
    /// overall gain 0.
    pub fn normalize_gains(eq_gains: &mut [f32; NUM_BANDS], overall_gain: &mut f32) {
        let max_gain = eq_gains.iter().fold(0.0, |m, &g| cpp_max(m, g));

        if max_gain < f32::MIN_POSITIVE {
            *overall_gain = 0.0;
            *eq_gains = [1.0; NUM_BANDS];
        } else {
            for g in eq_gains.iter_mut() {
                *g /= max_gain;
                *g = cpp_max(*g, MIN_EQ_GAIN);
            }

            *overall_gain *= max_gain;
        }
    }

    fn set_filter_gains(&mut self, index: usize, gains: [f32; NUM_BANDS]) {
        let rate = self.sampling_rate;
        self.filters[0][index].set_filter(Iir::low_shelf(
            HIGH_CUTOFF_FREQUENCIES[0],
            gains[0],
            rate,
        ));

        for i in 1..NUM_BANDS - 1 {
            self.filters[i][index].set_filter(Iir::peaking(
                LOW_CUTOFF_FREQUENCIES[i],
                HIGH_CUTOFF_FREQUENCIES[i],
                gains[i],
                rate,
            ));
        }

        let last = NUM_BANDS - 1;
        self.filters[last][index].set_filter(Iir::high_shelf(
            LOW_CUTOFF_FREQUENCIES[last],
            gains[last],
            rate,
        ));
    }
}

fn apply_filter_cascade(
    filters: &mut [[IirFilterer; 2]; NUM_BANDS],
    index: usize,
    samples: &mut [f32],
) {
    for band in filters {
        band[index].apply_in_place(samples);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalize_gains() {
        let mut gains = [0.5, 0.01, 0.25];
        let mut overall = 2.0;
        EqEffect::normalize_gains(&mut gains, &mut overall);
        assert_eq!(gains, [1.0, MIN_EQ_GAIN, 0.5]);
        assert_eq!(overall, 1.0);

        let mut gains = [0.0; NUM_BANDS];
        let mut overall = 2.0;
        EqEffect::normalize_gains(&mut gains, &mut overall);
        assert_eq!(gains, [1.0; NUM_BANDS]);
        assert_eq!(overall, 0.0);
    }

    #[test]
    fn unit_gains_pass_the_signal() {
        let mut effect = EqEffect::new(&AudioSettings::new(48000, 64));
        let input = AudioBuffer::from_planar(
            1,
            64,
            (0..64).map(|i| (i as f32 * 0.3).sin()).collect::<Vec<_>>(),
        );
        let mut out = AudioBuffer::new(1, 64);
        effect.apply(&[1.0; NUM_BANDS], &input, &mut out);
        for (o, x) in out.channel(0).iter().zip(input.channel(0)) {
            assert!((o - x).abs() < 1e-4, "{o} vs {x}");
        }
    }
}
