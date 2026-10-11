//! Opening a SOFA file for rendering (upstream `hrtf/easy.c`).

use std::path::Path;

use crate::{Error, Hrtf, Lookup, Neighborhood};

/// A checked HRTF resampled to the renderer's rate, with Cartesian positions, its nearest-measurement search and
/// the neighbours of every measurement (upstream `MYSOFA_EASY` without the filter buffer).
#[derive(Debug)]
pub struct Easy {
    pub hrtf: Hrtf,
    pub lookup: Lookup,
    pub neighborhood: Neighborhood,
}

impl Easy {
    /// Parses a SOFA file held in memory and prepares it for rendering at `sample_rate` Hz without normalising
    /// its loudness (upstream `mysofa_open_data_no_norm`, what Steam Audio calls): [`Hrtf::load`],
    /// [`Hrtf::check`], [`Hrtf::resample`], [`Hrtf::to_cartesian`], [`Lookup::new`] and [`Neighborhood::new`].
    ///
    /// Fails with the first error; [`Error::InvalidFormat`] if the source positions are not `M` 3-D points and
    /// [`Error::Internal`] if they are not Cartesian after the conversion.
    pub fn load_no_norm(data: &[u8], sample_rate: f32) -> Result<Self, Error> {
        Self::prepare(Hrtf::load(data)?, sample_rate)
    }

    /// Reads a SOFA file and prepares it like [`Easy::load_no_norm`] (upstream `mysofa_open_no_norm`).
    pub fn load_file_no_norm(path: impl AsRef<Path>, sample_rate: f32) -> Result<Self, Error> {
        Self::prepare(Hrtf::load_file(path)?, sample_rate)
    }

    /// `easy_processing` without loudness normalisation.
    fn prepare(mut hrtf: Hrtf, sample_rate: f32) -> Result<Self, Error> {
        hrtf.check()?;
        hrtf.resample(sample_rate)?;
        hrtf.to_cartesian();
        if hrtf.source_position.len() as u64 != u64::from(hrtf.c) * u64::from(hrtf.m) {
            return Err(Error::InvalidFormat);
        }
        let lookup = Lookup::new(&hrtf)?;
        let neighborhood = Neighborhood::new(&hrtf, &lookup)?;
        Ok(Self {
            hrtf,
            lookup,
            neighborhood,
        })
    }

    /// The number of samples per impulse response (`filterlength` upstream).
    pub fn filter_length(&self) -> u32 {
        self.hrtf.n
    }
}
