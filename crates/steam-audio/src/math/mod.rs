//! Linear algebra and coordinate systems (upstream `vector.h`, `matrix.h`, `quaternion.h`,
//! `coordinate_space.h`, `polar_vector.h`).
//!
//! Upstream templates these types on the scalar type, but the library only ever instantiates `float`, so the
//! port uses `f32` throughout (see `docs/DEVIATIONS.md`).

mod coordinate_space;
mod matrix;
mod polar;
mod quaternion;
mod vector;

pub use coordinate_space::CoordinateSpace3;
pub use matrix::{Matrix, Matrix2, Matrix3, Matrix4};
pub use polar::{CylindricalVector3, InterauralSphericalVector3, SphericalVector3};
pub use quaternion::Quaternion;
pub use vector::{Vector2, Vector3, Vector4};

/// π as upstream defines it (`Math::kPi = 3.141592f`). This is **not** `std::f32::consts::PI` (3.1415927);
/// using the upstream value keeps angle computations bit-compatible with the reference.
#[allow(clippy::approx_constant)] // Deliberately upstream's truncated value.
pub const PI: f32 = 3.141592;

/// π/2 as upstream defines it (`Math::kHalfPi = 1.570796f`).
#[allow(clippy::approx_constant)] // Deliberately upstream's truncated value.
pub const HALF_PI: f32 = 1.570796;

/// π in double precision (`Math::kPiD`).
pub const PI_F64: f64 = std::f64::consts::PI;

/// `std::min` semantics: returns `a` unless `b < a` (so NaN handling matches C++).
#[inline]
pub(crate) fn cpp_min(a: f32, b: f32) -> f32 {
    if b < a { b } else { a }
}

/// `std::max` semantics: returns `a` unless `a < b` (so NaN handling matches C++).
#[inline]
pub(crate) fn cpp_max(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}
