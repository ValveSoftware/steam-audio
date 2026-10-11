# Deviations from the reference implementation

Every intentional difference in behaviour between this port and upstream Steam Audio (`core/`) is recorded here.
Differences within a module's stated numerical tolerance (see `PORTING_PLAN.md` §5) are not deviations.

| Area | Upstream behaviour | Rust behaviour | Reason | Approved |
| --- | --- | --- | --- | --- |
| GPU backends | OpenCL, Radeon Rays, TrueAudioNext | Not implemented | Out of scope | yes (plan) |
| Vendor math libraries | IPP, MKL, vDSP, FFTS | Not used; `realfft`/`rustfft` and scalar/`std` SIMD | Out of scope; portability | yes (plan) |
| Embree ray tracer | Optional backend | Not implemented; built-in BVH tracer only | Out of scope | yes (plan) |
