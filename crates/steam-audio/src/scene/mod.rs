//! Scene geometry and the built-in ray tracer (upstream `mesh`, `bvh`, `static_mesh`, `instanced_mesh`,
//! `scene`, `custom_scene`, `material`, `hit`, `triangle`).
//!
//! [`Scene`] is upstream's default scene type. Upstream's `CustomScene` (user callbacks) is the [`RayTracer`]
//! trait: implement it for your own type. Embree and Radeon Rays scenes are out of scope.

mod bvh;
mod instanced_mesh;
mod mesh;
mod msvc_sort;
mod static_mesh;

use std::io::{self, Write};
use std::path::Path;

pub use bvh::{Bvh, BvhNode, BvhNodeKind};
pub use instanced_mesh::InstancedMesh;
pub use mesh::{Hit, Material, Mesh, Triangle};
pub use static_mesh::StaticMesh;

use crate::geometry::{Aabb, Ray};
use crate::math::{Matrix4, Vector3, Vector4};

/// Something rays can be traced through (upstream `IScene`'s ray queries).
///
/// [`Scene`] implements it; implementing it for another type replaces upstream's `CustomScene` and its
/// callbacks. The batched methods default to calling the single-ray ones, as `CustomScene` does without batched
/// callbacks.
pub trait RayTracer: Send + Sync {
    /// The closest hit at a distance in `[min_distance, max_distance]`, or [`Hit::default`] (upstream
    /// `closestHit`).
    fn closest_hit(&self, ray: &Ray, min_distance: f32, max_distance: f32) -> Hit;

    /// `true` if anything is hit at a distance in `[min_distance, max_distance)` (upstream `anyHit`).
    fn any_hit(&self, ray: &Ray, min_distance: f32, max_distance: f32) -> bool;

    /// Upstream `closestHits`.
    ///
    /// # Panics
    ///
    /// If the slices have different lengths.
    fn closest_hits(
        &self,
        rays: &[Ray],
        min_distances: &[f32],
        max_distances: &[f32],
        hits: &mut [Hit],
    ) {
        check_batch(
            rays.len(),
            min_distances.len(),
            max_distances.len(),
            hits.len(),
        );
        for (i, hit) in hits.iter_mut().enumerate() {
            *hit = self.closest_hit(&rays[i], min_distances[i], max_distances[i]);
        }
    }

    /// Upstream `anyHits`: a ray with a negative maximum distance counts as occluded.
    ///
    /// # Panics
    ///
    /// If the slices have different lengths.
    fn any_hits(
        &self,
        rays: &[Ray],
        min_distances: &[f32],
        max_distances: &[f32],
        occluded: &mut [bool],
    ) {
        check_batch(
            rays.len(),
            min_distances.len(),
            max_distances.len(),
            occluded.len(),
        );
        for (i, occluded) in occluded.iter_mut().enumerate() {
            *occluded = if max_distances[i] >= 0.0 {
                self.any_hit(&rays[i], min_distances[i], max_distances[i])
            } else {
                true
            };
        }
    }

    /// `true` if the segment from `from` to `to` hits anything (upstream `IScene::isOccluded`).
    fn is_occluded(&self, from: Vector3, to: Vector3) -> bool {
        self.any_hit(
            &Ray::new(from, (to - from).normalize_or_zero()),
            0.0,
            (to - from).length(),
        )
    }
}

fn check_batch(rays: usize, min_distances: usize, max_distances: usize, outputs: usize) {
    assert!(
        rays == min_distances && rays == max_distances && rays == outputs,
        "batch lengths differ: {rays} rays, {min_distances} minimum and {max_distances} maximum distances, \
         {outputs} outputs"
    );
}

/// Identifies a [`StaticMesh`] stored in a [`Scene`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct StaticMeshId(Key);

/// Identifies an [`InstancedMesh`] stored in a [`Scene`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct InstancedMeshId(Key);

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
struct Key {
    index: u32,
    generation: u32,
}

#[derive(Debug, Clone)]
struct Slot<T> {
    value: Option<T>,
    generation: u32,
    released: bool,
}

