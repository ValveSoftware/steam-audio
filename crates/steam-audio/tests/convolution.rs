//! Parity tests for stage 3.2 against golden data from `tools/golden/src/convolution.cpp`. The upstream Catch test
//! `ConvolutionEffect.test.cpp` is empty; correctness against direct convolution is unit-tested in `src/effects/`.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order. Overlap-save goldens are mono because the
//! reference build crashes with more channels (see the generator); multi-channel effects and mixers are checked
//! against mono ones here, which is exact because upstream processes channels independently.

use parity::{Element, Lcg, Tolerance, compare, golden};
use steam_audio::Complex;
use steam_audio::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use steam_audio::effects::{
    OverlapAddConvolutionEffect, OverlapAddConvolutionSettings, OverlapSaveConvolutionEffect,
    OverlapSaveConvolutionMixer, OverlapSaveConvolutionSettings, OverlapSaveFir,
    OverlapSavePartitioner,
};

const SAMPLING_RATE: u32 = 48000;

#[track_caller]
fn check<T: Element + Into<f64>>(name: &str, actual: &[T], tol: impl Fn(&[T]) -> Tolerance) {
    let expected = golden::<T>(name);
    if let Err(m) = compare(actual, &expected.data, tol(&expected.data)) {
        panic!("{name}: {m}");
    }
}

fn exact<T>(_: &[T]) -> Tolerance {
    Tolerance::EXACT
}

fn signal(expected: &[f32]) -> Tolerance {
    Tolerance::peak_relative(expected, 1e-5)
}

fn fill(lcg: &mut Lcg, buffer: &mut AudioBuffer) {
    for s in buffer.as_mut_slice() {
        *s = lcg.next_signed();
    }
}

/// A random spectrum of a real signal (real DC and Nyquist bins; see risk R11 in `docs/PORTING_PLAN.md`).
fn fill_spectrum(lcg: &mut Lcg, spectrum: &mut [Complex]) {
    for c in spectrum.iter_mut() {
        let re = lcg.next_signed();
        let im = lcg.next_signed();
        *c = Complex::new(re, im);
    }
    let last = spectrum.len() - 1;
    spectrum[0].im = 0.0;
    spectrum[last].im = 0.0;
}

fn fill_fir(lcg: &mut Lcg, fir: &mut OverlapSaveFir) {
    for i in 0..fir.num_channels() {
        for j in 0..fir.num_blocks() {
            fill_spectrum(lcg, fir.block_mut(i, j));
        }
    }
}

fn state_code(state: AudioEffectState) -> i32 {
    match state {
        AudioEffectState::TailRemaining => 0,
        AudioEffectState::TailComplete => 1,
    }
}

