//! Parity tests for stage 5.1 against golden data from `tools/golden/src/direct_simulator.cpp`.
//!
//! Each scene (a mesh from `core/data/meshes` with `Lcg` materials and an instanced box) is simulated for random
//! source/listener pairs and for pairs on opposite sides of the box, with the settings listed in the generator; the
//! inputs replay its `Lcg` draws in the same order.
//!
//! Bit-exact on Windows (debug and release). Off Windows the volumetric sample points come from libm (`acos`, `cbrt`,
//! `sin`, `cos`) and may move by an ulp, which could flip a sample whose ray grazes an edge; none does in these cases.

mod common;

use common::{
    assign_materials, committed_sub_scene, draw_point, draw_transform, draw_vector, draw01,
    load_obj, static_mesh,
};
use parity::{Lcg, Tolerance, golden};
use steam_audio::air_absorption::AirAbsorptionModel;
use steam_audio::directivity::Directivity;
use steam_audio::distance_attenuation::DistanceAttenuationModel;
use steam_audio::math::{CoordinateSpace3, Vector3};
use steam_audio::scene::{InstancedMesh, RayTracer, Scene};
use steam_audio::simulation::{
    DirectSimulationFlags, DirectSimulationParams, DirectSimulator, DirectSoundPath, OcclusionType,
};

const NUM_RANDOM_PAIRS: usize = 200;
const NUM_CROSSING_PAIRS: usize = 56;
const TRANSMISSION_RAYS: [usize; 6] = [1, 2, 3, 4, 8, 16];
const VOLUMETRIC_SAMPLES: [usize; 4] = [16, 64, 200, 1];
const ALL_FLAGS: DirectSimulationFlags = DirectSimulationFlags::from_bits(0x3f);

fn append_path(rows: &mut Vec<f32>, p: &DirectSoundPath) {
    rows.push(p.distance_attenuation);
    rows.extend(p.air_absorption);
    rows.push(p.delay);
    rows.push(p.occlusion);
    rows.extend(p.transmission);
    rows.push(p.directivity);
}

fn check_scene(name: &str, seed: u32) {
    let mut lcg = Lcg::new(seed);

    let mut obj = load_obj(name);
    let mut smallbox = load_obj("smallbox");
    assign_materials(&mut lcg, &mut obj);
    assign_materials(&mut lcg, &mut smallbox);

    let box_scene = committed_sub_scene(&smallbox);

    let mut scene = Scene::new();
    let mesh = scene.insert_static_mesh(static_mesh(&obj));
    let bounds = scene.static_mesh(mesh).unwrap().bounds();
    let transform = draw_transform(&mut lcg, &bounds);
    scene.add_static_mesh(mesh);
    let instance = scene.insert_instanced_mesh(InstancedMesh::new(box_scene, transform));
    scene.add_instanced_mesh(instance);
    scene.commit();

    let diagonal = bounds.extents().length();
    let box_center = Vector3::new(transform[(0, 3)], transform[(1, 3)], transform[(2, 3)]);

    let weight = draw01(&mut lcg);
    let power = draw01(&mut lcg) * 4.0;
    let distance_model = DistanceAttenuationModel::InverseDistance { min_distance: 0.5 };
    let air_model = AirAbsorptionModel::default();
    let directivity = Directivity::Dipole { weight, power };

    let simulator64 = DirectSimulator::new(64);
    let simulator256 = DirectSimulator::new(256);
    let simulator1 = DirectSimulator::new(1);

    let mut rows = Vec::new();
    for i in 0..NUM_RANDOM_PAIRS + NUM_CROSSING_PAIRS {
        let ahead = draw_vector(&mut lcg).normalize_or_zero();
        let (source_position, listener_position) = if i < NUM_RANDOM_PAIRS {
            let source = draw_point(&mut lcg, &bounds);
            let listener = draw_point(&mut lcg, &bounds);
            (source, listener)
        } else {
            let offset = draw_vector(&mut lcg);
            let scale = draw01(&mut lcg);
            let source = box_center + offset * (0.3 * diagonal * (0.1 + scale));
            (source, box_center * 2.0 - source)
        };
        let radius = draw01(&mut lcg) * 0.05 * diagonal;

        let base = DirectSimulationParams {
            flags: ALL_FLAGS,
            source: CoordinateSpace3::from_ahead(ahead, source_position),
            listener: CoordinateSpace3::from_ahead(Vector3::Z_AXIS, listener_position),
            distance_attenuation_model: &distance_model,
            air_absorption_model: &air_model,
            directivity: &directivity,
            occlusion_type: OcclusionType::Raycast,
            occlusion_radius: radius,
            num_occlusion_samples: VOLUMETRIC_SAMPLES[i % VOLUMETRIC_SAMPLES.len()],
            num_transmission_rays: TRANSMISSION_RAYS[i % TRANSMISSION_RAYS.len()],
        };
        let volumetric = DirectSimulationParams {
            occlusion_type: OcclusionType::Volumetric,
            ..base
        };
        let scene: &dyn RayTracer = &scene;

        let runs = [
            simulator64.simulate(Some(scene), &base),
            simulator64.simulate(Some(scene), &volumetric),
            simulator256.simulate(
                Some(scene),
                &DirectSimulationParams {
                    occlusion_radius: radius * 4.0,
                    num_occlusion_samples: 256,
                    ..volumetric
                },
            ),
            simulator1.simulate(
                Some(scene),
                &DirectSimulationParams {
                    flags: DirectSimulationFlags::OCCLUSION,
                    num_occlusion_samples: 32,
                    ..volumetric
                },
            ),
            simulator64.simulate(
                (i % 7 != 0).then_some(scene),
                &DirectSimulationParams {
                    flags: DirectSimulationFlags::from_bits((i % 64) as u32),
                    occlusion_type: if i % 2 == 0 {
                        OcclusionType::Raycast
                    } else {
                        OcclusionType::Volumetric
                    },
                    ..base
                },
            ),
        ];
        for path in &runs {
            append_path(&mut rows, path);
        }
    }

    let expected = golden::<f32>(&format!("direct_simulator/{name}"));
    assert_eq!(
        expected.shape,
        [NUM_RANDOM_PAIRS + NUM_CROSSING_PAIRS, 5, 10]
    );
    if let Err(m) = parity::compare(&rows, &expected.data, Tolerance::F32) {
        panic!("{name}: {m}");
    }
}

#[test]
fn direct_simulator_boxroom() {
    check_scene("boxroom", 501);
}

#[test]
fn direct_simulator_simplescene() {
    check_scene("simplescene", 502);
}
