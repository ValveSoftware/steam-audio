use std::ops::{
    Add, AddAssign, Div, DivAssign, Index, IndexMut, Mul, MulAssign, Neg, Sub, SubAssign,
};

use super::{PI_F64, cpp_max, cpp_min};

/// Generates the parts shared by all fixed-size vectors (upstream `Vector<T, D>`).
macro_rules! vector_type {
    ($name:ident, $dim:literal, $($field:ident),+) => {
        #[derive(Debug, Clone, Copy, Default, PartialEq)]
        pub struct $name {
            $(pub $field: f32,)+
        }

        impl $name {
            pub const ZERO: Self = Self { $($field: 0.0,)+ };

            pub const fn new($($field: f32),+) -> Self {
                Self { $($field,)+ }
            }

            pub const fn from_array(a: [f32; $dim]) -> Self {
                let [$($field),+] = a;
                Self { $($field,)+ }
            }

            pub const fn to_array(self) -> [f32; $dim] {
                [$(self.$field),+]
            }

            pub fn dot(self, rhs: Self) -> f32 {
                // Accumulates from 0 in element order, like upstream.
                let mut out = 0.0;
                $(out += self.$field * rhs.$field;)+
                out
            }

            pub fn length_squared(self) -> f32 {
                self.dot(self)
            }

            pub fn length(self) -> f32 {
                self.length_squared().sqrt()
            }

            /// Element-wise minimum (`std::min` semantics).
            pub fn min(self, rhs: Self) -> Self {
                Self { $($field: cpp_min(self.$field, rhs.$field),)+ }
            }

            /// Element-wise maximum (`std::max` semantics).
            pub fn max(self, rhs: Self) -> Self {
                Self { $($field: cpp_max(self.$field, rhs.$field),)+ }
            }

            /// Element-wise `1 / x`.
            pub fn recip(self) -> Self {
                Self { $($field: 1.0 / self.$field,)+ }
            }

            /// Element-wise square root.
            pub fn sqrt(self) -> Self {
                Self { $($field: self.$field.sqrt(),)+ }
            }

            pub fn min_component(self) -> f32 {
                self[self.index_of_min_component()]
            }

            pub fn max_component(self) -> f32 {
                self[self.index_of_max_component()]
            }

            pub fn min_abs_component(self) -> f32 {
                self[self.index_of_min_abs_component()]
            }

            pub fn max_abs_component(self) -> f32 {
                self[self.index_of_max_abs_component()]
            }

            /// Index of the first smallest element (`std::min_element`).
            pub fn index_of_min_component(self) -> usize {
                first_index_by(&self.to_array(), |best, x| x < best)
            }

            /// Index of the first largest element (`std::max_element`).
            pub fn index_of_max_component(self) -> usize {
                first_index_by(&self.to_array(), |best, x| best < x)
            }

            pub fn index_of_min_abs_component(self) -> usize {
                first_index_by(&self.to_array(), |best, x| x.abs() < best.abs())
            }

            pub fn index_of_max_abs_component(self) -> usize {
                first_index_by(&self.to_array(), |best, x| best.abs() < x.abs())
            }

            /// `true` if every element is `<` the corresponding element of `rhs` (upstream `operator<`).
            pub fn all_lt(self, rhs: Self) -> bool {
                true $(&& self.$field < rhs.$field)+
            }

            /// `true` if every element is `<=` the corresponding element of `rhs` (upstream `operator<=`).
            pub fn all_le(self, rhs: Self) -> bool {
                true $(&& self.$field <= rhs.$field)+
            }

            /// `true` if every element is `>` the corresponding element of `rhs` (upstream `operator>`).
            pub fn all_gt(self, rhs: Self) -> bool {
                true $(&& self.$field > rhs.$field)+
            }

            /// `true` if every element is `>=` the corresponding element of `rhs` (upstream `operator>=`).
            pub fn all_ge(self, rhs: Self) -> bool {
                true $(&& self.$field >= rhs.$field)+
            }
        }

        impl From<[f32; $dim]> for $name {
            fn from(a: [f32; $dim]) -> Self {
                Self::from_array(a)
            }
        }

        impl From<$name> for [f32; $dim] {
            fn from(v: $name) -> Self {
                v.to_array()
            }
        }

        impl Index<usize> for $name {
            type Output = f32;

            fn index(&self, i: usize) -> &f32 {
                let mut n = 0;
                $(
                    if i == n {
                        return &self.$field;
                    }
                    n += 1;
                )+
                panic!("index {i} out of range for {} (len {n})", stringify!($name))
            }
        }

        impl IndexMut<usize> for $name {
            fn index_mut(&mut self, i: usize) -> &mut f32 {
                let mut n = 0;
                $(
                    if i == n {
                        return &mut self.$field;
                    }
                    n += 1;
                )+
                panic!("index {i} out of range for {} (len {n})", stringify!($name))
            }
        }

        impl Neg for $name {
            type Output = Self;

            fn neg(self) -> Self {
                Self { $($field: -self.$field,)+ }
            }
        }

        impl AddAssign for $name {
            fn add_assign(&mut self, rhs: Self) {
                $(self.$field += rhs.$field;)+
            }
        }

        impl SubAssign for $name {
            fn sub_assign(&mut self, rhs: Self) {
                $(self.$field -= rhs.$field;)+
            }
        }

        /// Element-wise product.
        impl MulAssign for $name {
            fn mul_assign(&mut self, rhs: Self) {
                $(self.$field *= rhs.$field;)+
            }
        }

        impl MulAssign<f32> for $name {
            fn mul_assign(&mut self, s: f32) {
                $(self.$field *= s;)+
            }
        }

        /// Element-wise quotient, computed as multiplication by the reciprocal like upstream.
        impl DivAssign for $name {
            fn div_assign(&mut self, rhs: Self) {
                $(self.$field *= 1.0 / rhs.$field;)+
            }
        }

        /// Computed as multiplication by `1 / s` like upstream, which can differ from `x / s` in the last bit.
        impl DivAssign<f32> for $name {
            fn div_assign(&mut self, s: f32) {
                debug_assert!(s != 0.0, "division of a vector by zero");
                let r = 1.0 / s;
                $(self.$field *= r;)+
            }
        }

        vector_binary_op!($name, Add, add, AddAssign, add_assign);
        vector_binary_op!($name, Sub, sub, SubAssign, sub_assign);
        vector_binary_op!($name, Mul, mul, MulAssign, mul_assign);
        vector_binary_op!($name, Div, div, DivAssign, div_assign);

        impl Mul<f32> for $name {
            type Output = Self;

            fn mul(mut self, s: f32) -> Self {
                self *= s;
                self
            }
        }

        impl Mul<$name> for f32 {
            type Output = $name;

            fn mul(self, mut v: $name) -> $name {
                v *= self;
                v
            }
        }

        impl Div<f32> for $name {
            type Output = Self;

            fn div(mut self, s: f32) -> Self {
                self /= s;
                self
            }
        }
    };
}

