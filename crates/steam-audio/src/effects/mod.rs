//! Audio effects. Each effect allocates in its constructor and processes caller-provided buffers without allocating.

pub mod overlap_add;
pub mod overlap_save;

pub use overlap_add::{OverlapAddConvolutionEffect, OverlapAddConvolutionSettings};
pub use overlap_save::{
    OverlapSaveConvolutionEffect, OverlapSaveConvolutionMixer, OverlapSaveConvolutionSettings,
    OverlapSaveFir, OverlapSavePartitioner,
};

use crate::audio_buffer::AudioEffectState;

fn tail_state(tail_remaining: bool) -> AudioEffectState {
    if tail_remaining {
        AudioEffectState::TailRemaining
    } else {
        AudioEffectState::TailComplete
    }
}
