# steam-audio-rust

A Rust reimplementation of Valve's Steam Audio, verified numerically against the original C++.

## Where things are

- `core/`: **upstream C++, the specification. Never modify it.** (`core/deps*/` are gitignored build outputs.)
- `crates/steam-audio/`: the port (library). `crates/parity/`: test support (`.npy`, `Tolerance`, `Lcg`).
- `crates/sofa/`: SOFA/HDF5 reader, a port of the libmysofa subset Steam Audio uses (ADR 0001); fuzz target in
  `crates/sofa/fuzz` (nightly, outside the workspace).
- `tools/golden/`: reference build + golden-data generators (C++). See `tools/golden/README.md`.
- `testdata/golden/<module>/<case>.npy`: committed reference outputs.
- `docs/PORTING_PLAN.md`: **source of truth**: stage order, status table, tolerances, upstream→Rust name map,
  risks (R1–R6). Read the relevant rows before starting a stage and update them when it lands.
- `docs/DEVIATIONS.md`: every intentional behaviour difference. `docs/adr/`: decisions (0001: SOFA loader).

## Checks (must pass before every commit)

```bash
cargo fmt --all -- --check
cargo clippy --all-targets -- -D warnings
cargo test --all
```

CI runs the same on ubuntu, windows and macos.

## Reference build (Windows, MSVC + Ninja)

The machine-local helper `build/env.cmd` (gitignored) sets up MSVC, CMake and Ninja; prefix commands with it,
e.g. `cmd //c "build\env.cmd cmake --build build\reference --target golden"`. Full steps are in
`tools/golden/README.md`. After changing a generator:

```bash
build/reference/golden.exe testdata/golden <module> [<module> ...]
```

Goldens are generated **only with MSVC on Windows x64** (upstream RNG and libm are implementation-defined).

## How a porting slice is done

1. Read the upstream `.h`/`.cpp` and its `core/src/test/*.test.cpp`. Note operation order, mixed float/double,
   `std::min`/`std::max` semantics, SIMD-body vs scalar-tail differences, and unreachable or buggy code.
2. Add a generator in `tools/golden/src/<area>.cpp` (register it with `golden::Register` and list it in
   `golden.cmake`). Draw inputs from `golden::Lcg` **one value per statement**, because C++ argument evaluation
   order is unspecified. Use sizes that are not multiples of 4 so SIMD tails are exercised.
3. Port to idiomatic Rust, keeping upstream's arithmetic order.
4. Add `crates/steam-audio/tests/<area>.rs`: replay the same `Lcg` draws and compare with `parity::golden` +
   `Tolerance`. Use `F32` for scalar math, `peak_relative` for signals and `EXACT` for integers and data movement.
   Any looser tolerance needs a written reason in the plan.
5. Temporarily switch all tolerances to `EXACT` to measure bit-exactness, and report the result. Stages 2.1–2.3
   are bit-exact on Windows.
6. Update the plan (status, name map, risks) and `DEVIATIONS.md`.

## Parity pitfalls found so far

- Upstream constants are truncated: `math::PI = 3.141592`, `HALF_PI`, `DEGREES_TO_RADIANS = 0.017452`. Never use
  the `std` constants where upstream uses its own.
- Division by a scalar is multiplication by the reciprocal (`x * (1/s)`), not `x / s`.
- Some code silently computes in `double` (for example `Quaternion::normalize`); check MSVC C4244 warnings.
- Use `math::cpp_min` / `cpp_max` (crate-private) to get `std::min` / `std::max` NaN and tie semantics.
- Upstream's RNG: see risk R2 in the plan before porting anything random.
- `float /= double` (and `float = double expr`) rounds twice: the left side is already a `float`. Mirror it with
  `(f64::from(x) / a0) as f32`. MSVC's unqualified `sqrt(float)` is the `float` overload.
- Code dispatched on `gSIMDLevel()` may round differently per path; the port follows the AVX path (risk R8).
- `float4::round` is `_mm_cvtps_epi32` (half to even, out of range → `INT_MIN`), unlike `roundf` in scalar tails.

## Rust conventions

- `f32` only (upstream's `double` instantiations are unused). Edition 2024, MSRV 1.85.
- No `unsafe` (workspace lint `deny`). No allocation in per-buffer audio processing: allocate in constructors and
  write into caller-provided outputs.
- The core stays engine-agnostic: no Bevy, no globals, `Send` types. A Bevy crate comes later (plan stage 9).
- Complex numbers: `steam_audio::Complex` (`num_complex::Complex<f32>`, shared with `rustfft`).
- Panic on violated preconditions (shape or length mismatch); return `Result` for input-dependent failures.

## Git workflow

- One branch and one PR per stage, from up-to-date `master`. Start the next stage only after the previous PR's CI
  is green and it is merged.
- Each stage is done in its own new session: never start the next stage in the session that finished the previous
  one.
- Merge with merge commits and delete the branch (`gh pr merge --merge --delete-branch`) so `Co-Authored-By`
  trailers survive. Merge only when asked.
- Enable Auto-fix on every new PR.
