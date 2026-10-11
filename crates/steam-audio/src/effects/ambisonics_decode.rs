//! Decoding of Ambisonics for playback (upstream `AmbisonicsDecodeEffect`): the sound field is rotated into the
//! listener's orientation, then rendered binaurally (stereo layouts with an HRTF) or panned onto the speakers.

use super::{AmbisonicsBinauralEffect, AmbisonicsPanningEffect, AmbisonicsRotateEffect};
use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::hrtf::HrtfDatabase;
use crate::math::CoordinateSpace3;
use crate::sh;
use crate::speaker_layout::{SpeakerLayout, SpeakerLayoutType};

/// Per-frame parameters of an [`AmbisonicsDecodeEffect`] (upstream `AmbisonicsDecodeEffectParams`).
#[derive(Debug, Clone, Copy)]
pub struct AmbisonicsDecodeParams<'a> {
    pub order: usize,
    /// The listener's orientation.
    pub orientation: CoordinateSpace3,
    /// Render binaurally. Takes effect only for a stereo layout, with `hrtf` given and an effect created with an HRTF;
    /// otherwise the input is panned.
    pub binaural: bool,
    pub hrtf: Option<&'a HrtfDatabase>,
}

/// Rotates an Ambisonics buffer and renders it to a speaker layout or binaurally.
#[derive(Debug, Clone)]
pub struct AmbisonicsDecodeEffect {
    speaker_layout: SpeakerLayout,
    panning_effect: AmbisonicsPanningEffect,
    binaural_effect: Option<AmbisonicsBinauralEffect>,
    rotate_effect: AmbisonicsRotateEffect,
    /// The rotated input, of the maximum order. Channels above the current order keep earlier values, which panning
    /// mixes in (risk R15).
    rotated: AudioBuffer,
    prev_binaural: bool,
}

impl AmbisonicsDecodeEffect {
    /// Decodes up to `max_order` to `speaker_layout`. Binaural rendering is available only if `hrtf` is given (it
    /// only sizes the effect; [`apply`](Self::apply) takes the database to use).
    pub fn new(
        audio_settings: &AudioSettings,
        speaker_layout: &SpeakerLayout,
        max_order: usize,
        hrtf: Option<&HrtfDatabase>,
    ) -> Self {
        let mut effect = Self {
            speaker_layout: speaker_layout.clone(),
            panning_effect: AmbisonicsPanningEffect::new(audio_settings, speaker_layout, max_order),
            binaural_effect: hrtf
                .map(|hrtf| AmbisonicsBinauralEffect::new(audio_settings, max_order, hrtf)),
            rotate_effect: AmbisonicsRotateEffect::new(audio_settings, max_order),
            rotated: AudioBuffer::new(
                sh::num_coeffs_for_order(max_order),
                audio_settings.frame_size,
            ),
            prev_binaural: false,
        };
        effect.reset();
        effect
    }

    /// Resets the sub-effects. The rotated buffer keeps its contents, like upstream.
    pub fn reset(&mut self) {
        self.panning_effect.reset();
        if let Some(binaural_effect) = &mut self.binaural_effect {
            binaural_effect.reset();
        }
        self.rotate_effect.reset();
        self.prev_binaural = false;
    }

    /// Decodes one frame of order-`order` Ambisonics into one channel per speaker. Switching between panning and
    /// binaural rendering resets the effect switched to.
    ///
    /// # Panics
    ///
    /// If the input does not have `num_coeffs_for_order(order)` channels, the output does not have one channel per
    /// speaker, either is not one frame long, or binaural rendering is used with an order above the maximum order.
    pub fn apply(
        &mut self,
        params: AmbisonicsDecodeParams<'_>,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(
            input.num_channels(),
            sh::num_coeffs_for_order(params.order),
            "input channels must match the order"
        );

        self.rotate_effect
            .process(&params.orientation, params.order, input, &mut self.rotated);

        let hrtf = params.hrtf.filter(|_| {
            params.binaural
                && self.speaker_layout.layout_type() == SpeakerLayoutType::Stereo
                && self.binaural_effect.is_some()
        });
        let binaural = hrtf.is_some();

        if binaural && !self.prev_binaural {
            self.panning_effect.reset();
        } else if !binaural && self.prev_binaural {
            if let Some(binaural_effect) = &mut self.binaural_effect {
                binaural_effect.reset();
            }
        }

        let state = match (hrtf, &mut self.binaural_effect) {
            (Some(hrtf), Some(binaural_effect)) => {
                binaural_effect.process(hrtf, params.order, &self.rotated, out)
            }
            _ => self.panning_effect.process(&self.rotated, out),
        };

        self.prev_binaural = binaural;

        state
    }

    /// Outputs the next frame of the tail of the renderer used last.
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        match (&mut self.binaural_effect, self.prev_binaural) {
            (Some(binaural_effect), true) => binaural_effect.tail(out),
            _ => self.panning_effect.tail(out),
        }
    }

    pub fn num_tail_samples_remaining(&self) -> usize {
        match (&self.binaural_effect, self.prev_binaural) {
            (Some(binaural_effect), true) => binaural_effect.num_tail_samples_remaining(),
            _ => self.panning_effect.num_tail_samples_remaining(),
        }
    }
}
