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
| libmysofa (+ zlib) | SOFA HRTF loading | own crate `sofa` (ADR 0001) + `miniz_oxide` |
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
| 2.4 | Audio buffer (interleave, mix, downmix, ambisonics conversions) | `audio_buffer` | ☑ | ☑ | ☑ | `F32` (bit-exact on Windows). `SpeakerLayout` moves to 3.3 with panning |
| 2.5 | FFT | `fft`, `pffft_fft` | ☑ | ☑ | ☑ | `peak_relative(1e-5)`; measured worst error 5.4e-7 × peak (not bit-exact, R1) |
| 2.6 | Spherical harmonics, ambisonics primitives | `sh`, `sh/spherical_harmonics` | ☑ | ☑ | ☑ | `F32` (bit-exact on Windows, debug and release; bands 0–6, rotations up to order 5). `ProjectFunction` not ported (random, unused) |
| 2.7 | DSP primitives: bands, IIR, delay, window functions | `bands`, `iir`, `float8_iir`, `delay`, `float8_delay`, `window_function` | ☑ | ☑ | ☑ | Designs, spectra, allpass, windows: `F32`; delay: `EXACT`; filtering: `peak_relative(1e-5)`. Bit-exact on Windows (debug and release), filtering against upstream's AVX path (R8); the 4-sample path differs by ≤ 3.0e-6 × peak. `IIR8` and the order-switching wrappers not ported (dead code) |
| 3.1 | Built-in HRTF (`cipic_124`), HRTF map, HRTF database | `hrtf_map`, `hrtf_database`, `cipic_124.inl` | ☑ | ☑ | ☑ | Map (loading, nearest, bilinear weights), loudness, peak delays: `EXACT`/`F32`, bit-exact on Windows; bilinear weights use abs `2e-5` because they divide angles up to ~230° by a 5° grid, amplifying libm `asin`/`atan2` ulp differences on Linux/macOS (measured 4.8e-6). Stored and Ambisonics HRTFs: `peak_relative(1e-5)` (measured ≤ 2.2e-7 × peak; ≤ 1.0e-6 when projected from version-2 data). Synthesized HRTFs (interpolation, spatial blend): `peak_relative(5e-5)`, measured worst 2.03e-5 × peak, because the phase is interpolated unwrapped and accumulated in `f32` from FFT phases that already differ (R1, R10). `HRTFMapFactory` and the SOFA map move to 3.5 |
| 3.2 | Convolution (overlap-add / overlap-save) | `overlap_add_convolution_effect`, `overlap_save_convolution_effect` | ☑ | ☑ | ☑ | `peak_relative(1e-5)` for audio and partitioned IR spectra (measured worst 2.5e-7 × peak, debug and release; not bit-exact because of the FFT, R1); tail states and counters `EXACT`. Overlap-save goldens are mono (R12); multi-channel effects and mixers are checked bit-exactly against mono ones. `TripleBuffer` moves to stage 6 (a new IR is passed as `Option<&mut OverlapSaveFir>`); the partitioner takes an `AudioBuffer` until `ImpulseResponse` lands in stage 6 |
| 3.3 | Speaker layouts, panning, binaural, virtual surround | `speaker_layout`, `panning_effect`, `binaural_effect`, `virtual_surround_effect` | ☑ | ☑ | ☑ | Panning weights `F32`, panning audio `peak_relative(1e-5)`: bit-exact on Windows. Binaural and virtual surround with stored HRTFs: `peak_relative(1e-5)` (measured worst 2.5e-7 × peak); with synthesized HRTFs (bilinear, spatial blend of a stereo input): `peak_relative(5e-5)` as in 3.1 (R10; measured worst 4.3e-6 × peak). Not bit-exact because of the FFT (R1); debug and release identical. States, tail counters and peak delays `EXACT`. Upstream quirks kept: R14. `secondOrderPanningWeight` not ported (dead code) |
| 3.4 | Ambisonics effects (encode, decode, rotate, panning, binaural) | `ambisonics_encode_effect`, `ambisonics_rotate_effect`, `ambisonics_panning_effect`, `ambisonics_binaural_effect`, `ambisonics_decode_effect` | ☑ | ☑ | ☑ | Encode and rotate: `F32`; panning and decode audio: `peak_relative(1e-5)`. Encode, rotate, panning and panned decoding are bit-exact on Windows (debug and release). Binaural (alone and in decode): `peak_relative(1e-5)`, measured worst 2.5e-7 × peak, not bit-exact because of the FFT (R1). States and tail counters `EXACT`. Outputs are prefilled with a sentinel, so channels the effects leave untouched are checked too. The virtual speaker table moved from `hrtf` to `AmbisonicsPanningEffect::VIRTUAL_SPEAKERS`. Upstream quirks kept: R15 |
| 3.5a | `sofa` crate: HDF5 subset, SOFA loader and check (ADR 0001) | libmysofa 1.3.3 `src/hdf/*` (except `btreeRead`), `hrtf/reader.c`, `check.c`, `tools.c` (attributes) | ☑ | ☑ | ☑ | Loader results compared as a blob of status codes, dimensions, attributes and per-array FNV-1a hashes of the `f32` values: `EXACT`. Bit-exact on the three files in `core/data/hrtf` (plus source positions and sampled HRIRs as arrays), on libmysofa's 43 test files (including its fuzzing regressions; local only, the files are not in the repository) and on 256 Lcg-chosen corruptions of `cipic_124.sofa` (2000 checked once locally). Inflate uses `miniz_oxide`'s core with zlib's single-call semantics. Malformed input never panics; a cargo-fuzz target exists (`crates/sofa/fuzz`). Upstream quirks kept: R16 |
| 3.5b | `sofa` crate: Speex resampler, `mysofa_tocartesian`, k-d tree lookup, neighbourhood, `c2s`/`s2c` (the rest of `mysofa_open_no_norm`) | libmysofa `resample.c`, `speex_resampler.c`, `spherical.c`, `tools.c`, `kdtree.c`, `lookup.c`, `neighbors.c`, `easy.c` | ☑ | ☑ | ☑ | Coordinates (`c2s`, `s2c`, Cartesian source positions, lookup ranges, normalised queries) and delays: `F32`; resampled HRIRs: `peak_relative(1e-5)` per response (ADR 0001); lookup and neighbourhood indices, status codes and sizes: `EXACT` (on Linux and macOS an index may instead be another measurement exactly as far away: a tie flipped by libm rounding, R17). Bit-exact on Windows (debug and release): the three shipped files resampled to 8–96 kHz (direct and interpolated Speex filters, up- and downsampling), corrupt sampling rates, lookups of 600 Lcg queries per file (measured points, midpoints, scaled points, the zero vector), every neighbourhood, and libmysofa's 43 test files through `mysofa_open_data_no_norm` (local only). Corrupted files open or fail through `Easy` without panicking (3000 Lcg corruptions checked once locally, 64 in CI); the fuzz target now runs the whole of `Easy`. `radius` calls the C library's `powf` as upstream does (MSVC's `powf(x, 2)` is not always `x * x`). The k-d tree search uses an explicit stack. Upstream quirks kept: R17 |
| 3.5c | SOFA HRTF map and `HRTFMapFactory` (moved from 3.1) | `sofa_hrtf_map`, `hrtf_map_factory` | ☑ | ☑ | ☑ | Interpolation weights abs `1e-4` (as 3.1, but looser: angles from `c2s` in degrees, with an ulp of 3.1e-5° near 360°, are divided by the grid step, 1° in `sadie_d1`, which amplifies libm ulp differences off Windows; measured 4.4e-5 on Linux, bit-exact on Windows); nearest and interpolation indices, patched neighbourhoods, sizes: `EXACT`; HRIRs: `peak_relative(1e-5)` per response (ADR 0001). Bit-exact on Windows (debug and release) for the three shipped files at 48 kHz: HRIRs, patched neighbourhoods (including Lcg-chosen measurements whose azimuth neighbours are removed and patched again, since the shipped files only lack them at the poles), and 271 queries per file (axes, the zero vector, measured positions, positions moved by up to 0.05, random directions). On Linux and macOS a lookup tie may flip as in 3.5b (R17); the tests accept a differing query only if its nearest index is a tie, a measurement involved has a flipped neighbour, or the query lies on the nearest measurement's azimuth or elevation (within 1e-3°), where the side to interpolate towards depends on the sign of a difference of about an ulp (macOS, `sadie_d1`, straight behind). HRTF database and binaural effect with a SOFA HRTF, as in 3.1 and 3.3: stored HRTFs `peak_relative(1e-5)` (measured worst 2.7e-7 × peak); projected Ambisonics HRTFs, interpolated HRTFs and bilinear binaural `peak_relative(5e-5)` (R10, plus weights that differ by libm ulps off Windows; measured worst 1.18e-5 × peak on Windows, projected Ambisonics 1.24e-5 × peak on Linux), nearest binaural `peak_relative(1e-5)` (1.5e-7 × peak); peaks and states `EXACT`. Not bit-exact because of the FFT (R1). The `sofa` cargo feature (default) gates SOFA support, like upstream's `IPL_DISABLE_SOFA`. `HrtfDatabase::nearest_index` now takes `&mut self` (the SOFA lookup uses scratch space). Upstream quirks kept: R18 |
| 4 | Direct sound: distance attenuation, air absorption, directivity, direct effect, gain/EQ | `distance_attenuation`, `air_absorption`, `directivity`, `direct_effect`, `gain_effect`, `eq_effect`, `propagation_medium` | ☑ | ☑ | ☑ | Models (evaluation, correction curves, directivity in random coordinate spaces, `normalizeGains`): `F32`; `isDefault` and `==`: `EXACT`; gain, EQ and direct effect audio: `peak_relative(1e-5)`. Bit-exact on Windows (debug and release), including infinite and negative-zero attenuations, the NaN cases of `normalizeGains`, all 32 flag combinations with both transmission types, gain changes (EQ crossfades), unchanged gains, `reset`, `tailApply` and tails; the EQ filters follow upstream's AVX path (R8). Every effect run is checked out of place and in place against the same goldens; the generator also checks that upstream's `DirectEffect` gives the same result in place (R7). Callbacks are `Arc<dyn Fn>` and compare by identity. `DistanceAttenuationModel::dirty` moves to stage 6 with `SimulationManager`; `DirectSoundPath` lives in `effects::direct` until the direct simulator (5.1) produces it. Upstream quirks kept: R19 |
| 5 | Scene and geometry: mesh, static/instanced mesh, BVH, scene, materials, OBJ loading for tests | `mesh`, `static_mesh`, `instanced_mesh`, `bvh`, `scene`, `custom_scene`, `material`, `hit`, `triangle` | ☑ | ☑ | ☑ | BVH nodes, triangle and material indices, any-hit, occlusion and box results, versions, `dumpObj` bytes: `EXACT`; normals, hit distances and materials: `F32`. Bit-exact on Windows (debug and release) for the six meshes in `core/data/meshes` (Sponza: 262 267 triangles, its 524 533 nodes checked by hash; about 20 s in a debug build) and a scene with a static mesh and three instances traced through seven stages of changes. The builder sorts with a port of MSVC's `std::sort`, so equal centroids end up in upstream's order and the trees are identical (R21). Meshes are loaded with a port of `RayTracerCompare.test.cpp`'s OBJ loader. Serialization moves to stage 6; `CustomScene` is the `RayTracer` trait. Upstream quirks kept: R20 |
| 5.1 | Direct simulator (occlusion, transmission) | `direct_simulator` | ☑ | ☑ | ☑ | `F32` for every field of the direct path. Bit-exact on Windows (debug and release) for two scenes (`boxroom` and `simplescene` with `Lcg` materials, each with an instanced `smallbox`): 256 source/listener pairs per scene (200 random, 56 on opposite sides of the box), each simulated with raycast occlusion and 1–16 transmission rays, volumetric occlusion from 64- and 256-point simulators (asking for 1–256 points, so the request is clamped), a 1-point simulator (which keeps no points) and all 64 flag combinations, every 7th without a scene. The paths cross 0–9 surfaces. No randomness is involved (R2 does not apply): the volumetric points are the deterministic Halton samples of 2.2. Off Windows those points come from libm (`acos`, `cbrt`, `sin`, `cos`), so a ray grazing an edge could flip a sample; none does in these cases. `DirectSoundPath` moved from `effects::direct` to `simulation` (still re-exported). The OBJ loader and the scene's `Lcg` draws are shared with stage 5 (`tools/golden/src/scene_inputs.h`, `tests/common`). Upstream quirks kept: R22 |
| 6 | Reflections: energy field, IR, reflection simulator, reconstructor, reverb, hybrid reverb, job graph, baking, probes, serialisation (including mesh, static mesh and scene serialization, moved from 5) | `energy_field*`, `impulse_response*`, `reflection_*`, `reconstructor*`, `*reverb*`, `job*`, `thread_pool`, `probe_*`, `baked_*`, `serialized_object` | ☐ | ☐ | ☐ | R2 |
| 7 | Pathing | `probe_tree`, `path_*` | ☐ | ☐ | ☐ | R2 |
| 8 | C API (`phonon-capi`) | `api_*`, `phonon.h` | ☐ | ☐ | ☐ | upstream `itest` builds unchanged. Audio buffer bridging and precondition checks: R7 |
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
- **Extra:** `proptest` for math invariants; `cargo-fuzz` for the parsers (SOFA/HDF5 in `crates/sofa/fuzz`, OBJ,
  serialised objects); `criterion` benchmarks against `core/src/benchmark` for the hot paths (convolution, binaural,
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
| `AudioBuffer(channels, samples)`, `buf[i]` | `audio_buffer::AudioBuffer::new(channels, samples)`, `buf[i]` / `channel(i)` |
| `AudioBuffer(channels, samples, data)`, `AudioBuffer(other, channel)` | `AudioBufferRef`/`AudioBufferMut::from_planar`, `channel_view(i)` / `channel_view_mut(i)` |
| `AudioBuffer::read(out)`, `write(in)` | `interleave(&mut out)`, `deinterleave(&in)` |
| `AudioBuffer::mix`, `downmix`, `convertAmbisonics` | `audio_buffer::mix`, `downmix`, `convert_ambisonics` |
| `AmbisonicsType::N3D`, `SN3D`, `FuMa` | `AmbisonicsType::N3d`, `Sn3d`, `FuMa` |
| `FFT(n)` / `FFT(n, FFTDomain::Real)`, `numRealSamples`, `numComplexSamples` | `fft::RealFft::new(n)`, `num_real_samples()`, `num_complex_samples()` |
| `FFT(n, FFTDomain::Complex)` | `fft::ComplexFft::new(n)`, `num_samples()` |
| `FFT::applyForward`, `applyInverse` (`const`) | `forward`, `inverse` (`&mut self`: the transform owns its scratch) |
| `SphericalHarmonics::numCoeffsForOrder`, `legendre`, `evaluate`, `evaluateSum` | `sh::num_coeffs_for_order`, `legendre`, `evaluate`, `evaluate_sum` |
| `SphericalHarmonics::projectSinglePoint`, `projectSinglePointAndUpdate` | `sh::project_single_point`, `project_single_point_and_update` |
| `SphericalHarmonics::convertedDirection`, `convertedQuaternion`, `convertedCoordinateSpace` | `sh::converted_direction`, `converted_quaternion`, `converted_coordinate_space` |
| `SHRotation`, `setRotation(CoordinateSpace3f)`, `setRotation(Quaternionf)`, `apply(order, in, out)` | `sh::ShRotation`, `set_rotation(&space)`, `set_rotation_quaternion(q)`, `apply(order, in, out)` |
| `sh::GetIndex`, `GetCoefficientCount`, `ToVector`, `ToSphericalCoords` | `sh::google::index`, `coefficient_count`, `to_vector` (`[f64; 3]`), `to_spherical_coords` (→ `(phi, theta)`) |
| `sh::EvalSH(l, m, dir)`, `EvalSH(l, m, phi, theta)`, `EvalSHSlow`, `EvalSHSum<float>` | `sh::google::eval_sh`, `eval_sh_spherical`, `eval_sh_slow`, `eval_sh_sum` / `eval_sh_sum_spherical` |
| `sh::Rotation`, `SetRotation(...)`, `band_rotation(l)` | `sh::google::Rotation`, `set_rotation` / `set_rotation_quaternion` / `set_rotation_matrix`, `band_rotation(l)` |
| `Bands::kNumBands`, `kLowCutoffFrequencies`, `kHighCutoffFrequencies`, `kBandPassNorm` | `bands::NUM_BANDS`, `LOW_CUTOFF_FREQUENCIES`, `HIGH_CUTOFF_FREQUENCIES`, `BAND_PASS_NORM` |
| `IIR2`, `IIR` (always the biquad) | `iir::Iir` (fields `a1, a2, b0, b1, b2`) |
| `IIR2::lowPass(f, sr)` / `lowPass(f, Q, sr)`; same for `highPass`, `lowShelf`, `highShelf` | `Iir::low_pass` / `low_pass_q`, `high_pass(_q)`, `low_shelf(_q)`, `high_shelf(_q)` |
| `IIR2::bandPass`, `peaking`, `spectrum`, `bandFilters(filters, sr)` | `Iir::band_pass`, `peaking`, `spectrum`, `Iir::band_filters(sr) -> [Iir; NUM_BANDS]` |
| `IIR2Filterer`, `IIRFilterer` | `iir::IirFilterer` (`new(filter)`, `set_filter`, `reset`, `copy_state`) |
| `IIRFilterer::apply(float)`, `apply(size, in, out)` (also in place) | `apply_sample(x)`, `apply(&in, &mut out)`, `apply_in_place(&mut buf)` |
| `Delay(delay, frameSize)`, `get(n, out)`, `put(n, in)` | `delay::Delay::new(delay, frame_size)`, `get(&mut out)`, `put(&in)` |
| `Allpass::resize(delay, gain, frameSize)`, `apply(float)`, `apply(float4_t)` / `apply(float8_t)` | `delay::Allpass::new` / `resize`, `apply(x)`, `apply_block::<4>` / `apply_block::<8>` |
| `WindowFunction::hann(size, w)` etc., `tukey(size, overlap, w)` | `window::hann(&mut w)` etc., `window::tukey(size, overlap, &mut w)` |
| `gDefaultHrtfData` (`cipic_124.inl`) | `hrtf::BUILTIN_HRTF_DATA` (converted from the `.inl` by `build.rs`) |
| `IHRTFMap::kNumEars`, `kMaxAmbisonicsOrder` | `hrtf::NUM_EARS`, `MAX_AMBISONICS_ORDER` |
| `HRTFMap(samplingRate)` / `HRTFMap(samplingRate, hrtfData)` | `hrtf::HrtfMap::new(rate)` / `from_bytes(data, rate)` → `Result` |
| `hrtfData()`, `ambisonicsData()` | `hrirs()` (flat) / `hrir(ear, index)`, `ambisonics_hrirs()` (empty for version 2) |
| `nearestHRIR(dir)`, `interpolatedHRIRWeights(dir, indices, weights)` | `nearest_hrir(dir)`, `interpolated_hrir_weights(dir) -> HrirWeights` (`-1` → `None`) |
| `HRTFSettings` (`type`, `hrtfData`, `sofaFileName`, `sofaData`/`sofaDataSize`), `HRTFNormType::None`/`RMS` | `hrtf::HrtfSettings { source, volume, normalization }` with `HrtfSource::BuiltIn`, `Data(&[u8])`, `SofaFile(&Path)`, `SofaData(&[u8])`; `HrtfNormalization::None`/`Rms` |
| `IHRTFMap`, `HRTFMapFactory::create(settings, samplingRate)` | `hrtf::AnyHrtfMap` (enum: `Default(HrtfMap)`, `Sofa(SofaHrtfMap)`), `AnyHrtfMap::new(&settings, rate)` → `Result`; `HrtfDatabase::map()` returns it |
| `SOFAHRTFMap(settings, samplingRate)` | `hrtf::SofaHrtfMap::from_file(path, rate)` / `from_bytes(&data, rate)` → `Result` (`Error::Sofa`, `Error::UnsupportedSofaDelay`) |
| `SOFAHRTFMap::nearestHRIR`, `interpolatedHRIRWeights` | `nearest_hrir(&mut self, dir)`, `interpolated_hrir_weights(&mut self, dir)` (every slot holds an index) |
| `SOFAHRTFMap::patchSOFANeighborhood`, the patched `mSOFA->neighborhood` | done in the constructor; `SofaHrtfMap::neighbors(i)` |
| `IPL_DISABLE_SOFA` | `steam-audio` without its `sofa` cargo feature (on by default) |
| `HRTFPhaseType::None`, `SphereITD`, `Full` | `HrtfPhaseType::None`, `SphereItd`, `Full` |
| `HRTFDatabase(settings, samplingRate, frameSize)` | `HrtfDatabase::new(&settings, rate, frame_size)` → `Result` |
| `getHRTFByIndex(i, hrtf)`, `ambisonicsHRTF(i, hrtf)` | `hrtf(i)`, `ambisonics_hrtf(i)` → `[&[Complex]; 2]` |
| `nearestHRTF(dir, hrtf, blend, phase, hrtfWithBlend, peakDelays)` | `nearest_index(dir)` + `hrtf(i)` + `peak_delays(i)`; the `blend < 1` branch is `blended_hrtf(i, dir, blend, phase, out)` |
| `interpolatedHRTF(dir, hrtf, blend, phase, peakDelays)` | `interpolated_hrtf(dir, blend, phase, out) -> [usize; 2]` (peak delays) |
| `Loudness::gainToDB`, `dBToGain`, `calculateRMSLoudness`, `calculateGainScaling` | `hrtf::loudness::gain_to_db`, `db_to_gain`, `rms_loudness`, `gain_scaling` |
| `OverlapAddConvolutionEffect(audioSettings, {numChannels, irSize})` | `effects::OverlapAddConvolutionEffect::new(&audio_settings, &OverlapAddConvolutionSettings { .. })` |
| `OverlapAddConvolutionEffect::apply({fftIR, multipleInputs}, in, out)`, `tail(out)` | `apply(fft_ir: &[&[Complex]], multiple_inputs, &in, &mut out)`, `tail(&mut out)` |
| `wetAudioSize`, `irSpectrumSize`, `numTailSamplesRemaining` | `wet_audio_size`, `ir_spectrum_size`, `num_tail_samples_remaining` |
| `OverlapSaveFIR(numChannels, irSize, frameSize)`, `fir[i][j]`, `copy(src, dst)`, `swap(a, b)` | `effects::OverlapSaveFir::new(..)`, `block(i, j)` / `block_mut(i, j)`, `dst.copy_from(&src)`, `std::mem::swap` |
| `OverlapSavePartitioner(frameSize)`, `partition(ir, numChannels, numSamples, fftIR)` | `OverlapSavePartitioner::new(frame_size)`, `partition(&ir_buffer, num_channels, num_samples, &mut fir)` |
| `OverlapSaveConvolutionEffect::numBlocks(frameSize, irSize)` | `OverlapSaveConvolutionEffect::num_blocks(frame_size, ir_size)` |
| `OverlapSaveConvolutionEffect::apply(params, in, out)` (`TripleBuffer` or direct params) | `apply(new_ir: Option<&mut OverlapSaveFir>, num_channels, &in, &mut out)`; `Some` when the IR was updated |
| `apply(params, in, mixer)`, `tail(mixer)` | `apply_to_mixer(new_ir, num_channels, &in, &mut mixer)`, `tail_to_mixer(&mut mixer)` |
| `OverlapSaveConvolutionMixer(audioSettings, settings)`, `apply({numChannels}, out)` | `OverlapSaveConvolutionMixer::new(..)`, `apply(num_channels, &mut out)` |
| `SpeakerLayout(type)`, `SpeakerLayout(n, speakers)`, `numSpeakersForLayout`, `speakersForLayout` | `speaker_layout::SpeakerLayout::new(ty)` (panics for `Custom`), `custom(vec)`, `num_speakers_for_layout`, `speakers_for_layout` |
| `SpeakerLayoutType::FivePointOne`, `SevenPointOne` | `SpeakerLayoutType::FivePointOne`, `SevenPointOne` |
| `PanningEffect({speakerLayout})`, `apply({direction}, in, out)`, `panningWeight(dir, layout, i)` | `effects::PanningEffect::new(&layout)`, `apply(direction, &in, &mut out)`, `PanningEffect::panning_weight(dir, &layout, i)` |
| `HRTFInterpolation::NearestNeighbor`, `Bilinear` | `hrtf::HrtfInterpolation::NearestNeighbor`, `Bilinear` |
| `BinauralEffect(audioSettings, {hrtf})`, `apply({direction, interpolation, spatialBlend, phaseType, hrtf, peakDelays}, in, out)` | `effects::BinauralEffect::new(&audio_settings, &hrtf)`, `apply(BinauralParams { .. }, &in, &mut out)` (`hrtf: &mut HrtfDatabase`, `peak_delays: Option<&mut [f32; 2]>`) |
| `VirtualSurroundEffect(audioSettings, {speakerLayout, hrtf})`, `apply({hrtf}, in, out)` | `effects::VirtualSurroundEffect::new(&audio_settings, &layout, &hrtf)`, `apply(&mut hrtf, &in, &mut out)` |
| `AmbisonicsEncodeEffect({maxOrder})`, `apply({direction, order}, in, out)` | `effects::AmbisonicsEncodeEffect::new(max_order)`, `apply(direction, order, &in, &mut out)` |
| `AmbisonicsRotateEffect(audioSettings, {maxOrder})`, `apply({orientation, order}, in, out)` | `effects::AmbisonicsRotateEffect::new(&audio_settings, max_order)`, `apply(&orientation, order, &in, &mut out)` |
| `AmbisonicsPanningEffect(audioSettings, {speakerLayout, maxOrder})`, `apply({order}, in, out)` | `effects::AmbisonicsPanningEffect::new(&audio_settings, &layout, max_order)`, `apply(order, &in, &mut out)` |
| `AmbisonicsPanningEffect::kNumVirtualSpeakers`, `kVirtualSpeakers` | `AmbisonicsPanningEffect::NUM_VIRTUAL_SPEAKERS`, `VIRTUAL_SPEAKERS` |
| `AmbisonicsBinauralEffect(audioSettings, {maxOrder, hrtf})`, `apply({hrtf, order}, in, out)` | `effects::AmbisonicsBinauralEffect::new(&audio_settings, max_order, &hrtf)`, `apply(&hrtf, order, &in, &mut out)` |
| `AmbisonicsDecodeEffect(audioSettings, {speakerLayout, maxOrder, hrtf})`, `apply({order, orientation, binaural, hrtf}, in, out)` | `effects::AmbisonicsDecodeEffect::new(&audio_settings, &layout, max_order, Option<&HrtfDatabase>)`, `apply(AmbisonicsDecodeParams { .. }, &in, &mut out)` |
| libmysofa `mysofa_load_data(data, size, &err)` / `mysofa_load(filename, &err)` | `sofa::Hrtf::load(&data)` / `Hrtf::load_file(path)` → `Result<Hrtf, sofa::Error>` |
| `mysofa_check(hrtf)` | `hrtf.check()` |
| `mysofa_getAttribute(hrtf->attributes, name)` | `hrtf.attribute(name)`, `Array::attribute(name)` |
| `MYSOFA_HRTF` (`I`, `C`, `R`, `E`, `N`, `M`, `DataIR`, `SourcePosition`, …, `attributes`, `variables`) | `sofa::Hrtf` (`i`, `c`, `r`, `e`, `n`, `m`, `data_ir`, `source_position`, …, `attributes`, `variables`) |
| `MYSOFA_ARRAY` (`values` may be NULL), `MYSOFA_ATTRIBUTE`, `MYSOFA_VARIABLE` | `sofa::Array` (`values: Option<Vec<f32>>`), `Attribute` (bytes), `Variable`; lists in file order |
| `MYSOFA_*` status codes, `errno` | `sofa::Error` variants, `Error::code()` returns the same numbers |
| The status `mysofa_load` returns alongside a usable HRTF | `Hrtf::load_error` |
| `mysofa_resample(hrtf, rate)` | `hrtf.resample(rate)` |
| `mysofa_tocartesian(hrtf)` | `hrtf.to_cartesian()` |
| `mysofa_c2s(v)`, `mysofa_s2c(v)` (in place) | `sofa::c2s(v)`, `sofa::s2c(v)` (return the converted `[f32; 3]`) |
| `mysofa_lookup_init(hrtf)` (`NULL` unless Cartesian), `mysofa_lookup(lookup, c)` (`-1` without measurements) | `Lookup::new(&hrtf)` → `Result`, `lookup.lookup(&mut c)` or `lookup_with(&mut c, &mut scratch)` → `Option<usize>` |
| `MYSOFA_LOOKUP` (`radius_min`, …, `phi_max`) | `sofa::Lookup` (fields of the same names) |
| `mysofa_neighborhood_init`, `mysofa_neighborhood_init_withstepdefine`, `mysofa_neighborhood(nb, i)` (`int[6]`, `-1` for none) | `Neighborhood::new`, `Neighborhood::with_steps` → `Result`, `get(i)` → `Option<&[Option<usize>; 6]>` |
| `MYSOFA_DEFAULT_NEIGH_STEP_ANGLE`, `MYSOFA_DEFAULT_NEIGH_STEP_RADIUS` | `sofa::DEFAULT_ANGLE_STEP`, `DEFAULT_RADIUS_STEP` |
| `mysofa_open_no_norm(file, rate, &filterLength, &err)`, `mysofa_open_data_no_norm(data, size, …)` | `Easy::load_file_no_norm(path, rate)`, `Easy::load_no_norm(&data, rate)` → `Result<Easy, sofa::Error>`; `easy.filter_length()` |
| `MYSOFA_EASY` (`hrtf`, `lookup`, `neighborhood`, `fir`) | `sofa::Easy` (`hrtf`, `lookup`, `neighborhood`; no filter buffer) |
| Speex `RESAMPLER_ERR_*` codes returned by `mysofa_resample` | `Error::Resampler(code)` |
| `PropagationMedium::kSpeedOfSound` | `propagation_medium::SPEED_OF_SOUND` |
| `DistanceAttenuationModel(minDistance, callback, userData)`, `DistanceAttenuationCallback` | `distance_attenuation::DistanceAttenuationModel::InverseDistance { min_distance }` / `Callback(Arc<dyn Fn(f32) -> f32>)` (`Default`: inverse distance from 1 m) |
| `DistanceAttenuationModel::evaluate`, `isDefault`, `generateCorrectionCurve(from, to, rate, n, curve)` | `evaluate`, `is_default`, `DistanceAttenuationModel::correction_curve(&from, &to, rate, &mut curve)` |
| `AirAbsorptionModel(coefficients, callback, userData)`, `kDefaultCoefficients` | `air_absorption::AirAbsorptionModel::Exponential { coefficients }` / `Callback(Arc<dyn Fn(f32, usize) -> f32>)`, `DEFAULT_COEFFICIENTS` |
| `Directivity(dipoleWeight, dipolePower, callback, userData)`, `evaluateAt(point, coordinates)` | `directivity::Directivity::Dipole { weight, power }` / `Callback(Arc<dyn Fn(Vector3) -> f32>)`, `evaluate_at(point, &coordinates)` |
| `GainEffect(audioSettings)`, `apply({gain}, in, out)`, `tailApply(in, out)` | `effects::GainEffect::new(&audio_settings)`, `apply(gain, &in, &mut out)` / `apply_in_place(gain, &mut buf)`, `tail_apply(&in, &mut out)` |
| `EQEffect(audioSettings)`, `apply({gains}, in, out)`, `tailApply`, `normalizeGains(gains, overallGain)` | `effects::EqEffect::new(&audio_settings)`, `apply(&gains, &in, &mut out)` / `apply_in_place(&gains, &mut buf)`, `tail_apply`, `EqEffect::normalize_gains(&mut gains, &mut overall_gain)` |
| `DirectSoundPath` (`distanceAttenuation`, `airAbsorption`, `delay`, `occlusion`, `transmission`, `directivity`) | `simulation::DirectSoundPath` (snake_case fields; also `effects::DirectSoundPath`) |
| `DirectEffectFlags` (`ApplyDistanceAttenuation`, …, `ApplyTransmission`), `TransmissionType` | `effects::DirectEffectFlags::DISTANCE_ATTENUATION`, …, `TRANSMISSION` (same bit values; combined with `\|`), `effects::TransmissionType` |
| `DirectEffect(audioSettings, {numChannels})`, `apply(params, in, out)` (also `in == out`) | `effects::DirectEffect::new(&audio_settings, num_channels)`, `apply(&params, &in, &mut out)` / `apply_in_place(&params, &mut buf)` |
| `DirectEffect::calculateGainAndEQ` (private) | `DirectEffect::calculate_gain_and_eq(&path, flags, transmission_type) -> (gain, eq)` (public) |
| `Material` (`absorption`, `scattering`, `transmission`), `Triangle` (`indices`) | `scene::Material` (same fields), `scene::Triangle` (`indices: [u32; 3]`) |
| `Hit` (`distance`, `triangleIndex`, `objectIndex`, `materialIndex`, `normal`, `material`), `isValid` | `scene::Hit` (`distance`, `triangle_index`, `object_index`, `material_index`: `Option<usize>`; `normal`; `material: Option<Material>`), `is_valid` |
| `Mesh(numVertices, numTriangles, vertices, triangles)`, `triangleVertex(t, i)`, `normal(t)` | `scene::Mesh::new(&vertices, &triangles)`, `triangle_vertices(t)` (all three), `normal(t)` |
| `Ray::intersect(mesh, t)` | `mesh.intersect(&ray, t)` (or `ray.intersect_triangle(v0, v1, v2)`) |
| `BVH(mesh)`, `node(i)`, `numNodes`; `BVHNode::isLeaf`, `getTriangleIndex`, `getSplitAxis`, `boundingBox` | `scene::Bvh::new(&mesh)`, `node(i)` / `nodes()`, `num_nodes`; `BvhNode::is_leaf`, `kind(i)` → `BvhNodeKind::Leaf { triangle }` / `Internal { left_child, split_axis }`, `bounds`, `packed_data` |
| `BVH::intersect(ray, mesh, min, max)`, `isOccluded(ray, mesh, min, max)`, `isOccluded(start, end, mesh)`, `intersect(box, mesh)`, `boxIntersectsBox` | `Bvh::closest_hit`, `any_hit`, `is_occluded(start, end, &mesh)`, `intersects_aabb(&aabb, &mesh)`; `boxIntersectsBox` is crate-private |
| `GrowableBox`, `CentroidCoordinate`, `Split`, `ConstructionTask`, `TraversalTask`, `Stack<T, N>` | Private to `scene::bvh` |
| `StaticMesh(numVertices, numTriangles, numMaterials, vertices, triangles, materialIndices, materials)` | `scene::StaticMesh::new(&vertices, &triangles, &material_indices, &materials)` |
| `StaticMesh::closestHit`, `anyHit`, `intersectsBox`, `boundingBox`, `mesh`, `materials`, `materialIndices` | `closest_hit`, `any_hit`, `intersects_aabb`, `bounds`, `mesh`, `materials`, `material_indices` (also `bvh()`) |
| `InstancedMesh(subScene, transform)`, `updateTransform(scene, transform)`, `hasChanged`, `closestHit`, `anyHit` | `scene::InstancedMesh::new(Arc<Scene>, transform)`, `update_transform(transform)` (or `Scene::update_instanced_mesh_transform(id, transform)`), `has_changed`, `closest_hit`, `any_hit` |
| `IScene` ray queries: `closestHit`, `anyHit`, `closestHits`, `anyHits`, `isOccluded(from, to)` | `scene::RayTracer` trait: `closest_hit`, `any_hit`, `closest_hits`, `any_hits`, `is_occluded(from, to)` |
| `Scene()`, `SceneFactory::create(SceneType::Default, …)` | `scene::Scene::new()` |
| `CustomScene(closestHit, anyHit, batchedClosestHit, batchedAnyHit, userData)`, `SceneType::Custom` | A user type implementing `RayTracer` |
| `IScene::createStaticMesh(…)`, `createInstancedMesh(subScene, transform)` | `StaticMesh::new(…)` + `scene.insert_static_mesh(mesh)` → `StaticMeshId`; `scene.insert_instanced_mesh(InstancedMesh::new(…))` → `InstancedMeshId` |
| `addStaticMesh`, `removeStaticMesh`, `addInstancedMesh`, `removeInstancedMesh` | `add_static_mesh(id)`, `remove_static_mesh(id)`, `add_instanced_mesh(id)`, `remove_instanced_mesh(id)`; dropping the last reference: `release_static_mesh(id)`, `release_instanced_mesh(id)` |
| `commit`, `version`, `numStaticMeshes`, `numInstancedMeshes`, `setStaticMeshMaterial(mesh, material, i)` | `commit`, `version`, `num_static_meshes`, `num_instanced_meshes`, `set_static_mesh_material(id, material, i)` |
| `Scene::intersectsBox(box)`, `dumpObj(fileName)` | `intersects_aabb(&aabb)`, `dump_obj(&path)` / `write_obj(base_name, &mut obj, &mut mtl)` |
| `DirectSimulationFlags` (`CalcDistanceAttenuation`, …, `CalcDelay`) | `simulation::DirectSimulationFlags::DISTANCE_ATTENUATION`, …, `DELAY` (same bit values; combined with `\|`) |
| `OcclusionType::Raycast`, `Volumetric` | `simulation::OcclusionType::Raycast`, `Volumetric` |
| `DirectSimulator(maxNumOcclusionSamples)` | `simulation::DirectSimulator::new(max_num_occlusion_samples)` |
| `DirectSimulator::simulate(scene, flags, source, listener, distanceModel, airModel, directivity, occlusionType, radius, numOcclusionSamples, numTransmissionRays, path)` | `simulate(Option<&dyn RayTracer>, &DirectSimulationParams { flags, source, listener, .. }) -> DirectSoundPath` |
| `DirectSimulator::directPathDelay(listener, source)` | `DirectSimulator::direct_path_delay(listener, source)` |

## 6. Deviations

Tracked in [`DEVIATIONS.md`](DEVIATIONS.md).

## 7. Risks and open questions

- **R1. FFT differences.** PFFFT and `realfft` round differently, which affects everything built on
  convolution. Mitigation: peak-relative tolerances. If needed, generate goldens with an FFTW-grade
  reference to tell which side is more accurate. Measured at stage 2.5 (sizes 32–4096, real and complex): worst
  error 5.4e-7 × peak (complex inverse, 4096), forward transforms ≤ 2.5e-7 × peak; 15–70 % of values are
  bit-identical.
- **R2. Randomness.** Three sources, each handled differently:
  - `Reconstructor` white noise: an unseeded `std::default_random_engine` (`mt19937`, seed 5489, on MSVC) with
    `uniform_real_distribution<float>(-1, 1)`. This is deterministic on MSVC, so port MSVC's exact algorithms and
    compare against goldens.
  - `ReverbEffect` delay lengths: `rand() % 101`, MSVC's process-global LCG. Port the LCG; the global call order
    has to be modelled in tests.
  - Reflection and pathing ray directions: `RandomNumberGenerator` is seeded with `time(NULL)`, so **upstream
    itself is not reproducible**. Use statistical checks (band energies within ±0.5 dB, reverb times within 5 %)
    and seeded determinism on the Rust side.
- **R3. SOFA/HDF5:** see ADR 0001. Split into 3.5a (parser), 3.5b (resampling and lookup) and 3.5c (HRTF map). The
  parser (3.5a), the rest of `mysofa_open_no_norm` (3.5b) and `SOFAHRTFMap`'s indices and weights (3.5c) are checked
  bit for bit against the reference on Windows. libmysofa positions are a `long`, 32 bits on Windows, so
  the port truncates seek offsets to `i32` like the reference build; on Linux and macOS upstream would differ only on
  corrupt files with offsets of 2 GiB or more.
- **R4. FlatBuffers compatibility** of baked data. It must be readable by both implementations, which is
  verified by cross-loading files.
- **R6. Shipped binaries use Intel IPP** for `ArrayMath` and FFT on x86/x64. Our reference build uses upstream's
  generic path (IPP is not redistributable), so parity is against the generic path. IPP results may differ in the
  last bits.
- **R5. Reference build dependencies.** `core/build/get_dependencies.py` downloads ~15 third-party
  libraries. `tools/golden` uses a minimal configuration (PFFFT, mysofa, zlib, flatbuffers; no Embree,
  IPP, OpenCL, Radeon Rays or TrueAudioNext).
- **R7. `AudioBuffer` contract (decided in 2.4).** Buffers are one contiguous planar array, not `float**`
  (`DEVIATIONS.md`). Consequences:
  - Stage 8: when the user's channel pointers are adjacent (`ch[i + 1] == ch[i] + numSamples`, always true for
    `iplAudioBufferAllocate`), build one slice over them without copying (`unsafe`, allowed in `phonon-capi`);
    otherwise copy into scratch preallocated at effect creation. The core panics on violated preconditions (e.g.
    FuMa above order 3, zero channels), and a panic across `extern "C"` aborts the process, so the C API validates
    them first and returns `IPL_STATUS_FAILURE`.
  - Stage 4: `phonon.h` allows `DirectEffect` to run in place (`in == out`). Borrow rules forbid `&in` and `&mut out`
    on the same data, so it has a separate `apply_in_place(&params, &mut buffer)` (done in stage 4, as are
    `GainEffect::apply_in_place` and `EqEffect::apply_in_place`, which `DirectEffect` uses). Upstream gives the same
    result in place, which the generator checks. Every other effect is documented as not in-place.
- **R8. SIMD-width-dependent results (found in 2.7).** `IIR2Filterer` filters buffers 4 or 8 samples per step depending
  on `Context`'s SIMD level (AVX → 8), and the two recurrences round differently (measured ≤ 3.0e-6 × peak).
  `ReverbEffect` dispatches the same way. The port always uses the 8-sample recurrence, the one upstream runs on any
  x64 CPU with AVX, and is bit-exact against it; goldens for both are generated by setting `Context::sSIMDLevel` in
  the generator (which therefore needs an AVX CPU). Stage 6 (`ReverbEffect`) follows the same choice.
- **R9. Upstream quirks the port keeps (found in 2.7).** `Allpass::reset()` zeroes the gain as well as the delay
  line, and `ReverbEffect`'s constructor calls it, so its allpasses are plain delays. `IIR2Filterer`'s buffer path
  stores the last raw input (without the `1e-9` the recurrence adds) as state, which the single-sample path then
  uses. `lowPass` and `highShelf` design in `double` but round every coefficient to `float` before dividing by
  `a0` in `double`.
- **R10. Unwrapped-phase sensitivity (found in 3.1).** `HRTFDatabase` interpolates and blends HRTFs as log-magnitude and
  *unwrapped* phase in `f32`. The unwrapped phase grows with frequency (tens of radians), so FFT differences (R1) of
  ~1e-7 × peak in the stored spectra become up to 2.03e-5 × peak in synthesized HRTFs, growing towards Nyquist. A 2π
  decision flip in the unwrapping would cause a much larger error; none occurs on the tested directions. Stage 3.3
  (binaural) inherits this tolerance for interpolated HRTFs and spatial blend.
- **R11. Hidden state in upstream's real FFT (found in 3.2).** PFFFT writes `n / 2` packed bins, and `FFT::applyForward`
  copies `n / 2 + 1` out of its scratch spectrum, so the Nyquist bin's imaginary part is stale: the one of the last
  spectrum passed to `applyInverse` on the same `FFT` object (initially 0). It matters only when an inverse transform
  is fed a spectrum whose Nyquist bin is not real. In every upstream pipeline the IR spectra come from forward-only
  `FFT` objects (`HRTFDatabase::mFFTAudioProcessing`, `OverlapSavePartitioner`), so all Nyquist bins stay real and
  the stale value stays 0. The port's forward transform always returns 0 (`DEVIATIONS.md`); the convolution goldens
  use random spectra with real DC and Nyquist bins.
- **R12. Upstream crash in complex `multiplyAccumulate` (found in 3.2).** The generic x86 path does an aligned
  `float4::load` of the accumulator even in its unaligned branch. Overlap-save accumulator rows have `n / 2 + 1`
  (odd) bins, so every odd channel's row is unaligned and the reference build (SSE, no IPP) crashes with more than
  one channel. Shipped builds use IPP (R6) and are unaffected. Overlap-save goldens are therefore mono; channels are
  independent, so multi-channel effects and mixers are tested against mono ones.
- **R13. Convolution quirks the port keeps (found in 3.2).** `OverlapSavePartitioner` copies a partial last block
  over the previous block without clearing the rest of the frame, so stale IR samples (possibly from an earlier call)
  remain in it. The overlap-save tail lasts one block less than the IR and omits the last input frame's convolution
  with the leading blocks. `OverlapAddConvolutionEffect::tail` does not consume the pending tail, so a later `apply`
  adds it again. `OverlapSaveConvolutionEffect::reset` silences the current IR until a new one is passed.
- **R14. Spatialization quirks the port keeps (found in 3.3).** `PanningEffect` crossfades with
  `alpha = channel / numSamples` (the output channel index, not the sample index), so each channel uses one constant
  mix of the previous and current weights for the whole frame and there is no real crossfade. Its previous direction
  starts (and resets) at the zero vector, whose pairwise data comes from a NaN azimuth clamped to 0 and
  `acos(0) = π/2`, so the first frame on 5.1/7.1 mixes in a gain of −1 on the front-left speaker. `VirtualSurroundEffect`
  value-initializes its per-speaker states to `TailRemaining` and its mono path never updates them, so a mono layout
  reports `TailRemaining` until `reset` or `tail`. Standard 5.1 and 7.1 layouts put the LFE at the centre position,
  so virtual surround spatializes it from the front; only custom layouts can skip an LFE (position at the origin).
  `BinauralEffect` rebuilds its convolution (dropping the tail) when applied with a database of another HRIR length.
- **R15. Ambisonics quirks the port keeps (found in 3.4).** `AmbisonicsEncodeEffect` crossfades with
  `alpha = channel / numSamples`, like `PanningEffect` (R14), from a previous direction that starts (and resets) at the
  zero vector, whose SH weights are 0 except for order 0. Encode and rotate applied with an order above their maximum
  write only the maximum order's channels and leave the rest of the output untouched. `AmbisonicsPanningEffect` copies
  only the input's channels into its frame matrix and multiplies the whole matrix, so a frame of lower order than an
  earlier one mixes in that frame's higher-order channels; `reset` does not clear them. `AmbisonicsDecodeEffect` rotates
  into a buffer of the maximum order and passes the whole buffer to panning, so its stale higher-order channels (the last
  rotation of that order) are mixed in the same way. Decode thereby violates its sub-effects' own assertions (input
  channels = coefficients of the order); the port reproduces upstream's release behaviour through crate-private
  `process` methods with relaxed checks. `AmbisonicsBinauralEffect::tail` weights every channel up to the maximum order
  with the maximum order's max-rE gains, whatever order the last frame had, and the effect rebuilds its convolutions
  (dropping the tail) when applied with a database of another HRIR length, like `BinauralEffect`.
