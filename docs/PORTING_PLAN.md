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
| 3.5b | `sofa` crate: Speex resampler, `mysofa_tocartesian`, k-d tree lookup, neighbourhood, `c2s`/`s2c` (the rest of `mysofa_open_no_norm`) | libmysofa `resample.c`, `speex_resampler.c`, `spherical.c`, `tools.c`, `kdtree.c`, `lookup.c`, `neighbors.c`, `easy.c` | ☐ | ☐ | ☐ | ADR 0001: resampled HRIRs `peak_relative(1e-5)`, lookup and neighbourhood indices `EXACT` |
| 3.5c | SOFA HRTF map and `HRTFMapFactory` (moved from 3.1) | `sofa_hrtf_map`, `hrtf_map_factory` | ☐ | ☐ | ☐ | Weights `F32`, indices `EXACT`; binaural with a SOFA HRTF as in 3.3 |
| 4 | Direct sound: distance attenuation, air absorption, directivity, direct effect, gain/EQ | `distance_attenuation`, `air_absorption`, `directivity`, `direct_effect`, `gain_effect`, `eq_effect` | ☐ | ☐ | ☐ | `F32` / `peak_relative(1e-5)`. `DirectEffect` needs an in-place `apply` (R7) |
| 5 | Scene and geometry: mesh, static/instanced mesh, BVH, scene, materials, OBJ loading for tests | `mesh`, `static_mesh`, `instanced_mesh`, `bvh`, `scene`, `material`, `hit` | ☐ | ☐ | ☐ | hits: exact triangle index, `F32` distance |
| 5.1 | Direct simulator (occlusion, transmission) | `direct_simulator` | ☐ | ☐ | ☐ | R2 |
| 6 | Reflections: energy field, IR, reflection simulator, reconstructor, reverb, hybrid reverb, job graph, baking, probes, serialisation | `energy_field*`, `impulse_response*`, `reflection_*`, `reconstructor*`, `*reverb*`, `job*`, `thread_pool`, `probe_*`, `baked_*`, `serialized_object` | ☐ | ☐ | ☐ | R2 |
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
| `HRTFSettings` (`Default` type), `HRTFNormType::None`/`RMS` | `hrtf::HrtfSettings { data, volume, normalization }`, `HrtfNormalization::None`/`Rms` |
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
  parser is checked bit for bit against libmysofa (3.5a). libmysofa positions are a `long`, 32 bits on Windows, so
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
    on the same data, so it needs a separate in-place method (e.g. `apply_in_place(&mut buffer)`). Every other effect
    is documented as not in-place.
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
