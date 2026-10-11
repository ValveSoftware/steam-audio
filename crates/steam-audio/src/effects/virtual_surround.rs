//! Virtual surround (upstream `VirtualSurroundEffect`): each channel of a multichannel signal is treated as a speaker
//! and spatialized with the HRTF of the speaker's direction.

use super::binaural::{BinauralEffect, BinauralParams};
use crate::audio_buffer::{self, AudioBuffer, AudioEffectState, AudioSettings};
use crate::hrtf::HrtfDatabase;
use crate::speaker_layout::SpeakerLayout;

/// Renders a speaker mix binaurally. A mono input is copied to both ears.
#[derive(Debug, Clone)]
pub struct VirtualSurroundEffect {
    frame_size: usize,
    speaker_layout: SpeakerLayout,
    binaural_effects: Vec<BinauralEffect>,
    binaural_effect_states: Vec<AudioEffectState>,
    spatialized_channel: AudioBuffer,
}

impl VirtualSurroundEffect {
    /// `hrtf` only sizes the effect; [`apply`](Self::apply) takes the database to use.
    pub fn new(
        audio_settings: &AudioSettings,
        speaker_layout: &SpeakerLayout,
        hrtf: &HrtfDatabase,
    ) -> Self {
        let num_speakers = speaker_layout.num_speakers();
        Self {
            frame_size: audio_settings.frame_size,
            speaker_layout: speaker_layout.clone(),
            binaural_effects: (0..num_speakers)
                .map(|_| BinauralEffect::new(audio_settings, hrtf))
                .collect(),
            // Upstream value-initializes its states, so they start as `TailRemaining` until `reset` (risk R14).
            binaural_effect_states: vec![AudioEffectState::TailRemaining; num_speakers],
            spatialized_channel: AudioBuffer::new(2, audio_settings.frame_size),
        }
    }

    pub fn reset(&mut self) {
        for (effect, state) in self
            .binaural_effects
            .iter_mut()
            .zip(&mut self.binaural_effect_states)
        {
            effect.reset();
            *state = AudioEffectState::TailComplete;
        }
    }

    pub fn num_tail_samples_remaining(&self) -> usize {
        self.binaural_effects
            .iter()
            .map(BinauralEffect::num_tail_samples_remaining)
            .max()
            .unwrap_or(0)
    }

    /// Spatializes every input channel at its speaker's position and sums the results. Speakers at the origin
    /// (LFE channels) are skipped.
    ///
    /// # Panics
    ///
    /// If the input does not have one channel per speaker, the output is not stereo, or either is not one frame long.
    pub fn apply(
        &mut self,
        hrtf: &mut HrtfDatabase,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(input.num_samples(), out.num_samples(), "sample count");
        assert_eq!(
            input.num_channels(),
            self.speaker_layout.num_speakers(),
            "one input channel per speaker"
        );
        assert_eq!(out.num_channels(), 2, "output must be stereo");

        if input.num_channels() == 1 {
            for i in 0..2 {
                out.channel_mut(i)[..self.frame_size].copy_from_slice(input.channel(0));
            }
        } else {
            out.make_silent();

            for i in 0..input.num_channels() {
                let speaker = self.speaker_layout.speakers()[i];

                // Skip LFE channels, which are indicated by a position of (0, 0, 0).
                if speaker.length_squared() < 1e-3 {
                    continue;
                }

                let params = BinauralParams::new(speaker.normalize_or_zero(), hrtf);
                self.binaural_effect_states[i] = self.binaural_effects[i].apply(
                    params,
                    &input.channel_view(i),
                    &mut self.spatialized_channel,
                );

                audio_buffer::mix(&self.spatialized_channel, out);
            }
        }

        self.combined_state()
    }

    /// Outputs the next frame of the tails of all channels, summed.
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(out.num_channels(), 2, "output must be stereo");

        out.make_silent();

        for (effect, state) in self
            .binaural_effects
            .iter_mut()
            .zip(&mut self.binaural_effect_states)
        {
            *state = effect.tail(&mut self.spatialized_channel);
            audio_buffer::mix(&self.spatialized_channel, out);
        }

        self.combined_state()
    }

    fn combined_state(&self) -> AudioEffectState {
        if self
            .binaural_effect_states
            .contains(&AudioEffectState::TailRemaining)
        {
            AudioEffectState::TailRemaining
        } else {
            AudioEffectState::TailComplete
        }
    }
}
