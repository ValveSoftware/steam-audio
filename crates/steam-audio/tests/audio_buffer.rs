//! Parity tests for stage 2.4 against golden data from `tools/golden/src/audio_buffer.cpp`.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order.

use parity::{Lcg, Tolerance, compare, golden};
use steam_audio::audio_buffer::{self, AmbisonicsType, AudioBuffer};

const NUM_SAMPLES: usize = 37;
const AMBISONICS_SAMPLES: usize = 13;

#[track_caller]
fn check(name: &str, actual: &[f32], tol: Tolerance) {
    let expected = golden::<f32>(name);
    if let Err(m) = compare(actual, &expected.data, tol) {
        panic!("{name}: {m}");
    }
}

/// Fills the buffer channel by channel, sample by sample, like the generator's `fill`.
fn filled(lcg: &mut Lcg, num_channels: usize, num_samples: usize) -> AudioBuffer {
    let data = (0..num_channels * num_samples)
        .map(|_| lcg.next_signed())
        .collect();
    AudioBuffer::from_planar(num_channels, num_samples, data)
}

#[test]
fn audio_buffer() {
    let mut lcg = Lcg::new(11);

    let planar = filled(&mut lcg, 3, NUM_SAMPLES);
    let mut interleaved = vec![0.0; 3 * NUM_SAMPLES];
    planar.interleave(&mut interleaved);
    check("audio_buffer/read", &interleaved, Tolerance::EXACT);

    for x in &mut interleaved {
        *x = lcg.next_signed();
    }
    let mut deinterleaved = AudioBuffer::new(3, NUM_SAMPLES);
    deinterleaved.deinterleave(&interleaved);
    check(
        "audio_buffer/write",
        deinterleaved.as_slice(),
        Tolerance::EXACT,
    );

    let mut scaled = filled(&mut lcg, 3, NUM_SAMPLES);
    let volume = lcg.next_signed() * 2.0;
    scaled.scale(volume);
    check("audio_buffer/scale", scaled.as_slice(), Tolerance::F32);

    let mix_in = filled(&mut lcg, 3, NUM_SAMPLES);
    let mut mix_out = filled(&mut lcg, 3, NUM_SAMPLES);
    audio_buffer::mix(&mix_in, &mut mix_out);
    check("audio_buffer/mix", mix_out.as_slice(), Tolerance::F32);

    for num_channels in [1, 3, 5] {
        let input = filled(&mut lcg, num_channels, NUM_SAMPLES);
        let mut mono = AudioBuffer::new(1, NUM_SAMPLES);
        audio_buffer::downmix(&input, &mut mono);
        check(
            &format!("audio_buffer/downmix_{num_channels}"),
            mono.as_slice(),
            Tolerance::F32,
        );
    }
}

#[test]
fn ambisonics_conversion() {
    use AmbisonicsType::{FuMa, N3d, Sn3d};
    let types = [(N3d, "n3d"), (Sn3d, "sn3d"), (FuMa, "fuma")];

    let mut lcg = Lcg::new(13);
    for order in 0..=3 {
        let num_channels = (order + 1) * (order + 1);
        let input = filled(&mut lcg, num_channels, AMBISONICS_SAMPLES);

        for (in_type, in_name) in types {
            for (out_type, out_name) in types {
                let mut converted = AudioBuffer::new(num_channels, AMBISONICS_SAMPLES);
                audio_buffer::convert_ambisonics(in_type, out_type, &input, &mut converted);
                check(
                    &format!("ambisonics_conversion/order{order}_{in_name}_to_{out_name}"),
                    converted.as_slice(),
                    Tolerance::F32,
                );
            }
        }
    }
}

#[test]
fn ambisonics_round_trip_through_fuma() {
    let mut lcg = Lcg::new(17);
    let input = filled(&mut lcg, 16, AMBISONICS_SAMPLES);
    let mut fuma = AudioBuffer::new(16, AMBISONICS_SAMPLES);
    let mut back = AudioBuffer::new(16, AMBISONICS_SAMPLES);
    audio_buffer::convert_ambisonics(AmbisonicsType::N3d, AmbisonicsType::FuMa, &input, &mut fuma);
    audio_buffer::convert_ambisonics(AmbisonicsType::FuMa, AmbisonicsType::N3d, &fuma, &mut back);
    parity::assert_close(back.as_slice(), input.as_slice(), Tolerance::F32);
}

#[test]
fn n3d_sn3d_conversion_supports_higher_orders() {
    let mut lcg = Lcg::new(19);
    let input = filled(&mut lcg, 25, 3);
    let mut out = AudioBuffer::new(25, 3);
    audio_buffer::convert_ambisonics(AmbisonicsType::N3d, AmbisonicsType::Sn3d, &input, &mut out);
    // Order 4 channels are scaled by 1 / sqrt(2 * 4 + 1) = 1/3.
    assert_eq!(out[24][0], input[24][0] * (1.0 / 3.0));
}