macro_rules! vector_binary_op {
    ($name:ident, $op:ident, $method:ident, $assign_op:ident, $assign_method:ident) => {
        impl $op for $name {
            type Output = Self;

            fn $method(mut self, rhs: Self) -> Self {
                $assign_op::$assign_method(&mut self, rhs);
                self
            }
        }
    };
}

/// Index of the first element that no later element `beats` (the `std::min_element`/`max_element` contract).
fn first_index_by(values: &[f32], beats: impl Fn(f32, f32) -> bool) -> usize {
    let mut best = 0;
    for (i, &x) in values.iter().enumerate().skip(1) {
        if beats(values[best], x) {
            best = i;
        }
    }
    best
}

vector_type!(Vector2, 2, x, y);
vector_type!(Vector3, 3, x, y, z);
vector_type!(Vector4, 4, x, y, z, w);

impl Vector2 {
    pub const X_AXIS: Self = Self::new(1.0, 0.0);
    pub const Y_AXIS: Self = Self::new(0.0, 1.0);
}

impl Vector3 {
    pub const X_AXIS: Self = Self::new(1.0, 0.0, 0.0);
    pub const Y_AXIS: Self = Self::new(0.0, 1.0, 0.0);
    pub const Z_AXIS: Self = Self::new(0.0, 0.0, 1.0);

