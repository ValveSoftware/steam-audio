# Porting plan: Steam Audio (C++) → Rust

This document is the source of truth for the port. Update the status table in §4 as each slice lands.

## 1. Contract

- **Scope:** the Steam Audio SDK core (`core/src/core`). Unity, Unreal and FMOD plugins are out of scope.
- **Fidelity:** numerical parity within a stated **per-module tolerance** (§5). Bit-exactness is not a goal:
  the FFT, SIMD width and summation order differ.
- **Integration:**
  1. `steam-audio`: idiomatic Rust library. This is the primary API.
  2. `phonon-capi` (stage 8): thin `cdylib` matching `phonon.h`, so it can replace upstream without changes
     to calling code.
- **Constraints:** stable Rust, edition 2024, MSRV 1.85. No `unsafe` outside the C API crate and explicitly
  justified, measured hot paths. No system libraries needed to build.
- **Original:** C++14, CMake, Apache-2.0; sources in `core/` (upstream snapshot).
- **Oracle:** the original builds locally (MSVC 19.50 + CMake + Ninja; see `tools/golden/README.md`) and produces
  golden data via `tools/golden`. All 155 upstream Catch tests pass on this build (24,900 assertions); they are
  a second source of test cases.

## 2. Inventory summary

| Item | Notes |
| --- | --- |
| Size | ~68k lines of C++ in `core/src/core` (~150 modules, ~300 files); 40 Catch test files; 39 integration tests; 25 benchmarks |
| Public API | `phonon.h` (C) implemented by the `api_*.cpp` files on top of internal C++ classes |
| Global/mutable state | `Context` (allocator, logging, SIMD level), profiler, global thread pool |
| Concurrency | `ThreadPool` + `JobGraph` for simulation; `TripleBuffer` for handing results from simulation threads to the audio thread |
| Error model | `ipl::Exception(Status)` thrown internally and turned into `IPLerror` at the API boundary |
| Numerics | `float` everywhere; SIMD (`float4`/`float8`: SSE, AVX, NEON); FFT via PFFFT (default), FFTS, IPP or vDSP |
| Randomness | `std::default_random_engine` (**implementation-defined**: `mt19937` on MSVC, `minstd_rand0` on libstdc++) and `std::uniform_real_distribution` (algorithm also implementation-defined) → see risk R2 |
| Risky constructs | Class hierarchies with virtual interfaces (`IScene`, `IReflectionSimulator`, `IHRTFMap`…); manual reference counting in the API layer; raw pointers into scene data; SIMD intrinsics |

### Dependencies → Rust

| Upstream | Purpose | Rust replacement |
| --- | --- | --- |
| PFFFT / FFTS / IPP / vDSP | FFT | `realfft` + `rustfft` |
| libmysofa (+ zlib) | SOFA HRTF loading | own crate `sofa` (ADR 0001) + `flate2`/`miniz_oxide` |
| Embree, Radeon Rays, ISPC | Ray tracing | own BVH tracer (the upstream "Default" scene type is the reference) |
| FlatBuffers | Serialisation (scenes, baked data) | `flatbuffers` crate with the same `.fbs` schemas → binary-compatible files |
| OpenCL, TrueAudioNext | GPU convolution and IR processing | **out of scope** |
| IPP, MKL | Vector math and FFT acceleration | **out of scope** (scalar + `std` SIMD) |
| dr_wav, PortAudio, GLFW, ImGui | Samples and tools | `hound` in tests only |
| Catch | Tests | `cargo test` |

## 3. Rust design

- **Crates:**
  - `crates/steam-audio`: all logic. Modules follow upstream boundaries: `math`, `dsp`, `hrtf`, `effects`,
    `scene`, `simulation`, `baking`.
  - `crates/parity`: test support (golden `.npy` loading, `Tolerance`, `assert_close`). Not published.
  - `crates/sofa`: SOFA/HDF5 loader (ADR 0001).
  - `crates/phonon-capi`: C ABI (stage 8).
  - `crates/bevy_steam_audio`: Bevy integration (stage 9). The core stays engine-agnostic: no Bevy
    dependency, no globals, `Send` types, and non-allocating `&mut self` processing.
- **Tools:** `tools/golden`, a C++ program linked against `core/` that writes `.npy` files to `testdata/golden/`.
- **Errors:** `steam_audio::Error` (`thiserror`), mapping 1:1 onto `IPLerror` at the C API.
- **Polymorphism:** a closed set of backends becomes an `enum`. Extension points the user implements
  (custom scene, custom ray tracer callbacks) become traits.
- **Ownership:** the API's reference-counted handles become `Arc<T>` only in `phonon-capi`. Inside the
  library, ordinary ownership is used, with scene geometry held in arenas and referenced by index.
- **SIMD:** start with scalar code that the compiler auto-vectorises. Use `std::arch`/`wide` only in
  measured hot paths.
- **Real time:** effects allocate in `new()` only. `apply()` must not allocate (checked by a test allocator
  in debug builds).

## 4. Slices and status

Order: leaves first. A box is ticked only when the slice passes its parity tests.

