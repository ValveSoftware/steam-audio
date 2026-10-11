//! Errors for input-dependent failures (upstream `Status`, `IPLerror` at the C API).
//!
//! Violated preconditions (length or shape mismatches) panic instead.

/// An input-dependent failure. Every variant maps to upstream's `Status::Initialization`, except where noted.
#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
#[non_exhaustive]
pub enum Error {
    /// The HRTF data has a format version other than 2 or 3.
    #[error("unsupported HRTF data format version: {0}")]
    UnsupportedHrtfVersion(i32),

    /// The HRTF data has no HRIRs for the requested sampling rate.
    #[error("HRTF data has no HRIRs for a sampling rate of {0} Hz")]
    UnsupportedSamplingRate(u32),

    /// The HRTF data contains Ambisonics HRIRs of an order other than 3.
    #[error("HRTF data contains Ambisonics HRIRs of unsupported order: {0}")]
    UnsupportedAmbisonicsOrder(i32),

    /// The HRTF data is truncated or has invalid counts. Upstream reads out of bounds instead.
    #[error("malformed HRTF data: {0}")]
    MalformedHrtfData(&'static str),

    /// A SOFA file could not be loaded (upstream logs libmysofa's status code). Upstream reports
    /// `Status::Failure` when loading from a file name and `Status::Initialization` when loading from memory.
    #[cfg(feature = "sofa")]
    #[error("unable to load SOFA HRTF: {0}")]
    Sofa(#[from] SofaError),

    /// The SOFA file has a `Data.Delay` value of magnitude above 1e-3, which Steam Audio does not support
    /// (upstream `Status::Failure`).
    #[error("SOFA HRTF has non-zero Data.Delay values, which are not supported")]
    UnsupportedSofaDelay,
}

/// A [`sofa::Error`] that can be cloned and compared. Two errors are equal when libmysofa reports the same status
/// code for them ([`sofa::Error::code`]).
#[cfg(feature = "sofa")]
#[derive(Debug, Clone, thiserror::Error)]
#[error(transparent)]
pub struct SofaError(std::sync::Arc<sofa::Error>);

#[cfg(feature = "sofa")]
impl SofaError {
    /// The underlying error.
    pub fn get(&self) -> &sofa::Error {
        &self.0
    }
}

#[cfg(feature = "sofa")]
impl From<sofa::Error> for SofaError {
    fn from(error: sofa::Error) -> Self {
        Self(std::sync::Arc::new(error))
    }
}

#[cfg(feature = "sofa")]
impl From<sofa::Error> for Error {
    fn from(error: sofa::Error) -> Self {
        Self::Sofa(error.into())
    }
}

#[cfg(feature = "sofa")]
impl PartialEq for SofaError {
    fn eq(&self, other: &Self) -> bool {
        self.0.code() == other.0.code()
    }
}

#[cfg(feature = "sofa")]
impl Eq for SofaError {}

/// `Result` with [`Error`] as the default error type.
pub type Result<T, E = Error> = std::result::Result<T, E>;
