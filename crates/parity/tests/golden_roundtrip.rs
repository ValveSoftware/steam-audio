//! End-to-end check of the golden pipeline: the C++ generator (tools/golden) and the Rust side agree on the
//! `.npy` format and on the `Lcg` input sequence.
//!
//! `fft/real_inverse_<n>` is the reference FFT round trip of an `Lcg`-generated signal, so it must reproduce the
//! signal that `parity::Lcg` generates here.

use parity::{Lcg, Tolerance, assert_close, golden};

#[test]
fn fft_round_trip_reproduces_lcg_input() {
    for size in [20usize, 32, 256, 1024, 4096] {
        let round_trip = golden::<f32>(&format!("fft/real_inverse_{size}"));
        let padded = size.next_power_of_two();
        assert_eq!(round_trip.shape, [padded]);

        let mut lcg = Lcg::new(size as u32);
        let mut input = vec![0.0f32; padded];
        input[..size]
            .iter_mut()
            .for_each(|x| *x = lcg.next_signed());

        assert_close(&round_trip.data, &input, Tolerance::new(1e-5, 0.0));
    }
}
