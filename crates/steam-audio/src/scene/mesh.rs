use crate::bands::NUM_BANDS;
use crate::geometry::Ray;
use crate::math::Vector3;

/// An acoustic material (upstream `Material`): per-band absorption and transmission coefficients and one
/// random-incidence scattering coefficient.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Material {
    pub absorption: [f32; NUM_BANDS],
    pub scattering: f32,
    pub transmission: [f32; NUM_BANDS],
}

/// An indexed triangle (upstream `Triangle`): three indices into a mesh's vertices.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Hash)]
pub struct Triangle {
    pub indices: [u32; 3],
}

impl Triangle {
    pub const fn new(i0: u32, i1: u32, i2: u32) -> Self {
        Self {
            indices: [i0, i1, i2],
        }
    }
}

/// The result of tracing a ray (upstream `Hit`).
///
/// Upstream marks missing indices with `-1` and points at the material; here they are `None`, and the material is
/// copied into the hit.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Hit {
    /// Distance along the ray, `+∞` if nothing was hit.
    pub distance: f32,
    pub triangle_index: Option<usize>,
    /// Never set by [`Scene`](super::Scene); available to custom ray tracers.
    pub object_index: Option<usize>,
    pub material_index: Option<usize>,
    /// Surface normal at the hit point (zero if nothing was hit).
    pub normal: Vector3,
    pub material: Option<Material>,
}

impl Default for Hit {
    fn default() -> Self {
        Self {
            distance: f32::INFINITY,
            triangle_index: None,
            object_index: None,
            material_index: None,
            normal: Vector3::ZERO,
            material: None,
        }
    }
}

impl Hit {
    /// `true` if something was hit (`distance < ∞`; upstream `isValid`).
    pub fn is_valid(&self) -> bool {
        self.distance < f32::INFINITY
    }
}

/// An indexed triangle mesh with one normal per triangle (upstream `Mesh`).
#[derive(Debug, Clone, PartialEq)]
pub struct Mesh {
    vertices: Vec<Vector3>,
    triangles: Vec<Triangle>,
    normals: Vec<Vector3>,
}

impl Mesh {
    /// Copies the geometry and computes the normals, `normalize((v1 - v0) × (v2 - v0))` (zero for degenerate
    /// triangles).
    ///
    /// # Panics
    ///
    /// If a triangle refers to a vertex that does not exist.
    pub fn new(vertices: &[Vector3], triangles: &[Triangle]) -> Self {
        for (i, triangle) in triangles.iter().enumerate() {
            for &index in &triangle.indices {
                assert!(
                    (index as usize) < vertices.len(),
                    "triangle {i} refers to vertex {index}, but the mesh has {} vertices",
                    vertices.len()
                );
            }
        }

        let mut mesh = Self {
            vertices: vertices.to_vec(),
            triangles: triangles.to_vec(),
            normals: Vec::with_capacity(triangles.len()),
        };
        for i in 0..mesh.triangles.len() {
            let [v0, v1, v2] = mesh.triangle_vertices(i);
            mesh.normals
                .push((v1 - v0).cross(v2 - v0).normalize_or_zero());
        }
        mesh
    }

    pub fn num_vertices(&self) -> usize {
        self.vertices.len()
    }

    pub fn num_triangles(&self) -> usize {
        self.triangles.len()
    }

    pub fn vertices(&self) -> &[Vector3] {
        &self.vertices
    }

    pub fn triangles(&self) -> &[Triangle] {
        &self.triangles
    }

    pub fn vertex(&self, i: usize) -> Vector3 {
        self.vertices[i]
    }

    pub fn triangle(&self, i: usize) -> Triangle {
        self.triangles[i]
    }

    /// The three vertices of triangle `i` (upstream `triangleVertex(i, 0..3)`).
    pub fn triangle_vertices(&self, i: usize) -> [Vector3; 3] {
        self.triangles[i]
            .indices
            .map(|index| self.vertices[index as usize])
    }

    pub fn normal(&self, i: usize) -> Vector3 {
        self.normals[i]
    }

    /// Distance along `ray` to triangle `i`, or `+∞` (upstream `Ray::intersect(mesh, i)`).
    pub fn intersect(&self, ray: &Ray, i: usize) -> f32 {
        let [v0, v1, v2] = self.triangle_vertices(i);
        ray.intersect_triangle(v0, v1, v2)
    }
}
