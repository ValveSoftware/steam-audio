use std::ops::Mul;

use super::Matrix3;

/// A quaternion `xi + yj + zk + w` (upstream `Quaternion<T>`). Only unit quaternions represent rotations.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Quaternion {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl Quaternion {
    pub const IDENTITY: Self = Self::new(0.0, 0.0, 0.0, 1.0);

    pub const fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
        Self { x, y, z, w }
    }

    pub fn length_squared(self) -> f32 {
        self.x * self.x + self.y * self.y + self.z * self.z + self.w * self.w
    }

    pub fn length(self) -> f32 {
        self.length_squared().sqrt()
    }

    /// Scales to unit length.
    ///
    /// Upstream computes the reciprocal length in double precision (`1.0 / length()`) and multiplies each
    /// component in double before rounding back to `f32`; this does the same for parity.
    pub fn normalize(&mut self) {
        let one_over_len = 1.0 / f64::from(self.length());
        for c in [&mut self.x, &mut self.y, &mut self.z, &mut self.w] {
            *c = (f64::from(*c) * one_over_len) as f32;
        }
    }

    pub fn normalized(mut self) -> Self {
        self.normalize();
        self
    }

    /// The equivalent 3×3 rotation matrix. `self` must be normalized.
    pub fn to_rotation_matrix(self) -> Matrix3 {
        let Self { x, y, z, w } = self;
        let (x2, y2, z2) = (x + x, y + y, z + z);
        let (x_sq2, y_sq2, z_sq2) = (x * x2, y * y2, z * z2);
        let (xw2, yw2, zw2) = (x2 * w, y2 * w, z2 * w);
        let (xy2, xz2, yz2) = (x2 * y, x2 * z, y2 * z);

        Matrix3::from_rows([
            [1.0 - y_sq2 - z_sq2, xy2 - zw2, xz2 + yw2],
            [xy2 + zw2, 1.0 - x_sq2 - z_sq2, yz2 - xw2],
            [xz2 - yw2, yz2 + xw2, 1.0 - x_sq2 - y_sq2],
        ])
    }
}

/// Hamilton product (concatenation of rotations).
impl Mul for Quaternion {
    type Output = Self;

    fn mul(self, rhs: Self) -> Self {
        let (l, r) = (self, rhs);
        Self::new(
            l.w * r.x + l.x * r.w + l.y * r.z - l.z * r.y,
            l.w * r.y + l.y * r.w + l.z * r.x - l.x * r.z,
            l.w * r.z + l.z * r.w + l.x * r.y - l.y * r.x,
            l.w * r.w - l.x * r.x - l.y * r.y - l.z * r.z,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn identity_rotation() {
        assert_eq!(Quaternion::IDENTITY.to_rotation_matrix(), Matrix3::IDENTITY);
        let q = Quaternion::new(0.1, 0.2, 0.3, 0.4);
        assert_eq!(q * Quaternion::IDENTITY, q);
    }

    #[test]
    fn normalize_gives_unit_length() {
        let q = Quaternion::new(1.0, 2.0, 3.0, 4.0).normalized();
        assert!((q.length() - 1.0).abs() < 1e-6);
    }
}
