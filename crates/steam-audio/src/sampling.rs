//! Deterministic low-discrepancy direction and point sets (upstream `sampling.{h,cpp}`).
//!
//! Upstream's `RandomNumberGenerator` is seeded with the wall clock and only used by the reflection simulator; it is
//! ported with that stage.

use crate::geometry::Sphere;
use crate::math::{CoordinateSpace3, PI, Vector3, cpp_max, cpp_min};

/// Directions uniformly covering the unit sphere, one per element of `out`.
pub fn sphere_samples(out: &mut [Vector3]) {
    let n = out.len() as f32;
    for (i, sample) in out.iter_mut().enumerate() {
        let u = (i as f32 + 0.5) / n;
        let v = hammersley(i as u32);
        let azimuth = 2.0 * PI * u;
        let height = 2.0 * v - 1.0;
        let horizontal = (1.0 - height * height).sqrt();
        *sample = Vector3::new(
            horizontal * azimuth.cos(),
            height,
            horizontal * -azimuth.sin(),
        );
    }
}

/// Directions covering a unit hemisphere, one per element of `out`.
///
/// Upstream documents the hemisphere's normal as +y, but the samples actually have `z <= 0`, i.e. the hemisphere
/// is centred on −z (local "ahead"), which is what [`transform_hemisphere_sample`] expects.
pub fn hemisphere_samples(out: &mut [Vector3]) {
    let n = out.len() as f32;
    for (i, sample) in out.iter_mut().enumerate() {
        let u = (i as f32 + 0.5) / n;
        let v = hammersley(i as u32);
        let r = u.sqrt();
        let theta = 2.0 * PI * v;
        let x = r * theta.cos();
        let y = r * theta.sin();
        let z = -cpp_max(0.0, 1.0 - x * x - y * y).sqrt();
        *sample = Vector3::new(x, y, z);
    }
}

/// Rotates a hemisphere sample so that the hemisphere is centred on `normal`.
pub fn transform_hemisphere_sample(sample: Vector3, normal: Vector3) -> Vector3 {
    let surface = CoordinateSpace3::from_ahead(normal, Vector3::ZERO);
    surface.direction_local_to_world(sample).normalize_or_zero()
}

/// Points uniformly filling the unit ball, one per element of `out` (Halton sequence in bases 2, 3, 5).
pub fn sphere_volume_samples(out: &mut [Vector3]) {
    for (i, sample) in out.iter_mut().enumerate() {
        let i = i as u32;
        let u_phi = radical_inverse(2, i);
        let u_theta = radical_inverse(3, i);
        let u_r = radical_inverse(5, i);

        let phi = 2.0 * PI * u_phi;
        let theta = (2.0 * u_theta - 1.0).acos();
        let r = u_r.cbrt();

        *sample = Vector3::new(
            r * theta.sin() * phi.cos(),
            r * theta.sin() * phi.sin(),
            r * theta.cos(),
        );
    }
}

/// Maps a unit-ball sample into `sphere`.
pub fn transform_sphere_volume_sample(sample: Vector3, sphere: &Sphere) -> Vector3 {
    sphere.center + sample * sphere.radius
}

/// Base-2 radical inverse (van der Corput), accumulated in `f32` like upstream.
fn hammersley(i: u32) -> f32 {
    let mut value = 0.0;
    let mut p = 0.5;
    let mut k = i;
    while k > 0 {
        if k & 1 != 0 {
            value += p;
        }
        p *= 0.5;
        k >>= 1;
    }
    value
}

/// Radical inverse of `i` in base `p`, clamped to 1.
fn radical_inverse(p: u32, mut i: u32) -> f32 {
    let inv = 1.0 / p as f32;
    let mut reversed: u32 = 0;
    let mut inv_n = 1.0;
    while i != 0 {
        let next = i / p;
        let digit = i - next * p;
        reversed = reversed.wrapping_mul(p).wrapping_add(digit);
        inv_n *= inv;
        i = next;
    }
    cpp_min(reversed as f32 * inv_n, 1.0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn van_der_corput() {
        let values: Vec<f32> = (0..5).map(hammersley).collect();
        assert_eq!(values, [0.0, 0.5, 0.25, 0.75, 0.125]);
        assert_eq!(radical_inverse(3, 1), 1.0 / 3.0);
    }

    #[test]
    fn sphere_samples_are_unit_vectors() {
        let mut samples = [Vector3::ZERO; 50];
        sphere_samples(&mut samples);
        assert!(samples.iter().all(|s| (s.length() - 1.0).abs() < 1e-5));
    }
}
