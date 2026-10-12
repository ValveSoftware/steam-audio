use std::sync::Arc;

use crate::geometry::Ray;
use crate::math::{Matrix4, Vector3, Vector4};

use super::mesh::Hit;
use super::{RayTracer, Scene};

/// A scene placed into another scene with a 4×4 transform (upstream `InstancedMesh`).
///
/// The sub-scene is shared and immutable: its committed state at the time of tracing is what the instance shows.
/// Upstream commits the sub-scene whenever the parent scene commits, so a sub-scene must be committed before it
/// is wrapped in an `Arc` here (see `docs/DEVIATIONS.md`).
#[derive(Debug, Clone)]
pub struct InstancedMesh {
    sub_scene: Arc<Scene>,
    transform: Matrix4,
    inverse_transform: Matrix4,
    num_vertices: usize,
    num_triangles: usize,
    has_changed: bool,
}

fn bits(m: &Matrix4) -> impl Iterator<Item = u32> + '_ {
    m.column_major().map(f32::to_bits)
}

impl InstancedMesh {
    /// Upstream `InstancedMesh(subScene, transform)`. The vertex and triangle counts are those of the sub-scene's
    /// committed static meshes.
    pub fn new(sub_scene: Arc<Scene>, transform: Matrix4) -> Self {
        let (num_vertices, num_triangles) = sub_scene
            .committed_static_meshes()
            .fold((0, 0), |(v, t), mesh| {
                (v + mesh.num_vertices(), t + mesh.num_triangles())
            });

        Self {
            sub_scene,
            transform,
            inverse_transform: transform.inverse_or_zero(),
            num_vertices,
            num_triangles,
            has_changed: false,
        }
    }

    pub fn num_vertices(&self) -> usize {
        self.num_vertices
    }

    pub fn num_triangles(&self) -> usize {
        self.num_triangles
    }

    pub fn sub_scene(&self) -> &Arc<Scene> {
        &self.sub_scene
    }

    pub fn transform(&self) -> &Matrix4 {
        &self.transform
    }

    /// Replaces the transform. The instance counts as changed (for the scene's version) if any element differs
    /// bitwise, as upstream's `memcmp` compares them.
    pub fn update_transform(&mut self, transform: Matrix4) {
        if !bits(&transform).eq(bits(&self.transform)) {
            self.has_changed = true;
        }

        self.transform = transform;
        self.inverse_transform = transform.inverse_or_zero();
    }

    /// `true` if the transform has changed since the last commit (upstream `hasChanged`).
    pub fn has_changed(&self) -> bool {
        self.has_changed
    }

    /// Upstream `commit` (minus committing the sub-scene, which is immutable here).
    pub(crate) fn commit(&mut self) {
        self.has_changed = false;
    }

    /// The closest hit in the sub-scene, with the distance and normal transformed back into this scene (upstream
    /// `closestHit`).
    pub fn closest_hit(&self, ray: &Ray, min_distance: f32, max_distance: f32) -> Hit {
        let (transformed_ray, min_distance, max_distance) =
            self.inverse_transform_ray(ray, min_distance, max_distance);
        let hit = self
            .sub_scene
            .closest_hit(&transformed_ray, min_distance, max_distance);
        self.transform_hit(hit, &transformed_ray)
    }

    /// Upstream `anyHit`.
    pub fn any_hit(&self, ray: &Ray, min_distance: f32, max_distance: f32) -> bool {
        let (transformed_ray, min_distance, max_distance) =
            self.inverse_transform_ray(ray, min_distance, max_distance);
        self.sub_scene
            .any_hit(&transformed_ray, min_distance, max_distance)
    }

    /// Upstream `inverseTransformRay`: the ray in the sub-scene's space, and the distance range mapped onto it.
    fn inverse_transform_ray(
        &self,
        ray: &Ray,
        min_distance: f32,
        max_distance: f32,
    ) -> (Ray, f32, f32) {
        let origin = self.inverse_transform * Vector4::from_point(ray.origin);

        let start =
            self.inverse_transform * Vector4::from_point(ray.point_at_distance(min_distance));
        let min_distance = (start - origin).length();

        let mut max_distance = max_distance;
        if max_distance < f32::INFINITY {
            let end =
                self.inverse_transform * Vector4::from_point(ray.point_at_distance(max_distance));
            max_distance = (end - origin).length();
        }

        let p = self.inverse_transform * Vector4::from_point(ray.point_at_distance(1.0));
        let direction = (p.xyz() - origin.xyz()).normalize_or_zero();

        (
            Ray::new(origin.xyz(), direction),
            min_distance,
            max_distance,
        )
    }

    /// Upstream `transformHit`.
    fn transform_hit(&self, hit: Hit, ray: &Ray) -> Hit {
        let mut transformed = hit;

        if hit.distance < f32::INFINITY {
            let origin = self.transform * Vector4::from_point(ray.origin);
            let hit_point =
                self.transform * Vector4::from_point(ray.point_at_distance(hit.distance));
            transformed.distance = (hit_point - origin).length();
        }

        let normal = Vector4::new(hit.normal.x, hit.normal.y, hit.normal.z, 0.0);
        let transformed_normal = self.inverse_transform.transpose() * normal;
        transformed.normal = Vector3::new(
            transformed_normal.x,
            transformed_normal.y,
            transformed_normal.z,
        )
        .normalize_or_zero();

        transformed
    }
}
