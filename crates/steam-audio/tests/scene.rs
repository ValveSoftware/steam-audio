//! Parity tests for stage 5 against golden data from `tools/golden/src/scene.cpp`.
//!
//! Meshes are loaded from `core/data/meshes` with a port of the OBJ loader of upstream's
//! `RayTracerCompare.test.cpp`; other inputs replay the generator's `Lcg` draws in the same order.
//!
//! On Windows every value matches the reference bit for bit, BVH layouts included (the builder sorts with a port
//! of MSVC's `std::sort`, risk R21). Floats use `F32` by convention; the code involved uses no libm function, so
//! other platforms are expected to be bit-exact too.

use std::collections::HashMap;
use std::sync::Arc;

use parity::{Lcg, Tolerance, golden};
use steam_audio::geometry::{Aabb, Ray};
use steam_audio::math::{Matrix4, Quaternion, Vector3};
use steam_audio::scene::{InstancedMesh, Material, RayTracer, Scene, StaticMesh, Triangle};

// ---------------------------------------------------------------------------------------------------------------------
// OBJ loading (RayTracerCompare.test.cpp's loadMesh)
// ---------------------------------------------------------------------------------------------------------------------

struct ObjMesh {
    vertices: Vec<Vector3>,
    triangles: Vec<Triangle>,
    material_indices: Vec<u32>,
    materials: Vec<Material>,
}

/// Whitespace-separated tokens, read like `std::istream >> std::string`. A command token that ends the stream
/// (no whitespace after it) sets `eof`, and upstream's loops stop before handling it.
struct Tokens<'a> {
    tokens: Vec<&'a str>,
    next: usize,
    last_hits_eof: bool,
}

impl<'a> Tokens<'a> {
    fn new(text: &'a str) -> Self {
        Self {
            tokens: text.split_whitespace().collect(),
            next: 0,
            last_hits_eof: !text.ends_with(|c: char| c.is_whitespace()),
        }
    }

    /// `stream >> command; if (stream.eof()) break;`
    fn command(&mut self) -> Option<&'a str> {
        let token = self.tokens.get(self.next).copied()?;
        if self.next + 1 == self.tokens.len() && self.last_hits_eof {
            return None;
        }
        self.next += 1;
        Some(token)
    }

    fn arg(&mut self) -> &'a str {
        let token = self.tokens[self.next];
        self.next += 1;
        token
    }

    fn float(&mut self) -> f32 {
        self.arg().parse().expect("number")
    }
}

/// C `atoi`: optional sign and leading digits.
fn atoi(s: &str) -> i32 {
    let (sign, digits) = match s.strip_prefix('-') {
        Some(rest) => (-1, rest),
        None => (1, s.strip_prefix('+').unwrap_or(s)),
    };
    let end = digits
        .find(|c: char| !c.is_ascii_digit())
        .unwrap_or(digits.len());
    sign * digits[..end].parse::<i32>().unwrap_or(0)
}

fn load_obj(name: &str) -> ObjMesh {
    let read = |ext: &str| {
        std::fs::read_to_string(parity::data_path(&format!("meshes/{name}.{ext}")))
            .unwrap_or_else(|e| panic!("{name}.{ext}: {e}"))
    };

    let mut materials: Vec<Material> = Vec::new();
    let mut material_indices_by_name: HashMap<String, usize> = HashMap::new();
    let mut current = String::new();
    let mut kd = [0.0f32; 3];

    let mtl = read("mtl");
    let mut tokens = Tokens::new(&mtl);
    while let Some(command) = tokens.command() {
        match command {
            "newmtl" => {
                current = tokens.arg().to_string();
                materials.push(Material::default());
                material_indices_by_name.insert(current.clone(), materials.len() - 1);
            }
            // Upstream's `Ks` branch also sets the absorption from the last `Kd`.
            "Kd" | "Ks" => {
                let values = [tokens.float(), tokens.float(), tokens.float()];
                if command == "Kd" {
                    kd = values;
                }
                let i = material_indices_by_name[&current];
                for (absorption, kd) in materials[i].absorption.iter_mut().zip(kd) {
                    *absorption = 1.0 - kd;
                }
                materials[i].scattering = 1.0;
            }
            _ => {}
        }
    }

    let mut vertices = Vec::new();
    let mut indices = Vec::new();
    let mut material_indices = Vec::new();

    let obj = read("obj");
    let mut tokens = Tokens::new(&obj);
    while let Some(command) = tokens.command() {
        match command {
            "v" => {
                let x = tokens.float();
                let y = tokens.float();
                let z = tokens.float();
                vertices.push(Vector3::new(x, y, z));
            }
            "f" => {
                for _ in 0..3 {
                    let s = tokens.arg();
                    let prefix = &s[..s.find('/').unwrap_or(s.len())];
                    indices.push(u32::try_from(atoi(prefix) - 1).expect("valid vertex index"));
                }
                let material = material_indices_by_name
                    .get(&current)
                    .expect("face without a material");
                material_indices.push(*material as u32);
            }
            "usemtl" => current = tokens.arg().to_string(),
            _ => {}
        }
    }

    ObjMesh {
        vertices,
        triangles: indices
            .chunks(3)
            .map(|t| Triangle::new(t[0], t[1], t[2]))
            .collect(),
        material_indices,
        materials,
    }
}

