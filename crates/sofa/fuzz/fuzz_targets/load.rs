//! Opens arbitrary bytes as a SOFA file the way Steam Audio does (`Easy::load_no_norm`: loading, check, resampling,
//! lookup and neighbourhood): it must return an error, never panic, overflow the stack or run away with memory or time.
//! Run with `cargo +nightly fuzz run load` from `crates/sofa` (see `crates/sofa/README.md`).

#![no_main]

use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    if let Ok(easy) = sofa::Easy::load_no_norm(data, 48000.0) {
        let mut direction = [1.0, 0.0, 0.0];
        let _ = easy.lookup.lookup(&mut direction);
    }
});
