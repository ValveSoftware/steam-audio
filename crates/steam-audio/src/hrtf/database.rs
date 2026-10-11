//! HRTFs in the frequency domain, ready for convolution, with nearest-neighbour and interpolated direction queries,
//! spatial blend and Ambisonics HRTFs (upstream `HRTFDatabase`).
//!
//! Two transform sizes are involved: the *interpolation* FFT covers one HRIR (`num_samples` rounded up to a power of
//! two) and is used to interpolate HRTFs as log-magnitude and unwrapped phase; the *audio* FFT covers an audio frame
//! plus an HRIR (`frame + frame / 4 + num_samples - 1`, rounded up) and produces the spectra used for convolution.

use std::fmt;
#[cfg(feature = "sofa")]
use std::path::Path;

use super::loudness::{self, MAX_VOLUME_NORM_GAIN_DB};
use super::{AnyHrtfMap, HrirWeights, MAX_AMBISONICS_ORDER, NUM_EARS};
use crate::Complex;
use crate::effects::AmbisonicsPanningEffect;
use crate::error::Result;
use crate::fft::RealFft;
use crate::math::{PI, Vector3, cpp_min};
use crate::sh;

/// Where the HRTF comes from and how loud it is (upstream `HRTFSettings`).
#[derive(Debug, Clone, Copy, Default)]
pub struct HrtfSettings<'a> {
    /// The HRTF data.
    pub source: HrtfSource<'a>,
    /// Volume adjustment in dB; 0 leaves the HRIRs unscaled.
    pub volume: f32,
    /// Loudness normalization applied before the volume adjustment.
    pub normalization: HrtfNormalization,
}

/// Where HRTF data comes from (upstream `HRTFSettings::type` with `hrtfData`, `sofaFileName` or `sofaData`).
#[derive(Debug, Clone, Copy, Default)]
#[non_exhaustive]
pub enum HrtfSource<'a> {
    /// The built-in HRTF ([`BUILTIN_HRTF_DATA`](super::BUILTIN_HRTF_DATA)).
    #[default]
    BuiltIn,
    /// Data in the `.hrtf` format (see [`HrtfMap`](super::HrtfMap)).
    Data(&'a [u8]),
    /// A SOFA file, read from disk (see [`SofaHrtfMap`](super::SofaHrtfMap)).
    #[cfg(feature = "sofa")]
    SofaFile(&'a Path),
    /// A SOFA file held in memory.
    #[cfg(feature = "sofa")]
    SofaData(&'a [u8]),
}

/// HRTF loudness normalization (upstream `HRTFNormType`).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum HrtfNormalization {
    /// HRIRs are used as stored.
    #[default]
    None,
    /// Every HRIR pair is scaled to the [RMS loudness](loudness::rms_loudness) of the front HRIR pair, boosting by
    /// at most [`MAX_VOLUME_NORM_GAIN_DB`].
    Rms,
}

/// How an HRTF is looked up for a direction (upstream `HRTFInterpolation`).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum HrtfInterpolation {
    /// The measurement nearest to the direction.
    #[default]
    NearestNeighbor,
    /// A bilinear blend of the four surrounding measurements.
    Bilinear,
}

/// Phase response of the "unspatialized" HRTF that spatial blend mixes with (upstream `HRTFPhaseType`). Its
/// magnitude response is always flat.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum HrtfPhaseType {
    /// Zero phase: a spatial blend of 0 removes all spatialization.
    #[default]
    None,
    /// The interaural time difference of a spherical head.
    SphereItd,
    /// The phase of the queried HRTF.
    Full,
}