/// Storage for the meshes a scene owns. A slot is reused only after its mesh has been released and is no longer
/// part of the pending or committed contents; its generation then changes so old ids stop matching.
#[derive(Debug, Clone)]
struct Arena<T> {
    slots: Vec<Slot<T>>,
    free: Vec<u32>,
}

impl<T> Default for Arena<T> {
    fn default() -> Self {
        Self {
            slots: Vec::new(),
            free: Vec::new(),
        }
    }
}

impl<T> Arena<T> {
    fn insert(&mut self, value: T) -> Key {
        if let Some(index) = self.free.pop() {
            let slot = &mut self.slots[index as usize];
            slot.value = Some(value);
            slot.released = false;
            Key {
                index,
                generation: slot.generation,
            }
        } else {
            let index = u32::try_from(self.slots.len()).expect("too many meshes in one scene");
            self.slots.push(Slot {
                value: Some(value),
                generation: 0,
                released: false,
            });
            Key {
                index,
                generation: 0,
            }
        }
    }

    fn slot(&self, key: Key) -> Option<&Slot<T>> {
        self.slots
            .get(key.index as usize)
            .filter(|slot| slot.generation == key.generation && slot.value.is_some())
    }

    fn get(&self, key: Key) -> Option<&T> {
        self.slot(key)
            .filter(|slot| !slot.released)
            .and_then(|slot| slot.value.as_ref())
    }

    /// Like `get`, but also finds released meshes that are still committed.
    fn get_live(&self, key: Key) -> &T {
        self.slot(key)
            .and_then(|slot| slot.value.as_ref())
            .expect("committed mesh is stored")
    }

    fn get_live_mut(&mut self, key: Key) -> &mut T {
        self.slots[key.index as usize]
            .value
            .as_mut()
            .expect("committed mesh is stored")
    }

    #[track_caller]
    fn expect(&self, key: Key, what: &str) -> &T {
        self.get(key)
            .unwrap_or_else(|| panic!("{what} id does not refer to a mesh of this scene"))
    }

    #[track_caller]
    fn expect_mut(&mut self, key: Key, what: &str) -> &mut T {
        match self.slots.get_mut(key.index as usize) {
            Some(slot) if slot.generation == key.generation && !slot.released => {
                if let Some(value) = slot.value.as_mut() {
                    return value;
                }
            }
            _ => {}
        }
        panic!("{what} id does not refer to a mesh of this scene")
    }

    fn release(&mut self, key: Key) {
        if let Some(slot) = self.slots.get_mut(key.index as usize) {
            if slot.generation == key.generation && slot.value.is_some() {
                slot.released = true;
            }
        }
    }

    /// Frees released slots that `in_use` no longer mentions.
    fn collect(&mut self, in_use: impl Fn(Key) -> bool) {
        for (index, slot) in self.slots.iter_mut().enumerate() {
            let key = Key {
                index: index as u32,
                generation: slot.generation,
            };
            if slot.released && slot.value.is_some() && !in_use(key) {
                slot.value = None;
                slot.released = false;
                slot.generation = slot.generation.wrapping_add(1);
                self.free.push(index as u32);
            }
        }
    }
}

/// The built-in scene (upstream `Scene`, `SceneType::Default`).
///
/// A scene owns its meshes and refers to them by id. Changes (adding, removing, moving instances, setting
/// materials) are staged and become visible to rays at the next [`commit`](Scene::commit), as upstream's double
/// buffering does; tracing borrows the scene immutably, so it cannot overlap a change.
///
/// Upstream's meshes are reference-counted objects that any scene can hold; here a mesh belongs to the scene it
/// was inserted into, and [`release_static_mesh`](Scene::release_static_mesh) plays the role of dropping the last
/// reference (see `docs/DEVIATIONS.md`).
#[derive(Debug, Clone, Default)]
pub struct Scene {
    static_meshes: Arena<StaticMesh>,
    instanced_meshes: Arena<InstancedMesh>,
    /// `[committed, pending]`, upstream `mStaticMeshes[0]` and `[1]`.
    static_lists: [Vec<StaticMeshId>; 2],
    instanced_lists: [Vec<InstancedMeshId>; 2],
    has_changed: bool,
    version: u32,
}

