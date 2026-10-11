//! Frequency bands used by every frequency-dependent quantity (upstream `bands.h`).
//!
//! Upstream can be built with 11 octave bands (`IPL_ENABLE_OCTAVE_BANDS`); the default build, and this port, use
//! three bands.

/// Number of frequency bands.
pub const NUM_BANDS: usize = 3;

/// Lower edge of each band, in Hz.
pub const LOW_CUTOFF_FREQUENCIES: [f32; NUM_BANDS] = [0.0, 800.0, 8000.0];

/// Upper edge of each band, in Hz.
pub const HIGH_CUTOFF_FREQUENCIES: [f32; NUM_BANDS] = [800.0, 8000.0, 22000.0];

/// Peak-normalisation factors for 8th-order band-pass filters at 44.1 kHz (1 for the low-pass and high-pass bands).
///
/// Upstream only uses these for its 8th-order filters, which are never enabled (see `docs/DEVIATIONS.md`).
pub const BAND_PASS_NORM: [f32; NUM_BANDS] = [1.0, 0.999937, 1.0];
