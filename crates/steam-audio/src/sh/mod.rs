//! Spherical harmonics in Steam Audio's coordinate frame (upstream `sh.{h,cpp}`: namespace `SphericalHarmonics`
//! and class `SHRotation`).
//!
//! These wrap the Google library port in [`google`], converting directions and rotations from Steam Audio's frame
//! (+x right, +y up, +z backward) to the library's (+x forward, +y left, +z up). Coefficients are in ACN order with
//! orthonormal (N3D / √(4π)) normalisation.

pub mod google;

use crate::math::{CoordinateSpace3, Quaternion, Vector3};

pub use google::index;

/// Number of coefficients for an SH expansion of the given order: `(order + 1)²` (upstream `numCoeffsForOrder`).
pub fn num_coeffs_for_order(order: usize) -> usize {
    google::coefficient_count(order)
}

/// Legendre polynomial `P(n, x)` in `f32` (upstream `SphericalHarmonics::legendre`).
///
/// Orders 0–3 use upstream's closed forms. Upstream's general case calls itself with the same `n` and never
/// terminates, so for `n >= 4` this uses Bonnet's recurrence instead (see `docs/DEVIATIONS.md`).
pub fn legendre(n: usize, x: f32) -> f32 {
    match n {
        0 => 1.0,
        1 => x,
        2 => 0.5 * (3.0 * x * x - 1.0),
        3 => 0.5 * x * (5.0 * x * x - 3.0),
        _ => {
            let (mut p2, mut p1) = (legendre(2, x), legendre(3, x));
            for k in 4..=n {
                let p = ((2 * k - 1) as f32 * x * p1 - (k - 1) as f32 * p2) / k as f32;
                p2 = p1;
                p1 = p;
            }
            p1
        }
    }
}

/// Steam Audio direction → the Google library's frame: `(-z, -x, y)` (upstream `convertedDirection<float>`).
pub fn converted_direction(direction: Vector3) -> Vector3 {
    Vector3::new(-direction.z, -direction.x, direction.y)
}

/// As [`converted_direction`], widened to `f64` (upstream `convertedDirection<double>`).
fn converted_direction_f64(direction: Vector3) -> [f64; 3] {
    converted_direction(direction).to_array().map(f64::from)
}

/// Steam Audio rotation → the Google library's frame (upstream `convertedQuaternion`).
pub fn converted_quaternion(quaternion: Quaternion) -> Quaternion {
    Quaternion::new(0.5, -0.5, -0.5, 0.5) * quaternion
}

/// Steam Audio orientation → the Google library's frame; `right` is recomputed as `ahead × up` (upstream
/// `convertedCoordinateSpace`).
pub fn converted_coordinate_space(coordinate_space: &CoordinateSpace3) -> CoordinateSpace3 {
    CoordinateSpace3::from_ahead_up(
        converted_direction(coordinate_space.ahead),
        converted_direction(coordinate_space.up),
        coordinate_space.origin,
    )
}

/// Basis function `(l, m)` in `direction`, which should be a unit vector (upstream `evaluate`). Evaluated in `f64`
/// and rounded once. Panics unless `-l <= m <= l`.
pub fn evaluate(l: usize, m: i32, direction: Vector3) -> f32 {
    google::eval_sh(l, m, converted_direction_f64(direction)) as f32
}

/// `Σ coefficients[index(l, m)] · Y(l, m)(direction)` over bands `0..=order` (upstream `evaluateSum`).
pub fn evaluate_sum(order: usize, coefficients: &[f32], direction: Vector3) -> f32 {
    google::eval_sh_sum(order, coefficients, converted_direction_f64(direction))
}

/// Writes every basis function of bands `0..=order` in `direction` into `coefficients` (upstream
/// `projectSinglePoint`). Panics if `coefficients` is shorter than [`num_coeffs_for_order`]`(order)`.
pub fn project_single_point(direction: Vector3, order: usize, coefficients: &mut [f32]) {
    let dir = converted_direction_f64(direction);
    for_each_basis(order, coefficients, |l, m, c| {
        *c = google::eval_sh(l, m, dir) as f32;
    });
}

