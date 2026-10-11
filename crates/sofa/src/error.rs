//! Errors, with libmysofa's status codes.

/// Why a SOFA file could not be loaded or is not usable. [`Error::code`] gives libmysofa's status code.
#[derive(Debug, thiserror::Error)]
#[non_exhaustive]
pub enum Error {
    #[error("internal error")]
    Internal,
    #[error("invalid format")]
    InvalidFormat,
    #[error("unsupported format")]
    UnsupportedFormat,
    #[error("out of memory")]
    NoMemory,
    #[error("read error")]
    ReadError,
    #[error("invalid attributes")]
    InvalidAttributes,
    #[error("invalid dimensions")]
    InvalidDimensions,
    #[error("invalid dimension list")]
    InvalidDimensionList,
    #[error("invalid coordinate type")]
    InvalidCoordinateType,
    #[error("only emitters with dimensions E,C,I are supported")]
    OnlyEmitterWithEciSupported,
    #[error("only delays with dimensions I,R or M,R are supported")]
    OnlyDelaysWithIrOrMrSupported,
    #[error("only a single sampling rate is supported")]
    OnlyTheSameSamplingRateSupported,
    #[error("only receivers with dimensions R,C,I are supported")]
    ReceiversWithRciSupported,
    #[error("only receivers with cartesian coordinates are supported")]
    ReceiversWithCartesianSupported,
    #[error("invalid receiver positions")]
    InvalidReceiverPositions,
    #[error("only sources with dimensions M,C are supported")]
    OnlySourcesWithMcSupported,
    /// A C library error code (`errno`), which libmysofa returns as is. The in-memory reader only produces
    /// `EINVAL` (22), for a seek outside the data.
    #[error("system error {0}")]
    Errno(i32),
    /// The file could not be read.
    #[error(transparent)]
    Io(#[from] std::io::Error),
    /// The input exceeds a limit the port adds to bound memory, recursion or work on malicious files, where
    /// upstream crashes or does not terminate (`docs/DEVIATIONS.md`).
    #[error("input exceeds a parser limit")]
    ResourceLimit,
}

impl Error {
    /// libmysofa's status code (`MYSOFA_*`, or an `errno` value). [`Error::ResourceLimit`] reports
    /// `MYSOFA_UNSUPPORTED_FORMAT`.
    pub fn code(&self) -> i32 {
        match self {
            Self::Internal => -1,
            Self::InvalidFormat => 10000,
            Self::UnsupportedFormat | Self::ResourceLimit => 10001,
            Self::NoMemory => 10002,
            Self::ReadError => 10003,
            Self::InvalidAttributes => 10004,
            Self::InvalidDimensions => 10005,
            Self::InvalidDimensionList => 10006,
            Self::InvalidCoordinateType => 10007,
            Self::OnlyEmitterWithEciSupported => 10008,
            Self::OnlyDelaysWithIrOrMrSupported => 10009,
            Self::OnlyTheSameSamplingRateSupported => 10010,
            Self::ReceiversWithRciSupported => 10011,
            Self::ReceiversWithCartesianSupported => 10012,
            Self::InvalidReceiverPositions => 10013,
            Self::OnlySourcesWithMcSupported => 10014,
            Self::Errno(errno) => *errno,
            Self::Io(error) => error.raw_os_error().unwrap_or(-1),
        }
    }
}
