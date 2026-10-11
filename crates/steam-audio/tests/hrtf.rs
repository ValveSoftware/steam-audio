//! Parity tests for stage 3.1 against golden data from `tools/golden/src/hrtf.cpp`. The upstream Catch test
//! `HRTFDatabase.test.cpp` is ported as a unit test in `src/hrtf/database.rs`.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order.

use parity::{Element, Lcg, Tolerance, compare, golden};
use steam_audio::Complex;
use steam_audio::hrtf::{
    BUILTIN_HRTF_DATA, HrtfDatabase, HrtfMap, HrtfNormalization, HrtfPhaseType, HrtfSettings,
    NUM_EARS, loudness,
};
use steam_audio::math::Vector3;

const SAMPLING_RATES: [u32; 3] = [44100, 48000, 24000];
const NUM_AMBISONICS_COEFFS: usize = 16;

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

fn f32_tol(_: &[f32]) -> Tolerance {
    Tolerance::F32
}

/// Bilinear weights are angle differences (in degrees, up to ~230°) divided by the grid spacing (5°–5.625°). One ulp
/// of a 230° angle is 1.5e-5°, i.e. ~2.7e-6 of weight. Bit-exact on Windows; elsewhere `asin`/`atan2` from the
/// platform libm differ by a few ulps (measured 4.8e-6 on Linux and macOS), so allow ~8 ulps.
fn weight_tol(_: &[f32]) -> Tolerance {
    Tolerance::new(2e-5, 0.0)
}

fn signal(expected: &[f32]) -> Tolerance {
    Tolerance::peak_relative(expected, 1e-5)
}

/// HRTFs synthesized from interpolated log-magnitude and unwrapped phase. The unwrapped phase grows with frequency
/// and is accumulated in `f32` from FFT phases that already differ (R1), so the error grows towards Nyquist; measured
/// worst 2.03e-5 × peak (see `docs/PORTING_PLAN.md`).
fn synthesized(expected: &[f32]) -> Tolerance {
    Tolerance::peak_relative(expected, 5e-5)
}

fn ints(values: impl IntoIterator<Item = usize>) -> Vec<i32> {
    values.into_iter().map(|v| v as i32).collect()
}

fn extend_flat(to: &mut Vec<f32>, from: &[Complex]) {
    to.extend(from.iter().flat_map(|c| [c.re, c.im]));
}

/// Axes, near-lateral directions on both sides of the median plane, then `count` random ones.
fn query_directions(lcg: &mut Lcg, count: usize) -> Vec<Vector3> {
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
    ];
    for _ in 0..count {
        let x = lcg.next_signed();
        let y = lcg.next_signed();
        let z = lcg.next_signed();
        directions.push(Vector3::new(x, y, z));
    }
    directions
}

#[test]
fn hrtf_map() {
    let mut lcg = Lcg::new(31);

    for rate in SAMPLING_RATES {
        let map = HrtfMap::new(rate).unwrap();
        let prefix = format!("hrtf_map/{rate}_");
        check(
            &format!("{prefix}info"),
            &ints([map.num_hrirs(), map.num_samples()]),
            exact,
        );

        let mut indices = Vec::new();
        let mut selected = Vec::new();
        for _ in 0..6 {
            let index = (lcg.next_u32() % map.num_hrirs() as u32) as usize;
            indices.push(index);
            for ear in 0..NUM_EARS {
                selected.extend_from_slice(map.hrir(ear, index));
            }
        }
        check(&format!("{prefix}selected_indices"), &ints(indices), exact);
        check(&format!("{prefix}selected_hrirs"), &selected, exact);
        check(
            &format!("{prefix}ambisonics_hrirs"),
            map.ambisonics_hrirs(),
            exact,
        );
    }

    let map = HrtfMap::new(44100).unwrap();
    let directions = query_directions(&mut lcg, 300);
    let mut nearest = Vec::new();
    let mut indices = Vec::new();
    let mut weights = Vec::new();
    for &direction in &directions {
        nearest.push(map.nearest_hrir(direction) as i32);
        let w = map.interpolated_hrir_weights(direction);
        indices.extend(w.indices.iter().map(|i| i.map_or(-1, |i| i as i32)));
        weights.extend_from_slice(&w.weights);
    }
    check("hrtf_map/nearest", &nearest, exact);
    check("hrtf_map/interpolated_indices", &indices, exact);
    check("hrtf_map/interpolated_weights", &weights, weight_tol);
}

struct DatabaseCase {
    name: &'static str,
    sampling_rate: u32,
    frame_size: usize,
    normalization: HrtfNormalization,
    volume: f32,
    version_two: bool,
}

const DATABASE_CASES: [DatabaseCase; 3] = [
    DatabaseCase {
        name: "plain",
        sampling_rate: 48000,
        frame_size: 64,
        normalization: HrtfNormalization::None,
        volume: 0.0,
        version_two: false,
    },
    DatabaseCase {
        name: "rms",
        sampling_rate: 44100,
        frame_size: 160,
        normalization: HrtfNormalization::Rms,
        volume: -3.0,
        version_two: false,
    },
    DatabaseCase {
        name: "v2",
        sampling_rate: 24000,
        frame_size: 100,
        normalization: HrtfNormalization::None,
        volume: 4.5,
        version_two: true,
    },
];

