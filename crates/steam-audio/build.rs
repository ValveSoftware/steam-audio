//! Converts upstream's built-in HRTF (`core/src/core/cipic_124.inl`, a C byte-array initializer) into a binary file in
//! `OUT_DIR`, which `src/hrtf/mod.rs` embeds with `include_bytes!`. The `.inl` stays the single source of the data.

use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let manifest_dir =
        PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let source = manifest_dir.join("../../core/src/core/cipic_124.inl");
    println!("cargo::rerun-if-changed={}", source.display());

    let text =
        fs::read(&source).unwrap_or_else(|e| panic!("cannot read {}: {e}", source.display()));
    let start = text
        .iter()
        .position(|&c| c == b'{')
        .expect("no array initializer in cipic_124.inl")
        + 1;

    let mut bytes = Vec::with_capacity(text.len() / 3);
    let mut value: Option<u32> = None;
    for &c in &text[start..] {
        if c.is_ascii_digit() {
            value = Some(value.unwrap_or(0) * 10 + u32::from(c - b'0'));
            continue;
        }
        if let Some(v) = value.take() {
            bytes.push(u8::try_from(v).expect("byte value out of range in cipic_124.inl"));
        }
        if c == b'}' {
            break;
        }
    }

    let out = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR")).join("cipic_124.hrtf");
    fs::write(&out, bytes).unwrap_or_else(|e| panic!("cannot write {}: {e}", out.display()));
}