/// HRTFs for one sampling rate and audio frame size (upstream `HRTFDatabase`).
///
/// Queries that synthesize an HRTF, and nearest-measurement lookups (a SOFA map searches with scratch space), take
/// `&mut self` because they use internal scratch buffers; none of them allocate.
#[derive(Clone)]
pub struct HrtfDatabase {
    map: AnyHrtfMap,
    audio_fft: RealFft,
    /// `NUM_EARS × num_hrirs × num_spectrum_samples`.
    hrtf: Vec<Complex>,
    /// Index of the largest sample of each HRIR, `NUM_EARS × num_hrirs`.
    peak_delays: Vec<usize>,
    /// `NUM_EARS × 16 × num_spectrum_samples`.
    ambisonics_hrtf: Vec<Complex>,
    interpolator: Interpolator,
    /// Scratch: one HRIR zero-padded to the audio FFT size.
    hrir: Vec<f32>,
}

impl HrtfDatabase {
    /// Loads and transforms the HRTF for `sampling_rate` and audio frames of `frame_size` samples. Panics if
    /// `frame_size` is 0.
    pub fn new(settings: &HrtfSettings<'_>, sampling_rate: u32, frame_size: usize) -> Result<Self> {
        assert!(frame_size > 0, "frame size must be positive");

        let mut map = AnyHrtfMap::new(settings, sampling_rate)?;
        let num_hrirs = map.num_hrirs();
        let num_samples = map.num_samples();

        let mut audio_fft = RealFft::new(frame_size + frame_size / 4 + num_samples - 1);
        let num_bins = audio_fft.num_complex_samples();
        let mut hrir = vec![0.0f32; audio_fft.num_real_samples()];
        let mut spectra = [
            vec![Complex::default(); num_bins],
            vec![Complex::default(); num_bins],
        ];

        // Loudness of a pair of HRIRs, transformed with zero-padding.
        let mut pair_loudness = |map: &AnyHrtfMap, index: usize| {
            for (ear, spectrum) in spectra.iter_mut().enumerate() {
                hrir[..num_samples].copy_from_slice(map.hrir(ear, index));
                audio_fft.forward(&hrir, spectrum);
            }
            loudness::rms_loudness([&spectra[0], &spectra[1]], sampling_rate)
        };

        if settings.normalization == HrtfNormalization::Rms {
            let front = map.nearest_hrir(Vector3::new(0.0, 0.0, -1.0));
            let reference = pair_loudness(&map, front);
            let max_gain = loudness::db_to_gain(MAX_VOLUME_NORM_GAIN_DB);
            for index in 0..num_hrirs {
                let gain = cpp_min(
                    max_gain,
                    loudness::gain_scaling(pair_loudness(&map, index), reference),
                );
                for ear in 0..NUM_EARS {
                    let start = (ear * num_hrirs + index) * num_samples;
                    for x in &mut map.hrirs_mut()[start..start + num_samples] {
                        *x *= gain;
                    }
                }
            }
        }

        if settings.volume != 0.0 {
            let gain = loudness::db_to_gain(settings.volume);
            for x in map.hrirs_mut() {
                *x *= gain;
            }
        }

        let hrtf = transform_hrirs(map.hrirs(), num_samples, &mut audio_fft, &mut hrir);
        let peak_delays = map
            .hrirs()
            .chunks_exact(num_samples)
            .map(peak_delay)
            .collect();
        let interpolator = Interpolator::new(&map, sampling_rate);

        let mut database = Self {
            map,
            audio_fft,
            hrtf,
            peak_delays,
            ambisonics_hrtf: Vec::new(),
            interpolator,
            hrir,
        };

        if database.map.ambisonics_hrirs().is_empty() {
            database.precompute_ambisonics_hrtfs();
        } else {
            // Upstream does not apply normalization or volume to stored Ambisonics HRIRs.
            database.ambisonics_hrtf = transform_hrirs(
                database.map.ambisonics_hrirs(),
                num_samples,
                &mut database.audio_fft,
                &mut database.hrir,
            );
        }

        Ok(database)
    }

    /// The underlying HRIRs, after normalization and volume adjustment.
    pub fn map(&self) -> &AnyHrtfMap {
        &self.map
    }

    /// Number of measurement directions (upstream `numHRIRs`).
    pub fn num_hrirs(&self) -> usize {
        self.map.num_hrirs()
    }

