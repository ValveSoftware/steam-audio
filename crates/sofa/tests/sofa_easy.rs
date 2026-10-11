//! Parity with libmysofa's `mysofa_open_data_no_norm` beyond loading (stage 3.5b): resampling, `tocartesian`,
//! lookup, neighbourhood and `c2s`/`s2c`. Golden data: `tools/golden/src/sofa_easy.cpp`.

use std::path::{Path, PathBuf};

use parity::{Lcg, Tolerance, assert_close, data_path, golden, repo_root};
use sofa::{Easy, Error, Hrtf, Lookup, Neighborhood, c2s, s2c};

const FILES: [&str; 3] = ["cipic_124", "sadie_d1", "sadie_h12"];
const RATES: [f32; 6] = [8000.0, 22050.0, 44100.0, 48000.0, 88200.0, 96000.0];
const EASY_RATE: f32 = 48000.0;
const NUM_COORDS: usize = 4001;
const NUM_ROWS: usize = 6;
const NUM_QUERIES: usize = 600;

fn fnv1a(bytes: impl IntoIterator<Item = u8>) -> u64 {
    bytes.into_iter().fold(0xcbf2_9ce4_8422_2325, |hash, b| {
        (hash ^ u64::from(b)).wrapping_mul(0x100_0000_01b3)
    })
}

fn hash_f32(values: &[f32]) -> u64 {
    fnv1a(values.iter().flat_map(|v| v.to_le_bytes()))
}

fn hash_neighbors(neighborhood: &Neighborhood) -> u64 {
    fnv1a(
        (0..neighborhood.len())
            .flat_map(|i| *neighborhood.get(i).unwrap())
            .flat_map(|n| n.map_or(-1, |n| n as i32).to_le_bytes()),
    )
}

fn push_hash(row: &mut Vec<i32>, hash: u64) {
    row.push(hash as u32 as i32);
    row.push((hash >> 32) as u32 as i32);
}

fn code(result: Result<(), Error>) -> i32 {
    result.err().map_or(0, |e| e.code())
}

fn load(name: &str) -> Hrtf {
    Hrtf::load_file(data_path(&format!("hrtf/{name}.sofa"))).expect("loads")
}

/// `spread` in the generator: a value whose binary exponent is spread over [-20, 20).
fn spread(lcg: &mut Lcg) -> f32 {
    let mantissa = lcg.next_signed();
    let exponent = (lcg.next_u32() % 40) as i32 - 20;
    mantissa * 2.0f32.powi(exponent)
}

#[test]
fn coordinate_conversions() {
    let mut lcg = Lcg::new(61);
    let expected = golden::<f32>("sofa_easy/c2s");
    for i in 0..NUM_COORDS {
        let v = [spread(&mut lcg), spread(&mut lcg), spread(&mut lcg)];
        assert_close(&c2s(v), expected.row(i), Tolerance::F32);
    }

    let expected = golden::<f32>("sofa_easy/s2c");
    for i in 0..NUM_COORDS {
        let phi = lcg.next_signed() * 540.0;
        let theta = lcg.next_signed() * 120.0;
        let v = [phi, theta, spread(&mut lcg)];
        let actual = s2c(v);
        let tol = Tolerance::peak_relative(expected.row(i), 1e-5);
        assert_close(&actual, expected.row(i), tol);
    }
}

/// `selectRows` in the generator.
fn select_rows(hrtf: &Hrtf, lcg: &mut Lcg) -> Vec<usize> {
    let mut rows = vec![0, hrtf.m as usize - 1];
    while rows.len() < NUM_ROWS {
        rows.push((lcg.next_u32() % hrtf.m) as usize);
    }
    rows
}