    /// Lengths at or below this are treated as zero by [`Vector3::normalize_or_zero`] (upstream `kNearlyZero`).
    pub const NEARLY_ZERO: f32 = 1e-5;

    /// Unit vector in the direction of `self`, or zero if `length() <= 1e-5` (upstream `unitVector`).
    pub fn normalize_or_zero(self) -> Self {
        let length = self.length();
        if length <= Self::NEARLY_ZERO {
            Self::ZERO
        } else {
            self / length
        }
    }

    pub fn cross(self, rhs: Self) -> Self {
        Self::new(
            self.y * rhs.z - self.z * rhs.y,
            self.z * rhs.x - self.x * rhs.z,
            self.x * rhs.y - self.y * rhs.x,
        )
    }

    /// Angle between `from` and `to`, in `[0, π]`. `from` must be a unit vector (upstream `angleBetween`).
    pub fn angle_between(from: Self, to: Self) -> f32 {
        let parallel = to.dot(from) * from;
        let perpendicular = to - parallel;
        let mut angle = perpendicular.length().atan2(parallel.length());
        // Unreachable for finite input (atan2 of a non-negative y is non-negative); kept for parity.
        if angle < 0.0 {
            angle += (2.0 * PI_F64) as f32;
        }
        angle
    }

    /// Mirror direction of `incident` about `normal`, normalized (upstream `reflect`).
    pub fn reflect(incident: Self, normal: Self) -> Self {
        (incident - normal * 2.0 * incident.dot(normal)).normalize_or_zero()
    }
}

impl Vector4 {
    pub const X_AXIS: Self = Self::new(1.0, 0.0, 0.0, 0.0);
    pub const Y_AXIS: Self = Self::new(0.0, 1.0, 0.0, 0.0);
    pub const Z_AXIS: Self = Self::new(0.0, 0.0, 1.0, 0.0);
    pub const W_AXIS: Self = Self::new(0.0, 0.0, 0.0, 1.0);

    /// Homogeneous point: `(x, y, z, 1)`.
    pub const fn from_point(p: Vector3) -> Self {
        Self::new(p.x, p.y, p.z, 1.0)
    }

    pub const fn xyz(self) -> Vector3 {
        Vector3::new(self.x, self.y, self.z)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn arithmetic() {
        let a = Vector3::new(1.0, 2.0, 3.0);
        let b = Vector3::new(4.0, -5.0, 6.0);
        assert_eq!(a + b, Vector3::new(5.0, -3.0, 9.0));
        assert_eq!(a - b, Vector3::new(-3.0, 7.0, -3.0));
        assert_eq!(a * b, Vector3::new(4.0, -10.0, 18.0));
        assert_eq!(2.0 * a, a * 2.0);
        assert_eq!(-a, Vector3::new(-1.0, -2.0, -3.0));
        assert_eq!(a.dot(b), 12.0);
        assert_eq!(Vector3::X_AXIS.cross(Vector3::Y_AXIS), Vector3::Z_AXIS);
    }

    #[test]
    fn component_queries_pick_first_on_ties() {
        let v = Vector4::new(2.0, -3.0, 2.0, 3.0);
        assert_eq!(v.index_of_max_component(), 3);
        assert_eq!(v.index_of_min_component(), 1);
        assert_eq!(v.index_of_max_abs_component(), 1);
        assert_eq!(v.index_of_min_abs_component(), 0);
        assert_eq!(v.max_abs_component(), -3.0);
    }

    #[test]
    fn element_wise_comparisons() {
        let lo = Vector3::new(0.0, 0.0, 0.0);
        let hi = Vector3::new(1.0, 1.0, 0.0);
        assert!(lo.all_le(hi));
        assert!(!lo.all_lt(hi));
        assert!(hi.all_ge(lo));
        assert!(!hi.all_gt(lo));
    }

    #[test]
    fn indexing() {
        let mut v = Vector2::new(1.0, 2.0);
        v[1] = 5.0;
        assert_eq!(v[0] + v[1], 6.0);
        assert_eq!(Vector4::from_point(Vector3::new(1.0, 2.0, 3.0)).w, 1.0);
    }

    #[test]
    #[should_panic(expected = "out of range")]
    fn index_out_of_range_panics() {
        let _ = Vector3::ZERO[3];
    }
}