    /// HRIR length in samples (upstream `numSamples`).
    pub fn num_samples(&self) -> usize {
        self.map.num_samples()
    }

    /// Number of bins of every HRTF spectrum (upstream `numSpectrumSamples`).
    pub fn num_spectrum_samples(&self) -> usize {
        self.audio_fft.num_complex_samples()
    }

    /// Index of the measurement nearest to `direction` (upstream `nearestHRTF`'s lookup).
    pub fn nearest_index(&mut self, direction: Vector3) -> usize {
        self.map.nearest_hrir(direction)
    }

    /// Left and right HRTF spectra of a measurement (upstream `getHRTFByIndex`).
    pub fn hrtf(&self, index: usize) -> [&[Complex]; NUM_EARS] {
        assert!(index < self.num_hrirs(), "HRTF index out of range");
        let bins = self.num_spectrum_samples();
        let n = self.num_hrirs();
        [0, 1].map(|ear| {
            let start = (ear * n + index) * bins;
            &self.hrtf[start..start + bins]
        })
    }

    /// Sample index of the peak of the left and right HRIRs of a measurement (upstream `nearestHRTF`'s
    /// `peakDelays`).
    pub fn peak_delays(&self, index: usize) -> [usize; NUM_EARS] {
        assert!(index < self.num_hrirs(), "HRTF index out of range");
        [
            self.peak_delays[index],
            self.peak_delays[self.num_hrirs() + index],
        ]
    }

    /// Mixes a measured HRTF with an unspatialized one (flat magnitude, phase from `phase_type`) and writes the
    /// spectra to `out`, `spatial_blend` being the weight of the measured HRTF (upstream `nearestHRTF` with
    /// `spatialBlend < 1`). Panics if an output is not [`num_spectrum_samples`](Self::num_spectrum_samples) long.
    pub fn blended_hrtf(
        &mut self,
        index: usize,
        direction: Vector3,
        spatial_blend: f32,
        phase_type: HrtfPhaseType,
        out: [&mut [Complex]; NUM_EARS],
    ) {
        assert!(index < self.num_hrirs(), "HRTF index out of range");
        for (ear, out) in out.into_iter().enumerate() {
            self.interpolator
                .blend_measurement(ear, index, spatial_blend, phase_type, direction);
            self.synthesize(out);
        }
    }

    /// Bilinearly interpolates the HRTF for `direction`, optionally with spatial blend (a `spatial_blend` below 1;
    /// see [`blended_hrtf`](Self::blended_hrtf)), writes the spectra to `out` and returns the peak index of each
    /// interpolated HRIR (upstream `interpolatedHRTF`). Panics if an output is not
    /// [`num_spectrum_samples`](Self::num_spectrum_samples) long.
    pub fn interpolated_hrtf(
        &mut self,
        direction: Vector3,
        spatial_blend: f32,
        phase_type: HrtfPhaseType,
        out: [&mut [Complex]; NUM_EARS],
    ) -> [usize; NUM_EARS] {
        let weights = self.map.interpolated_hrir_weights(direction);
        let mut peaks = [0; NUM_EARS];
        for (ear, out) in out.into_iter().enumerate() {
            self.interpolator
                .interpolate(ear, &weights, spatial_blend, phase_type, direction);
            peaks[ear] = self.synthesize(out);
        }
        peaks
    }

    /// Left and right spectra of an order-3 Ambisonics HRTF, by ACN coefficient index (upstream `ambisonicsHRTF`).
    pub fn ambisonics_hrtf(&self, coefficient: usize) -> [&[Complex]; NUM_EARS] {
        let num_coeffs = sh::num_coeffs_for_order(MAX_AMBISONICS_ORDER);
        assert!(
            coefficient < num_coeffs,
            "Ambisonics coefficient out of range"
        );
        let bins = self.num_spectrum_samples();
        [0, 1].map(|ear| {
            let start = (ear * num_coeffs + coefficient) * bins;
            &self.ambisonics_hrtf[start..start + bins]
        })
    }