const NEAREST_QUERIES: [(f32, HrtfPhaseType); 3] = [
    (0.3, HrtfPhaseType::None),
    (0.6, HrtfPhaseType::SphereItd),
    (0.5, HrtfPhaseType::Full),
];

const INTERPOLATED_QUERIES: [(f32, HrtfPhaseType); 4] = [
    (1.0, HrtfPhaseType::None),
    (0.4, HrtfPhaseType::SphereItd),
    (0.7, HrtfPhaseType::None),
    (0.2, HrtfPhaseType::Full),
];

fn read_i32(data: &[u8], offset: usize) -> i32 {
    i32::from_le_bytes(data[offset..offset + 4].try_into().unwrap())
}

/// The built-in data up to the end of its HRIRs, relabelled as format version 2 (no Ambisonics HRIRs).
fn version_two_data() -> Vec<u8> {
    let data = BUILTIN_HRTF_DATA;
    let num_hrirs = read_i32(data, 8) as usize;
    let mut offset = 12 + 8 * num_hrirs;
    let num_rates = read_i32(data, offset) as usize;
    offset += 4;
    let hrir_bytes: usize = (0..num_rates)
        .map(|i| 2 * num_hrirs * read_i32(data, offset + 8 * i + 4) as usize * 4)
        .sum();
    offset += 8 * num_rates;

    let mut v2 = data[..offset + hrir_bytes].to_vec();
    v2[4..8].copy_from_slice(&2i32.to_le_bytes());
    v2
}

fn check_database_case(c: &DatabaseCase, lcg: &mut Lcg) {
    let v2 = version_two_data();
    let settings = HrtfSettings {
        data: c.version_two.then_some(v2.as_slice()),
        volume: c.volume,
        normalization: c.normalization,
    };
    let mut database = HrtfDatabase::new(&settings, c.sampling_rate, c.frame_size).unwrap();
    let prefix = format!("hrtf_database/{}_", c.name);
    let bins = database.num_spectrum_samples();

    check(
        &format!("{prefix}info"),
        &ints([database.num_hrirs(), database.num_samples(), bins]),
        exact,
    );

    let mut indices = Vec::new();
    let mut selected = Vec::new();
    let mut loudness = Vec::new();
    for _ in 0..4 {
        let index = (lcg.next_u32() % database.num_hrirs() as u32) as usize;
        indices.push(index);
        let hrtf = database.hrtf(index);
        for ear in hrtf {
            extend_flat(&mut selected, ear);
        }
        loudness.push(loudness::rms_loudness(hrtf, c.sampling_rate));
    }
    check(&format!("{prefix}selected_indices"), &ints(indices), exact);
    check(&format!("{prefix}selected_hrtfs"), &selected, signal);
    check(&format!("{prefix}selected_loudness"), &loudness, f32_tol);

    let mut ambisonics = Vec::new();
    for k in 0..NUM_AMBISONICS_COEFFS {
        for ear in database.ambisonics_hrtf(k) {
            extend_flat(&mut ambisonics, ear);
        }
    }
    check(&format!("{prefix}ambisonics_hrtfs"), &ambisonics, signal);

    let directions = query_directions(lcg, 3);
    let mut left = vec![Complex::default(); bins];
    let mut right = vec![Complex::default(); bins];
    let mut nearest_peaks = Vec::new();
    let mut nearest_blended = Vec::new();
    let mut interpolated_peaks = Vec::new();
    let mut interpolated = Vec::new();
    for &direction in &directions {
        for (blend, phase_type) in NEAREST_QUERIES {
            let index = database.nearest_index(direction);
            nearest_peaks.extend(ints(database.peak_delays(index)));
            database.blended_hrtf(index, direction, blend, phase_type, [&mut left, &mut right]);
            extend_flat(&mut nearest_blended, &left);
            extend_flat(&mut nearest_blended, &right);
        }
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
        &format!("{prefix}nearest_blended"),
        &nearest_blended,
        synthesized,
    );
    check(
        &format!("{prefix}interpolated_peaks"),
        &interpolated_peaks,
        exact,
    );
    check(&format!("{prefix}interpolated"), &interpolated, synthesized);
}

#[test]
fn hrtf_database() {
    let mut lcg = Lcg::new(37);
    for c in &DATABASE_CASES {
        check_database_case(c, &mut lcg);
    }
}

#[test]
fn loudness_conversions() {
    let gains = [0.0, 1e-5, 3.2e-5, 3.3e-5, 0.1, 0.5, 1.0, 2.0, 10.0];
    let levels = [-120.0, -90.0, -89.9, -40.0, -6.0, 0.0, 3.0, 12.0, 20.0];
    let to_db: Vec<f32> = gains.iter().map(|&g| loudness::gain_to_db(g)).collect();
    let to_gain: Vec<f32> = levels.iter().map(|&l| loudness::db_to_gain(l)).collect();
    check("hrtf_database/gain_to_db", &to_db, f32_tol);
    check("hrtf_database/db_to_gain", &to_gain, f32_tol);
}
