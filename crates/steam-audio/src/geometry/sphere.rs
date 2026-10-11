use crate::math::Vector3;

/// A sphere (upstream `Sphere`).
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Sphere {
    pub center: Vector3,
    pub radius: f32,
}

impl Sphere {
    pub const fn new(center: Vector3, radius: f32) -> Self {
        Self { center, radius }
    }

    /// `true` if `point` is inside or on the surface.
    pub fn contains(&self, point: Vector3) -> bool {
        (point - self.center).length_squared() <= self.radius * self.radius
    }

    /// The smallest sphere containing both `a` and `b` (upstream `computeBoundingSphere`).
    pub fn bounding(a: &Self, b: &Self) -> Self {
        let between_centers = a.center - b.center;
        let sq_dist = between_centers.length_squared();
        let radii_difference = a.radius - b.radius;

        // One sphere contains the other.
        if sq_dist < radii_difference * radii_difference {
            return if a.radius > b.radius { *a } else { *b };
        }

        let radius = (a.radius + b.radius + sq_dist.sqrt()) * 0.5;
        let axis = between_centers.normalize_or_zero();
        Self {
            center: b.center + axis * (radius - b.radius),
            radius,
        }
    }
}