    /// Turns the interpolator's spectrum into an HRIR, then into an audio-sized spectrum in `out`. Returns the HRIR's
    /// peak index.
    fn synthesize(&mut self, out: &mut [Complex]) -> usize {
        let interpolation_size = self.interpolator.fft.num_real_samples();
        self.hrir.fill(0.0);
        self.interpolator.fft.inverse(
            &self.interpolator.hrtf,
            &mut self.hrir[..interpolation_size],
        );
        let peak = peak_delay(&self.hrir[..self.map.num_samples()]);
        self.audio_fft.forward(&self.hrir, out);
        peak
    }

    /// Projects the HRTF onto order-3 spherical harmonics by sampling it at the virtual speakers (upstream
    /// `precomputeAmbisonicsHRTFs`).
    ///
    /// Upstream also computes minimum-phase versions of the HRIRs here and decomposes them into magnitude and phase,
    /// but never uses the result (the projection reads the database's own magnitude and phase), so that is omitted.
    fn precompute_ambisonics_hrtfs(&mut self) {
        let num_coeffs = sh::num_coeffs_for_order(MAX_AMBISONICS_ORDER);
        let num_samples = self.num_samples();
        let bins = self.num_spectrum_samples();
        let interpolation_size = self.interpolator.fft.num_real_samples();
        let interpolation_bins = self.interpolator.fft.num_complex_samples();
        let speaker_weight = (4.0 * PI) / AmbisonicsPanningEffect::NUM_VIRTUAL_SPEAKERS as f32;

        self.ambisonics_hrtf = vec![Complex::default(); NUM_EARS * num_coeffs * bins];

        let mut index = 0;
        for l in 0..=MAX_AMBISONICS_ORDER {
            for m in -(l as i32)..=l as i32 {
                for speaker in AmbisonicsPanningEffect::VIRTUAL_SPEAKERS {
                    let weight = speaker_weight * sh::evaluate(l, m, speaker);
                    let weights = self.map.interpolated_hrir_weights(speaker);
                    for ear in 0..NUM_EARS {
                        self.interpolator.interpolate(
                            ear,
                            &weights,
                            1.0,
                            HrtfPhaseType::None,
                            Vector3::ZERO,
                        );
                        let start = (ear * num_coeffs + index) * bins;
                        let accum = &mut self.ambisonics_hrtf[start..start + interpolation_bins];
                        for (a, &h) in accum.iter_mut().zip(&self.interpolator.hrtf) {
                            *a += h * weight;
                        }
                    }
                }

                for ear in 0..NUM_EARS {
                    let start = (ear * num_coeffs + index) * bins;
                    let spectrum = &mut self.ambisonics_hrtf[start..start + bins];
                    self.hrir.fill(0.0);
                    self.interpolator.fft.inverse(
                        &spectrum[..interpolation_bins],
                        &mut self.hrir[..interpolation_size],
                    );
                    self.hrir[num_samples..interpolation_size].fill(0.0);
                    self.audio_fft.forward(&self.hrir, spectrum);
                }

                index += 1;
            }
        }
    }
}

impl fmt::Debug for HrtfDatabase {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("HrtfDatabase")
            .field("map", &self.map)
            .field("num_spectrum_samples", &self.num_spectrum_samples())
            .finish_non_exhaustive()
    }
}

/// Transforms each HRIR in `hrirs` (rows of `num_samples`), zero-padded to the audio FFT size (upstream
/// `fourierTransformHRIRs`).
fn transform_hrirs(
    hrirs: &[f32],
    num_samples: usize,
    fft: &mut RealFft,
    padded: &mut [f32],
) -> Vec<Complex> {
    let bins = fft.num_complex_samples();
    let mut spectra = vec![Complex::default(); hrirs.len() / num_samples * bins];
    padded.fill(0.0);
    for (hrir, spectrum) in hrirs
        .chunks_exact(num_samples)
        .zip(spectra.chunks_exact_mut(bins))
    {
        padded[..num_samples].copy_from_slice(hrir);
        fft.forward(padded, spectrum);
    }
    spectra
}