/// Adds `gain` times every basis function of bands `0..=order` in `direction` to `coefficients` (upstream
/// `projectSinglePointAndUpdate`). Panics if `coefficients` is shorter than [`num_coeffs_for_order`]`(order)`.
pub fn project_single_point_and_update(
    direction: Vector3,
    order: usize,
    gain: f32,
    coefficients: &mut [f32],
) {
    let dir = converted_direction_f64(direction);
    for_each_basis(order, coefficients, |l, m, c| {
        *c += gain * google::eval_sh(l, m, dir) as f32;
    });
}

fn for_each_basis(order: usize, coefficients: &mut [f32], mut f: impl FnMut(usize, i32, &mut f32)) {
    let mut coefficients = coefficients[..num_coeffs_for_order(order)].iter_mut();
    for l in 0..=order {
        for m in -(l as i32)..=l as i32 {
            f(l, m, coefficients.next().expect("sized above"));
        }
    }
}

/// Rotation of SH coefficients given in Steam Audio's frame (upstream `SHRotation`).
///
/// Upstream's stateful single-coefficient API (`setCoeff`, `getRotatedCoeff`, `apply(order)`) is unused and not
/// ported.
#[derive(Debug, Clone)]
pub struct ShRotation {
    rotation: google::Rotation,
}

impl ShRotation {
    /// A rotation for bands `0..=order`. Set a rotation before [`apply`](Self::apply).
    pub fn new(order: usize) -> Self {
        Self {
            rotation: google::Rotation::new(order),
        }
    }

    /// The highest band this rotation covers.
    pub fn order(&self) -> usize {
        self.rotation.order()
    }

    /// Rotation from a unit quaternion (upstream `setRotation(Quaternionf)`).
    pub fn set_rotation_quaternion(&mut self, quaternion: Quaternion) {
        self.rotation
            .set_rotation_quaternion(converted_quaternion(quaternion));
    }

    /// Rotation into the given orientation (upstream `setRotation(CoordinateSpace3f)`).
    pub fn set_rotation(&mut self, coordinate_space: &CoordinateSpace3) {
        self.rotation
            .set_rotation(&converted_coordinate_space(coordinate_space));
    }

    /// Rotates bands `0..=min(order, self.order())` of `coeffs` into `rotated`, leaving higher bands of `rotated`
    /// untouched (upstream `apply(order, coeffs, rotatedCoeffs)`).
    pub fn apply(&self, order: usize, coeffs: &[f32], rotated: &mut [f32]) {
        self.rotation.apply(order, coeffs, rotated);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn legendre_recurrence_matches_closed_form() {
        // P4(x) = (35x⁴ - 30x² + 3) / 8, P5(x) = (63x⁵ - 70x³ + 15x) / 8
        for x in [-0.9_f32, -0.3, 0.0, 0.5, 1.0] {
            let p4 = (35.0 * x.powi(4) - 30.0 * x * x + 3.0) / 8.0;
            let p5 = (63.0 * x.powi(5) - 70.0 * x.powi(3) + 15.0 * x) / 8.0;
            assert!((legendre(4, x) - p4).abs() < 1e-6, "P4({x})");
            assert!((legendre(5, x) - p5).abs() < 1e-6, "P5({x})");
        }
    }

    #[test]
    fn identity_rotation_is_identity() {
        let mut rotation = ShRotation::new(3);
        rotation.set_rotation(&CoordinateSpace3::default());
        let coeffs: Vec<f32> = (0..16).map(|i| i as f32 - 7.5).collect();
        let mut rotated = vec![0.0; 16];
        rotation.apply(3, &coeffs, &mut rotated);
        for (r, c) in rotated.iter().zip(&coeffs) {
            assert!((r - c).abs() < 1e-5);
        }
    }

    #[test]
    #[should_panic(expected = "outside")]
    fn evaluate_rejects_bad_m() {
        evaluate(1, 2, Vector3::X_AXIS);
    }
}
