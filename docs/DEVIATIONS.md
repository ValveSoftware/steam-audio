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