/// Index of the first largest positive sample, or 0 (upstream `extractPeakDelay`).
fn peak_delay(hrir: &[f32]) -> usize {
    let mut peak_value = 0.0f32;
    let mut peak_index = 0;
    for (i, &x) in hrir.iter().enumerate() {
        if x > peak_value {
            peak_value = x;
            peak_index = i;
        }
    }
    peak_index
}

/// Per-measurement log-magnitude and unwrapped phase on the interpolation FFT grid, and the scratch used to
/// synthesize one ear's HRTF from them.
#[derive(Clone)]
struct Interpolator {
    fft: RealFft,
    sampling_rate: u32,
    num_hrirs: usize,
    /// `NUM_EARS × num_hrirs × bins`: `ln(|H| + 1e-9)`.
    magnitude: Vec<f32>,
    /// `NUM_EARS × num_hrirs × bins`: unwrapped phase.
    phase: Vec<f32>,
    out_magnitude: Vec<f32>,
    out_phase: Vec<f32>,
    /// The synthesized spectrum of one ear, `bins` long.
    hrtf: Vec<Complex>,
}

impl Interpolator {
    /// Decomposes every HRIR of `map` (upstream `decomposeToMagnitudePhase`).
    fn new(map: &AnyHrtfMap, sampling_rate: u32) -> Self {
        let num_samples = map.num_samples();
        let mut fft = RealFft::new(num_samples);
        let bins = fft.num_complex_samples();

        let mut signal = vec![0.0f32; fft.num_real_samples()];
        let mut spectrum = vec![Complex::default(); bins];
        let mut magnitude = vec![0.0f32; map.hrirs().len() / num_samples * bins];
        let mut phase = vec![0.0f32; magnitude.len()];
        let mut unwrap_scratch = UnwrapScratch::new(bins);

        for ((hrir, mag), ph) in map
            .hrirs()
            .chunks_exact(num_samples)
            .zip(magnitude.chunks_exact_mut(bins))
            .zip(phase.chunks_exact_mut(bins))
        {
            signal[..num_samples].copy_from_slice(hrir);
            fft.forward(&signal, &mut spectrum);
            for ((m, p), s) in mag.iter_mut().zip(ph.iter_mut()).zip(&spectrum) {
                *m = (s.norm() + 1e-9).ln();
                *p = s.arg();
            }
            unwrap_scratch.unwrap_phase(ph);
        }

        Self {
            fft,
            sampling_rate,
            num_hrirs: map.num_hrirs(),
            magnitude,
            phase,
            out_magnitude: vec![0.0; bins],
            out_phase: vec![0.0; bins],
            hrtf: vec![Complex::default(); bins],
        }
    }

    fn row(&self, ear: usize, index: usize) -> std::ops::Range<usize> {
        let bins = self.out_magnitude.len();
        let start = (ear * self.num_hrirs + index) * bins;
        start..start + bins
    }

    /// Spatial blend of one measurement into `self.hrtf` (the per-ear body of upstream `nearestHRTF`).
    fn blend_measurement(
        &mut self,
        ear: usize,
        index: usize,
        spatial_blend: f32,
        phase_type: HrtfPhaseType,
        direction: Vector3,
    ) {
        let row = self.row(ear, index);
        self.out_magnitude
            .copy_from_slice(&self.magnitude[row.clone()]);
        self.out_phase.copy_from_slice(&self.phase[row]);
        self.apply_spatial_blend(ear, spatial_blend, phase_type, direction);
        self.finish();
    }