const COMMITTED: usize = 0;
const PENDING: usize = 1;

impl Scene {
    /// An empty scene (upstream `Scene()`, `SceneFactory::create(SceneType::Default)`).
    pub fn new() -> Self {
        Self::default()
    }

    /// Number of committed static meshes (upstream `numStaticMeshes`).
    pub fn num_static_meshes(&self) -> usize {
        self.static_lists[COMMITTED].len()
    }

    /// Number of committed instanced meshes (upstream `numInstancedMeshes`).
    pub fn num_instanced_meshes(&self) -> usize {
        self.instanced_lists[COMMITTED].len()
    }

    /// Stores a static mesh in the scene without adding it (the second half of upstream's `createStaticMesh`).
    pub fn insert_static_mesh(&mut self, mesh: StaticMesh) -> StaticMeshId {
        StaticMeshId(self.static_meshes.insert(mesh))
    }

    /// Stores an instanced mesh in the scene without adding it (upstream `createInstancedMesh`).
    pub fn insert_instanced_mesh(&mut self, mesh: InstancedMesh) -> InstancedMeshId {
        InstancedMeshId(self.instanced_meshes.insert(mesh))
    }

    /// A stored, unreleased static mesh.
    pub fn static_mesh(&self, id: StaticMeshId) -> Option<&StaticMesh> {
        self.static_meshes.get(id.0)
    }

    /// A stored, unreleased instanced mesh.
    pub fn instanced_mesh(&self, id: InstancedMeshId) -> Option<&InstancedMesh> {
        self.instanced_meshes.get(id.0)
    }

    /// Stages adding the mesh (upstream `addStaticMesh`). Adding it twice makes rays test it twice, as upstream.
    ///
    /// # Panics
    ///
    /// If `id` does not refer to an unreleased mesh of this scene.
    pub fn add_static_mesh(&mut self, id: StaticMeshId) {
        self.static_meshes.expect(id.0, "static mesh");
        self.static_lists[PENDING].push(id);
        self.has_changed = true;
    }

    /// Stages removing every occurrence of the mesh (upstream `removeStaticMesh`). The mesh stays stored and can
    /// be added again.
    pub fn remove_static_mesh(&mut self, id: StaticMeshId) {
        self.static_lists[PENDING].retain(|&m| m != id);
        self.has_changed = true;
    }

    /// Stages adding the instanced mesh (upstream `addInstancedMesh`).
    ///
    /// # Panics
    ///
    /// If `id` does not refer to an unreleased mesh of this scene.
    pub fn add_instanced_mesh(&mut self, id: InstancedMeshId) {
        self.instanced_meshes.expect(id.0, "instanced mesh");
        self.instanced_lists[PENDING].push(id);
        self.has_changed = true;
    }

    /// Stages removing every occurrence of the instanced mesh (upstream `removeInstancedMesh`).
    pub fn remove_instanced_mesh(&mut self, id: InstancedMeshId) {
        self.instanced_lists[PENDING].retain(|&m| m != id);
        self.has_changed = true;
    }

    /// Removes the mesh from the pending contents and gives up its id (upstream: dropping the last reference).
    /// Rays keep seeing it until the next commit; its storage is freed then.
    pub fn release_static_mesh(&mut self, id: StaticMeshId) {
        if self.static_meshes.get(id.0).is_some() {
            self.static_lists[PENDING].retain(|&m| m != id);
            self.static_meshes.release(id.0);
            self.collect();
        }
    }

    /// Like [`release_static_mesh`](Scene::release_static_mesh), for an instanced mesh.
    pub fn release_instanced_mesh(&mut self, id: InstancedMeshId) {
        if self.instanced_meshes.get(id.0).is_some() {
            self.instanced_lists[PENDING].retain(|&m| m != id);
            self.instanced_meshes.release(id.0);
            self.collect();
        }
    }

