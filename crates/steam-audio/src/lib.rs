//! Rust reimplementation of [Steam Audio](https://github.com/ValveSoftware/steam-audio).
//!
//! The reference C++ sources live in `core/` at the repository root and act as the specification:
//! every module here is verified numerically against golden data produced by that code
//! (see `docs/PORTING_PLAN.md`).

pub mod array_math;
pub mod audio_buffer;
pub mod bands;
pub mod delay;
pub mod fft;
pub mod geometry;
pub mod iir;
pub mod math;
pub mod sampling;
pub mod sh;
pub mod window;

/// Single-precision complex number (upstream `complex_t`).
pub type Complex = num_complex::Complex<f32>;