    /// Blends the measurements in `weights` as log-magnitude and unwrapped phase into `self.hrtf` (upstream
    /// `interpolateHRIRs`, one ear).
    fn interpolate(
        &mut self,
        ear: usize,
        weights: &HrirWeights,
        spatial_blend: f32,
        phase_type: HrtfPhaseType,
        direction: Vector3,
    ) {
        // Upstream compacts the valid entries into arrays initialized to index 0 and weight 0.
        let mut valid_indices = [0usize; 8];
        let mut valid_weights = [0.0f32; 8];
        let mut num_valid = 0;
        for (index, &weight) in weights.indices.iter().zip(&weights.weights) {
            if let Some(index) = *index {
                valid_indices[num_valid] = index;
                valid_weights[num_valid] = weight;
                num_valid += 1;
            }
        }

        let row = self.row(ear, valid_indices[0]);
        for ((om, op), (&m, &p)) in self
            .out_magnitude
            .iter_mut()
            .zip(self.out_phase.iter_mut())
            .zip(self.magnitude[row.clone()].iter().zip(&self.phase[row]))
        {
            *om = m * valid_weights[0];
            *op = p * valid_weights[0];
        }
        for j in 1..num_valid {
            let row = self.row(ear, valid_indices[j]);
            let w = valid_weights[j];
            for ((om, op), (&m, &p)) in self
                .out_magnitude
                .iter_mut()
                .zip(self.out_phase.iter_mut())
                .zip(self.magnitude[row.clone()].iter().zip(&self.phase[row]))
            {
                *om += w * m;
                *op += w * p;
            }
        }

        if spatial_blend < 1.0 {
            self.apply_spatial_blend(ear, spatial_blend, phase_type, direction);
        }
        self.finish();
    }

    /// Mixes `out_magnitude`/`out_phase` in place with a flat-magnitude response (upstream `applySpatialBlend`).
    fn apply_spatial_blend(
        &mut self,
        ear: usize,
        spatial_blend: f32,
        phase_type: HrtfPhaseType,
        direction: Vector3,
    ) {
        for m in &mut self.out_magnitude {
            *m = (1.0 - spatial_blend) * 0.0 + spatial_blend * *m;
        }

        match phase_type {
            HrtfPhaseType::None => {
                for p in &mut self.out_phase {
                    *p *= spatial_blend;
                }
            }
            HrtfPhaseType::SphereItd => {
                let itd_delay = sphere_itd(direction)[ear];
                let itd_delay_in_samples = (itd_delay * self.sampling_rate as f32).floor();
                let num_real_samples = self.fft.num_real_samples() as f32;
                for (k, p) in self.out_phase.iter_mut().enumerate() {
                    let w = k as f32 * (2.0 * PI) / num_real_samples;
                    let itd_phase = -w * itd_delay_in_samples;
                    *p = (1.0 - spatial_blend) * itd_phase + spatial_blend * *p;
                }
            }
            HrtfPhaseType::Full => {}
        }
    }

    /// Wraps the phase and converts log-magnitude and phase to `self.hrtf`.
    fn finish(&mut self) {
        wrap_phase(&mut self.out_phase);
        for ((h, &m), &p) in self
            .hrtf
            .iter_mut()
            .zip(&self.out_magnitude)
            .zip(&self.out_phase)
        {
            *h = Complex::from_polar(m.exp(), p);
        }
    }
}

/// Left and right ear delays in seconds for a spherical head of 9 cm, relative to the nearer ear's earliest
/// possible arrival (upstream `calcSphereITD`).
fn sphere_itd(direction: Vector3) -> [f32; NUM_EARS] {
    const HEAD_SIZE: f32 = 0.09;
    const MIN_DISTANCE: f32 = 1.0 - (HEAD_SIZE / 2.0);
    const MAX_DISTANCE: f32 = 1.0 + (HEAD_SIZE / 2.0);
    const SPEED_OF_SOUND: f32 = 340.0;
    const MAX_ITD: f32 = HEAD_SIZE / SPEED_OF_SOUND;

    let left_ear = -0.5 * HEAD_SIZE * Vector3::X_AXIS;
    let right_ear = 0.5 * HEAD_SIZE * Vector3::X_AXIS;

    [left_ear, right_ear].map(|ear| {
        let distance = (direction - ear).length();
        let fraction = (distance - MIN_DISTANCE) / (MAX_DISTANCE - MIN_DISTANCE);
        fraction * MAX_ITD
    })
}

