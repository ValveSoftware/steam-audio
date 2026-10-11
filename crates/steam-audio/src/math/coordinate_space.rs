use super::{Matrix3, Vector3};

/// A right-handed Cartesian coordinate system: axes and origin (upstream `CoordinateSpace3<T>`).
///
/// Local +x is `right`, +y is `up`, and −z is `ahead`.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct CoordinateSpace3 {
    pub right: Vector3,
    pub up: Vector3,
    pub ahead: Vector3,
    pub origin: Vector3,
}

impl Default for CoordinateSpace3 {
    /// The canonical space: right = +x, up = +y, ahead = −z, at the world origin.
    fn default() -> Self {
        Self::canonical_at(Vector3::ZERO)
    }
}

impl CoordinateSpace3 {
    /// Canonical axes at `origin`.
    pub const fn canonical_at(origin: Vector3) -> Self {
        Self {
            right: Vector3::new(1.0, 0.0, 0.0),
            up: Vector3::new(0.0, 1.0, 0.0),
            ahead: Vector3::new(0.0, 0.0, -1.0),
            origin,
        }
    }

    /// From two mutually perpendicular unit vectors; `right = ahead × up`.
    pub fn from_ahead_up(ahead: Vector3, up: Vector3, origin: Vector3) -> Self {
        Self {
            right: ahead.cross(up),
            up,
            ahead,
            origin,
        }
    }

    /// From a single unit `ahead` vector. The remaining axes are chosen with the heuristic of Hughes & Möller,
    /// "Building an orthonormal basis from a unit vector" (1999), as upstream does.
    pub fn from_ahead(ahead: Vector3, origin: Vector3) -> Self {
        let right = if ahead.x.abs() > ahead.z.abs() {
            Vector3::new(-ahead.y, ahead.x, 0.0).normalize_or_zero()
        } else {
            Vector3::new(0.0, -ahead.z, ahead.y).normalize_or_zero()
        };
        let up = right.cross(ahead);
        Self {
            right,
            up,
            ahead,
            origin,
        }
    }

    /// Rotation from the canonical space into this one: rows are `right`, `up`, `−ahead`.
    pub fn to_rotation_matrix(&self) -> Matrix3 {
        Matrix3::from_rows([
            self.right.to_array(),
            self.up.to_array(),
            (-self.ahead).to_array(),
        ])
    }

    pub fn direction_world_to_local(&self, direction: Vector3) -> Vector3 {
        Vector3::new(
            direction.dot(self.right),
            direction.dot(self.up),
            -direction.dot(self.ahead),
        )
    }

    pub fn direction_local_to_world(&self, direction: Vector3) -> Vector3 {
        direction.x * self.right + direction.y * self.up - direction.z * self.ahead
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn canonical_round_trip() {
        let space = CoordinateSpace3::default();
        let d = Vector3::new(0.3, -0.5, 0.8);
        assert_eq!(space.direction_world_to_local(d), d);
        assert_eq!(space.direction_local_to_world(d), d);
        assert_eq!(space.to_rotation_matrix(), Matrix3::IDENTITY);
    }

    #[test]
    fn from_ahead_up_is_right_handed() {
        let space =
            CoordinateSpace3::from_ahead_up(-Vector3::Z_AXIS, Vector3::Y_AXIS, Vector3::ZERO);
        assert_eq!(space.right, Vector3::X_AXIS);
    }
}
