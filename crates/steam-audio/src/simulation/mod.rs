//! Simulation of sound propagation through a scene. So far: the direct path (upstream `direct_simulator`).

pub mod direct;

pub use direct::{
    DirectSimulationFlags, DirectSimulationParams, DirectSimulator, DirectSoundPath, OcclusionType,
};