fn static_mesh(obj: &ObjMesh) -> StaticMesh {
    StaticMesh::new(
        &obj.vertices,
        &obj.triangles,
        &obj.material_indices,
        &obj.materials,
    )
}

// ---------------------------------------------------------------------------------------------------------------------
// Inputs
// ---------------------------------------------------------------------------------------------------------------------

fn draw01(lcg: &mut Lcg) -> f32 {
    0.5 * (lcg.next_signed() + 1.0)
}

fn draw_vector(lcg: &mut Lcg) -> Vector3 {
    let x = lcg.next_signed();
    let y = lcg.next_signed();
    let z = lcg.next_signed();
    Vector3::new(x, y, z)
}

fn draw_point(lcg: &mut Lcg, aabb: &Aabb) -> Vector3 {
    let extents = aabb.extents();
    let mut p = Vector3::ZERO;
    for i in 0..3 {
        let t = draw01(lcg) * 1.1 - 0.05;
        p[i] = aabb.min[i] + extents[i] * t;
    }
    p
}

#[derive(Clone, Copy)]
struct RayInput {
    ray: Ray,
    min_distance: f32,
    max_distance: f32,
}

fn draw_ray(lcg: &mut Lcg, aabb: &Aabb, i: usize) -> RayInput {
    let diagonal = aabb.extents().length();
    let origin = draw_point(lcg, aabb);

    let direction = if i % 16 == 0 {
        let k = i / 16;
        let zero = if (k / 6) % 2 == 0 { 0.0 } else { -0.0 };
        let mut d = Vector3::new(zero, zero, zero);
        d[(k % 6) / 2] = if k % 2 == 0 { 1.0 } else { -1.0 };
        d
    } else {
        draw_vector(lcg).normalize_or_zero()
    };

    let min_distance = if i % 3 == 0 {
        0.0
    } else {
        draw01(lcg) * 0.1 * diagonal
    };
    let max_distance = if i % 4 == 0 {
        f32::INFINITY
    } else {
        draw01(lcg) * diagonal
    };
    RayInput {
        ray: Ray::new(origin, direction),
        min_distance,
        max_distance,
    }
}

fn draw_aabb(lcg: &mut Lcg, bounds: &Aabb) -> Aabb {
    let center = draw_point(lcg, bounds);
    let size = draw01(lcg) * 0.05 * bounds.extents().length();
    let half = Vector3::new(size, size, size);
    Aabb::new(center - half, center + half)
}

// ---------------------------------------------------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------------------------------------------------

#[track_caller]
fn check<T: Into<f64> + Copy + parity::Element>(name: &str, actual: &[T], tol: Tolerance) {
    let expected = golden::<T>(name);
    if let Err(m) = parity::compare(actual, &expected.data, tol) {
        panic!("{name}: {m}");
    }
}

fn index(i: Option<usize>) -> i32 {
    i.map_or(-1, |i| i32::try_from(i).expect("index fits"))
}

#[derive(Default)]
struct HitRows {
    closest: Vec<f32>,
    indices: Vec<i32>,
    any: Vec<i32>,
    occluded: Vec<i32>,
}

