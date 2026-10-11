//! Parity with libmysofa's loader and `mysofa_check` (stage 3.5a). Golden data: `tools/golden/src/sofa.cpp`.
//!
//! Every loaded file is compared through a "blob" listing the load status, dimensions, check status, attributes and
//! every array (element count and FNV-1a hash of the values), built the same way on both sides and compared
//! byte for byte (`EXACT`).

use std::path::{Path, PathBuf};

use parity::{Lcg, Tolerance, assert_close, data_path, golden, repo_root};
use sofa::{Array, Attribute, Error, Hrtf};

const FILES: [&str; 3] = ["cipic_124", "sadie_d1", "sadie_h12"];
const NUM_RANDOM_ROWS: usize = 4;
const NUM_MUTATIONS: usize = 256;
const METADATA_REGION: u32 = 0x10000;

fn fnv1a(bytes: &[u8]) -> u64 {
    bytes.iter().fold(0xcbf2_9ce4_8422_2325, |hash, &b| {
        (hash ^ u64::from(b)).wrapping_mul(0x100_0000_01b3)
    })
}

#[derive(Default)]
struct Blob(Vec<u8>);

impl Blob {
    fn token(&mut self, text: impl AsRef<[u8]>) {
        self.0.extend_from_slice(text.as_ref());
        self.0.push(0);
    }

    fn attributes(&mut self, attributes: &[Attribute]) {
        for attribute in attributes {
            match &attribute.value {
                Some(value) => {
                    self.token("attr");
                    self.token(&attribute.name);
                    self.token(value);
                }
                None => {
                    self.token("attr-null");
                    self.token(&attribute.name);
                }
            }
        }
    }

    fn array(&mut self, kind: &str, name: &[u8], array: &Array) {
        self.token(kind);
        self.token(name);
        match &array.values {
            Some(values) => {
                let bytes: Vec<u8> = values.iter().flat_map(|v| v.to_le_bytes()).collect();
                self.token("values");
                self.token(values.len().to_string());
                self.token(format!("{:016x}", fnv1a(&bytes)));
            }
            None => self.token("values-null"),
        }
        self.attributes(&array.attributes);
    }
}

/// The Rust counterpart of `describe` in the generator.
fn describe(result: &Result<Hrtf, Error>) -> Vec<u8> {
    let mut blob = Blob::default();
    blob.token("load");
    let hrtf = match result {
        Err(error) => {
            blob.token(error.code().to_string());
            blob.token("null");
            return blob.0;
        }
        Ok(hrtf) => hrtf,
    };
    blob.token(hrtf.load_error.as_ref().map_or(0, Error::code).to_string());

    blob.token("dims");
    for dim in [hrtf.i, hrtf.c, hrtf.r, hrtf.e, hrtf.n, hrtf.m] {
        blob.token(dim.to_string());
    }
    blob.token("check");
    blob.token(hrtf.check().err().map_or(0, |e| e.code()).to_string());

    blob.attributes(&hrtf.attributes);
    let arrays = [
        ("ListenerPosition", &hrtf.listener_position),
        ("ReceiverPosition", &hrtf.receiver_position),
        ("SourcePosition", &hrtf.source_position),
        ("EmitterPosition", &hrtf.emitter_position),
        ("ListenerUp", &hrtf.listener_up),
        ("ListenerView", &hrtf.listener_view),
        ("Data.IR", &hrtf.data_ir),
        ("Data.SamplingRate", &hrtf.data_sampling_rate),
        ("Data.Delay", &hrtf.data_delay),
    ];
    for (name, array) in arrays {
        blob.array("array", name.as_bytes(), array);
    }
    for variable in &hrtf.variables {
        blob.array("var", &variable.name, &variable.value);
    }
    blob.0
}

#[track_caller]
fn assert_blob(actual: &[u8], expected: &[u8], case: &str) {
    if actual != expected {
        let text = |b: &[u8]| String::from_utf8_lossy(b).replace('\0', "\n");
        panic!(
            "{case}: blob mismatch\n--- expected\n{}\n--- actual\n{}",
            text(expected),
            text(actual)
        );
    }
}

#[test]
fn shipped_files() {
    let mut lcg = Lcg::new(41);
    for name in FILES {
        let result = Hrtf::load_file(data_path(&format!("hrtf/{name}.sofa")));
        assert_blob(
            &describe(&result),
            &golden::<u8>(&format!("sofa/{name}_blob")).data,
            name,
        );
        let hrtf = result.expect("loads");
        assert!(hrtf.load_error.is_none());
        hrtf.check().expect("passes the check");

        let positions = hrtf
            .source_position
            .values
            .as_deref()
            .expect("source positions");
        assert_close(
            positions,
            &golden::<f32>(&format!("sofa/{name}_source_position")).data,
            Tolerance::EXACT,
        );

        let m = hrtf.m as usize;
        let row_size = (hrtf.r * hrtf.n) as usize;
        let mut rows = vec![0, m - 1];
        rows.extend((0..NUM_RANDOM_ROWS).map(|_| (lcg.next_u32() % hrtf.m) as usize));
        let ir = hrtf.data_ir.values.as_deref().expect("impulse responses");
        let selected: Vec<f32> = rows
            .iter()
            .flat_map(|&row| &ir[row * row_size..(row + 1) * row_size])
            .copied()
            .collect();
        assert_close(
            &selected,
            &golden::<f32>(&format!("sofa/{name}_data_ir_rows")).data,
            Tolerance::EXACT,
        );
    }
}