| # | Stage / slice | Upstream files | Tests | Ported | Parity | Tolerance |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | Repository cleanup, Cargo workspace, CI | — | ☑ | ☑ | — | — |
| 1a | `parity` crate: `.npy`, `Tolerance`, `assert_close` | — | ☑ | ☑ | — | — |
| 1b | `tools/golden`: reference build + first generator (FFT) | `CMakeLists.txt`, `build/` | ☑ | ☑ | ☑ | — |
| 2.1 | Vector, matrix, quaternion, coordinate space, polar vector, sphere, box (`Aabb`), ray | `vector`, `matrix`, `quaternion`, `coordinate_space`, `polar_vector`, `sphere`, `box`, `ray` | ☑ | ☑ | ☑ | `F32` (bit-exact on Windows). `Triangle`, `Hit` and `Ray::intersect(Mesh)` move to stage 5 with `Mesh`; `DynamicMatrix` moves to 2.3 |
| 2.2 | Math functions, sampling | `math_functions`, `sampling` | ☑ | ☑ | ☑ | `F32` (bit-exact on Windows). `RandomNumberGenerator` moves to stage 6 |
| 2.3 | Array math, `DynamicMatrix` | `array`, `array_math`, `matrix` | ☑ | ☑ | ☑ | `F32` (bit-exact on Windows). `Array<T, N>` → `Vec`/slices |
| 2.4 | Audio buffer (interleave, mix, downmix, ambisonics conversions) | `audio_buffer` | ☐ | ☐ | ☐ | `F32` |
| 2.5 | FFT | `fft`, `pffft_fft` | ☐ | ☐ | ☐ | `peak_relative(1e-5)` |
| 2.6 | Spherical harmonics, ambisonics primitives | `sh`, `sh/spherical_harmonics` | ☐ | ☐ | ☐ | `F32` |
| 2.7 | DSP primitives: bands, IIR, delay, window functions | `bands`, `iir`, `delay`, `window_function` | ☐ | ☐ | ☐ | IIR: `peak_relative(1e-4)` |
| 3.1 | Built-in HRTF (`cipic_124`), HRTF map, HRTF database | `hrtf_map`, `hrtf_database`, `cipic_124.inl` | ☐ | ☐ | ☐ | `peak_relative(1e-5)` |
| 3.2 | Convolution (overlap-add / overlap-save) | `overlap_add_convolution_effect`, `overlap_save_convolution_effect` | ☐ | ☐ | ☐ | `peak_relative(1e-5)` |
| 3.3 | Panning, binaural, virtual surround | `panning_effect`, `binaural_effect`, `virtual_surround_effect` | ☐ | ☐ | ☐ | `peak_relative(1e-5)` |
| 3.4 | Ambisonics effects (encode, decode, rotate, panning, binaural) | `ambisonics_*_effect` | ☐ | ☐ | ☐ | `peak_relative(1e-5)` |
| 3.5 | `sofa` crate (ADR 0001) | `sofa_hrtf_map` + libmysofa | ☐ | ☐ | ☐ | see ADR |
| 4 | Direct sound: distance attenuation, air absorption, directivity, direct effect, gain/EQ | `distance_attenuation`, `air_absorption`, `directivity`, `direct_effect`, `gain_effect`, `eq_effect` | ☐ | ☐ | ☐ | `F32` / `peak_relative(1e-5)` |
| 5 | Scene and geometry: mesh, static/instanced mesh, BVH, scene, materials, OBJ loading for tests | `mesh`, `static_mesh`, `instanced_mesh`, `bvh`, `scene`, `material`, `hit` | ☐ | ☐ | ☐ | hits: exact triangle index, `F32` distance |
| 5.1 | Direct simulator (occlusion, transmission) | `direct_simulator` | ☐ | ☐ | ☐ | R2 |
| 6 | Reflections: energy field, IR, reflection simulator, reconstructor, reverb, hybrid reverb, job graph, baking, probes, serialisation | `energy_field*`, `impulse_response*`, `reflection_*`, `reconstructor*`, `*reverb*`, `job*`, `thread_pool`, `probe_*`, `baked_*`, `serialized_object` | ☐ | ☐ | ☐ | R2 |
| 7 | Pathing | `probe_tree`, `path_*` | ☐ | ☐ | ☐ | R2 |
| 8 | C API (`phonon-capi`) | `api_*`, `phonon.h` | ☐ | ☐ | ☐ | upstream `itest` builds unchanged |
| 9 | Bevy integration (`bevy_steam_audio`); an MVP is possible after stage 4 | — | ☐ | ☐ | — | — |

## 5. Verification plan

- **Golden data:** `tools/golden` runs each module on fixed inputs (`core/data/audio/*.wav`,
  `core/data/hrtf/*.sofa`, `core/data/meshes/*.obj`, plus synthetic inputs: impulses, sine waves, seeded
  noise) and writes `testdata/golden/<module>/<case>.npy`. Golden files are committed. Anything over ~1 MB
  per case is regenerated locally and not committed (Git LFS is decided when the first one appears).