/// Outputs and `[state, tail samples remaining]` rows, as the generator's `Recorder`.
#[derive(Default, PartialEq, Debug)]
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
    fn check(&self, name: &str) {
        check(&format!("convolution/{name}"), &self.outputs, signal);

        // Upstream's overlap-save tail counter goes negative when `tail` is called past the end; the port reports 0
        // (DEVIATIONS.md).
        let mut expected = golden::<i32>(&format!("convolution/{name}_states")).data;
        for v in expected.iter_mut().skip(1).step_by(2) {
            *v = (*v).max(0);
        }
        assert_eq!(self.states, expected, "{name}_states");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Overlap-add
// ---------------------------------------------------------------------------------------------------------------------

struct OverlapAddCase {
    name: &'static str,
    num_channels: usize,
    in_channels: usize,
    ir_size: usize,
    frame_size: usize,
    script: &'static str,
}

const OVERLAP_ADD_CASES: [OverlapAddCase; 3] = [
    OverlapAddCase {
        name: "overlap_add_mono",
        num_channels: 2,
        in_channels: 1,
        ir_size: 100,
        frame_size: 64,
        script: "IAAAIAATTTTAA",
    },
    OverlapAddCase {
        name: "overlap_add_multi",
        num_channels: 3,
        in_channels: 3,
        ir_size: 50,
        frame_size: 37,
        script: "IMMAIMMTTTTRMM",
    },
    OverlapAddCase {
        name: "overlap_add_long",
        num_channels: 2,
        in_channels: 1,
        ir_size: 1000,
        frame_size: 128,
        script: "IAIAATTTTAAA",
    },
];

#[test]
fn overlap_add() {
    let mut lcg = Lcg::new(41);

    for c in &OVERLAP_ADD_CASES {
        let audio = AudioSettings::new(SAMPLING_RATE, c.frame_size);
        let mut effect = OverlapAddConvolutionEffect::new(
            &audio,
            &OverlapAddConvolutionSettings {
                num_channels: c.num_channels,
                ir_size: c.ir_size,
            },
        );

        let mut ir = vec![vec![Complex::default(); effect.ir_spectrum_size()]; c.num_channels];
        let mut input = AudioBuffer::new(c.in_channels, c.frame_size);
        let mut out = AudioBuffer::new(c.num_channels, c.frame_size);
        let mut recorder = Recorder::default();

        for op in c.script.chars() {
            match op {
                'I' => ir.iter_mut().for_each(|s| fill_spectrum(&mut lcg, s)),
                'A' | 'M' => {
                    fill(&mut lcg, &mut input);
                    let spectra: Vec<&[Complex]> = ir.iter().map(Vec::as_slice).collect();
                    let state = effect.apply(&spectra, op == 'M', &input, &mut out);
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

        recorder.check(c.name);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Partitioner
// ---------------------------------------------------------------------------------------------------------------------

fn flatten(fir: &OverlapSaveFir) -> Vec<f32> {
    let mut v = Vec::new();
    for i in 0..fir.num_channels() {
        for j in 0..fir.num_blocks() {
            v.extend(fir.block(i, j).iter().flat_map(|c| [c.re, c.im]));
        }
    }
    v
}

#[test]
fn partitioner() {
    // The IR's Lcg draws (seed 43) are stored by the generator; it is input data, so use it directly.
    let ir = golden::<f32>("convolution/partition_ir");
    let (num_channels, num_samples) = (ir.shape[0], ir.shape[1]);
    let mut lcg = Lcg::new(43);
    assert!(ir.data.iter().all(|&x| x == lcg.next_signed()), "IR draws");
    let ir = AudioBuffer::from_planar(num_channels, num_samples, ir.data);

    let mut partitioner = OverlapSavePartitioner::new(64);
    let mut fir = OverlapSaveFir::new(3, 300, 64);

    partitioner.partition(&ir, 2, 250, &mut fir);
    check("convolution/partition_a", &flatten(&fir), signal);

    partitioner.partition(&ir, 8, 150, &mut fir);
    check("convolution/partition_b", &flatten(&fir), signal);

    let mut short_fir = OverlapSaveFir::new(4, 100, 64);
    partitioner.partition(&ir, 4, 1000, &mut short_fir);
    check("convolution/partition_c", &flatten(&short_fir), signal);
}

// ---------------------------------------------------------------------------------------------------------------------
// Overlap-save effect
// ---------------------------------------------------------------------------------------------------------------------

struct OverlapSaveCase {
    name: &'static str,
    ir_size: usize,
    frame_size: usize,
    script: &'static str,
}

const OVERLAP_SAVE_CASES: [OverlapSaveCase; 3] = [
    OverlapSaveCase {
        name: "overlap_save_a",
        ir_size: 150,
        frame_size: 64,
        script: "UAAAUATTTARAUA",
    },
    OverlapSaveCase {
        name: "overlap_save_b",
        ir_size: 100,
        frame_size: 37,
        script: "UAUAATTTUA",
    },
    OverlapSaveCase {
        name: "overlap_save_c",
        ir_size: 64,
        frame_size: 64,
        script: "UATAUT",
    },
];

/// One drawn step of an overlap-save script. `firs[i]` is channel `i`'s mono FIR.
enum Step {
    Apply {
        firs: Option<Vec<OverlapSaveFir>>,
        input: AudioBuffer,
        all_channels: bool,
    },
    Tail,
    Reset,
}

/// Draws a script's data: on `U`/`u`, channel `i`'s FIR from `lcgs[i]`; then the input from `lcgs[0]`. `U`/`A` apply
/// to all channels, `u`/`a` to the first only.
fn draw_steps(lcgs: &mut [Lcg], ir_size: usize, frame_size: usize, script: &str) -> Vec<Step> {
    script
        .chars()
        .map(|op| match op {
            'U' | 'u' | 'A' | 'a' => {
                let firs = op.eq_ignore_ascii_case(&'u').then(|| {
                    lcgs.iter_mut()
                        .map(|lcg| {
                            let mut fir = OverlapSaveFir::new(1, ir_size, frame_size);
                            fill_fir(lcg, &mut fir);
                            fir
                        })
                        .collect()
                });
                let mut input = AudioBuffer::new(1, frame_size);
                fill(&mut lcgs[0], &mut input);
                Step::Apply {
                    firs,
                    input,
                    all_channels: op.is_ascii_uppercase(),
                }
            }
            'T' => Step::Tail,
            'R' => Step::Reset,
            _ => unreachable!(),
        })
        .collect()
}

/// A FIR whose channels are the given channels of `firs`.
fn gather(
    firs: &[OverlapSaveFir],
    channels: &[usize],
    ir_size: usize,
    frame_size: usize,
) -> OverlapSaveFir {
    let mut fir = OverlapSaveFir::new(channels.len(), ir_size, frame_size);
    for (i, &c) in channels.iter().enumerate() {
        for j in 0..fir.num_blocks() {
            fir.block_mut(i, j).copy_from_slice(firs[c].block(0, j));
        }
    }
    fir
}

/// Runs `steps` on an effect with the given channels of the drawn FIRs. A step on the first channel only processes
/// none of them unless `channels` starts with channel 0, which is what a multi-channel effect does to its other
/// channels.
fn run_overlap_save(
    steps: &[Step],
    channels: &[usize],
    ir_size: usize,
    frame_size: usize,
) -> Recorder {
    let num_channels = channels.len();
    let audio = AudioSettings::new(SAMPLING_RATE, frame_size);
    let mut effect = OverlapSaveConvolutionEffect::new(
        &audio,
        &OverlapSaveConvolutionSettings {
            num_channels,
            ir_size,
        },
    );
    let mut out = AudioBuffer::new(num_channels, frame_size);
    let mut recorder = Recorder::default();

    for step in steps {
        match step {
            Step::Apply {
                firs,
                input,
                all_channels,
            } => {
                let mut fir = firs
                    .as_ref()
                    .map(|f| gather(f, channels, ir_size, frame_size));
                let n = match (all_channels, channels[0]) {
                    (true, _) => num_channels,
                    (false, 0) => 1,
                    (false, _) => 0,
                };
                let state = effect.apply(fir.as_mut(), n, input, &mut out);
                recorder.record(&out, state, effect.num_tail_samples_remaining());
            }
            Step::Tail => {
                let state = effect.tail(&mut out);
                recorder.record(&out, state, effect.num_tail_samples_remaining());
            }
            Step::Reset => effect.reset(),
        }
    }

    recorder
}

#[test]
fn overlap_save() {
    let mut lcg = [Lcg::new(47)];
    for c in &OVERLAP_SAVE_CASES {
        let steps = draw_steps(&mut lcg, c.ir_size, c.frame_size, c.script);
        run_overlap_save(&steps, &[0], c.ir_size, c.frame_size).check(c.name);
    }
}

/// Channel `channel` of every recorded frame of `num_channels` channels.
fn channel_of(outputs: &[f32], num_channels: usize, frame_size: usize, channel: usize) -> Vec<f32> {
    outputs
        .chunks(num_channels * frame_size)
        .flat_map(|frame| &frame[channel * frame_size..(channel + 1) * frame_size])
        .copied()
        .collect()
}

/// A multi-channel effect produces, channel by channel, exactly what mono effects with the same IRs produce.
#[test]
fn overlap_save_channels_are_independent() {
    let script = "UAAuAaTARUATTTT";
    for (ir_size, frame_size) in [(150, 64), (100, 37)] {
        let mut lcgs = [101, 202, 303].map(Lcg::new);
        let steps = draw_steps(&mut lcgs, ir_size, frame_size, script);
        let multi = run_overlap_save(&steps, &[0, 1, 2], ir_size, frame_size);

        for c in 0..3 {
            let mono = run_overlap_save(&steps, &[c], ir_size, frame_size);
            assert_eq!(
                channel_of(&multi.outputs, 3, frame_size, c),
                mono.outputs,
                "channel {c} (IR {ir_size}, frame {frame_size})"
            );
            assert_eq!(multi.states, mono.states);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Overlap-save mixer
// ---------------------------------------------------------------------------------------------------------------------

const MIXER_IR_SIZES: [usize; 2] = [150, 100];
const MIXER_FRAME_SIZE: usize = 64;

/// Per frame: each effect's operation (`U`, `A`, `T`, or `-` when skipped), then whether the mixer outputs all its
/// channels (or none).
const MIXER_FRAMES: [([char; 2], bool); 9] = [
    (['U', 'U'], true),
    (['A', 'A'], true),
    (['A', 'U'], true),
    (['U', 'A'], false),
    (['A', '-'], true),
    (['T', 'T'], true),
    (['T', 'T'], true),
    (['-', 'T'], true),
    (['-', '-'], true),
];

/// Draws each effect's steps frame by frame, interleaved as in the generator.
fn draw_mixer_steps(lcgs: &mut [Lcg]) -> Vec<[Option<Step>; 2]> {
    MIXER_FRAMES
        .iter()
        .map(|(ops, _)| {
            [0, 1].map(|e| match ops[e] {
                '-' => None,
                op => draw_steps(lcgs, MIXER_IR_SIZES[e], MIXER_FRAME_SIZE, &op.to_string()).pop(),
            })
        })
        .collect()
}

/// Two effects with the given channels of the drawn FIRs, mixed into one mixer. Returns the mixer outputs and the
/// effect states (-1 when skipped).
fn run_mixer(frames: &[[Option<Step>; 2]], channels: &[usize]) -> (Vec<f32>, Vec<i32>) {
    let num_channels = channels.len();
    let audio = AudioSettings::new(SAMPLING_RATE, MIXER_FRAME_SIZE);
    let mut mixer = OverlapSaveConvolutionMixer::new(
        &audio,
        &OverlapSaveConvolutionSettings {
            num_channels,
            ir_size: 150,
        },
    );
    let mut effects = MIXER_IR_SIZES.map(|ir_size| {
        OverlapSaveConvolutionEffect::new(
            &audio,
            &OverlapSaveConvolutionSettings {
                num_channels,
                ir_size,
            },
        )
    });

    let mut out = AudioBuffer::new(num_channels, MIXER_FRAME_SIZE);
    let mut outputs = Vec::new();
    let mut states = Vec::new();

    for (steps, (_, mix_all)) in frames.iter().zip(&MIXER_FRAMES) {
        for (e, step) in steps.iter().enumerate() {
            let state = match step {
                Some(Step::Apply { firs, input, .. }) => {
                    let mut fir = firs
                        .as_ref()
                        .map(|f| gather(f, channels, MIXER_IR_SIZES[e], MIXER_FRAME_SIZE));
                    effects[e].apply_to_mixer(fir.as_mut(), num_channels, input, &mut mixer)
                }
                Some(Step::Tail) => effects[e].tail_to_mixer(&mut mixer),
                Some(Step::Reset) => unreachable!(),
                None => {
                    states.push(-1);
                    continue;
                }
            };
            states.push(state_code(state));
        }

        mixer.apply(if *mix_all { num_channels } else { 0 }, &mut out);
        outputs.extend_from_slice(out.as_slice());
    }

    (outputs, states)
}

#[test]
fn mixer() {
    let frames = draw_mixer_steps(&mut [Lcg::new(53)]);
    let (outputs, states) = run_mixer(&frames, &[0]);
    check("convolution/mixer", &outputs, signal);
    check("convolution/mixer_states", &states, exact);
}

#[test]
fn mixer_channels_are_independent() {
    let frames = draw_mixer_steps(&mut [11, 22].map(Lcg::new));
    let (multi, multi_states) = run_mixer(&frames, &[0, 1]);
    for c in 0..2 {
        let (mono, mono_states) = run_mixer(&frames, &[c]);
        assert_eq!(
            channel_of(&multi, 2, MIXER_FRAME_SIZE, c),
            mono,
            "channel {c}"
        );
        assert_eq!(multi_states, mono_states);
    }
}