#[test]
fn memory_and_file_loading_agree() {
    let path = data_path("hrtf/cipic_124.sofa");
    let from_file = describe(&Hrtf::load_file(&path));
    let from_memory = describe(&Hrtf::load(&std::fs::read(&path).expect("readable")));
    assert_blob(&from_memory, &from_file, "cipic_124");
}

/// libmysofa's own test files, including its fuzzing regressions. They are not in this repository: the test uses
/// `MYSOFA_TESTS_DIR`, or the dependency checkout of the reference build, and skips the files it cannot find.
#[test]
fn libmysofa_corpus() {
    let dir = std::env::var_os("MYSOFA_TESTS_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repo_root().join("core/deps-build/mysofa/src/mysofa/tests"));
    let golden_dir = repo_root().join("testdata/golden/sofa");

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
        let expected = golden::<u8>(&format!("sofa/corpus_{name}")).data;
        assert_blob(&describe(&Hrtf::load_file(&path)), &expected, name);
        checked += 1;
    }
    eprintln!(
        "libmysofa corpus: {checked} files checked, {skipped} skipped (not found in {})",
        dir.display()
    );
}

/// cipic_124.sofa with byte changes and truncations: the same status, and the same contents when it still loads.
#[test]
fn mutated_files() {
    let original = std::fs::read(data_path("hrtf/cipic_124.sofa")).expect("readable");
    let size = original.len() as u32;
    let metadata = size.min(METADATA_REGION);
    let expected = golden::<i32>("sofa/mutations");
    assert_eq!(expected.shape, [NUM_MUTATIONS, 7]);

    let mut lcg = Lcg::new(53);
    let mut mismatches = Vec::new();
    for case in 0..NUM_MUTATIONS {
        let mut data = original.clone();
        let kind = lcg.next_u32() % 4;
        let (position, value);
        match kind {
            0 | 1 => {
                position = lcg.next_u32() % if kind == 0 { metadata } else { size };
                value = lcg.next_u32() % 255 + 1;
                data[position as usize] ^= value as u8;
            }
            2 => {
                position = lcg.next_u32() % metadata;
                value = lcg.next_u32() % 256;
                data[position as usize] = value as u8;
            }
            _ => {
                position = lcg.next_u32() % size;
                value = 0;
                data.truncate(position as usize);
            }
        }

        let result = Hrtf::load(&data);
        let hash = fnv1a(&describe(&result));
        let load_status = match &result {
            Ok(hrtf) => hrtf.load_error.as_ref().map_or(0, Error::code),
            Err(error) => error.code(),
        };
        let actual = [
            kind as i32,
            position as i32,
            value as i32,
            load_status,
            i32::from(result.is_ok()),
            hash as i32,
            (hash >> 32) as i32,
        ];
        if actual != expected.row(case) {
            mismatches.push(format!(
                "mutation {case}: {actual:?}, expected {:?}",
                expected.row(case)
            ));
        }
    }
    assert!(
        mismatches.is_empty(),
        "(kind, position, value, status, loaded, blob hash)
{}",
        mismatches.join(
            "
"
        )
    );
}

/// Malformed input never panics: every truncation of the metadata, and random bytes behind a valid signature.
#[test]
fn malformed_input_returns_errors() {
    let original = std::fs::read(data_path("hrtf/cipic_124.sofa")).expect("readable");
    for len in (0..0xa000).step_by(97) {
        assert!(
            Hrtf::load(&original[..len]).is_err(),
            "truncated to {len} bytes"
        );
    }

    let mut lcg = Lcg::new(7);
    for case in 0..64 {
        let len = 8 + (lcg.next_u32() % 4096) as usize;
        let mut data: Vec<u8> = (0..len).map(|_| lcg.next_u32() as u8).collect();
        data[..8].copy_from_slice(b"\x89HDF\r\n\x1a\n");
        data[8] = (case % 4) as u8;
        let _ = Hrtf::load(&data);
    }

    assert_eq!(Hrtf::load(&[]).unwrap_err().code(), 10000);
    assert_eq!(
        Hrtf::load(b"\x89HDF\r\n\x1a\n\x07").unwrap_err().code(),
        10000
    );
}

#[test]
fn missing_file_reports_io_error() {
    let error = Hrtf::load_file(repo_root().join("no-such-file.sofa")).unwrap_err();
    assert!(matches!(error, Error::Io(_)));
}
