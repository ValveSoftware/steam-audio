//! Audio effects. Each effect allocates in its constructor and processes caller-provided buffers without allocating.

pub mod binaural;
pub mod overlap_add;
pub mod overlap_save;
pub mod panning;
pub mod virtual_surround;

pub use binaural::{BinauralEffect, BinauralParams};
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