    /// Changes the instance's transform (upstream `IInstancedMesh::updateTransform`). It takes effect
    /// immediately, as upstream's does; a changed transform bumps the version at the next commit.
    ///
    /// # Panics
    ///
    /// If `id` does not refer to an unreleased mesh of this scene.
    pub fn update_instanced_mesh_transform(&mut self, id: InstancedMeshId, transform: Matrix4) {
        self.instanced_meshes
            .expect_mut(id.0, "instanced mesh")
            .update_transform(transform);
    }

    /// Stages replacing material `index` of a committed static mesh, applied at the next commit (upstream
    /// `setStaticMeshMaterial`). Does nothing if the mesh is not committed in this scene.
    ///
    /// # Panics
    ///
    /// If `index` is out of range.
    pub fn set_static_mesh_material(&mut self, id: StaticMeshId, material: Material, index: usize) {
        if self.static_lists[COMMITTED].contains(&id) {
            let mesh = self.static_meshes.get_live_mut(id.0);
            mesh.pending_materials[index] = material;
            mesh.materials_changed = true;
        }
    }

    /// Makes the staged changes visible to rays (upstream `commit`). The version is incremented if a mesh was
    /// added or removed, or an instanced mesh's transform changed, since the last commit.
    pub fn commit(&mut self) {
        if !self.has_changed {
            self.has_changed = self.instanced_lists[COMMITTED]
                .iter()
                .any(|&id| self.instanced_meshes.get_live(id.0).has_changed());
        }

        if self.has_changed {
            self.version = self.version.wrapping_add(1);
        }

        let [committed, pending] = &mut self.static_lists;
        committed.clone_from(pending);
        let [committed, pending] = &mut self.instanced_lists;
        committed.clone_from(pending);

        for &id in &self.instanced_lists[COMMITTED] {
            self.instanced_meshes.get_live_mut(id.0).commit();
        }

        for &id in &self.static_lists[COMMITTED] {
            self.static_meshes
                .get_live_mut(id.0)
                .apply_pending_materials();
        }

        self.has_changed = false;
        self.collect();
    }

    /// The change version (upstream `version`).
    pub fn version(&self) -> u32 {
        self.version
    }

    /// `true` if a committed static mesh intersects `aabb` (upstream `intersectsBox`; instanced meshes are
    /// ignored, as upstream).
    pub fn intersects_aabb(&self, aabb: &Aabb) -> bool {
        self.committed_static_meshes()
            .any(|mesh| mesh.intersects_aabb(aabb))
    }

    pub(crate) fn committed_static_meshes(&self) -> impl Iterator<Item = &StaticMesh> {
        self.static_lists[COMMITTED]
            .iter()
            .map(|&id| self.static_meshes.get_live(id.0))
    }

    fn committed_instanced_meshes(&self) -> impl Iterator<Item = &InstancedMesh> {
        self.instanced_lists[COMMITTED]
            .iter()
            .map(|&id| self.instanced_meshes.get_live(id.0))
    }

    fn collect(&mut self) {
        let [committed, pending] = &self.static_lists;
        self.static_meshes.collect(|key| {
            committed.contains(&StaticMeshId(key)) || pending.contains(&StaticMeshId(key))
        });
        let [committed, pending] = &self.instanced_lists;
        self.instanced_meshes.collect(|key| {
            committed.contains(&InstancedMeshId(key)) || pending.contains(&InstancedMeshId(key))
        });
    }

    /// Writes the committed geometry as a Wavefront OBJ file and a `.mtl` file next to it (upstream `dumpObj`).
    ///
    /// Materials become `Kd = (1 - absorption) * scattering`, `Ks = (1 - absorption) * (1 - scattering)` and
    /// `Tf = transmission`. An instanced mesh contributes the first static mesh of its sub-scene, transformed.
    pub fn dump_obj(&self, path: &Path) -> io::Result<()> {
        let mut obj = Vec::new();
        let mut mtl = Vec::new();
        let base_name = path.file_stem().unwrap_or_default().to_string_lossy();
        self.write_obj(&base_name, &mut obj, &mut mtl)?;
        std::fs::write(path.with_extension("mtl"), mtl)?;
        std::fs::write(path, obj)
    }

