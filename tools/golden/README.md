# tools/golden

Builds the reference Steam Audio from `core/` (unmodified) and generates golden data for the Rust parity tests
in `testdata/golden/`.

## How it fits together

| File | Role |
| --- | --- |
| `fetch_deps.py` | Fetches and builds the minimal dependency set (zlib, libmysofa, PFFFT, FlatBuffers, Catch) at the commits pinned in `core/build/dependencies.json`, into `core/deps/` (gitignored). Replaces upstream's `get_dependencies.py`, which only supports Visual Studio up to 2022. |
| `hook.cmake` | Injected via `CMAKE_PROJECT_Phonon_INCLUDE`. Adds the `golden` target to the upstream build without editing `core/`, so it gets exactly the same compile flags and definitions as upstream's `phonon_test`. |
| `golden.cmake` | The `golden` executable target (C++17, links the internal `core` object library). |
| `src/npy.h` | `.npy` writer and `Lcg`, the deterministic input generator mirrored by `parity::Lcg`. |
| `src/<module>.cpp` | One generator per upstream module, registered with `golden::Register`. |

## Building (Windows, MSVC + Ninja)

Run everything from an MSVC x64 developer environment (`vcvarsall.bat x64`) with `cmake` and `ninja` on `PATH`.

```bash
python tools/golden/fetch_deps.py
cmake -G Ninja -S core -B build/reference -DCMAKE_BUILD_TYPE=Release -DCMAKE_PROJECT_Phonon_INCLUDE=tools/golden/hook.cmake -DSTEAMAUDIO_BUILD_TESTS=ON -DSTEAMAUDIO_BUILD_BENCHMARKS=OFF -DSTEAMAUDIO_BUILD_SAMPLES=OFF -DSTEAMAUDIO_BUILD_ITESTS=OFF -DSTEAMAUDIO_BUILD_DOCS=OFF -DSTEAMAUDIO_ENABLE_IPP=OFF -DSTEAMAUDIO_ENABLE_MKL=OFF -DSTEAMAUDIO_ENABLE_EMBREE=OFF -DSTEAMAUDIO_ENABLE_RADEONRAYS=OFF -DSTEAMAUDIO_ENABLE_TRUEAUDIONEXT=OFF
cmake --build build/reference --target golden phonon_test
```

`-DCMAKE_PROJECT_Phonon_INCLUDE` must be an absolute path.

Sanity check: upstream's own tests must pass. They look for data at `../../data`, so run them from two levels
below `core/`:

```bash
mkdir -p core/deps-build/run && cd core/deps-build/run && ../../../build/reference/src/test/phonon_test.exe
```

## Generating golden data

```bash
build/reference/golden.exe testdata/golden          # all modules
build/reference/golden.exe testdata/golden fft      # selected modules
```

Golden files are generated with **MSVC on Windows x64** only. Upstream's random number generation is
implementation-defined (`std::default_random_engine`), so other toolchains produce different golden data
(see risk R2 in `docs/PORTING_PLAN.md`). Commit the regenerated files together with the generator change.
The `iir` generator also needs a CPU with AVX: it records upstream's AVX filtering path (risk R8).

The `sofa` generator reads `core/data/hrtf/*.sofa`, so run it from the repository root. It also describes libmysofa's
own test files, taken from `MYSOFA_TESTS_DIR` (default `core/deps-build/mysofa/src/mysofa/tests`, fetched by
`fetch_deps.py`); without them it skips those cases, and so does the Rust test.

## Adding a generator

1. Create `src/<module>.cpp` with a function `void generate(const std::filesystem::path& out)`, and register it
   with `golden::Register name("<module>", generate);`.
2. Add the file to `golden.cmake`.
3. Prefer `Lcg`-generated inputs (the Rust test regenerates them with `parity::Lcg`) over storing inputs.
   Store complex arrays as `[n, 2]`.
4. Rebuild, regenerate, and write the Rust test against `parity::golden("<module>/<case>")`.
