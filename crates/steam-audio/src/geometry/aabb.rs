use crate::math::Vector3;

/// An axis-aligned bounding box (upstream `Box`, renamed to avoid clashing with `std::boxed::Box`).
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Aabb {
    pub min: Vector3,
    pub max: Vector3,
}

impl Default for Aabb {
    /// The empty box: `min` at +∞ and `max` at −∞, so it contains no points and grows correctly under
    /// element-wise min/max.
    fn default() -> Self {
        Self::EMPTY
    }
}

impl Aabb {
    pub const EMPTY: Self = Self {
        min: Vector3::new(f32::INFINITY, f32::INFINITY, f32::INFINITY),
        max: Vector3::new(f32::NEG_INFINITY, f32::NEG_INFINITY, f32::NEG_INFINITY),
    };

    pub const fn new(min: Vector3, max: Vector3) -> Self {
        Self { min, max }
    }

    pub fn contains(&self, point: Vector3) -> bool {
        point.all_ge(self.min) && point.all_le(self.max)
    }

    /// `min` for `index == 0`, `max` for `index == 1` (upstream `coordinates`, used by the slab test).
    pub fn bound(&self, index: usize) -> Vector3 {
        match index {
            0 => self.min,
            1 => self.max,
            _ => panic!("Aabb::bound index must be 0 or 1, got {index}"),
        }
    }

    pub fn center(&self) -> Vector3 {
        (self.min + self.max) * 0.5
    }

    /// Vector from `min` to `max`.
    pub fn extents(&self) -> Vector3 {
        self.max - self.min
    }

    pub fn surface_area(&self) -> f32 {
        let e = self.extents();
        2.0 * (e.x * e.y + e.y * e.z + e.z * e.x)
    }
}