#[test]
fn resampling() {
    let mut lcg = Lcg::new(67);
    for name in FILES {
        for rate in RATES {
            let mut hrtf = load(name);
            let status = code(hrtf.resample(rate));
            let stem = format!("sofa_easy/resample_{name}_{}", rate as i32);
            let info = vec![
                status,
                hrtf.n as i32,
                hrtf.data_ir.len() as i32,
                hrtf.data_delay.len() as i32,
            ];
            assert_eq!(info, golden::<i32>(&format!("{stem}_info")).data, "{stem}");

            let expected = golden::<f32>(&format!("{stem}_rows"));
            let row_size = (hrtf.r * hrtf.n) as usize;
            let values = hrtf.data_ir.values.as_deref().unwrap();
            for (i, row) in select_rows(&hrtf, &mut lcg).into_iter().enumerate() {
                let actual = &values[row * row_size..(row + 1) * row_size];
                let expected = expected.row(i);
                // Per impulse response (one ear).
                for (actual, expected) in actual
                    .chunks_exact(hrtf.n as usize)
                    .zip(expected.chunks_exact(hrtf.n as usize))
                {
                    assert_close(actual, expected, Tolerance::peak_relative(expected, 1e-5));
                }
            }

            if !hrtf.data_delay.is_empty() {
                let expected = golden::<f32>(&format!("{stem}_delay")).data;
                assert_close(
                    hrtf.data_delay.values.as_deref().unwrap(),
                    &expected,
                    Tolerance::F32,
                );
            }
        }
    }
}

/// cipic_124 with a corrupt sampling rate or dimensions. The hashes of resampled data are only compared on Windows,
/// where the C library is the reference's (bit-exact there; elsewhere `resampling` covers the values).
#[test]
fn resampling_edge_cases() {
    let rates = [0.5, -1.5, f32::NAN, f32::INFINITY, 1.0e6, 24000.0, 44100.5];
    let expected = golden::<i32>("sofa_easy/resample_edges");
    let num_cases = rates.len() + 3;
    assert_eq!(expected.shape, [num_cases, 7]);
    for case in 0..num_cases {
        let mut hrtf = load("cipic_124");
        let mut target = 48000.0;
        if case < rates.len() {
            hrtf.data_sampling_rate.values.as_mut().unwrap()[0] = rates[case];
        } else if case == num_cases - 3 {
            target = 7999.0;
        } else if case == num_cases - 2 {
            target = 44100.0;
        } else {
            hrtf.n += 1;
        }
        let status = code(hrtf.resample(target));
        let mut row = vec![status, hrtf.n as i32, hrtf.data_ir.len() as i32];
        push_hash(&mut row, hash_f32(hrtf.data_ir.values.as_deref().unwrap()));
        push_hash(
            &mut row,
            hash_f32(hrtf.data_delay.values.as_deref().unwrap_or_default()),
        );

        let expected = expected.row(case);
        let resampled = status == 0 && case < rates.len();
        if resampled && !cfg!(windows) {
            assert_eq!(row[..3], expected[..3], "case {case}");
        } else {
            assert_eq!(row, expected, "case {case}");
        }
    }
}

#[test]
fn resampling_limits() {
    // Upsampling from 1 Hz makes 48000 × 200 samples per response: more than 2^27 values in all.
    let mut hrtf = load("cipic_124");
    hrtf.data_sampling_rate.values.as_mut().unwrap()[0] = 1.0;
    assert!(matches!(hrtf.resample(48000.0), Err(Error::ResourceLimit)));
    // Downsampling by more than 64:1 needs a filter longer than 2^14 taps.
    let mut hrtf = load("cipic_124");
    hrtf.data_sampling_rate.values.as_mut().unwrap()[0] = 8000.0 * 65.0;
    assert!(matches!(hrtf.resample(8000.0), Err(Error::ResourceLimit)));
}

/// The query points of `generateEasy`.
fn queries(positions: &[f32], seed: u32) -> Vec<[f32; 3]> {
    let m = (positions.len() / 3) as u32;
    let point = |a: u32| {
        let a = a as usize * 3;
        [positions[a], positions[a + 1], positions[a + 2]]
    };
    let mut lcg = Lcg::new(seed);
    let mut queries = Vec::with_capacity(NUM_QUERIES);
    for _ in 0..NUM_QUERIES - 1 {
        queries.push(match lcg.next_u32() % 4 {
            0 => {
                let x = lcg.next_signed() * 2.0;
                let y = lcg.next_signed() * 2.0;
                let z = lcg.next_signed() * 2.0;
                [x, y, z]
            }
            1 => point(lcg.next_u32() % m),
            2 => {
                let a = point(lcg.next_u32() % m);
                let b = point(lcg.next_u32() % m);
                [0, 1, 2].map(|k| (a[k] + b[k]) * 0.5)
            }
            _ => {
                let a = point(lcg.next_u32() % m);
                let scale = lcg.next_signed() * 4.0;
                a.map(|v| v * scale)
            }
        });
    }
    queries.push([0.0; 3]);
    queries
}

