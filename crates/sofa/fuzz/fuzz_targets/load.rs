//! Parses arbitrary bytes as a SOFA file: the parser must return an error, never panic, overflow the stack or run
//! away with memory or time. Run with `cargo +nightly fuzz run load` from `crates/sofa` (see `crates/sofa/README.md`).

#![no_main]

use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    if let Ok(hrtf) = sofa::Hrtf::load(data) {
        let _ = hrtf.check();
    }
});