- **Comparison:** `parity::assert_close(actual, expected, tol)`, which passes when
  `|a − e| ≤ abs + rel·|e|`. Built-in presets:
  - `Tolerance::F32` (abs 1e-6, rel 1e-5): closed-form scalar math.
  - `Tolerance::peak_relative(expected, r)`: signals, with error measured against the signal peak.
  - `Tolerance::EXACT`: integers, indices, data movement.

  Every tolerance looser than these needs a written reason in the table in §4.
- **Upstream tests:** each `core/src/test/*.test.cpp` is ported along with its module.
- **Extra:** `proptest` for math invariants; `cargo-fuzz` for the parsers (SOFA/HDF5, OBJ, serialised
  objects); `criterion` benchmarks against `core/src/benchmark` for the hot paths (convolution, binaural,
  reflections).

### Name mapping (upstream → Rust)

| Upstream | Rust |
| --- | --- |
| `Vector2f/3f/4f`, `Vector3f::kZero`, `kXAxis` | `math::Vector2/3/4`, `Vector3::ZERO`, `X_AXIS` |
| `Vector3f::unitVector(v)` | `v.normalize_or_zero()` |
| `Vector::reciprocal(v)` | `v.recip()` |
| `operator<`, `<=`, `>`, `>=` on vectors (element-wise "all") | `all_lt`, `all_le`, `all_gt`, `all_ge` |
| `Matrix<T,R,C>`, `Matrix4x4f`, `m(i, j)` | `math::Matrix<R,C>`, `Matrix4`, `m[(i, j)]` |
| `transposedCopy()` | `transpose()` |
| `determinant(m)`, `inverse(m, out)` | `m.determinant()`, `m.inverse_or_zero()` |
| `CoordinateSpace3f(ahead, origin)` / `(ahead, up, origin)` | `CoordinateSpace3::from_ahead` / `from_ahead_up` |
| `transformDirectionFromWorldToLocal` | `direction_world_to_local` |
| `Box`, `Box::coordinates(i)` | `geometry::Aabb`, `Aabb::bound(i)` |
| `computeBoundingSphere(a, b)` | `Sphere::bounding(&a, &b)` |
| `Ray::intersect(box, …)`, `intersect(sphere)` | `intersect_aabb`, `intersect_sphere`, `intersect_triangle(v0, v1, v2)` |
| `Math::kPi` (3.141592f), `kDegreesToRadians` (0.017452f) | `math::PI`, `math::DEGREES_TO_RADIANS` (**not** the `std` constants) |
| `Math::nextpow2`, `factorial`, `doubleFactorial`, `clamp` | `math::next_pow2`, `factorial`, `double_factorial`, `clamp` |
| `Array<T, 1>`, `Array<T, 2>` | `Vec<T>` / slices (2-D layouts are introduced where needed) |
| `ArrayMath::multiplyAccumulate(size, a, b, accum)` | `array_math::multiply_accumulate(a, b, accum)`; complex variants have a `_complex` suffix |
| `DynamicMatrixf`, `multiplyMatrices(a, b, c)` | `math::DynamicMatrix`, `DynamicMatrix::multiply(&a, &b, &mut c)` |
| `Sampling::generateSphereSamples(n, out)` | `sampling::sphere_samples(&mut out)` |
| `complex_t` | `steam_audio::Complex` (`num_complex::Complex<f32>`) |

## 6. Deviations

Tracked in [`DEVIATIONS.md`](DEVIATIONS.md).

## 7. Risks and open questions

- **R1. FFT differences.** PFFFT and `realfft` round differently, which affects everything built on
  convolution. Mitigation: peak-relative tolerances. If needed, generate goldens with an FFTW-grade
  reference to tell which side is more accurate.
- **R2. Randomness.** Three sources, each handled differently:
  - `Reconstructor` white noise: an unseeded `std::default_random_engine` (`mt19937`, seed 5489, on MSVC) with
    `uniform_real_distribution<float>(-1, 1)`. This is deterministic on MSVC, so port MSVC's exact algorithms and
    compare against goldens.
  - `ReverbEffect` delay lengths: `rand() % 101`, MSVC's process-global LCG. Port the LCG; the global call order
    has to be modelled in tests.
  - Reflection and pathing ray directions: `RandomNumberGenerator` is seeded with `time(NULL)`, so **upstream
    itself is not reproducible**. Use statistical checks (band energies within ±0.5 dB, reverb times within 5 %)
    and seeded determinism on the Rust side.
- **R3. SOFA/HDF5:** see ADR 0001.
- **R4. FlatBuffers compatibility** of baked data. It must be readable by both implementations, which is
  verified by cross-loading files.
- **R6. Shipped binaries use Intel IPP** for `ArrayMath` and FFT on x86/x64. Our reference build uses upstream's
  generic path (IPP is not redistributable), so parity is against the generic path. IPP results may differ in the
  last bits.
- **R5. Reference build dependencies.** `core/build/get_dependencies.py` downloads ~15 third-party
  libraries. `tools/golden` uses a minimal configuration (PFFFT, mysofa, zlib, flatbuffers; no Embree,
  IPP, OpenCL, Radeon Rays or TrueAudioNext).