impl HitRows {
    fn trace(&mut self, scene: &impl RayTracer, input: &RayInput) {
        let hit = scene.closest_hit(&input.ray, input.min_distance, input.max_distance);
        self.closest
            .extend([hit.distance, hit.normal.x, hit.normal.y, hit.normal.z]);
        match hit.material {
            Some(m) => {
                self.closest.extend(m.absorption);
                self.closest.push(m.scattering);
                self.closest.extend(m.transmission);
            }
            None => self.closest.extend([f32::NAN; 7]),
        }
        self.indices
            .extend([index(hit.triangle_index), index(hit.material_index)]);

        self.any.push(i32::from(scene.any_hit(
            &input.ray,
            input.min_distance,
            input.max_distance,
        )));

        let length = if input.max_distance < f32::INFINITY {
            input.max_distance
        } else {
            10.0
        };
        let end = input.ray.origin + length * input.ray.direction;
        self.occluded
            .push(i32::from(scene.is_occluded(input.ray.origin, end)));
    }

    #[track_caller]
    fn check(&self, prefix: &str) {
        check(&format!("{prefix}_closest"), &self.closest, Tolerance::F32);
        check(
            &format!("{prefix}_indices"),
            &self.indices,
            Tolerance::EXACT,
        );
        check(&format!("{prefix}_any"), &self.any, Tolerance::EXACT);
        check(
            &format!("{prefix}_occluded"),
            &self.occluded,
            Tolerance::EXACT,
        );
    }
}

