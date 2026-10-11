# Deviations from the reference implementation

Every intentional difference in behaviour between this port and upstream Steam Audio (`core/`) is recorded here.
Differences within a module's stated numerical tolerance (see `PORTING_PLAN.md` §5) are not deviations.

| Area | Upstream behaviour | Rust behaviour | Reason | Approved |
| --- | --- | --- | --- | --- |
| GPU backends | OpenCL, Radeon Rays, TrueAudioNext | Not implemented | Out of scope | yes (plan) |
| Vendor math libraries | IPP, MKL, vDSP, FFTS | Not used; `realfft`/`rustfft` and scalar/`std` SIMD | Out of scope; portability | yes (plan) |
| Embree ray tracer | Optional backend | Not implemented; built-in BVH tracer only | Out of scope | yes (plan) |
| Scalar type of math types | Templated on `T`; `float` and `double` typedefs | `f32` only | Only `float` is instantiated by the library; `double` variants are unused | yes (2.1) |
| `Vector<T,D> / Vector<T,D>` | Does not compile if instantiated (`lhs /= rhs` on a const reference); never used | Element-wise division by reciprocal multiplication | Upstream bug, unreachable | yes (2.1) |
| `SquareMatrix::transpose()` (in place) | Swaps every `(i,j)`/`(j,i)` pair twice, so it is a no-op; never called | Not ported; `transpose()` returns a copy (upstream `transposedCopy`) | Upstream bug, unreachable | yes (2.1) |
| Uninitialized matrices | Default constructor leaves elements uninitialized | Always initialized (`ZERO`) | Safety | yes (2.1) |
| `Box::coordinates(i)` | Pointer arithmetic over 16-byte-aligned members, unchecked index | `Aabb::bound(i)`, panics on `i > 1` | Safety | yes (2.1) |
| `leastSquares`, `leastSquaresL1` | Implemented only with Intel MKL; the default build throws `Status::Failure` | Not ported | Unused without MKL (out of scope) | yes (2.3) |
| `Array<T, N>` | Custom multi-dimensional array with pointer tables | `Vec<T>` and slices | Idiomatic Rust; no behaviour change | yes (2.3) |
| `Sampling` hemisphere docs | Comment says the hemisphere's normal is +y | Samples have `z <= 0` (normal −z) in both; only the doc comment is corrected | Upstream comment is wrong | yes (2.2) |
| `AudioBuffer` storage | `float* const*`: one pointer per channel, channels may live anywhere | One contiguous planar array (owned `Vec<f32>` or a borrowed slice view) | No `unsafe`, no pointer tables. Upstream only builds non-owning buffers over contiguous data internally; the C API (stage 8) copies user channel arrays into preallocated scratch | yes (2.4) |
| `convertAmbisonics` with FuMa above order 3 | Reads past the 16-entry index/gain tables (undefined behaviour) | Panics | Safety | yes (2.4) |
| `AudioBuffer::downmix` with 0 input channels | Reads `in[0]` (undefined behaviour) | Panics | Safety | yes (2.4) |
| FFT sizes below PFFFT's minimum | `FFT` throws `Status::Initialization` for real sizes < 32 and complex sizes < 16 (after rounding up to a power of two) | Every positive size works; size 0 panics | No reason to fail; upstream never relies on the exception | yes (2.5) |
| `FFT` domain | One class; the domain is a constructor argument and calling the wrong `apply*` overload is undefined | Two types, `RealFft` and `ComplexFft` | Type safety | yes (2.5) |
| `FFT::apply*` buffer sizes | Reads/writes exactly `numRealSamples`/`numComplexSamples` through raw pointers | Slices must have exactly those lengths, otherwise panics | Safety | yes (2.5) |
