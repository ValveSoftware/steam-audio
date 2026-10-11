//! Polar coordinate systems (upstream `polar_vector.h`). All angles are in radians and use upstream's
//! truncated π ([`super::PI`]).

use super::{HALF_PI, PI, Vector3};

/// Spherical coordinates: elevation in `[-π/2, π/2]` from the horizontal, azimuth in `[0, 2π)` from straight
/// ahead (−z), counter-clockwise.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct SphericalVector3 {
    pub radius: f32,
    pub elevation: f32,
    pub azimuth: f32,
}

impl SphericalVector3 {
    pub const fn new(radius: f32, elevation: f32, azimuth: f32) -> Self {
        Self {
            radius,
            elevation,
            azimuth,
        }
    }

    pub fn to_cartesian(self) -> Vector3 {
        Vector3::new(
            self.radius * self.elevation.cos() * -self.azimuth.sin(),
            self.radius * self.elevation.sin(),
            self.radius * self.elevation.cos() * -self.azimuth.cos(),
        )
    }
}

impl From<Vector3> for SphericalVector3 {
    /// Azimuth is 0 at the poles. The origin yields NaN angles, as upstream.
    fn from(c: Vector3) -> Self {
        let radius = c.length();
        let elevation = (c.y / radius).asin();
        let azimuth = if (elevation - HALF_PI).abs() < 1e-5 || (elevation + HALF_PI).abs() < 1e-5 {
            0.0
        } else {
            (PI + c.x.atan2(c.z)) % (2.0 * PI)
        };
        Self {
            radius,
            elevation,
            azimuth,
        }
    }
}

/// Interaural spherical coordinates: azimuth in `[-π/2, π/2]` from straight ahead, elevation in `[0, 2π)`
/// measured from downwards.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct InterauralSphericalVector3 {
    pub radius: f32,
    pub azimuth: f32,
    pub elevation: f32,
}

impl InterauralSphericalVector3 {
    pub const fn new(radius: f32, azimuth: f32, elevation: f32) -> Self {
        Self {
            radius,
            azimuth,
            elevation,
        }
    }

    pub fn to_cartesian(self) -> Vector3 {
        Vector3::new(
            self.radius * self.azimuth.sin(),
            self.radius * self.azimuth.cos() * -self.elevation.cos(),
            self.radius * self.azimuth.cos() * -self.elevation.sin(),
        )
    }

    pub fn to_spherical(self) -> SphericalVector3 {
        self.to_cartesian().into()
    }
}

impl From<Vector3> for InterauralSphericalVector3 {
    /// Elevation is 0 on the interaural axis (±x).
    fn from(c: Vector3) -> Self {
        let radius = c.length();
        let azimuth = (c.x / radius).asin();
        let elevation = if (azimuth - HALF_PI).abs() < 1e-5 || (azimuth + HALF_PI).abs() < 1e-5 {
            0.0
        } else {
            (PI + (c.z / radius).atan2(c.y / radius)) % (2.0 * PI)
        };
        Self {
            radius,
            azimuth,
            elevation,
        }
    }
}

impl From<SphericalVector3> for InterauralSphericalVector3 {
    fn from(s: SphericalVector3) -> Self {
        s.to_cartesian().into()
    }
}

/// Cylindrical coordinates: radius from the vertical axis, height above the horizontal, azimuth in `[0, 2π)`
/// from straight ahead, counter-clockwise.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct CylindricalVector3 {
    pub radius: f32,
    pub height: f32,
    pub azimuth: f32,
}

impl CylindricalVector3 {
    pub const fn new(radius: f32, height: f32, azimuth: f32) -> Self {
        Self {
            radius,
            height,
            azimuth,
        }
    }

    pub fn to_cartesian(self) -> Vector3 {
        Vector3::new(
            self.radius * -self.azimuth.sin(),
            self.height,
            self.radius * -self.azimuth.cos(),
        )
    }
}

impl From<Vector3> for CylindricalVector3 {
    /// Azimuth is 0 on the vertical axis.
    fn from(c: Vector3) -> Self {
        let radius = (c.x * c.x + c.z * c.z).sqrt();
        let azimuth = if radius.abs() < 1e-5 {
            0.0
        } else {
            (PI + c.x.atan2(c.z)) % (2.0 * PI)
        };
        Self {
            radius,
            height: c.y,
            azimuth,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn straight_ahead_has_zero_azimuth_wrapped() {
        // Ahead is -z: atan2(0, -1) = π, so azimuth = (PI + π) mod 2PI ≈ 0.
        let s = SphericalVector3::from(Vector3::new(0.0, 0.0, -2.0));
        assert_eq!(s.radius, 2.0);
        assert_eq!(s.elevation, 0.0);
        assert!(s.azimuth.abs() < 1e-5);
    }

    #[test]
    fn pole_forces_zero_azimuth() {
        assert_eq!(
            SphericalVector3::from(Vector3::new(0.0, 1.0, 0.0)).azimuth,
            0.0
        );
        assert_eq!(
            CylindricalVector3::from(Vector3::new(0.0, 5.0, 0.0)).azimuth,
            0.0
        );
        assert_eq!(
            InterauralSphericalVector3::from(Vector3::new(3.0, 0.0, 0.0)).elevation,
            0.0
        );
    }
}
