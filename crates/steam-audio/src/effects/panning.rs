//! Panning of a mono signal onto a speaker layout (upstream `PanningEffect`): constant-power stereo panning,
//! pairwise constant-power panning for quadraphonic, 5.1 and 7.1, and a first-order fallback for custom layouts.

use crate::audio_buffer::{AudioBuffer, AudioEffectState};
use crate::math::{PI, SphericalVector3, Vector3, cpp_max};
use crate::speaker_layout::{SpeakerLayout, SpeakerLayoutType, speakers_for_layout};

/// Intermediate data for 2D pairwise constant-power panning (upstream `PanningData`).
#[derive(Debug, Clone, Copy, Default)]
struct PanningData {
    /// The two speakers to pan between.
    speaker_indices: [usize; 2],
    /// The angle between the speakers.
    angle_between_speakers: f32,
    /// The angle between the first speaker and the source.
    d_phi: f32,
}

/// Applies per-speaker panning gains to a mono input, crossfaded from the previous frame's direction.
#[derive(Debug, Clone)]
pub struct PanningEffect {
    speaker_layout: SpeakerLayout,
    prev_direction: Vector3,
}

impl PanningEffect {
    pub fn new(speaker_layout: &SpeakerLayout) -> Self {
        Self {
            speaker_layout: speaker_layout.clone(),
            prev_direction: Vector3::ZERO,
        }
    }

    pub fn reset(&mut self) {
        self.prev_direction = Vector3::ZERO;
    }

    /// Pans the mono `input` towards `direction` (relative to the listener, need not be normalized).
    ///
    /// Like upstream (risk R14), the crossfade factor is `channel / num_samples`: it depends on the *output channel
    /// index*, not on the sample index, so each channel uses one constant mix of the current and previous weights.
    ///
    /// # Panics
    ///
    /// If the input is not mono, the output does not have one channel per speaker, or the lengths differ.
    pub fn apply(
        &mut self,
        direction: Vector3,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(input.num_samples(), out.num_samples(), "sample count");
        assert_eq!(input.num_channels(), 1, "input must be mono");
        assert_eq!(
            out.num_channels(),
            self.speaker_layout.num_speakers(),
            "one output channel per speaker"
        );

        let mut panning_data = PanningData::default();
        let mut prev_panning_data = PanningData::default();
        if is_pairwise(self.speaker_layout.layout_type()) {
            // Pairwise panning shares its per-direction work between the output channels.
            calc_pairwise_panning_data(direction, &self.speaker_layout, &mut panning_data);
            calc_pairwise_panning_data(
                self.prev_direction,
                &self.speaker_layout,
                &mut prev_panning_data,
            );
        }

        let num_samples = input.num_samples();
        for i in 0..out.num_channels() {
            let weight = weight_with(direction, &self.speaker_layout, i, &panning_data);
            let weight_prev = weight_with(
                self.prev_direction,
                &self.speaker_layout,
                i,
                &prev_panning_data,
            );

            let alpha = i as f32 / num_samples as f32;
            let blended_weight = alpha * weight + (1.0 - alpha) * weight_prev;

            for (o, &x) in out.channel_mut(i).iter_mut().zip(input.channel(0)) {
                *o = blended_weight * x;
            }
        }

        self.prev_direction = direction;

        AudioEffectState::TailComplete
    }

    /// Panning has no tail: outputs silence.
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

    /// Gain of speaker `index` for a source in `direction` (upstream `panningWeight`).
    pub fn panning_weight(direction: Vector3, speaker_layout: &SpeakerLayout, index: usize) -> f32 {
        let mut data = PanningData::default();
        if is_pairwise(speaker_layout.layout_type()) {
            calc_pairwise_panning_data(direction, speaker_layout, &mut data);
        }
        weight_with(direction, speaker_layout, index, &data)
    }
}

fn is_pairwise(ty: SpeakerLayoutType) -> bool {
    matches!(
        ty,
        SpeakerLayoutType::Quadraphonic
            | SpeakerLayoutType::FivePointOne
            | SpeakerLayoutType::SevenPointOne
    )
}

