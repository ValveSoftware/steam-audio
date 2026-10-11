//! Speaker layouts (upstream `speaker_layout.{h,cpp}`).

use crate::math::Vector3;

/// Supported speaker layouts.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum SpeakerLayoutType {
    Mono,
    Stereo,
    Quadraphonic,
    FivePointOne,
    SevenPointOne,
    Custom,
}

const MONO: [Vector3; 1] = [Vector3::new(0.0, 0.0, 0.0)];

const STEREO: [Vector3; 2] = [Vector3::new(-1.0, 0.0, 0.0), Vector3::new(1.0, 0.0, 0.0)];

const QUAD: [Vector3; 4] = [
    Vector3::new(-1.0, 0.0, -1.0),
    Vector3::new(1.0, 0.0, -1.0),
    Vector3::new(-1.0, 0.0, 1.0),
    Vector3::new(1.0, 0.0, 1.0),
];

const FIVE_ONE: [Vector3; 6] = [
    Vector3::new(-1.0, 0.0, -1.0),
    Vector3::new(1.0, 0.0, -1.0),
    Vector3::new(0.0, 0.0, -1.0),
    Vector3::new(0.0, 0.0, -1.0),
    Vector3::new(-1.0, 0.0, 1.0),
    Vector3::new(1.0, 0.0, 1.0),
];

const SEVEN_ONE: [Vector3; 8] = [
    Vector3::new(-1.0, 0.0, -1.0),
    Vector3::new(1.0, 0.0, -1.0),
    Vector3::new(0.0, 0.0, -1.0),
    Vector3::new(0.0, 0.0, -1.0),
    Vector3::new(-1.0, 0.0, 1.0),
    Vector3::new(1.0, 0.0, 1.0),
    Vector3::new(-1.0, 0.0, 0.0),
    Vector3::new(1.0, 0.0, 0.0),
];

/// Positions of the speakers of a standard layout (upstream `speakersForLayout`); empty for
/// [`Custom`](SpeakerLayoutType::Custom). In custom layouts a speaker at the origin marks an LFE channel; like upstream,
/// the 5.1 and 7.1 layouts put their LFE channel at the centre speaker's position instead.
pub fn speakers_for_layout(ty: SpeakerLayoutType) -> &'static [Vector3] {
    match ty {
        SpeakerLayoutType::Mono => &MONO,
        SpeakerLayoutType::Stereo => &STEREO,
        SpeakerLayoutType::Quadraphonic => &QUAD,
        SpeakerLayoutType::FivePointOne => &FIVE_ONE,
        SpeakerLayoutType::SevenPointOne => &SEVEN_ONE,
        SpeakerLayoutType::Custom => &[],
    }
}

/// Number of speakers of a standard layout (upstream `numSpeakersForLayout`); 0 for
/// [`Custom`](SpeakerLayoutType::Custom).
pub fn num_speakers_for_layout(ty: SpeakerLayoutType) -> usize {
    speakers_for_layout(ty).len()
}

/// A speaker layout: a type and one position (relative to the listener) per speaker.
#[derive(Debug, Clone, PartialEq)]
pub struct SpeakerLayout {
    ty: SpeakerLayoutType,
    speakers: Vec<Vector3>,
}

impl SpeakerLayout {
    /// A standard layout. Panics for [`Custom`](SpeakerLayoutType::Custom); use [`custom`](Self::custom).
    pub fn new(ty: SpeakerLayoutType) -> Self {
        assert!(
            ty != SpeakerLayoutType::Custom,
            "custom layouts need speaker positions"
        );
        Self {
            ty,
            speakers: speakers_for_layout(ty).to_vec(),
        }
    }

    /// A custom layout with the given speaker positions.
    pub fn custom(speakers: Vec<Vector3>) -> Self {
        Self {
            ty: SpeakerLayoutType::Custom,
            speakers,
        }
    }

    pub fn layout_type(&self) -> SpeakerLayoutType {
        self.ty
    }

    pub fn num_speakers(&self) -> usize {
        self.speakers.len()
    }

    pub fn speakers(&self) -> &[Vector3] {
        &self.speakers
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn standard_layouts_have_matching_counts() {
        for (ty, n) in [
            (SpeakerLayoutType::Mono, 1),
            (SpeakerLayoutType::Stereo, 2),
            (SpeakerLayoutType::Quadraphonic, 4),
            (SpeakerLayoutType::FivePointOne, 6),
            (SpeakerLayoutType::SevenPointOne, 8),
            (SpeakerLayoutType::Custom, 0),
        ] {
            assert_eq!(num_speakers_for_layout(ty), n);
            if ty != SpeakerLayoutType::Custom {
                assert_eq!(SpeakerLayout::new(ty).num_speakers(), n);
            }
        }
    }

    #[test]
    fn custom_layout_keeps_positions() {
        let layout = SpeakerLayout::custom(vec![Vector3::X_AXIS, Vector3::ZERO]);
        assert_eq!(layout.layout_type(), SpeakerLayoutType::Custom);
        assert_eq!(layout.speakers(), &[Vector3::X_AXIS, Vector3::ZERO]);
    }
}