/// Lookup and neighbourhood indices are bit-exact with the MSVC reference. Elsewhere the C library's `sinf`, `cosf`,
/// `atan2f` and `powf` round differently by an ulp, which can flip a tie between measurements that are exactly
/// equidistant on a regular grid (for example stepping up in elevation to midway between two azimuths). There a
/// different index is accepted only if it is as far from `point` as the expected one; returns whether it was a tie.
#[track_caller]
fn check_index(
    actual: i32,
    expected: i32,
    point: [f32; 3],
    positions: &[f32],
    context: &str,
) -> bool {
    if actual == expected {
        return false;
    }
    assert!(
        !cfg!(all(windows, target_env = "msvc")) && actual >= 0 && expected >= 0,
        "{context}: index {actual}, expected {expected}"
    );
    let distance = |index: i32| {
        let p = &positions[index as usize * 3..index as usize * 3 + 3];
        (0..3)
            .map(|k| (f64::from(p[k]) - f64::from(point[k])).powi(2))
            .sum::<f64>()
            .sqrt()
    };
    let (a, e) = (distance(actual), distance(expected));
    assert!(
        (a - e).abs() <= 1e-5 * e.max(1e-6),
        "{context}: index {actual} at {a}, expected {expected} at {e}: not a tie"
    );
    true
}

#[test]
fn shipped_files_open() {
    for (k, name) in FILES.into_iter().enumerate() {
        let easy = Easy::load_file_no_norm(data_path(&format!("hrtf/{name}.sofa")), EASY_RATE)
            .expect("opens");
        let stem = format!("sofa_easy/easy_{name}");
        let hrtf = &easy.hrtf;
        assert_eq!(
            vec![0, easy.filter_length() as i32, hrtf.m as i32],
            golden::<i32>(&format!("{stem}_info")).data,
            "{stem}"
        );

        let lookup = &easy.lookup;
        let ranges = [
            lookup.radius_min,
            lookup.radius_max,
            lookup.theta_min,
            lookup.theta_max,
            lookup.phi_min,
            lookup.phi_max,
        ];
        assert_close(
            &ranges,
            &golden::<f32>(&format!("{stem}_ranges")).data,
            Tolerance::F32,
        );

        let positions = hrtf.source_position.values.as_deref().unwrap();
        let expected = golden::<f32>(&format!("{stem}_positions")).data;
        assert_close(positions, &expected, Tolerance::F32);

        let expected = golden::<i32>(&format!("{stem}_neighbors"));
        assert_eq!(expected.shape, [hrtf.m as usize, 6]);
        let mut ties = 0;
        for i in 0..hrtf.m as usize {
            let actual = easy
                .neighborhood
                .get(i)
                .unwrap()
                .map(|n| n.map_or(-1, |n| n as i32));
            let origin = [positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]];
            for (slot, (&a, &e)) in actual.iter().zip(expected.row(i)).enumerate() {
                let context = format!("{name}: neighbour {slot} of measurement {i}");
                ties += usize::from(check_index(a, e, origin, positions, &context));
            }
        }

        let expected_points = golden::<f32>(&format!("{stem}_query_points"));
        let expected_indices = golden::<i32>(&format!("{stem}_query_indices")).data;
        let mut scratch = lookup.scratch();
        for (q, mut point) in queries(positions, 71 + k as u32).into_iter().enumerate() {
            let index = lookup.lookup_with(&mut point, &mut scratch);
            let context = format!("{name}: query {q}");
            let actual = index.map_or(-1, |i| i as i32);
            ties += usize::from(check_index(
                actual,
                expected_indices[q],
                point,
                positions,
                &context,
            ));
            assert_close(&point, expected_points.row(q), Tolerance::F32);
        }
        if ties > 0 {
            eprintln!("{name}: {ties} ties resolved differently from the reference");
        }
    }
}