fn weight_with(
    direction: Vector3,
    speaker_layout: &SpeakerLayout,
    index: usize,
    data: &PanningData,
) -> f32 {
    match speaker_layout.layout_type() {
        SpeakerLayoutType::Mono => 1.0,
        SpeakerLayoutType::Stereo => stereo_panning_weight(direction, index),
        SpeakerLayoutType::Quadraphonic
        | SpeakerLayoutType::FivePointOne
        | SpeakerLayoutType::SevenPointOne => pairwise_panning_weight(index, data),
        SpeakerLayoutType::Custom => first_order_panning_weight(direction, speaker_layout, index),
    }
}

fn stereo_panning_weight(direction: Vector3, index: usize) -> f32 {
    let horizontal = Vector3::new(direction.x, 0.0, direction.z).normalize_or_zero();
    let p = horizontal.x;
    let q = (p + 1.0) * (PI / 4.0);
    if index == 0 { q.cos() } else { q.sin() }
}

fn first_order_panning_weight(
    direction: Vector3,
    speaker_layout: &SpeakerLayout,
    index: usize,
) -> f32 {
    let source_direction = direction.normalize_or_zero();
    let speaker_direction = speaker_layout.speakers()[index].normalize_or_zero();

    // To handle LFE channels, the speaker position is (0, 0, 0).
    if speaker_direction.length_squared() < 1e-3 {
        return 0.0;
    }

    let num_speakers = speaker_layout.num_speakers();
    let cos_theta = source_direction.dot(speaker_direction);

    (1.0 + cos_theta) / num_speakers as f32
}

fn pairwise_panning_weight(index: usize, data: &PanningData) -> f32 {
    if index == data.speaker_indices[0] {
        ((data.d_phi / data.angle_between_speakers) * (PI / 2.0)).cos()
    } else if index == data.speaker_indices[1] {
        ((data.d_phi / data.angle_between_speakers) * (PI / 2.0)).sin()
    } else {
        0.0
    }
}

fn calc_pairwise_panning_data(
    direction: Vector3,
    speaker_layout: &SpeakerLayout,
    data: &mut PanningData,
) {
    // The azimuth of the source relative to forward. The zero vector has a NaN azimuth, which `max` replaces by 0.
    let polar_direction = SphericalVector3::from(direction);
    let phi = cpp_max(0.0, polar_direction.azimuth);

    let q = PI / 4.0;

    // Which speaker pair to pan between, and the angle between the speakers.
    match speaker_layout.layout_type() {
        SpeakerLayoutType::Quadraphonic => {
            if phi <= q || 7.0 * q < phi {
                *data = pair(0, 1, PI / 2.0); // fl, fr
            } else if q < phi && phi <= 3.0 * q {
                *data = pair(2, 0, PI / 2.0); // rl, fl
            } else if 3.0 * q < phi && phi <= 5.0 * q {
                *data = pair(3, 2, PI / 2.0); // rr, rl
            } else if 5.0 * q < phi && phi <= 7.0 * q {
                *data = pair(1, 3, PI / 2.0); // fr, rr
            }
        }
        SpeakerLayoutType::FivePointOne => {
            if (0.0..q).contains(&phi) {
                *data = pair(0, 2, q); // fl, c
            } else if (q..3.0 * q).contains(&phi) {
                *data = pair(4, 0, PI / 2.0); // rl, fl
            } else if (3.0 * q..5.0 * q).contains(&phi) {
                *data = pair(5, 4, PI / 2.0); // rr, rl
            } else if (5.0 * q..7.0 * q).contains(&phi) {
                *data = pair(1, 5, PI / 2.0); // fr, rr
            } else if 7.0 * q <= phi {
                *data = pair(2, 1, q); // c, fr
            }
        }
        SpeakerLayoutType::SevenPointOne => {
            if (0.0..q).contains(&phi) {
                *data = pair(0, 2, q); // fl, c
            } else if (q..2.0 * q).contains(&phi) {
                *data = pair(6, 0, q); // sl, fl
            } else if (2.0 * q..3.0 * q).contains(&phi) {
                *data = pair(4, 6, q); // rl, sl
            } else if (3.0 * q..5.0 * q).contains(&phi) {
                *data = pair(5, 4, PI / 2.0); // rr, rl
            } else if (5.0 * q..6.0 * q).contains(&phi) {
                *data = pair(7, 5, q); // sr, rr
            } else if (6.0 * q..7.0 * q).contains(&phi) {
                *data = pair(1, 7, q); // fr, sr
            } else if 7.0 * q <= phi {
                *data = pair(2, 1, q); // c, fr
            }
        }
        _ => return,
    }

    // The angle between the first speaker and the source.
    let speakers = speakers_for_layout(speaker_layout.layout_type());
    let first_speaker = speakers[data.speaker_indices[0]];
    data.d_phi = direction
        .normalize_or_zero()
        .dot(first_speaker.normalize_or_zero())
        .acos();
}

