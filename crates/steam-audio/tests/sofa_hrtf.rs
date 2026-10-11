//! Parity tests for stage 3.5c against golden data from `tools/golden/src/sofa_hrtf.cpp`: the SOFA HRTF map, the
//! HRTF map factory, and the HRTF database and binaural effect with a SOFA HRTF.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order.
#![cfg(feature = "sofa")]

use parity::{Element, Lcg, Tolerance, compare, data_path, golden};
use steam_audio::Complex;
use steam_audio::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use steam_audio::effects::{BinauralEffect, BinauralParams};
use steam_audio::hrtf::{
    AnyHrtfMap, HrtfDatabase, HrtfInterpolation, HrtfNormalization, HrtfPhaseType, HrtfSettings,
    HrtfSource, NUM_EARS, SofaHrtfMap,
};
use steam_audio::math::Vector3;

const FILES: [&str; 3] = ["cipic_124", "sadie_d1", "sadie_h12"];
const MAP_RATE: u32 = 48000;
const NUM_AMBISONICS_COEFFS: usize = 16;

/// Lookup ties may resolve differently off MSVC (risk R17).
const MSVC: bool = cfg!(all(windows, target_env = "msvc"));

/// Tolerance for HRTFs that are synthesized (interpolated or blended), see risk R10 in `docs/PORTING_PLAN.md`.
const SYNTHESIZED: f64 = 5e-5;

#[track_caller]
fn check<T: Element + Into<f64>>(name: &str, actual: &[T], tol: impl Fn(&[T]) -> Tolerance) {
    let expected = golden::<T>(name);
    if let Err(m) = compare(actual, &expected.data, tol(&expected.data)) {
        panic!("{name}: {m}");
    }
}

fn exact<T>(_: &[T]) -> Tolerance {
    Tolerance::EXACT
}

fn signal(expected: &[f32]) -> Tolerance {
    Tolerance::peak_relative(expected, 1e-5)
}

fn synthesized(expected: &[f32]) -> Tolerance {
    Tolerance::peak_relative(expected, SYNTHESIZED)
}

fn ints(values: impl IntoIterator<Item = usize>) -> Vec<i32> {
    values.into_iter().map(|v| v as i32).collect()
}

fn index_code(index: Option<usize>) -> i32 {
    index.map_or(-1, |i| i as i32)
}

fn extend_flat(to: &mut Vec<f32>, from: &[Complex]) {
    to.extend(from.iter().flat_map(|c| [c.re, c.im]));
}

fn draw_vector(lcg: &mut Lcg) -> Vector3 {
    let x = lcg.next_signed();
    let y = lcg.next_signed();
    let z = lcg.next_signed();
    Vector3::new(x, y, z)
}

/// The Steam Audio direction that the map converts to the SOFA position `p`.
fn from_sofa(p: [f32; 3]) -> Vector3 {
    Vector3::new(-p[1], p[2], -p[0])
}

fn read_sofa(name: &str) -> Vec<u8> {
    std::fs::read(data_path(&format!("hrtf/{name}.sofa"))).expect("SOFA file")
}

fn distance(a: [f32; 3], b: [f32; 3]) -> f64 {
    (0..3)
        .map(|k| (f64::from(a[k]) - f64::from(b[k])).powi(2))
        .sum::<f64>()
        .sqrt()
}

/// Whether measurements `a` and `b` are equally far from `point` (a lookup tie).
fn is_tie(map: &SofaHrtfMap, point: [f32; 3], a: usize, b: usize) -> bool {
    let (da, db) = (
        distance(point, map.positions()[a]),
        distance(point, map.positions()[b]),
    );
    (da - db).abs() <= 1e-5 * db.max(1e-6)
}

/// The point, scaled onto the measured radii, that the lookup for `direction` searches.
fn lookup_point(map: &SofaHrtfMap, direction: Vector3) -> [f32; 3] {
    let mut point = [-direction.z, -direction.x, direction.y];
    map.lookup().lookup(&mut point);
    point
}