fn fnv1a(hash: &mut u64, bytes: &[u8]) {
    for &b in bytes {
        *hash ^= u64::from(b);
        *hash = hash.wrapping_mul(0x100_0000_01b3);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// bvh
// ---------------------------------------------------------------------------------------------------------------------

fn check_mesh(name: &str, seed: u32, num_rays: usize, num_boxes: usize, full_nodes: bool) {
    let obj = load_obj(name);
    let mut scene = Scene::new();
    let id = scene.insert_static_mesh(static_mesh(&obj));
    scene.add_static_mesh(id);
    scene.commit();
    let mesh = scene.static_mesh(id).unwrap();
    let bvh = mesh.bvh();
    let prefix = format!("bvh/{name}");

    let mut hash = 0xcbf2_9ce4_8422_2325u64;
    let mut nodes = Vec::new();
    let mut node_data = Vec::new();
    for node in bvh.nodes() {
        let b = node.bounds;
        let coordinates = [b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z];
        for c in coordinates {
            fnv1a(&mut hash, &c.to_le_bytes());
        }
        fnv1a(&mut hash, &node.packed_data().to_le_bytes());
        nodes.extend(coordinates);
        node_data.push(node.packed_data());
    }
    let hash_words = [
        hash as u32 as i32,
        (hash >> 32) as u32 as i32,
        bvh.num_nodes() as i32,
    ];
    check(
        &format!("{prefix}_nodes_hash"),
        &hash_words,
        Tolerance::EXACT,
    );

    if full_nodes {
        check(&format!("{prefix}_nodes"), &nodes, Tolerance::EXACT);
        check(&format!("{prefix}_node_data"), &node_data, Tolerance::EXACT);
        let normals: Vec<f32> = (0..mesh.num_triangles())
            .flat_map(|i| mesh.mesh().normal(i).to_array())
            .collect();
        check(&format!("{prefix}_normals"), &normals, Tolerance::F32);
    }

    let mut lcg = Lcg::new(seed);
    let bounds = mesh.bounds();

    let mut rows = HitRows::default();
    for i in 0..num_rays {
        rows.trace(&scene, &draw_ray(&mut lcg, &bounds, i));
    }
    rows.check(&prefix);

    let boxes: Vec<i32> = (0..num_boxes)
        .map(|_| i32::from(mesh.intersects_aabb(&draw_aabb(&mut lcg, &bounds))))
        .collect();
    check(&format!("{prefix}_boxes"), &boxes, Tolerance::EXACT);
}

#[test]
fn bvh_smallbox() {
    check_mesh("smallbox", 11, 509, 131, true);
}

#[test]
fn bvh_boxroom() {
    check_mesh("boxroom", 12, 509, 131, true);
}

#[test]
fn bvh_simplescene() {
    check_mesh("simplescene", 13, 509, 131, true);
}

#[test]
fn bvh_sphere() {
    check_mesh("sphere", 14, 509, 131, true);
}

#[test]
fn bvh_teapot() {
    check_mesh("teapot", 15, 1021, 257, false);
}

/// 262 267 triangles from a 25 MB OBJ file (about 20 s in a debug build).
#[test]
fn bvh_sponza() {
    check_mesh("sponza", 16, 2039, 509, false);
}

// ---------------------------------------------------------------------------------------------------------------------
// scene
// ---------------------------------------------------------------------------------------------------------------------

fn draw_material(lcg: &mut Lcg) -> Material {
    let mut m = Material::default();
    for a in &mut m.absorption {
        *a = draw01(lcg);
    }
    m.scattering = draw01(lcg);
    for t in &mut m.transmission {
        *t = draw01(lcg);
    }
    m
}

fn assign_materials(lcg: &mut Lcg, obj: &mut ObjMesh) {
    obj.materials = (0..3).map(|_| draw_material(lcg)).collect();
    for (i, m) in obj.material_indices.iter_mut().enumerate() {
        *m = (i % 3) as u32;
    }
}

fn draw_transform(lcg: &mut Lcg, bounds: &Aabb) -> Matrix4 {
    let x = lcg.next_signed();
    let y = lcg.next_signed();
    let z = lcg.next_signed();
    let w = lcg.next_signed();
    let mut q = Quaternion::new(x, y, z, w);
    q.normalize();
    let rotation = q.to_rotation_matrix();

    let size = bounds.extents().length();
    let mut scale = [0.0f32; 3];
    for s in &mut scale {
        *s = (0.2 + draw01(lcg)) * 0.1 * size;
    }

    let translation = draw_point(lcg, bounds);

    let mut m = Matrix4::ZERO;
    for i in 0..3 {
        for j in 0..3 {
            m[(i, j)] = rotation[(i, j)] * scale[j];
        }
        m[(i, 3)] = translation[i];
    }
    m[(3, 3)] = 1.0;
    m
}

fn committed_sub_scene(obj: &ObjMesh) -> Arc<Scene> {
    let mut scene = Scene::new();
    let id = scene.insert_static_mesh(static_mesh(obj));
    scene.add_static_mesh(id);
    scene.commit();
    Arc::new(scene)
}

#[test]
fn scene() {
    let mut lcg = Lcg::new(20);

    let mut boxroom = load_obj("boxroom");
    let mut smallbox = load_obj("smallbox");
    let mut sphere = load_obj("sphere");
    assign_materials(&mut lcg, &mut boxroom);
    assign_materials(&mut lcg, &mut smallbox);
    assign_materials(&mut lcg, &mut sphere);

    let box_scene = committed_sub_scene(&smallbox);
    let sphere_scene = committed_sub_scene(&sphere);

    let mut scene = Scene::new();
    let room = scene.insert_static_mesh(static_mesh(&boxroom));
    let bounds = scene.static_mesh(room).unwrap().bounds();

    let transform0 = draw_transform(&mut lcg, &bounds);
    let transform1 = draw_transform(&mut lcg, &bounds);
    let transform2 = draw_transform(&mut lcg, &bounds);
    let transform1b = draw_transform(&mut lcg, &bounds);
    let instance0 = scene.insert_instanced_mesh(InstancedMesh::new(box_scene.clone(), transform0));
    let instance1 = scene.insert_instanced_mesh(InstancedMesh::new(sphere_scene, transform1));
    let instance2 = scene.insert_instanced_mesh(InstancedMesh::new(box_scene, transform2));

    let new_material = draw_material(&mut lcg);

    let mut rays: Vec<RayInput> = (0..509).map(|i| draw_ray(&mut lcg, &bounds, i)).collect();

    // Rays aimed near the instances' origins (the translations, before transform1b moves instance 1).
    let transforms = [transform0, transform1, transform2];
    for i in 0..256 {
        let t = &transforms[i % 3];
        let origin = draw_point(&mut lcg, &bounds);
        let offset = draw_vector(&mut lcg);
        let target = Vector3::new(t[(0, 3)], t[(1, 3)], t[(2, 3)]) + offset * 0.5;
        rays.push(RayInput {
            ray: Ray::new(origin, (target - origin).normalize_or_zero()),
            min_distance: 0.0,
            max_distance: f32::INFINITY,
        });
    }
    let boxes: Vec<Aabb> = (0..131).map(|_| draw_aabb(&mut lcg, &bounds)).collect();

    let mut versions = Vec::new();
    let record = |scene: &Scene, versions: &mut Vec<i32>| {
        versions.extend([
            scene.version() as i32,
            scene.num_static_meshes() as i32,
            scene.num_instanced_meshes() as i32,
        ]);
    };

    let mut stage = 0;
    let mut trace = |scene: &Scene| {
        let prefix = format!("scene/stage{stage}");
        let mut rows = HitRows::default();
        for ray in &rays {
            rows.trace(scene, ray);
        }
        rows.check(&prefix);

        let batch_rays: Vec<Ray> = rays.iter().map(|r| r.ray).collect();
        let min_distances: Vec<f32> = rays.iter().map(|r| r.min_distance).collect();
        let max_distances: Vec<f32> = rays
            .iter()
            .enumerate()
            .map(|(i, r)| if i % 5 == 0 { -1.0 } else { r.max_distance })
            .collect();
        let mut occluded = vec![false; rays.len()];
        scene.any_hits(&batch_rays, &min_distances, &max_distances, &mut occluded);
        let any_hits: Vec<i32> = occluded.into_iter().map(i32::from).collect();
        check(&format!("{prefix}_any_hits"), &any_hits, Tolerance::EXACT);

        let box_hits: Vec<i32> = boxes
            .iter()
            .map(|b| i32::from(scene.intersects_aabb(b)))
            .collect();
        check(&format!("{prefix}_boxes"), &box_hits, Tolerance::EXACT);

        stage += 1;
    };

    scene.add_static_mesh(room);
    scene.add_instanced_mesh(instance0);
    scene.add_instanced_mesh(instance1);
    scene.add_instanced_mesh(instance2);
    record(&scene, &mut versions);
    trace(&scene);

    scene.commit();
    record(&scene, &mut versions);
    trace(&scene);

    scene.commit();
    record(&scene, &mut versions);

    scene.update_instanced_mesh_transform(instance0, transform0);
    scene.commit();
    record(&scene, &mut versions);

    scene.update_instanced_mesh_transform(instance1, transform1b);
    record(&scene, &mut versions);
    trace(&scene);
    scene.commit();
    record(&scene, &mut versions);

    scene.set_static_mesh_material(room, new_material, 1);
    scene.commit();
    record(&scene, &mut versions);
    trace(&scene);

    scene.remove_static_mesh(room);
    record(&scene, &mut versions);
    trace(&scene);
    scene.commit();
    record(&scene, &mut versions);
    trace(&scene);

    scene.remove_instanced_mesh(instance2);
    scene.add_static_mesh(room);
    scene.add_static_mesh(room);
    scene.commit();
    record(&scene, &mut versions);
    trace(&scene);

    check("scene/versions", &versions, Tolerance::EXACT);

    // Upstream writes in text mode, which turns every "\n" into "\r\n" on Windows.
    let crlf = |bytes: Vec<u8>| {
        String::from_utf8(bytes)
            .unwrap()
            .replace('\n', "\r\n")
            .into_bytes()
    };
    let mut obj = Vec::new();
    let mut mtl = Vec::new();
    scene.write_obj("dump_tmp", &mut obj, &mut mtl).unwrap();
    check("scene/dump_obj", &crlf(obj), Tolerance::EXACT);
    check("scene/dump_mtl", &crlf(mtl), Tolerance::EXACT);
}

#[test]
fn released_meshes_stay_visible_until_commit() {
    let obj = load_obj("smallbox");
    let mut scene = Scene::new();
    let id = scene.insert_static_mesh(static_mesh(&obj));
    scene.add_static_mesh(id);
    scene.commit();

    let ray = Ray::new(Vector3::new(0.0, 0.0, -5.0), Vector3::Z_AXIS);
    assert!(scene.any_hit(&ray, 0.0, f32::INFINITY));

    scene.release_static_mesh(id);
    assert!(scene.static_mesh(id).is_none());
    assert!(scene.any_hit(&ray, 0.0, f32::INFINITY));

    scene.commit();
    assert!(!scene.any_hit(&ray, 0.0, f32::INFINITY));
    assert_eq!(scene.num_static_meshes(), 0);

    // The slot is reused under a new generation; the old id stays dead.
    let id2 = scene.insert_static_mesh(static_mesh(&obj));
    assert_ne!(id, id2);
    assert!(scene.static_mesh(id).is_none());
    assert!(scene.static_mesh(id2).is_some());
}
