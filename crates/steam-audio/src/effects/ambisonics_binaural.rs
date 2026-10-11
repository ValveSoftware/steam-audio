//! Binaural rendering of Ambisonics (upstream `AmbisonicsBinauralEffect`): each channel is convolved with the
//! Ambisonics HRTF of its coefficient and the results are summed, weighted per band by max-rE gains.

use super::OverlapAddConvolutionEffect;
use super::overlap_add::OverlapAddConvolutionSettings;
use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::hrtf::{HrtfDatabase, NUM_EARS};
use crate::math::DEGREES_TO_RADIANS;
use crate::{array_math, sh};

/// Renders an Ambisonics buffer to stereo with the Ambisonics HRTFs of an [`HrtfDatabase`].
#[derive(Debug, Clone)]
pub struct AmbisonicsBinauralEffect {
    sampling_rate: u32,
    frame_size: usize,
    max_order: usize,
    hrir_size: usize,
    overlap_add_effects: Vec<OverlapAddConvolutionEffect>,
    overlap_add_effect_states: Vec<AudioEffectState>,
    spatialized_channel: AudioBuffer,
}

impl AmbisonicsBinauralEffect {
    /// Renders up to `max_order`. `hrtf` only sizes the effect; [`apply`](Self::apply) takes the database to use.
    ///
    /// The Ambisonics HRTFs go up to order [`MAX_AMBISONICS_ORDER`](crate::hrtf::MAX_AMBISONICS_ORDER), so applying a
    /// higher order panics.
    pub fn new(audio_settings: &AudioSettings, max_order: usize, hrtf: &HrtfDatabase) -> Self {
        let num_coeffs = sh::num_coeffs_for_order(max_order);
        let mut effect = Self {
            sampling_rate: audio_settings.sampling_rate,
            frame_size: audio_settings.frame_size,
            max_order,
            hrir_size: hrtf.num_samples(),
            overlap_add_effects: Vec::with_capacity(num_coeffs),
            overlap_add_effect_states: vec![AudioEffectState::TailComplete; num_coeffs],
            spatialized_channel: AudioBuffer::new(NUM_EARS, audio_settings.frame_size),
        };
        effect.init(hrtf);
        effect.reset();
        effect
    }

    /// (Re)creates the convolution effects for the HRIR length of `hrtf`.
    fn init(&mut self, hrtf: &HrtfDatabase) {
        self.hrir_size = hrtf.num_samples();
        let audio_settings = AudioSettings::new(self.sampling_rate, self.frame_size);
        let settings = OverlapAddConvolutionSettings {
            num_channels: NUM_EARS,
            ir_size: self.hrir_size,
        };
        let num_coeffs = self.overlap_add_effect_states.len();
        self.overlap_add_effects.clear();
        self.overlap_add_effects.extend(
            (0..num_coeffs).map(|_| OverlapAddConvolutionEffect::new(&audio_settings, &settings)),
        );
    }

    pub fn reset(&mut self) {
        for (effect, state) in self
            .overlap_add_effects
            .iter_mut()
            .zip(&mut self.overlap_add_effect_states)
        {
            effect.reset();
            *state = AudioEffectState::TailComplete;
        }
    }

    pub fn num_tail_samples_remaining(&self) -> usize {
        self.overlap_add_effects
            .iter()
            .map(OverlapAddConvolutionEffect::num_tail_samples_remaining)
            .max()
            .unwrap_or(0)
    }

    /// Renders one frame of order-`order` Ambisonics to stereo.
    ///
    /// If `hrtf` has a different HRIR length than the database the effect was last used with, the convolutions are
    /// rebuilt (allocating, and dropping the tail), like upstream.
    ///
    /// # Panics
    ///
    /// If `order` exceeds the maximum order (or [`MAX_AMBISONICS_ORDER`](crate::hrtf::MAX_AMBISONICS_ORDER)), the
    /// input does not have `num_coeffs_for_order(order)` channels, the output is not stereo, or either is not one frame
    /// long.
    pub fn apply(
        &mut self,
        hrtf: &HrtfDatabase,
        order: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(
            input.num_channels(),
            sh::num_coeffs_for_order(order),
            "input channels must match the order"
        );
        self.process(hrtf, order, input, out)
    }

    /// [`apply`](Self::apply) with an input that may have more channels than `order` needs (upstream
    /// `AmbisonicsDecodeEffect` passes a buffer of the maximum order); only the first channels are read.
    pub(crate) fn process(
        &mut self,
        hrtf: &HrtfDatabase,
        order: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(input.num_samples(), out.num_samples(), "sample count");
        assert!(order <= self.max_order, "order exceeds the maximum order");
        assert!(
            input.num_channels() >= sh::num_coeffs_for_order(order),
            "input channels must cover the order"
        );
        assert_eq!(out.num_channels(), NUM_EARS, "output must be stereo");

        if self.hrir_size != hrtf.num_samples() {
            self.init(hrtf);
        }

        out.make_silent();

        let cosine = ((137.9 * DEGREES_TO_RADIANS) / (order as f32 + 1.51)).cos();

        let mut i = 0;
        for l in 0..=order {
            let scalar = sh::legendre(l, cosine);
            for _ in 0..2 * l + 1 {
                let hrtf_data = hrtf.ambisonics_hrtf(i);
                self.overlap_add_effect_states[i] = self.overlap_add_effects[i].apply(
                    &hrtf_data,
                    false,
                    &input.channel_view(i),
                    &mut self.spatialized_channel,
                );
                self.accumulate(scalar, out);
                i += 1;
            }
        }

        self.combined_state()
    }

    /// Outputs the next frame of the tails of all channels up to the maximum order, weighted with the maximum order's
    /// gains (like upstream, even if the last frame had a lower order).
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(out.num_channels(), NUM_EARS, "output must be stereo");

        out.make_silent();

        let cosine = ((137.9 * DEGREES_TO_RADIANS) / (self.max_order as f32 + 1.51)).cos();

        let mut i = 0;
        for l in 0..=self.max_order {
            let scalar = sh::legendre(l, cosine);
            for _ in 0..2 * l + 1 {
                self.overlap_add_effect_states[i] =
                    self.overlap_add_effects[i].tail(&mut self.spatialized_channel);
                self.accumulate(scalar, out);
                i += 1;
            }
        }

        self.combined_state()
    }

    /// `out += scalar * spatialized_channel`, over one frame of each ear.
    fn accumulate(&self, scalar: f32, out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>) {
        for j in 0..NUM_EARS {
            array_math::scale_accumulate(
                self.spatialized_channel.channel(j),
                scalar,
                &mut out.channel_mut(j)[..self.frame_size],
            );
        }
    }

    fn combined_state(&self) -> AudioEffectState {
        if self
            .overlap_add_effect_states
            .contains(&AudioEffectState::TailRemaining)
        {
            AudioEffectState::TailRemaining
        } else {
            AudioEffectState::TailComplete
        }
    }
}
