//! Parity tests for stage 3.3 against golden data from `tools/golden/src/spatialization.cpp`: panning, binaural and
//! virtual surround effects. The upstream Catch test `BinauralEffect.test.cpp` only checks for finite output; that is
//! covered by `binaural_output_is_finite` below.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order.

use parity::{Element, Lcg, Tolerance, compare, golden};
use steam_audio::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use steam_audio::effects::{BinauralEffect, BinauralParams, PanningEffect, VirtualSurroundEffect};
use steam_audio::hrtf::{
    HrtfDatabase, HrtfInterpolation, HrtfNormalization, HrtfPhaseType, HrtfSettings,
};
use steam_audio::math::Vector3;
use steam_audio::speaker_layout::{SpeakerLayout, SpeakerLayoutType};

const SAMPLING_RATE: u32 = 48000;

/// Tolerance for HRTFs that are synthesized (interpolated or blended), see risk R10 in `docs/PORTING_PLAN.md`.
const SYNTHESIZED: f64 = 5e-5;

#[track_caller]
fn check<T: Element + Into<f64>>(name: &str, actual: &[T], tol: impl Fn(&[T]) -> Tolerance) {
    let expected = golden::<T>(name);
    if let Err(m) = compare(actual, &expected.data, tol(&expected.data)) {
        panic!("{name}: {m}");
    }
}

fn draw_vector(lcg: &mut Lcg) -> Vector3 {
    let x = lcg.next_signed();
    let y = lcg.next_signed();
    let z = lcg.next_signed();
    Vector3::new(x, y, z)
}

fn fill(lcg: &mut Lcg, buffer: &mut AudioBuffer) {
    for s in buffer.as_mut_slice() {
        *s = lcg.next_signed();
    }
}

fn state_code(state: AudioEffectState) -> i32 {
    match state {
        AudioEffectState::TailRemaining => 0,
        AudioEffectState::TailComplete => 1,
    }
}

/// Outputs and `[state, tail samples remaining]` rows, as the generator's `Recorder`.
#[derive(Default)]
struct Recorder {
    outputs: Vec<f32>,
    states: Vec<i32>,
}

impl Recorder {
    fn record(
        &mut self,
        out: &AudioBuffer,
        state: AudioEffectState,
        tail_samples_remaining: usize,
    ) {
        self.outputs.extend_from_slice(out.as_slice());
        self.states.push(state_code(state));
        self.states.push(tail_samples_remaining as i32);
    }

    #[track_caller]
    fn check(&self, name: &str, relative: f64) {
        check(name, &self.outputs, |e| {
            Tolerance::peak_relative(e, relative)
        });
        assert_eq!(
            self.states,
            golden::<i32>(&format!("{name}_states")).data,
            "{name}_states"
        );
    }
}

fn layouts() -> Vec<(&'static str, SpeakerLayout)> {
    // Same custom layout as the generator's `kCustomSpeakers`.
    let custom = SpeakerLayout::custom(vec![
        Vector3::new(-1.0, 0.0, -1.0),
        Vector3::new(1.0, 0.0, -1.0),
        Vector3::new(0.0, 0.0, 0.0),
        Vector3::new(0.0, 1.0, 0.0),
        Vector3::new(0.3, -0.5, 0.8),
    ]);
    vec![
        ("mono", SpeakerLayout::new(SpeakerLayoutType::Mono)),
        ("stereo", SpeakerLayout::new(SpeakerLayoutType::Stereo)),
        ("quad", SpeakerLayout::new(SpeakerLayoutType::Quadraphonic)),
        (
            "surround51",
            SpeakerLayout::new(SpeakerLayoutType::FivePointOne),
        ),
        (
            "surround71",
            SpeakerLayout::new(SpeakerLayoutType::SevenPointOne),
        ),
        ("custom", custom),
    ]
}

fn database(sampling_rate: u32, frame_size: usize) -> HrtfDatabase {
    let settings = HrtfSettings {
        data: None,
        volume: 0.0,
        normalization: HrtfNormalization::None,
    };
    HrtfDatabase::new(&settings, sampling_rate, frame_size).unwrap()
}

