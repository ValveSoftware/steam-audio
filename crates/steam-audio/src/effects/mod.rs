//! Audio effects. Each effect allocates in its constructor and processes caller-provided buffers without allocating.

pub mod ambisonics_binaural;
pub mod ambisonics_decode;
pub mod ambisonics_encode;
pub mod ambisonics_panning;
pub mod ambisonics_rotate;
pub mod binaural;
pub mod direct;
pub mod eq;
pub mod gain;
pub mod overlap_add;
pub mod overlap_save;
pub mod panning;
pub mod virtual_surround;

pub use ambisonics_binaural::AmbisonicsBinauralEffect;
pub use ambisonics_decode::{AmbisonicsDecodeEffect, AmbisonicsDecodeParams};
pub use ambisonics_encode::AmbisonicsEncodeEffect;
pub use ambisonics_panning::AmbisonicsPanningEffect;
pub use ambisonics_rotate::AmbisonicsRotateEffect;
pub use binaural::{BinauralEffect, BinauralParams};
pub use direct::{
    DirectEffect, DirectEffectFlags, DirectEffectParams, DirectSoundPath, TransmissionType,
};
pub use eq::EqEffect;
pub use gain::GainEffect;
pub use overlap_add::{OverlapAddConvolutionEffect, OverlapAddConvolutionSettings};
pub use overlap_save::{
    OverlapSaveConvolutionEffect, OverlapSaveConvolutionMixer, OverlapSaveConvolutionSettings,
    OverlapSaveFir, OverlapSavePartitioner,
};

pub use panning::PanningEffect;
pub use virtual_surround::VirtualSurroundEffect;

use crate::audio_buffer::AudioEffectState;

fn tail_state(tail_remaining: bool) -> AudioEffectState {
    if tail_remaining {
        AudioEffectState::TailRemaining
    } else {
        AudioEffectState::TailComplete
    }
}