    /// [`dump_obj`](Scene::dump_obj) into writers; `base_name` names the `.mtl` file in the `mtllib` line.
    pub fn write_obj(
        &self,
        base_name: &str,
        obj: &mut impl Write,
        mtl: &mut impl Write,
    ) -> io::Result<()> {
        writeln!(mtl, "# Generated by Steam Audio")?;
        writeln!(obj, "# Generated by Steam Audio")?;
        writeln!(obj, "mtllib {base_name}.mtl")?;

        let meshes = self
            .committed_static_meshes()
            .map(|mesh| (mesh, Matrix4::IDENTITY))
            .chain(self.committed_instanced_meshes().filter_map(|instance| {
                // Upstream takes `front()` of the sub-scene's list, undefined behaviour when it is empty.
                instance
                    .sub_scene()
                    .committed_static_meshes()
                    .next()
                    .map(|mesh| (mesh, *instance.transform()))
            }));

        let mut vertex_offset = 1;
        let mut material_offset = 0;

        for (mesh, transform) in meshes {
            for (k, material) in mesh.materials().iter().enumerate() {
                let mut diffuse = [0.0; 3];
                let mut specular = [0.0; 3];
                let mut transmission = [0.0; 3];
                for j in 0..3 {
                    diffuse[j] = (1.0 - material.absorption[j]) * material.scattering;
                    specular[j] = (1.0 - material.absorption[j]) * (1.0 - material.scattering);
                    transmission[j] = material.transmission[j];
                }

                writeln!(mtl, "newmtl material_{}", material_offset + k)?;
                writeln!(
                    mtl,
                    "Kd {:.6} {:.6} {:.6}",
                    diffuse[0], diffuse[1], diffuse[2]
                )?;
                writeln!(
                    mtl,
                    "Ks {:.6} {:.6} {:.6}",
                    specular[0], specular[1], specular[2]
                )?;
                writeln!(
                    mtl,
                    "Tf {:.6} {:.6} {:.6}\n",
                    transmission[0], transmission[1], transmission[2]
                )?;
            }

            for &vertex in mesh.mesh().vertices() {
                let v = transform * Vector4::from_point(vertex);
                writeln!(obj, "v {:.6} {:.6} {:.6}", v.x, v.y, v.z)?;
            }

            let mut previous_material = None;
            for (k, triangle) in mesh.mesh().triangles().iter().enumerate() {
                let material = mesh.material_indices()[k] as usize;
                if previous_material != Some(material) {
                    writeln!(obj, "usemtl material_{}", material_offset + material)?;
                    previous_material = Some(material);
                }

                let [i0, i1, i2] = triangle.indices.map(|i| vertex_offset + i as usize);
                writeln!(obj, "f {i0} {i1} {i2}")?;
            }

            vertex_offset += mesh.num_vertices();
            material_offset += mesh.num_materials();
        }

        Ok(())
    }
}

impl RayTracer for Scene {
    /// Tests every committed static mesh, then every committed instanced mesh, keeping the first closest hit.
    fn closest_hit(&self, ray: &Ray, min_distance: f32, max_distance: f32) -> Hit {
        let mut hit = Hit::default();

        for mesh in self.committed_static_meshes() {
            let mesh_hit = mesh.closest_hit(ray, min_distance, max_distance);
            if mesh_hit.distance < hit.distance {
                hit = mesh_hit;
            }
        }

        for mesh in self.committed_instanced_meshes() {
            let mesh_hit = mesh.closest_hit(ray, min_distance, max_distance);
            if mesh_hit.distance < hit.distance {
                hit = mesh_hit;
            }
        }

        hit
    }

    fn any_hit(&self, ray: &Ray, min_distance: f32, max_distance: f32) -> bool {
        self.committed_static_meshes()
            .any(|mesh| mesh.any_hit(ray, min_distance, max_distance))
            || self
                .committed_instanced_meshes()
                .any(|mesh| mesh.any_hit(ray, min_distance, max_distance))
    }
}