// ---------------------------------------------------------------------------------------------------------------------
// Panning
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn panning() {
    let mut lcg = Lcg::new(53);

    // Axes and the zero vector, then random directions, as the generator.
    let mut directions = vec![
        Vector3::new(1.0, 0.0, 0.0),
        Vector3::new(-1.0, 0.0, 0.0),
        Vector3::new(0.0, 1.0, 0.0),
        Vector3::new(0.0, -1.0, 0.0),
        Vector3::new(0.0, 0.0, 1.0),
        Vector3::new(0.0, 0.0, -1.0),
        Vector3::new(0.0, 0.0, 0.0),
    ];
    directions.extend((0..300).map(|_| draw_vector(&mut lcg)));

    for (name, layout) in layouts() {
        let weights: Vec<f32> = directions
            .iter()
            .flat_map(|&d| {
                let layout = &layout;
                (0..layout.num_speakers()).map(move |i| PanningEffect::panning_weight(d, layout, i))
            })
            .collect();
        check(&format!("panning_effect/weights_{name}"), &weights, |_| {
            Tolerance::F32
        });
    }

    const FRAME_SIZE: usize = 37;
    for (name, layout) in layouts() {
        let mut effect = PanningEffect::new(&layout);
        let mut input = AudioBuffer::new(1, FRAME_SIZE);
        let mut out = AudioBuffer::new(layout.num_speakers(), FRAME_SIZE);
        let mut recorder = Recorder::default();

        for op in "AAARAAATA".chars() {
            match op {
                'A' => {
                    let direction = draw_vector(&mut lcg);
                    fill(&mut lcg, &mut input);
                    let state = effect.apply(direction, &input, &mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'T' => {
                    let state = effect.tail(&mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'R' => effect.reset(),
                _ => unreachable!(),
            }
        }

        recorder.check(&format!("panning_effect/effect_{name}"), 1e-5);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Binaural
// ---------------------------------------------------------------------------------------------------------------------

struct BinauralCase {
    name: &'static str,
    in_channels: usize,
    interpolation: HrtfInterpolation,
    spatial_blend: f32,
    phase_type: HrtfPhaseType,
    frame_size: usize,
    script: &'static str,
}

const fn case(
    name: &'static str,
    in_channels: usize,
    interpolation: HrtfInterpolation,
    spatial_blend: f32,
    phase_type: HrtfPhaseType,
    frame_size: usize,
    script: &'static str,
) -> BinauralCase {
    BinauralCase {
        name,
        in_channels,
        interpolation,
        spatial_blend,
        phase_type,
        frame_size,
        script,
    }
}

const NEAREST: HrtfInterpolation = HrtfInterpolation::NearestNeighbor;
const BILINEAR: HrtfInterpolation = HrtfInterpolation::Bilinear;

const BINAURAL_CASES: [BinauralCase; 11] = [
    case(
        "nearest_mono",
        1,
        NEAREST,
        1.0,
        HrtfPhaseType::None,
        128,
        "AAAATTTTRAA",
    ),
    case(
        "bilinear_mono",
        1,
        BILINEAR,
        1.0,
        HrtfPhaseType::None,
        77,
        "AAAATTTTAA",
    ),
    case(
        "nearest_stereo",
        2,
        NEAREST,
        1.0,
        HrtfPhaseType::None,
        100,
        "AAATTTA",
    ),
    case(
        "nearest_stereo_blend_none",
        2,
        NEAREST,
        0.4,
        HrtfPhaseType::None,
        100,
        "AAATTA",
    ),
    case(
        "nearest_stereo_blend_itd",
        2,
        NEAREST,
        0.7,
        HrtfPhaseType::SphereItd,
        100,
        "AAATTA",
    ),
    case(
        "nearest_stereo_blend_full",
        2,
        NEAREST,
        0.2,
        HrtfPhaseType::Full,
        100,
        "AAATTA",
    ),
    case(
        "nearest_mono_blend",
        1,
        NEAREST,
        0.5,
        HrtfPhaseType::Full,
        64,
        "AAATA",
    ),
    case(
        "bilinear_stereo_blend_itd",
        2,
        BILINEAR,
        0.6,
        HrtfPhaseType::SphereItd,
        100,
        "AAATTA",
    ),
    case(
        "bilinear_stereo_blend_full",
        2,
        BILINEAR,
        0.3,
        HrtfPhaseType::Full,
        64,
        "AAATTA",
    ),
    case(
        "bilinear_mono_blend",
        1,
        BILINEAR,
        0.5,
        HrtfPhaseType::SphereItd,
        64,
        "AAATA",
    ),
    case(
        "switch_database",
        1,
        NEAREST,
        1.0,
        HrtfPhaseType::None,
        64,
        "AASAATTSAA",
    ),
];

#[test]
fn binaural() {
    let mut lcg = Lcg::new(59);

    for c in &BINAURAL_CASES {
        let mut first = database(SAMPLING_RATE, c.frame_size);
        let mut second = database(44100, c.frame_size);
        let mut use_second = false;

        let audio = AudioSettings::new(SAMPLING_RATE, c.frame_size);
        let mut effect = BinauralEffect::new(&audio, &first);

        let mut input = AudioBuffer::new(c.in_channels, c.frame_size);
        let mut out = AudioBuffer::new(2, c.frame_size);
        let mut recorder = Recorder::default();
        let mut peak_delays = Vec::new();

        for op in c.script.chars() {
            match op {
                'A' => {
                    let direction = draw_vector(&mut lcg).normalize_or_zero();
                    fill(&mut lcg, &mut input);
                    let mut peaks = [0.0; 2];
                    let params = BinauralParams {
                        direction,
                        interpolation: c.interpolation,
                        spatial_blend: c.spatial_blend,
                        phase_type: c.phase_type,
                        hrtf: if use_second { &mut second } else { &mut first },
                        peak_delays: Some(&mut peaks),
                    };
                    let state = effect.apply(params, &input, &mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                    peak_delays.extend_from_slice(&peaks);
                }
                'T' => {
                    let state = effect.tail(&mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'R' => effect.reset(),
                'S' => use_second = !use_second,
                _ => unreachable!(),
            }
        }

        // A mono input ignores the spatial blend with nearest-neighbour lookup and uses the stored HRTF.
        let synthesized =
            (c.spatial_blend < 1.0 && c.in_channels == 2) || c.interpolation == BILINEAR;
        let name = format!("binaural_effect/{}", c.name);
        recorder.check(&name, if synthesized { SYNTHESIZED } else { 1e-5 });
        check(&format!("{name}_peak_delays"), &peak_delays, |_| {
            Tolerance::EXACT
        });
    }
}

/// Upstream's own Catch test: random directions never produce non-finite output.
#[test]
fn binaural_output_is_finite() {
    const FRAME_SIZE: usize = 512;
    let mut lcg = Lcg::new(7);
    for interpolation in [NEAREST, BILINEAR] {
        let mut hrtf = database(SAMPLING_RATE, FRAME_SIZE);
        let audio = AudioSettings::new(SAMPLING_RATE, FRAME_SIZE);
        let mut effect = BinauralEffect::new(&audio, &hrtf);
        let mut input = AudioBuffer::new(1, FRAME_SIZE);
        let mut out = AudioBuffer::new(2, FRAME_SIZE);
        for _ in 0..50 {
            fill(&mut lcg, &mut input);
            let direction = draw_vector(&mut lcg).normalize_or_zero();
            let mut params = BinauralParams::new(direction, &mut hrtf);
            params.interpolation = interpolation;
            effect.apply(params, &input, &mut out);
            assert!(out.as_slice().iter().all(|x| x.is_finite()));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Virtual surround
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn virtual_surround() {
    const FRAME_SIZE: usize = 64;
    let mut lcg = Lcg::new(61);

    for (name, layout) in layouts() {
        let mut hrtf = database(SAMPLING_RATE, FRAME_SIZE);
        let audio = AudioSettings::new(SAMPLING_RATE, FRAME_SIZE);
        let mut effect = VirtualSurroundEffect::new(&audio, &layout, &hrtf);

        let mut input = AudioBuffer::new(layout.num_speakers(), FRAME_SIZE);
        let mut out = AudioBuffer::new(2, FRAME_SIZE);
        let mut recorder = Recorder::default();

        for op in "AATTAARAATTTT".chars() {
            match op {
                'A' => {
                    fill(&mut lcg, &mut input);
                    let state = effect.apply(&mut hrtf, &input, &mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'T' => {
                    let state = effect.tail(&mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'R' => effect.reset(),
                _ => unreachable!(),
            }
        }

        recorder.check(&format!("virtual_surround_effect/{name}"), 1e-5);
    }
}