#[test]
fn lookup_and_neighborhood_preconditions() {
    let mut hrtf = load("cipic_124");
    // Still spherical.
    assert!(matches!(Lookup::new(&hrtf), Err(Error::Internal)));
    hrtf.to_cartesian();
    let lookup = Lookup::new(&hrtf).expect("cartesian");
    assert!(lookup.lookup(&mut [1.0, 0.0, 0.0]).is_some());
    hrtf.m += 1;
    assert!(matches!(Lookup::new(&hrtf), Err(Error::InvalidFormat)));
    assert!(matches!(
        Neighborhood::new(&hrtf, &lookup),
        Err(Error::InvalidFormat)
    ));

    let mut empty = Hrtf {
        c: 3,
        ..Hrtf::default()
    };
    empty.source_position.values = Some(Vec::new());
    empty.source_position.attributes.push(sofa::Attribute {
        name: b"Type".to_vec(),
        value: Some(b"cartesian".to_vec()),
    });
    let lookup = Lookup::new(&empty).expect("empty");
    assert_eq!(lookup.lookup(&mut [1.0, 0.0, 0.0]), None);
}

/// libmysofa's own test files through `mysofa_open_data_no_norm`; see `libmysofa_corpus` in `tests/sofa.rs`.
#[test]
fn libmysofa_corpus() {
    let dir = std::env::var_os("MYSOFA_TESTS_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repo_root().join("core/deps-build/mysofa/src/mysofa/tests"));
    let golden_dir = repo_root().join("testdata/golden/sofa_easy");

    let mut names: Vec<String> = std::fs::read_dir(&golden_dir)
        .expect("golden directory")
        .filter_map(|entry| {
            let name = entry.ok()?.file_name().into_string().ok()?;
            Some(
                name.strip_prefix("corpus_")?
                    .strip_suffix(".npy")?
                    .to_owned(),
            )
        })
        .collect();
    names.sort();
    assert!(!names.is_empty(), "no corpus goldens");

    let (mut checked, mut skipped) = (0, 0);
    for name in &names {
        let path = dir.join(format!("{name}.sofa"));
        if !Path::new(&path).is_file() {
            skipped += 1;
            continue;
        }
        let row = match Easy::load_file_no_norm(&path, EASY_RATE) {
            Err(error) => vec![error.code()],
            Ok(easy) => {
                let hrtf = &easy.hrtf;
                let mut row = vec![0, easy.filter_length() as i32, hrtf.m as i32];
                push_hash(&mut row, hash_f32(hrtf.data_ir.values.as_deref().unwrap()));
                push_hash(
                    &mut row,
                    hash_f32(hrtf.source_position.values.as_deref().unwrap()),
                );
                push_hash(&mut row, hash_neighbors(&easy.neighborhood));
                row
            }
        };
        assert_eq!(
            row,
            golden::<i32>(&format!("sofa_easy/corpus_{name}")).data,
            "{name}"
        );
        checked += 1;
    }
    eprintln!(
        "libmysofa corpus: {checked} files checked, {skipped} skipped (not found in {})",
        dir.display()
    );
}

/// Corrupted variants of cipic_124.sofa through the whole of `Easy`: an error or a usable HRTF, never a panic.
#[test]
fn mutated_files_open_or_fail() {
    let original = std::fs::read(data_path("hrtf/cipic_124.sofa")).expect("readable");
    let num_cases: usize = std::env::var("SOFA_EASY_MUTATIONS")
        .ok()
        .and_then(|n| n.parse().ok())
        .unwrap_or(64);
    let mut lcg = Lcg::new(89);
    let (mut opened, mut failed) = (0, 0);
    for _ in 0..num_cases {
        let mut data = original.clone();
        for _ in 0..1 + lcg.next_u32() % 4 {
            let position = (lcg.next_u32() % 0x10000) as usize;
            let value = lcg.next_u32() as u8;
            data[position] = value;
        }
        // The file's own rate: no resampling, to keep the test fast.
        match Easy::load_no_norm(&data, 44100.0) {
            Ok(easy) => {
                let mut direction = [0.0, 1.0, 0.0];
                let _ = easy.lookup.lookup(&mut direction);
                opened += 1;
            }
            Err(_) => failed += 1,
        }
    }
    eprintln!("{opened} opened, {failed} failed");
}
