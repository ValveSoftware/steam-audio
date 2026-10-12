use crate::geometry::{Aabb, Ray};
use crate::math::Vector3;

use super::bvh::Bvh;
use super::mesh::{Hit, Material, Mesh, Triangle};

/// A triangle mesh whose geometry never changes, with a material per triangle (upstream `StaticMesh`, the
/// built-in ray tracer's `IStaticMesh`).
#[derive(Debug, Clone)]
pub struct StaticMesh {
    mesh: Mesh,
    bvh: Bvh,
    material_indices: Vec<u32>,
    materials: Vec<Material>,
    /// Materials set by [`Scene::set_static_mesh_material`](super::Scene::set_static_mesh_material), applied by
    /// the next commit (upstream `materialsToUpdate`).
    pub(crate) pending_materials: Vec<Material>,
    pub(crate) materials_changed: bool,
}

impl StaticMesh {
    /// Builds the mesh and its BVH (upstream `StaticMesh(...)`, `IScene::createStaticMesh`).
    ///
    /// # Panics
    ///
    /// If there are no triangles, a triangle refers to a missing vertex, `material_indices` does not have one entry
    /// per triangle, or a material index is out of range.
    pub fn new(
        vertices: &[Vector3],
        triangles: &[Triangle],
        material_indices: &[u32],
        materials: &[Material],
    ) -> Self {
        assert_eq!(
            material_indices.len(),
            triangles.len(),
            "expected one material index per triangle"
        );
        if let Some(&index) = material_indices
            .iter()
            .find(|&&i| i as usize >= materials.len())
        {
            panic!(
                "material index {index} out of range ({} materials)",
                materials.len()
            );
        }

        let mesh = Mesh::new(vertices, triangles);
        let bvh = Bvh::new(&mesh);
        Self {
            mesh,
            bvh,
            material_indices: material_indices.to_vec(),
            materials: materials.to_vec(),
            pending_materials: materials.to_vec(),
            materials_changed: false,
        }
    }

    pub fn num_vertices(&self) -> usize {
        self.mesh.num_vertices()
    }

    pub fn num_triangles(&self) -> usize {
        self.mesh.num_triangles()
    }

    pub fn num_materials(&self) -> usize {
        self.materials.len()
    }

    pub fn mesh(&self) -> &Mesh {
        &self.mesh
    }

    pub fn bvh(&self) -> &Bvh {
        &self.bvh
    }

    pub fn bounds(&self) -> Aabb {
        self.bvh.bounds()
    }

    pub fn material_indices(&self) -> &[u32] {
        &self.material_indices
    }

    /// The materials rays see (set changes become visible here at the next commit).
    pub fn materials(&self) -> &[Material] {
        &self.materials
    }

    /// The closest hit, with normal and material filled in (upstream `closestHit`).
    pub fn closest_hit(&self, ray: &Ray, min_distance: f32, max_distance: f32) -> Hit {
        let mut hit = self
            .bvh
            .closest_hit(ray, &self.mesh, min_distance, max_distance);

        if let (true, Some(triangle)) = (hit.is_valid(), hit.triangle_index) {
            let material_index = self.material_indices[triangle] as usize;
            hit.normal = self.mesh.normal(triangle);
            hit.material_index = Some(material_index);
            hit.material = Some(self.materials[material_index]);
        }

        hit
    }

    /// Upstream `anyHit`.
    pub fn any_hit(&self, ray: &Ray, min_distance: f32, max_distance: f32) -> bool {
        self.bvh
            .any_hit(ray, &self.mesh, min_distance, max_distance)
    }

    /// `true` if any triangle intersects `aabb` (upstream `intersectsBox`).
    pub fn intersects_aabb(&self, aabb: &Aabb) -> bool {
        self.bvh.intersects_aabb(aabb, &self.mesh)
    }

    pub(crate) fn apply_pending_materials(&mut self) {
        if self.materials_changed {
            self.materials.copy_from_slice(&self.pending_materials);
            self.materials_changed = false;
        }
    }
}
