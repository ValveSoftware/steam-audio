//! The HRTF map for a given HRTF source (upstream `IHRTFMap` and `HRTFMapFactory`).

use super::{HrirWeights, HrtfMap, HrtfSettings, HrtfSource};
use crate::error::Result;
use crate::math::Vector3;

#[cfg(feature = "sofa")]
use super::SofaHrtfMap;

/// HRIRs with direction queries, from any supported source (upstream `IHRTFMap`, whose implementations form a closed
/// set).
#[derive(Debug, Clone)]
#[non_exhaustive]
pub enum AnyHrtfMap {
    /// The built-in HRTF or data in the `.hrtf` format (upstream `HRTFMap`).
    Default(HrtfMap),
    /// A SOFA file (upstream `SOFAHRTFMap`).
    #[cfg(feature = "sofa")]
    Sofa(SofaHrtfMap),
}

impl AnyHrtfMap {
    /// Loads the HRTF described by `settings` for `sampling_rate` (upstream `HRTFMapFactory::create`). Volume and
    /// normalization are applied by [`HrtfDatabase`](super::HrtfDatabase), not here.
    pub fn new(settings: &HrtfSettings<'_>, sampling_rate: u32) -> Result<Self> {
        Ok(match settings.source {
            HrtfSource::BuiltIn => Self::Default(HrtfMap::new(sampling_rate)?),
            HrtfSource::Data(data) => Self::Default(HrtfMap::from_bytes(data, sampling_rate)?),
            #[cfg(feature = "sofa")]
            HrtfSource::SofaFile(path) => Self::Sofa(SofaHrtfMap::from_file(path, sampling_rate)?),
            #[cfg(feature = "sofa")]
            HrtfSource::SofaData(data) => Self::Sofa(SofaHrtfMap::from_bytes(data, sampling_rate)?),
        })
    }

    /// Number of measurement directions (upstream `numHRIRs`).
    pub fn num_hrirs(&self) -> usize {
        match self {
            Self::Default(map) => map.num_hrirs(),
            #[cfg(feature = "sofa")]
            Self::Sofa(map) => map.num_hrirs(),
        }
    }

    /// HRIR length in samples (upstream `numSamples`).
    pub fn num_samples(&self) -> usize {
        match self {
            Self::Default(map) => map.num_samples(),
            #[cfg(feature = "sofa")]
            Self::Sofa(map) => map.num_samples(),
        }
    }

    /// HRIR of one ear (0 = left, 1 = right) for one measurement direction.
    pub fn hrir(&self, ear: usize, index: usize) -> &[f32] {
        match self {
            Self::Default(map) => map.hrir(ear, index),
            #[cfg(feature = "sofa")]
            Self::Sofa(map) => map.hrir(ear, index),
        }
    }

    /// All HRIRs, `NUM_EARS × num_hrirs × num_samples` (upstream `hrtfData`).
    pub fn hrirs(&self) -> &[f32] {
        match self {
            Self::Default(map) => map.hrirs(),
            #[cfg(feature = "sofa")]
            Self::Sofa(map) => map.hrirs(),
        }
    }

    pub(crate) fn hrirs_mut(&mut self) -> &mut [f32] {
        match self {
            Self::Default(map) => map.hrirs_mut(),
            #[cfg(feature = "sofa")]
            Self::Sofa(map) => map.hrirs_mut(),
        }
    }

    /// Precomputed order-3 Ambisonics HRIRs, `NUM_EARS × 16 × num_samples`, or an empty slice (always for SOFA
    /// files) if [`HrtfDatabase`](super::HrtfDatabase) has to project them itself (upstream `ambisonicsData`).
    pub fn ambisonics_hrirs(&self) -> &[f32] {
        match self {
            Self::Default(map) => map.ambisonics_hrirs(),
            #[cfg(feature = "sofa")]
            Self::Sofa(_) => &[],
        }
    }

    /// Index of the measurement nearest to `direction` (upstream `nearestHRIR`).
    pub fn nearest_hrir(&mut self, direction: Vector3) -> usize {
        match self {
            Self::Default(map) => map.nearest_hrir(direction),
            #[cfg(feature = "sofa")]
            Self::Sofa(map) => map.nearest_hrir(direction),
        }
    }

    /// Measurements around `direction` and their interpolation weights (upstream `interpolatedHRIRWeights`).
    pub fn interpolated_hrir_weights(&mut self, direction: Vector3) -> HrirWeights {
        match self {
            Self::Default(map) => map.interpolated_hrir_weights(direction),
            #[cfg(feature = "sofa")]
            Self::Sofa(map) => map.interpolated_hrir_weights(direction),
        }
    }
}