/// The point at which upstream's `patchSOFANeighborhood` stops for measurement `i` (`slot` 0: +azimuth, 1: −azimuth).
fn patch_point(map: &SofaHrtfMap, i: usize, slot: usize) -> Option<[f32; 3]> {
    let measurement = sofa::c2s(map.positions()[i]);
    let mut d_phi = 0.5f32;
    while d_phi <= 360.0 {
        let mut point = measurement;
        point[0] += if slot == 0 { d_phi } else { -d_phi };
        point[0] = if point[0] > 360.0 {
            point[0] - 360.0
        } else if point[0] < 0.0 {
            point[0] + 360.0
        } else {
            point[0]
        };
        let mut point = sofa::s2c(point);
        if map.lookup().lookup(&mut point) != Some(i) {
            return Some(point);
        }
        d_phi += 0.5;
    }
    None
}

// ---------------------------------------------------------------------------------------------------------------------
// SOFA HRTF map
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn sofa_hrtf_map() {
    let mut lcg = Lcg::new(73);

    for name in FILES {
        let data = read_sofa(name);
        let settings = HrtfSettings {
            source: HrtfSource::SofaData(&data),
            ..Default::default()
        };
        let AnyHrtfMap::Sofa(mut map) = AnyHrtfMap::new(&settings, MAP_RATE).unwrap() else {
            panic!("{name}: not a SOFA map");
        };
        let prefix = format!("sofa_hrtf_map/{name}_");
        let num_hrirs = map.num_hrirs();
        let num_samples = map.num_samples();
        check(
            &format!("{prefix}info"),
            &ints([num_hrirs, num_samples, 0]),
            exact,
        );

        let mut indices = Vec::new();
        let expected = golden::<f32>(&format!("{prefix}selected_hrirs"));
        for k in 0..6 {
            let index = (lcg.next_u32() % num_hrirs as u32) as usize;
            indices.push(index);
            // Resampled HRIRs: `peak_relative(1e-5)` per response (ADR 0001).
            for ear in 0..NUM_EARS {
                let row = &expected.data[(k * NUM_EARS + ear) * num_samples..][..num_samples];
                let result = compare(map.hrir(ear, index), row, signal(row));
                assert!(result.is_ok(), "{prefix}selected_hrirs: {result:?}");
            }
        }
        check(&format!("{prefix}selected_indices"), &ints(indices), exact);

        // The neighbourhood: libmysofa's (checked by the sofa crate's tests) with patched azimuth neighbours.
        let mut table: Vec<[i32; 6]> = golden::<i32>(&format!("sofa_easy/easy_{name}_neighbors"))
            .data
            .chunks_exact(6)
            .map(|row| row.try_into().unwrap())
            .collect();
        let patched = golden::<i32>(&format!("{prefix}patched"));
        let mut flipped = vec![false; num_hrirs];
        for row in patched.data.chunks_exact(3) {
            let i = row[0] as usize;
            for slot in 0..2 {
                let (actual, expected) = (index_code(map.neighbors(i)[slot]), row[1 + slot]);
                table[i][slot] = expected;
                if actual != expected {
                    let point = patch_point(&map, i, slot);
                    assert!(
                        !MSVC
                            && actual >= 0
                            && expected >= 0
                            && point.is_some_and(|p| is_tie(
                                &map,
                                p,
                                actual as usize,
                                expected as usize
                            )),
                        "{prefix}patched: measurement {i} slot {slot}: {actual}, expected {expected}"
                    );
                    flipped[i] = true;
                }
            }
        }
        for (i, row) in table.iter().enumerate() {
            let actual = map.neighbors(i).map(index_code);
            if actual != *row {
                if MSVC {
                    panic!("{prefix}neighbors {i}: {actual:?}, expected {row:?}");
                }
                flipped[i] = true;
            }
        }

        // Directions as the generator: fixed ones, measured positions, positions moved slightly, random ones.
        let mut directions = vec![
            Vector3::new(0.0, 0.0, -1.0),
            Vector3::new(1.0, 0.0, 0.0),
            Vector3::new(0.0, 0.0, 1.0),
            Vector3::new(-1.0, 0.0, 0.0),
            Vector3::new(0.0, 1.0, 0.0),
            Vector3::new(0.0, -1.0, 0.0),
            Vector3::new(0.98, 0.0, 0.2),
            Vector3::new(0.98, 0.1, -0.2),
            Vector3::new(-0.99, 0.05, 0.1),
            Vector3::new(-0.3, 0.5, 0.8),
            Vector3::ZERO,
        ];
        for _ in 0..20 {
            let index = (lcg.next_u32() % num_hrirs as u32) as usize;
            directions.push(from_sofa(map.positions()[index]));
        }
        for _ in 0..40 {
            let index = (lcg.next_u32() % num_hrirs as u32) as usize;
            let dx = lcg.next_signed() * 0.05;
            let dy = lcg.next_signed() * 0.05;
            let dz = lcg.next_signed() * 0.05;
            directions.push(from_sofa(map.positions()[index]) + Vector3::new(dx, dy, dz));
        }
        for _ in 0..200 {
            directions.push(draw_vector(&mut lcg));
        }

        let golden_nearest = golden::<i32>(&format!("{prefix}nearest")).data;
        let golden_indices = golden::<i32>(&format!("{prefix}interpolated_indices")).data;
        let golden_weights = golden::<f32>(&format!("{prefix}interpolated_weights")).data;
        let mut weights = Vec::new();
        let mut expected_weights = Vec::new();
        let mut ties = 0;
        for (q, &direction) in directions.iter().enumerate() {
            let nearest = map.nearest_hrir(direction);
            let w = map.interpolated_hrir_weights(direction);
            let indices = w.indices.map(index_code);
            let expected_indices = &golden_indices[q * 8..q * 8 + 8];
            let expected_nearest = golden_nearest[q];
            if nearest as i32 == expected_nearest && indices == expected_indices {
                weights.extend_from_slice(&w.weights);
                expected_weights.extend_from_slice(&golden_weights[q * 8..q * 8 + 8]);
                continue;
            }

            // Off MSVC, a different nearest measurement must be a tie, and different neighbours must come from a
            // flipped neighbourhood entry of one of the measurements involved.
            let context = format!(
                "{prefix}query {q}: nearest {nearest} indices {indices:?}, expected {expected_nearest} {expected_indices:?}"
            );
            if MSVC {
                panic!("{context}");
            }
            if nearest as i32 != expected_nearest {
                let point = lookup_point(&map, direction);
                assert!(
                    is_tie(&map, point, nearest, expected_nearest as usize),
                    "{context}: not a tie"
                );
            } else {
                let involved = indices.iter().chain(expected_indices).filter(|&&i| i >= 0);
                assert!(
                    involved.into_iter().any(|&i| flipped[i as usize]),
                    "{context}: no flipped neighbour involved"
                );
            }
            ties += 1;
        }
        if ties > 0 {
            eprintln!("{prefix}: {ties} queries differ by lookup ties (R17)");
        }
        // As the built-in map's bilinear weights (3.1): angles in degrees from `c2s` (`atan2f`, `sqrtf`) are divided
        // by grid steps of a few degrees, which amplifies libm ulp differences off Windows (measured 1.25e-5 on Linux).
        let result = compare(&weights, &expected_weights, Tolerance::new(2e-5, 0.0));
        assert!(result.is_ok(), "{prefix}interpolated_weights: {result:?}");
    }
}