fn pair(first: usize, second: usize, angle_between_speakers: f32) -> PanningData {
    PanningData {
        speaker_indices: [first, second],
        angle_between_speakers,
        d_phi: 0.0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn weights(layout: &SpeakerLayout, direction: Vector3) -> Vec<f32> {
        (0..layout.num_speakers())
            .map(|i| PanningEffect::panning_weight(direction, layout, i))
            .collect()
    }

    #[test]
    fn stereo_is_constant_power() {
        let layout = SpeakerLayout::new(SpeakerLayoutType::Stereo);
        for x in [-1.0, -0.5, 0.0, 0.3, 1.0] {
            let w = weights(&layout, Vector3::new(x, 0.2, -1.0));
            assert!((w[0] * w[0] + w[1] * w[1] - 1.0).abs() < 1e-5);
        }
        let left = weights(&layout, Vector3::new(-1.0, 0.0, 0.0));
        assert!((left[0] - 1.0).abs() < 1e-5 && left[1].abs() < 1e-5);
    }

    #[test]
    fn surround_pairs_are_constant_power() {
        for ty in [
            SpeakerLayoutType::Quadraphonic,
            SpeakerLayoutType::FivePointOne,
            SpeakerLayoutType::SevenPointOne,
        ] {
            let layout = SpeakerLayout::new(ty);
            for k in 0..40 {
                let a = k as f32 * 0.157 + 0.01;
                let w = weights(&layout, Vector3::new(-a.sin(), 0.0, -a.cos()));
                let power: f32 = w.iter().map(|w| w * w).sum();
                assert!((power - 1.0).abs() < 1e-4, "{ty:?} at {a}: {w:?}");
            }
        }
    }

    #[test]
    fn custom_layout_skips_lfe() {
        let layout = SpeakerLayout::custom(vec![Vector3::new(0.0, 0.0, -1.0), Vector3::ZERO]);
        let w = weights(&layout, Vector3::new(0.0, 0.0, -1.0));
        assert!((w[0] - 1.0).abs() < 1e-6);
        assert_eq!(w[1], 0.0);
    }

    #[test]
    fn crossfade_depends_on_channel_index() {
        let layout = SpeakerLayout::new(SpeakerLayoutType::Stereo);
        let mut effect = PanningEffect::new(&layout);
        let input = AudioBuffer::from_planar(1, 4, vec![1.0; 4]);
        let mut out = AudioBuffer::new(2, 4);
        effect.apply(Vector3::X_AXIS, &input, &mut out);
        // Channel 0 has alpha 0 (the previous weight), channel 1 has alpha 1/4; both are constant over the frame.
        let prev = [
            PanningEffect::panning_weight(Vector3::ZERO, &layout, 0),
            PanningEffect::panning_weight(Vector3::ZERO, &layout, 1),
        ];
        let cur = PanningEffect::panning_weight(Vector3::X_AXIS, &layout, 1);
        assert_eq!(out.channel(0)[0], prev[0]);
        assert_eq!(out.channel(1)[3], 0.25 * cur + 0.75 * prev[1]);
        assert_eq!(out.channel(1)[0], out.channel(1)[3]);
    }
}
