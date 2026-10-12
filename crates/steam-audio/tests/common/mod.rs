//! Scene inputs shared by the stage 5 parity tests, mirroring `tools/golden/src/scene_inputs.h`: meshes loaded with
//! a port of the OBJ loader of upstream's `RayTracerCompare.test.cpp`, and `Lcg` draws of points, materials and
//! transforms.

// Each test crate uses its own subset.
#![allow(dead_code)]

use std::collections::HashMap;
use std::sync::Arc;

use parity::Lcg;
use steam_audio::geometry::Aabb;
use steam_audio::math::{Matrix4, Quaternion, Vector3};
use steam_audio::scene::{Material, Scene, StaticMesh, Triangle};

// ---------------------------------------------------------------------------------------------------------------------
// OBJ loading (RayTracerCompare.test.cpp's loadMesh)
// ---------------------------------------------------------------------------------------------------------------------

pub struct ObjMesh {
    pub vertices: Vec<Vector3>,
    pub triangles: Vec<Triangle>,
    pub material_indices: Vec<u32>,
    pub materials: Vec<Material>,
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

pub fn load_obj(name: &str) -> ObjMesh {
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

pub fn static_mesh(obj: &ObjMesh) -> StaticMesh {
    StaticMesh::new(
        &obj.vertices,
        &obj.triangles,
        &obj.material_indices,
        &obj.materials,
    )
}

/// A scene holding only `obj`, committed.
pub fn committed_sub_scene(obj: &ObjMesh) -> Arc<Scene> {
    let mut scene = Scene::new();
    let id = scene.insert_static_mesh(static_mesh(obj));
    scene.add_static_mesh(id);
    scene.commit();
    Arc::new(scene)
}

// ---------------------------------------------------------------------------------------------------------------------
// Lcg draws
// ---------------------------------------------------------------------------------------------------------------------

/// Uniform in [0, 1).
pub fn draw01(lcg: &mut Lcg) -> f32 {
    0.5 * (lcg.next_signed() + 1.0)
}

pub fn draw_vector(lcg: &mut Lcg) -> Vector3 {
    let x = lcg.next_signed();
    let y = lcg.next_signed();
    let z = lcg.next_signed();
    Vector3::new(x, y, z)
}

/// A point in the box grown by 5% of its extents on every side.
pub fn draw_point(lcg: &mut Lcg, aabb: &Aabb) -> Vector3 {
    let extents = aabb.extents();
    let mut p = Vector3::ZERO;
    for i in 0..3 {
        let t = draw01(lcg) * 1.1 - 0.05;
        p[i] = aabb.min[i] + extents[i] * t;
    }
    p
}

pub fn draw_material(lcg: &mut Lcg) -> Material {
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

/// Replaces the OBJ's materials with three Lcg ones, assigned to triangles in turn.
pub fn assign_materials(lcg: &mut Lcg, obj: &mut ObjMesh) {
    obj.materials = (0..3).map(|_| draw_material(lcg)).collect();
    for (i, m) in obj.material_indices.iter_mut().enumerate() {
        *m = (i % 3) as u32;
    }
}

/// Translation * rotation * scale, with the translation inside `bounds` and the scale up to a tenth of its size.
pub fn draw_transform(lcg: &mut Lcg, bounds: &Aabb) -> Matrix4 {
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
