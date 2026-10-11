//! Geometric primitives used by the ray tracer (upstream `sphere.h`, `box.h`, `ray.{h,cpp}`).

mod aabb;
mod ray;
mod sphere;

pub use aabb::Aabb;
pub use ray::Ray;
pub use sphere::Sphere;
