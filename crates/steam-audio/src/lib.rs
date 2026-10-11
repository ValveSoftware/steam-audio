//! Rust reimplementation of [Steam Audio](https://github.com/ValveSoftware/steam-audio).
//!
//! The reference C++ sources live in `core/` at the repository root and act as the specification:
//! every module here is verified numerically against golden data produced by that code
//! (see `docs/PORTING_PLAN.md`).

pub mod geometry;
pub mod math;
