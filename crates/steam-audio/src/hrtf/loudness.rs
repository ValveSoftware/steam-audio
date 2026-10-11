//! Loudness measurement and gain conversions used for HRTF volume normalization (upstream `Loudness`).

use super::NUM_EARS;
use crate::Complex;

/// Lowest level in dB; quieter gains map to it (upstream `kMinDBLevel`).
pub const MIN_DB_LEVEL: f32 = -90.0;

/// Gain at or below which [`gain_to_db`] returns [`MIN_DB_LEVEL`] (upstream `kGainForMinDB`).
pub const GAIN_FOR_MIN_DB: f32 = 0.000032;

/// Largest boost RMS normalization applies to an HRIR, in dB (upstream `sMaxVolumeNormGainDB`, a mutable global
/// that nothing changes).
pub const MAX_VOLUME_NORM_GAIN_DB: f32 = 12.0;

/// Upper edges of the frequency bands averaged by [`rms_loudness`], in Hz; the last band is open-ended.
const BAND_EDGES: [i32; 8] = [90, 180, 360, 720, 1440, 2880, 5760, i32::MAX];

/// Converts a linear gain to dB (upstream `gainToDB`).
pub fn gain_to_db(gain: f32) -> f32 {
    if gain <= GAIN_FOR_MIN_DB {
        return MIN_DB_LEVEL;
    }
    20.0 * gain.log10()
}

/// Converts a level in dB to a linear gain (upstream `dBToGain`).
pub fn db_to_gain(db: f32) -> f32 {
    if db <= MIN_DB_LEVEL {
        return 0.0;
    }
    10.0f32.powf(db * (1.0 / 20.0))
}

/// Gain that brings a level of `in_db` to `ref_db` (upstream `calculateGainScaling`).
pub fn gain_scaling(in_db: f32, ref_db: f32) -> f32 {
    db_to_gain(ref_db - in_db)
}

/// Loudness in dB of a pair of HRTF spectra: the mean power of both ears in each of 8 octave-spaced bands, averaged
/// over the bands (upstream `calculateRMSLoudness`). Panics if the spectra are empty or differ in length.
pub fn rms_loudness(hrtf: [&[Complex]; NUM_EARS], sampling_rate: u32) -> f32 {
    let num_spectrum_samples = hrtf[0].len();
    assert!(num_spectrum_samples > 0, "empty spectrum");
    assert_eq!(
        hrtf[1].len(),
        num_spectrum_samples,
        "spectrum lengths differ"
    );

    let mut band_loudness = [0.0f32; BAND_EDGES.len()];
    let mut band = 0;
    let mut count = 0;
    for j in 0..num_spectrum_samples {
        for ear in hrtf {
            band_loudness[band] += ear[j].norm_sqr();
            count += 1;
        }

        // Upstream multiplies in `int` before converting to `float`.
        let freq =
            (j as u64 * u64::from(sampling_rate)) as f32 / (2.0 * num_spectrum_samples as f32);
        if freq > BAND_EDGES[band] as f32 || j == num_spectrum_samples - 1 {
            if count > 0 {
                band_loudness[band] /= count as f32;
            }
            count = 0;
            band += 1;
        }
    }

    let mut rms = 0.0f32;
    for loudness in band_loudness {
        rms += loudness / BAND_EDGES.len() as f32;
    }

    gain_to_db(rms.sqrt())
}
