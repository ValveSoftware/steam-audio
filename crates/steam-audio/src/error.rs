//! Errors for input-dependent failures (upstream `Status`, `IPLerror` at the C API).
//!
//! Violated preconditions (length or shape mismatches) panic instead.

/// An input-dependent failure. Every variant maps to upstream's `Status::Initialization`.
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
}

/// `Result` with [`Error`] as the default error type.
pub type Result<T, E = Error> = std::result::Result<T, E>;