#[test]
fn map_factory_builds_each_map_type() {
    let built_in = AnyHrtfMap::new(&HrtfSettings::default(), 44100).unwrap();
    assert!(matches!(built_in, AnyHrtfMap::Default(_)));
    assert_eq!(built_in.num_hrirs(), 1250);

    let path = data_path("hrtf/cipic_124.sofa");
    let settings = HrtfSettings {
        source: HrtfSource::SofaFile(&path),
        ..Default::default()
    };
    let from_file = AnyHrtfMap::new(&settings, MAP_RATE).unwrap();
    let data = read_sofa("cipic_124");
    let settings = HrtfSettings {
        source: HrtfSource::SofaData(&data),
        ..Default::default()
    };
    let from_data = AnyHrtfMap::new(&settings, MAP_RATE).unwrap();
    assert!(matches!(from_file, AnyHrtfMap::Sofa(_)));
    assert_eq!(from_file.hrirs(), from_data.hrirs());
    assert!(from_file.ambisonics_hrirs().is_empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// HRTF database and binaural effect
// ---------------------------------------------------------------------------------------------------------------------

struct DatabaseCase {
    name: &'static str,
    file: &'static str,
    from_file_name: bool,
    sampling_rate: u32,
    frame_size: usize,
    normalization: HrtfNormalization,
    volume: f32,
}

const DATABASE_CASES: [DatabaseCase; 2] = [
    DatabaseCase {
        name: "cipic",
        file: "cipic_124",
        from_file_name: true,
        sampling_rate: 48000,
        frame_size: 64,
        normalization: HrtfNormalization::None,
        volume: 0.0,
    },
    DatabaseCase {
        name: "h12_rms",
        file: "sadie_h12",
        from_file_name: false,
        sampling_rate: 44100,
        frame_size: 100,
        normalization: HrtfNormalization::Rms,
        volume: -3.0,
    },
];

const INTERPOLATED_QUERIES: [(f32, HrtfPhaseType); 2] =
    [(1.0, HrtfPhaseType::None), (0.4, HrtfPhaseType::SphereItd)];

fn check_database_case(c: &DatabaseCase, lcg: &mut Lcg) {
    let path = data_path(&format!("hrtf/{}.sofa", c.file));
    let data = read_sofa(c.file);
    let settings = HrtfSettings {
        source: if c.from_file_name {
            HrtfSource::SofaFile(&path)
        } else {
            HrtfSource::SofaData(&data)
        },
        volume: c.volume,
        normalization: c.normalization,
    };
    let mut database = HrtfDatabase::new(&settings, c.sampling_rate, c.frame_size).unwrap();
    let prefix = format!("sofa_hrtf_database/{}_", c.name);
    let bins = database.num_spectrum_samples();

    check(
        &format!("{prefix}info"),
        &ints([database.num_hrirs(), database.num_samples(), bins]),
        exact,
    );

    let mut indices = Vec::new();
    let mut selected = Vec::new();
    for _ in 0..3 {
        let index = (lcg.next_u32() % database.num_hrirs() as u32) as usize;
        indices.push(index);
        for ear in database.hrtf(index) {
            extend_flat(&mut selected, ear);
        }
    }
    check(&format!("{prefix}selected_indices"), &ints(indices), exact);
    check(&format!("{prefix}selected_hrtfs"), &selected, signal);

    let mut ambisonics = Vec::new();
    for k in 0..NUM_AMBISONICS_COEFFS {
        for ear in database.ambisonics_hrtf(k) {
            extend_flat(&mut ambisonics, ear);
        }
    }
    // Projected from interpolated HRTFs (R10) whose weights differ by libm ulps off Windows: measured 3.1e-6 × peak on
    // Windows, 1.24e-5 × peak on Linux.
    check(
        &format!("{prefix}ambisonics_hrtfs"),
        &ambisonics,
        synthesized,
    );

    let mut directions = vec![
        Vector3::new(0.0, 0.0, -1.0),
        Vector3::new(1.0, 0.0, 0.0),
        Vector3::new(-0.3, 0.5, 0.8),
        Vector3::new(0.98, 0.1, -0.2),
    ];
    directions.extend((0..3).map(|_| draw_vector(lcg)));

    let mut left = vec![Complex::default(); bins];
    let mut right = vec![Complex::default(); bins];
    let mut nearest_peaks = Vec::new();
    let mut interpolated_peaks = Vec::new();
    let mut interpolated = Vec::new();
    for &direction in &directions {
        let index = database.nearest_index(direction);
        nearest_peaks.extend(ints(database.peak_delays(index)));
        for (blend, phase_type) in INTERPOLATED_QUERIES {
            let peaks =
                database.interpolated_hrtf(direction, blend, phase_type, [&mut left, &mut right]);
            interpolated_peaks.extend(ints(peaks));
            extend_flat(&mut interpolated, &left);
            extend_flat(&mut interpolated, &right);
        }
    }
    check(&format!("{prefix}nearest_peaks"), &nearest_peaks, exact);
    check(
        &format!("{prefix}interpolated_peaks"),
        &interpolated_peaks,
        exact,
    );
    check(&format!("{prefix}interpolated"), &interpolated, synthesized);
}

const BINAURAL_FRAME_SIZE: usize = 64;

const BINAURAL_CASES: [(&str, usize, HrtfInterpolation, f32, HrtfPhaseType, &str); 3] = [
    (
        "binaural_nearest_mono",
        1,
        HrtfInterpolation::NearestNeighbor,
        1.0,
        HrtfPhaseType::None,
        "AAAATTRAA",
    ),
    (
        "binaural_bilinear_mono",
        1,
        HrtfInterpolation::Bilinear,
        1.0,
        HrtfPhaseType::None,
        "AAAATTTTAA",
    ),
    (
        "binaural_bilinear_stereo_blend_itd",
        2,
        HrtfInterpolation::Bilinear,
        0.6,
        HrtfPhaseType::SphereItd,
        "AAATTA",
    ),
];

fn check_binaural(lcg: &mut Lcg) {
    let data = read_sofa("cipic_124");
    let settings = HrtfSettings {
        source: HrtfSource::SofaData(&data),
        ..Default::default()
    };
    let mut database = HrtfDatabase::new(&settings, MAP_RATE, BINAURAL_FRAME_SIZE).unwrap();
    let audio = AudioSettings::new(MAP_RATE, BINAURAL_FRAME_SIZE);

    for (name, in_channels, interpolation, spatial_blend, phase_type, script) in BINAURAL_CASES {
        let mut effect = BinauralEffect::new(&audio, &database);
        let mut input = AudioBuffer::new(in_channels, BINAURAL_FRAME_SIZE);
        let mut out = AudioBuffer::new(2, BINAURAL_FRAME_SIZE);
        let mut outputs = Vec::new();
        let mut states = Vec::new();
        let mut peak_delays = Vec::new();
        let mut record = |out: &AudioBuffer, state: AudioEffectState, effect: &BinauralEffect| {
            outputs.extend_from_slice(out.as_slice());
            states.push(i32::from(state == AudioEffectState::TailComplete));
            states.push(effect.num_tail_samples_remaining() as i32);
        };

        for op in script.chars() {
            match op {
                'A' => {
                    let direction = draw_vector(lcg).normalize_or_zero();
                    for s in input.as_mut_slice() {
                        *s = lcg.next_signed();
                    }
                    let mut peaks = [0.0; 2];
                    let params = BinauralParams {
                        direction,
                        interpolation,
                        spatial_blend,
                        phase_type,
                        hrtf: &mut database,
                        peak_delays: Some(&mut peaks),
                    };
                    let state = effect.apply(params, &input, &mut out);
                    record(&out, state, &effect);
                    peak_delays.extend_from_slice(&peaks);
                }
                'T' => {
                    let state = effect.tail(&mut out);
                    record(&out, state, &effect);
                }
                'R' => effect.reset(),
                _ => unreachable!(),
            }
        }

        let name = format!("sofa_hrtf_database/{name}");
        let relative = if interpolation == HrtfInterpolation::Bilinear {
            SYNTHESIZED
        } else {
            1e-5
        };
        check(&name, &outputs, |e| Tolerance::peak_relative(e, relative));
        check(&format!("{name}_states"), &states, exact);
        check(&format!("{name}_peak_delays"), &peak_delays, exact);
    }
}

#[test]
fn sofa_hrtf_database() {
    let mut lcg = Lcg::new(79);
    for c in &DATABASE_CASES {
        check_database_case(c, &mut lcg);
    }
    check_binaural(&mut lcg);
}
