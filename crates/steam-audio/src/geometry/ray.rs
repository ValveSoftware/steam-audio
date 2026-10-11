use crate::math::{Vector3, cpp_max, cpp_min};

use super::{Aabb, Sphere};

/// A ray (upstream `Ray`). `direction` is expected to be a unit vector.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Ray {
    pub origin: Vector3,
    pub direction: Vector3,
}

impl Ray {
    pub const fn new(origin: Vector3, direction: Vector3) -> Self {
        Self { origin, direction }
    }

    pub fn point_at_distance(&self, distance: f32) -> Vector3 {
        self.origin + distance * self.direction
    }

    /// Distance to the triangle `(v0, v1, v2)`, or +∞ if the ray misses it or is parallel to it.
    ///
    /// Möller–Trumbore, "Fast, Minimum Storage Ray/Triangle Intersection" (1997). The returned distance can be
    /// negative (hit behind the origin); callers filter by range, as upstream's BVH does.
    pub fn intersect_triangle(&self, v0: Vector3, v1: Vector3, v2: Vector3) -> f32 {
        let edge1 = v1 - v0;
        let edge2 = v2 - v0;

        let p = self.direction.cross(edge2);
        let determinant = edge1.dot(p);
        if determinant == 0.0 {
            return f32::INFINITY;
        }
        let inverse_determinant = 1.0 / determinant;

        let t = self.origin - v0;
        let u = t.dot(p) * inverse_determinant;
        // Not `!(0.0..=1.0).contains(&u)`: that would also reject NaN, which upstream lets through.
        #[allow(clippy::manual_range_contains)]
        if u < 0.0 || 1.0 < u {
            return f32::INFINITY;
        }

        let q = t.cross(edge1);
        let v = self.direction.dot(q) * inverse_determinant;
        if v < 0.0 || 1.0 - u < v {
            return f32::INFINITY;
        }

        edge2.dot(q) * inverse_determinant
    }

    /// Branchless slab test (Williams et al., "An Efficient and Robust Ray-Box Intersection Algorithm", 2005).
    ///
    /// `inverse_direction` is `direction.recip()`, and `direction_signs[i]` is `1` if `direction[i] >= 0`, else
    /// `0`. On entry `t_min..=t_max` is the interval of interest; on return it is narrowed to where the ray is
    /// inside the box, **even when the result is `false`** (upstream behaviour, relied on by BVH traversal).
    pub fn intersect_aabb(
        &self,
        aabb: &Aabb,
        inverse_direction: Vector3,
        direction_signs: [usize; 3],
        t_min: &mut f32,
        t_max: &mut f32,
    ) -> bool {
        for axis in 0..3 {
            let near = aabb.bound(direction_signs[axis] ^ 1)[axis];
            let far = aabb.bound(direction_signs[axis])[axis];
            let minimum = (near - self.origin[axis]) * inverse_direction[axis];
            let maximum = (far - self.origin[axis]) * inverse_direction[axis];
            *t_min = cpp_max(*t_min, minimum);
            *t_max = cpp_min(*t_max, maximum);
        }
        *t_min <= *t_max
    }

    /// Distance to the nearer intersection with `sphere` along the ray, or +∞ if the ray's line misses it.
    ///
    /// As upstream, this is the smaller root of the quadratic, so it is negative when the origin is inside or
    /// in front of the sphere.
    pub fn intersect_sphere(&self, sphere: &Sphere) -> f32 {
        let v = self.origin - sphere.center;
        let r = sphere.radius;
        let b = 2.0 * v.dot(self.direction);
        let c = v.length_squared() - r * r;
        let d = b * b - 4.0 * c;
        if d < 0.0 {
            return f32::INFINITY;
        }
        -0.5 * (b + d.sqrt())
    }
}
