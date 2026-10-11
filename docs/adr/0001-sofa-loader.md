# ADR 0001: Loading SOFA HRTF files

- **Status:** accepted
- **Date:** 2026-10-11

## Context

Steam Audio accepts custom HRTFs in the [SOFA](https://www.sofaconventions.org/) format (AES69). SOFA files are
HDF5 containers. The reference implementation (`core/src/core/sofa_hrtf_map.cpp`) loads them through
[libmysofa](https://github.com/hoene/libmysofa) and uses more of that library than file parsing alone:

| libmysofa call | What it does for Steam Audio |
| --- | --- |
| `mysofa_open_no_norm` / `mysofa_open_data_no_norm` | Parses HDF5, validates SOFA conventions, **resamples the HRIRs** to the engine's sampling rate (bundled Speex resampler) |
| `mysofa_lookup` | Nearest-measurement search (k-d tree) |
| `mysofa_neighborhood` | Neighbouring measurements, used for interpolation |
| `mysofa_c2s` | Cartesian → spherical conversion |

Things that bound the decision:

- The built-in HRTF (`cipic_124.inl`) uses Steam Audio's own binary format and does not need SOFA. SOFA is an
  optional path; upstream can be built without it (`IPL_DISABLE_SOFA`).
- We aim for numerical parity, so the resampler, lookup and neighbourhood logic must behave like libmysofa's.
  An HDF5 reader alone is not enough.
- SOFA files come from users, so the parser handles untrusted input. libmysofa has had several CVEs in its
  HDF5 code (heap overflows and out-of-bounds reads).
- Project goals: memory safety, `cargo build` with no system libraries, easy cross-compilation.

## Options

### A. `hdf5` crate (bindings to the official libhdf5)

- ➕ Complete and mature HDF5 support.
- ➖ Needs libhdf5 installed, or built from source through CMake. That is painful on Windows, Android and iOS
  and breaks "just `cargo build`".
- ➖ Covers parsing only. Resampling, lookup and neighbourhood would still have to be ported for parity.
- ➖ A large C dependency for a small subset of HDF5 (SOFA files use a narrow set of features).

### B. FFI to libmysofa (built with the `cc` crate)

- ➕ Bit-for-bit the same behaviour as the reference, and the fastest to deliver.
- ➖ C code parsing untrusted files inside a "memory-safe" library: exactly the risk the project exists to remove.
- ➖ Needs a C toolchain plus zlib on every target, and `unsafe` FFI glue.

### C. Pure-Rust port of the libmysofa subset (**chosen**)

Port into a separate crate, `crates/sofa`:

1. The read-only HDF5 subset libmysofa implements (superblock v0/v2/v3, object headers, B-trees, global/fractal
   heaps, attributes, chunked and contiguous datasets, deflate filter).
2. SOFA validation (`SimpleFreeFieldHRIR`).
3. The Speex resampler at libmysofa's quality setting.
4. `lookup` (k-d tree), `neighborhood`, `c2s`.

- ➕ No system dependencies or `unsafe`, and parsing is safe by construction. Easy to fuzz with `cargo-fuzz`.
- ➕ Parity is reachable, because the same algorithms are ported. libmysofa itself serves as the oracle in
  `tools/golden`.
- ➕ Works on every target Rust supports, including `wasm32`.
- ➖ The most work: roughly 3–5k lines of C to port, plus a resampler.
- ➖ May hit HDF5 features libmysofa does not support either. Accepted: we match libmysofa's coverage, not more.

`flate2` with the pure-Rust `miniz_oxide` backend replaces zlib (refined below: `miniz_oxide` directly).

### D. Offline conversion of SOFA into Steam Audio's own format

- ➕ Simplest runtime.
- ➖ Breaks the public contract: upstream accepts a `.sofa` file name or an in-memory buffer at runtime.

## Decision

**Option C.** SOFA support lives in its own crate, `sofa`, enabled in `steam-audio` through the `sofa` cargo
feature (on by default, like upstream). The built-in HRTF does not depend on it.

Ordering: the SOFA crate is only a prerequisite for SOFA-based tests in the "Binaural rendering" stage. All
binaural work starts with the built-in `cipic_124` HRTF, so SOFA can be done in parallel or afterwards.

## Consequences

- `tools/golden` builds the reference with libmysofa and dumps parsed and resampled HRIRs, lookup results and
  neighbourhoods for the three files in `core/data/hrtf/`. These are the `sofa` crate's parity tests.
- Resampled HRIRs: tolerance `peak_relative(1e-5)` per HRIR. Lookup and neighbourhood indices: exact.
- Add a `cargo-fuzz` target for the HDF5 parser once it exists.
- If the port stalls, option B stays available as a temporary, feature-gated fallback that does not touch the
  public API.

## Refinements (2026-10-12, stage 3.5a)

- **Split.** The port is done in three stages: 3.5a, the HDF5 subset with the SOFA loader and `mysofa_check`; 3.5b,
  resampling, `tocartesian`, lookup, neighbourhood and `c2s`/`s2c`; 3.5c, `SOFAHRTFMap` and `HRTFMapFactory` in
  `steam-audio`. The `sofa` cargo feature of `steam-audio` arrives with 3.5c.
- **Inflate.** libmysofa calls zlib's `inflate` once per chunk into a buffer of the chunk's size. zlib returns as soon
  as that buffer is full, so trailing data and even a corrupt checksum after a pending literal or match go unchecked.
  `flate2`'s streaming API decodes ahead into its window and reports those errors, so the crate uses `miniz_oxide`'s
  core inflater with a non-wrapping output buffer of exactly the chunk's size, which stops at the same point.
- **Parity scope.** Beyond the three shipped files, the goldens cover libmysofa's own test files (including its
  fuzzing regressions; checked locally, as the files are not in this repository) and corrupted variants of
  `cipic_124.sofa`: status codes, attributes and values match libmysofa bit for bit. The port reproduces
  libmysofa's quirks (plan, R16) and adds limits only where libmysofa crashes or does not terminate
  (`DEVIATIONS.md`).
- **Fuzzing.** `crates/sofa/fuzz` holds a cargo-fuzz target (nightly, outside the workspace).
- **License.** The crate is a derived work of libmysofa (BSD 3-Clause); its notice is kept in
  `crates/sofa/LICENSE-libmysofa`.