- **R16. libmysofa quirks the port keeps (found in 3.5a).** These decide which corrupt or unusual files load, so they
  are reproduced: only the B-tree node a chunked dataset points to is read (child pointers are taken as chunks);
  the shuffle filter is accepted but never applied; a global heap collection's address is kept in 16 bits; one
  recursion counter, never decremented for continuation messages, limits both continuation messages (25) and nested
  heap blocks (20); a short attribute name is dropped silently while `errno` is still 0; unresolvable object
  references are ignored and forward ones read as `REF%08X`; `getArray` reads the precision through a union, so
  64-bit integers load as doubles; MSVC's `realloc(p, 0)` makes a dataset of fewer than 8 bytes a missing variable,
  while one without data is present and empty; a variable that fails to load leaves the field missing and only the
  last failure is reported (`Hrtf::load_error`, ignored by `mysofa_open`); with duplicate children the first in the
  file wins, with duplicate attributes the last. zlib's `inflate` returns as soon as its output buffer is full with a literal or
  match still pending, so what follows a chunk's declared size in its stream (more data, a corrupt checksum) goes
  unchecked and the chunk loads; the port's inflater stops at the same point, and the corruption tests found no case
  where the two differ.
- **R17. libmysofa quirks the port keeps (found in 3.5b).** `mysofa_resample` gives the Speex filter both sampling rates
  truncated to integers, with MSVC's `float`-to-`unsigned` conversion (through a 64-bit integer: −1.5 becomes 2^32 − 1;
  NaN, infinities and values of 2^63 or more become 0), but scales the length and delays by the exact `float` ratio, and
  returns resampler errors as Speex's codes (1: the filter cannot be built, 3: a rate of 0). Each response is resampled
  separately, from silence, with the filter latency skipped, and zeros are fed until it has `ceil(N × ratio)` samples.
  Speex's `_muldiv` bounds use `UINT32_MAX = 2^32`, the file's own fallback, because `stdint.h` is not included there.
  `mysofa_tocartesian` treats an array whose `Type` attribute has no value as spherical and changes the last `Type` and
  `Units` attributes in the file. `mysofa_lookup_init` starts the maxima of the coordinate ranges at `FLT_MIN`
  (positive), so a coordinate whose values are all negative keeps that maximum; the ranges decide which coordinates the
  neighbourhood searches. `mysofa_lookup` scales the query in place onto the measured radius range, so the zero vector
  becomes NaN and returns measurement 0 (the tree's root). Between equidistant measurements the k-d tree search returns
  the first one it visits, which depends on the insertion (measurement) order, so the port keeps the unbalanced tree. Such ties are common on regular
  grids: in `sadie_h12`, stepping up in elevation from (310°, −60°) lands midway between the measurements at 305° and
  315° (−54°). Which one wins is decided by an ulp of `sinf`/`cosf`/`atan2f`/`powf`, so Linux and macOS (glibc and
  Apple libm) pick the other one in a few places; the tests accept a different index there only if it is equidistant.
- **R18. `SOFAHRTFMap` quirks the port keeps (found in 3.5c).** `interpolatedHRIRWeights` skips interpolation when the
  direction, *unnormalized*, is within 1e-5 of a measured position, so for files measured at another radius than the
  direction's length (SADIE: 1.2 m) a unit direction is always interpolated. That path writes only slot 0 (weight 1);
  the other slots keep what the caller initialized, which `interpolatedHRTF` sets to index 0, weight 0, and
  `HRTFDatabase` blends all eight slots. Otherwise every unused slot repeats the nearest measurement with weight 0 (not
  −1), and the weights are divided (not multiplied by a reciprocal) by their sum, which gives NaN if every term is 0.
  Azimuths are unwrapped in several places by comparing with ±180°; the query's side of each neighbour is decided before
  the query's own azimuth is unwrapped. `patchSOFANeighborhood` fills only missing ±azimuth neighbours, stepping by
  0.5° up to 360° with `mysofa_lookup`; at ±90° elevation no other measurement is ever nearest, so the shipped files'
  poles stay without azimuth neighbours. A `Data.Delay` value is rejected only if its magnitude exceeds 1e-3 (NaN
  passes). Loading from a file name reports `Status::Failure`, from memory `Status::Initialization`.
- **R19. Direct sound quirks the port keeps (found in 4).** `EQEffect::reset` resets the gains to 1 but not the filter
  states, so a reset effect still rings with the previous signal. The EQ crossfades only when a gain changes (compared
  with `!=`, so a NaN gain crossfades every frame) and copies the old filters' state into the new ones first.
  `GainEffect` applies the first frame's gain without ramping; afterwards each frame moves a quarter of the way to the
  new gain, linearly within the frame, so it never quite arrives; `reset` makes the next frame jump again.
  `DirectEffect` runs the EQ for air absorption, or for frequency-dependent transmission even without occlusion (when
  the transmission coefficients are not used and the EQ gains stay 1); `normalizeGains` divides the band gains by their
  maximum (`std::max`, so NaN gains are skipped when finding it and stay NaN), raises them to at least 0.0625, and sets
  the overall gain to 0 when no band gain reaches `FLT_MIN`. The average transmission is accumulated in band order and
  divided (not multiplied by a reciprocal) by 3. `DirectSoundPath::delay` is not used by `DirectEffect`. An
  inverse-distance model with a minimum distance of 0 gives an infinite gain at distance 0 (−∞ at −0), and
  `generateCorrectionCurve` divides by the `from` model, so with such models its first sample is infinite, 0 or NaN.
- **R20. Scene quirks the port keeps (found in 5).** The BVH's closest-hit search accepts a triangle when
  `minDistance <= t < (closest so far)`, so `maxDistance` only prunes boxes: a triangle beyond it is reported when its
  box reaches into the range (the any-hit search does check `t < maxDistance`). A zero direction component of either
  sign gets a reciprocal of `+∞` and counts as positive. Boxes grow with SSE `min`/`max` from `±FLT_MAX`, so a NaN
  coordinate is dropped unless it comes last, and the SAH surface area is summed as `dx·dy + (dz·dx + dy·dz)`. The SAH
  split breaks cost ties by balance, with the best balance reset for each axis, and falls back to an object median
  split (along the longest axis, using the order the SAH pass sorted) when no cost is below `FLT_MAX`.
  `Scene::closestHit` tests static meshes before instances and keeps the first of equal distances;
  `Scene::intersectsBox` ignores instances. `setStaticMeshMaterial` does nothing unless the mesh is committed in that
  scene and applies at the next commit; `updateTransform` takes effect at once (the instance is shared with the
  committed list) and only bumps the version at the next commit, when any element differs bitwise. Adding a mesh
  twice makes rays test it twice; removing removes every copy. An instance's vertex and triangle counts are those of its
  sub-scene when it was created. `InstancedMesh` maps the distance range into the sub-scene through transformed points
  (an infinite maximum stays infinite) and transforms normals by the inverse transpose; a singular transform gives a
  zero inverse.
- **R21. Implementation-defined `std::sort` (found in 5).** The BVH builder sorts triangle centroids with
  `std::sort`, which is not stable: which of several equal centroids comes first depends on the standard library.
  Axis-aligned meshes are full of equal coordinates, so libstdc++ or libc++ builds of upstream lay out other trees than
  MSVC. Which triangle a ray reports can differ only on exact distance ties (shared edges), but node-by-node goldens
  need the same tree, so the port includes MSVC's introsort (`scene::msvc_sort`, from VS 2022 17.14's `<algorithm>`)
  and is bit-exact against the reference build.
- **R22. Direct simulator quirks the port keeps (found in 5.1).** A simulator built for at most 1 occlusion sample keeps
  no sample points at all, so its volumetric occlusion is always 0 (fully occluded); a request for more points than the
  simulator holds is clamped silently. Volumetric occlusion discards the points the source's centre cannot see (they poke
  through nearby geometry) and gives 0 when none is left. Raycast occlusion is one `isOccluded` test from the listener to
  the source. Without a scene, requested occlusion and transmission are 1. Transmission traces alternately from the
  listener and from the source, each ray resuming 1 cm past its last hit, and stops after `numTransmissionRays` hits, at
  a miss, or when the two rays' start distances add up to the source distance; because closest-hit searches report
  triangles beyond their maximum distance (R20), a surface just past the far end can still be counted. One hit gives
  its material's coefficients, two or more the square root of the product of all of them (also for an odd count).
