//! Test support for numerical parity with the reference C++ Steam Audio.
//!
//! Golden data is produced by `tools/golden` (a small C++ program linked against `core/`) and
//! stored as `.npy` files under `testdata/golden/`. Rust tests load it with [`golden`] and compare
//! with [`assert_close`] using a per-module [`Tolerance`].

mod compare;
mod npy;

use std::path::{Path, PathBuf};

pub use compare::{Mismatch, Tolerance, compare};
pub use npy::{Array, Element, NpyError, read_npy, write_npy};

/// Repository root (two levels above this crate's manifest).
pub fn repo_root() -> &'static Path {
    Path::new(concat!(env!("CARGO_MANIFEST_DIR"), "/../.."))
}

/// Path to a file in the upstream test assets, e.g. `data_path("hrtf/sadie_d1.sofa")`.
pub fn data_path(relative: &str) -> PathBuf {
    repo_root().join("core/data").join(relative)
}

/// Path to a golden file, e.g. `golden_path("fft/forward_256")` → `testdata/golden/fft/forward_256.npy`.
pub fn golden_path(name: &str) -> PathBuf {
    repo_root()
        .join("testdata/golden")
        .join(format!("{name}.npy"))
}

/// Loads a golden array, panicking with a hint on how to regenerate it if it is missing.
pub fn golden<T: Element>(name: &str) -> Array<T> {
    let path = golden_path(name);
    read_npy(&path).unwrap_or_else(|e| {
        panic!(
            "cannot load golden `{name}` from {}: {e}\n\
             regenerate it with tools/golden (see docs/PORTING_PLAN.md, section 5)",
            path.display()
        )
    })
}

/// Panics with a readable report if `actual` is not within `tol` of `expected`.
#[track_caller]
pub fn assert_close<T: Into<f64> + Copy>(actual: &[T], expected: &[T], tol: Tolerance) {
    if let Err(m) = compare(actual, expected, tol) {
        panic!("parity check failed: {m}");
    }
}
