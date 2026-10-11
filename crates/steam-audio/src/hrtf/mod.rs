//! Head-related transfer functions: the built-in HRTF data, the HRTF map that queries it by direction, and the HRTF
//! database that holds the HRTFs in the frequency domain (upstream `hrtf_map`, `hrtf_database`, `cipic_124.inl`).

mod database;
pub mod loudness;
mod map;

pub use database::{HrtfDatabase, HrtfNormalization, HrtfPhaseType, HrtfSettings};
pub use map::{HrirWeights, HrtfMap};

/// Number of ears (upstream `IHRTFMap::kNumEars`).
pub const NUM_EARS: usize = 2;

/// Order of the Ambisonics HRTFs (upstream `IHRTFMap::kMaxAmbisonicsOrder`). Limited to 3 because Ambisonics are
/// decoded through 24 virtual speakers.
pub const MAX_AMBISONICS_ORDER: usize = 3;

/// Upstream's built-in HRTF (`gDefaultHrtfData`, from the CIPIC database, subject 124) in the `.hrtf` format read by
/// [`HrtfMap`]: 1250 measurement directions, HRIRs at 44100, 48000 and 24000 Hz, and order-3 Ambisonics HRIRs.
pub static BUILTIN_HRTF_DATA: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/cipic_124.hrtf"));