/// Scratch for [`UnwrapScratch::unwrap_phase`] (upstream `unwrapPhase`'s temporary arrays).
struct UnwrapScratch {
    correction: Vec<f32>,
}

impl UnwrapScratch {
    fn new(len: usize) -> Self {
        Self {
            correction: vec![0.0; len],
        }
    }

    /// Removes 2π jumps between consecutive bins (upstream `unwrapPhase`, one row).
    fn unwrap_phase(&mut self, data: &mut [f32]) {
        let n = data.len();
        if n < 2 {
            return;
        }
        let two_pi = 2.0 * PI;

        for k in 0..n - 1 {
            let delta = data[k + 1] - data[k];
            let mut equivalent = (delta + PI) - ((delta + PI) / two_pi).floor() * two_pi - PI;
            if equivalent == -PI && delta > 0.0 {
                equivalent = PI;
            }
            let mut correction = equivalent - delta;
            if delta.abs() < PI {
                correction = 0.0;
            }
            self.correction[k] = correction;
        }

        // Cumulative sum of the corrections, added from the second bin on.
        let mut cumulative = 0.0f32;
        for k in 0..n - 1 {
            cumulative = if k == 0 {
                self.correction[0]
            } else {
                cumulative + self.correction[k]
            };
            data[k + 1] += cumulative;
        }
    }
}

/// Wraps phases into `[-π, π]` (upstream `wrapPhase`). Upstream's 4-wide SIMD body rounds half to even through an
/// `int` conversion and multiplies by `1 / 2π`; its scalar tail rounds half away from zero and divides by 2π.
fn wrap_phase(phase: &mut [f32]) {
    let body = phase.len() & !3;
    let two_pi = 2.0 * PI;
    let two_pi_inverse = 1.0 / (2.0 * PI);

    for x in &mut phase[..body] {
        *x -= two_pi * cvtps_round(*x * two_pi_inverse);
    }
    for x in &mut phase[body..] {
        *x -= two_pi * (*x / two_pi).round();
    }
}

/// `_mm_cvtepi32_ps(_mm_cvtps_epi32(x))`: round half to even; NaN and values outside the `i32` range become
/// `i32::MIN`.
fn cvtps_round(x: f32) -> f32 {
    let r = x.round_ties_even();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&r) {
        r
    } else {
        -2_147_483_648.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_send<T: Send>() {}

    #[test]
    fn database_is_send() {
        assert_send::<HrtfDatabase>();
    }

    /// Upstream `HRTFDatabase.test.cpp`.
    #[test]
    fn built_in_hrtf_is_loaded_and_parsed() {
        let database = HrtfDatabase::new(&HrtfSettings::default(), 44100, 1024).unwrap();
        assert_eq!(database.num_hrirs(), 1250);
        assert_eq!(database.num_samples(), 200);
    }

    #[test]
    fn wrap_phase_rounding() {
        // SIMD body: half to even, out-of-range and NaN to i32::MIN.
        assert_eq!(cvtps_round(0.5), 0.0);
        assert_eq!(cvtps_round(1.5), 2.0);
        assert_eq!(cvtps_round(-2.5), -2.0);
        assert_eq!(cvtps_round(f32::NAN), -2_147_483_648.0);
        assert_eq!(cvtps_round(3e9), -2_147_483_648.0);

        // Scalar tail: half a turn (exactly π / 2π = 0.5) rounds away from zero.
        let mut phase = [0.0, 0.0, 0.0, 0.0, PI];
        wrap_phase(&mut phase);
        assert_eq!(phase[4], PI - 2.0 * PI);
    }
}
