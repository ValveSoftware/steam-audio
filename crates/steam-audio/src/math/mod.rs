//! Linear algebra, coordinate systems and scalar helpers (upstream `vector.h`, `matrix.h`, `quaternion.h`,
//! `coordinate_space.h`, `polar_vector.h`, `math_functions.h`).
//!
//! Upstream templates these types on the scalar type, but the library only ever instantiates `float`, so the
//! port uses `f32` throughout (see `docs/DEVIATIONS.md`).

mod coordinate_space;
mod dynamic_matrix;
mod matrix;
mod polar;
mod quaternion;
mod vector;

pub use coordinate_space::CoordinateSpace3;
pub use dynamic_matrix::DynamicMatrix;
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

/// Degrees → radians factor as upstream defines it (`Math::kDegreesToRadians = 0.017452f`), slightly below the
/// exact π/180 = 0.0174533.
pub const DEGREES_TO_RADIANS: f32 = 0.017452;

/// Smallest power of two `>= x` (upstream `Math::nextpow2`). `x` must be positive.
pub fn next_pow2(x: usize) -> usize {
    debug_assert!(x > 0, "next_pow2 of 0");
    x.next_power_of_two()
}

/// `x!`, wrapping on overflow like upstream's `uint64_t` arithmetic (exact up to `20!`).
pub fn factorial(x: u32) -> u64 {
    (2..=u64::from(x)).fold(1, u64::wrapping_mul)
}

/// `x!!`: the product of all integers in `[1, x]` with the same parity as `x`, wrapping on overflow.
pub fn double_factorial(x: u32) -> u64 {
    let start = if x % 2 == 1 { 1 } else { 2 };
    (start..=u64::from(x)).step_by(2).fold(1, u64::wrapping_mul)
}

/// `std::max(lower, std::min(x, upper))` (upstream `Math::clamp`).
///
/// Unlike [`f32::clamp`] this never panics: if `lower > upper` it returns `lower`, and a NaN `x` also yields
/// `lower`.
pub fn clamp<T: PartialOrd + Copy>(x: T, lower: T, upper: T) -> T {
    let min = if upper < x { upper } else { x };
    if lower < min { min } else { lower }
}

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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn next_pow2_matches_upstream() {
        let values: Vec<usize> = [1, 2, 3, 5, 20, 64, 65].map(next_pow2).to_vec();
        assert_eq!(values, [1, 2, 4, 8, 32, 64, 128]);
    }

    #[test]
    fn factorials() {
        assert_eq!(factorial(0), 1);
        assert_eq!(factorial(5), 120);
        assert_eq!(factorial(20), 2_432_902_008_176_640_000);
        assert_eq!(double_factorial(0), 1);
        assert_eq!(double_factorial(7), 105);
        assert_eq!(double_factorial(8), 384);
    }

    #[test]
    fn clamp_never_panics() {
        assert_eq!(clamp(5.0, 0.0, 1.0), 1.0);
        assert_eq!(clamp(-5.0, 0.0, 1.0), 0.0);
        assert_eq!(clamp(0.5, 1.0, 0.0), 1.0);
        assert_eq!(clamp(f32::NAN, 0.0, 1.0), 0.0);
        assert_eq!(clamp(7, 0, 3), 3);
    }
}
