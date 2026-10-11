//! Loading and direction queries of HRTF data in upstream's `.hrtf` format (upstream `HRTFMap`).
//!
//! Layout (little-endian):
//!
//! ```text
//! FOURCC ('HRTF', not checked)                    i32
//! format version (2 or 3)                         i32
//! number of measurement directions (D)            i32
//! for each direction: elevation, azimuth          f32, f32   (canonical spherical coordinates, degrees)
//! number of sampling rates (R)                    i32
//! for each sampling rate: rate (Hz), length (N)   i32, i32
//! for each sampling rate, for each ear, for each direction: HRIR     N × f32
//! version 3, for each sampling rate:
//!     Ambisonics order (3), rate (Hz), length (N) i32, i32, i32
//!     for each ear, for each of 16 coefficients: HRIR                N × f32
//! ```
//!
//! The data is assumed to be measured on a regular grid in interaural coordinates (as CIPIC is): rings of equal
//! azimuth with the same number of elevations each, stored ring after ring.

use std::fmt;

use super::{MAX_AMBISONICS_ORDER, NUM_EARS};
use crate::error::{Error, Result};
use crate::math::{DEGREES_TO_RADIANS, InterauralSphericalVector3, SphericalVector3, Vector3};
use crate::sh;

const MIN_SUPPORTED_VERSION: i32 = 2;
const MAX_SUPPORTED_VERSION: i32 = 3;
const MIN_VERSION_WITH_SH_DATA: i32 = 3;

/// Measurement indices and weights to blend for a direction (upstream `interpolatedHRIRWeights` output). Unused
/// slots have no index and a weight of 0.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct HrirWeights {
    pub indices: [Option<usize>; 8],
    pub weights: [f32; 8],
}

/// HRIRs measured at a set of directions, for one sampling rate, with nearest-neighbour and bilinear direction
/// queries (upstream `HRTFMap`, the `IHRTFMap` for the built-in data format).
#[derive(Clone)]
pub struct HrtfMap {
    num_hrirs: usize,
    num_samples: usize,
    num_elevations: usize,
    /// Unique interaural azimuths in degrees, ascending.
    azimuths: Vec<f32>,
    /// Interaural elevations in degrees, `num_elevations` per azimuth, in file order.
    elevations: Vec<f32>,
    /// `NUM_EARS × num_hrirs × num_samples`.
    hrirs: Vec<f32>,
    /// `NUM_EARS × 16 × num_samples`, or empty for version-2 data.
    ambisonics_hrirs: Vec<f32>,
}

impl HrtfMap {
    /// Loads the built-in HRTF ([`BUILTIN_HRTF_DATA`](super::BUILTIN_HRTF_DATA)) for `sampling_rate`, which must be
    /// 44100, 48000 or 24000 Hz.
    pub fn new(sampling_rate: u32) -> Result<Self> {
        Self::from_bytes(super::BUILTIN_HRTF_DATA, sampling_rate)
    }

    /// Loads HRTF data in the `.hrtf` format for `sampling_rate` (upstream `HRTFMap(samplingRate, hrtfData)`).
    pub fn from_bytes(data: &[u8], sampling_rate: u32) -> Result<Self> {
        let mut reader = Reader { data, pos: 0 };

        reader.skip(4)?;
        let version = reader.i32()?;
        if !(MIN_SUPPORTED_VERSION..=MAX_SUPPORTED_VERSION).contains(&version) {
            return Err(Error::UnsupportedHrtfVersion(version));
        }

        let num_hrirs = reader.count()?;
        if num_hrirs == 0 {
            return Err(Error::MalformedHrtfData("no measurement directions"));
        }

        // Directions are stored in canonical spherical coordinates, but were measured on a regular grid in
        // interaural coordinates, which the queries use.
        let mut directions = vec![0.0f32; 2 * num_hrirs];
        reader.f32s(&mut directions)?;
        let mut interaural_azimuths = Vec::with_capacity(num_hrirs);
        let mut interaural_elevations = Vec::with_capacity(num_hrirs);
        for direction in directions.chunks_exact(2) {
            let canonical = SphericalVector3::new(
                1.0,
                direction[0] * DEGREES_TO_RADIANS,
                direction[1] * DEGREES_TO_RADIANS,
            );
            let interaural = InterauralSphericalVector3::from(canonical);
            interaural_azimuths.push((interaural.azimuth / DEGREES_TO_RADIANS).round() as i32);
            interaural_elevations.push((interaural.elevation / DEGREES_TO_RADIANS).round() as i32);
        }

        interaural_azimuths.sort_unstable();
        interaural_azimuths.dedup();
        let azimuths: Vec<f32> = interaural_azimuths.iter().map(|&a| a as f32).collect();
        let num_elevations = num_hrirs / azimuths.len();
        let elevations = interaural_elevations[..azimuths.len() * num_elevations]
            .iter()
            .map(|&e| e as f32)
            .collect();

        let num_sampling_rates = reader.count()?;
        let mut lengths = Vec::with_capacity(num_sampling_rates);
        let mut selected = None;
        for i in 0..num_sampling_rates {
            let rate = reader.i32()?;
            lengths.push(reader.count()?);
            if selected.is_none() && i64::from(rate) == i64::from(sampling_rate) {
                selected = Some(i);
            }
        }
        let selected = selected.ok_or(Error::UnsupportedSamplingRate(sampling_rate))?;
        let num_samples = lengths[selected];
        if num_samples == 0 {
            return Err(Error::MalformedHrtfData("empty HRIRs"));
        }

        let hrir_set_floats = |length: usize| {
            (NUM_EARS * num_hrirs)
                .checked_mul(length)
                .ok_or(Error::MalformedHrtfData("HRIR data too large"))
        };

        for &length in &lengths[..selected] {
            reader.skip_f32s(hrir_set_floats(length)?)?;
        }
        let mut hrirs = vec![0.0f32; hrir_set_floats(num_samples)?];
        reader.f32s(&mut hrirs)?;
        for &length in &lengths[selected + 1..] {
            reader.skip_f32s(hrir_set_floats(length)?)?;
        }

        let mut ambisonics_hrirs = Vec::new();
        if version >= MIN_VERSION_WITH_SH_DATA {
            let num_coeffs = sh::num_coeffs_for_order(MAX_AMBISONICS_ORDER);
            loop {
                let order = reader.i32()?;
                if order != MAX_AMBISONICS_ORDER as i32 {
                    return Err(Error::UnsupportedAmbisonicsOrder(order));
                }
                let rate = reader.i32()?;
                let length = reader.count()?;
                if i64::from(rate) == i64::from(sampling_rate) {
                    // Upstream reads the length of the selected HRIRs, not the length stored here.
                    ambisonics_hrirs = vec![0.0f32; NUM_EARS * num_coeffs * num_samples];
                    reader.f32s(&mut ambisonics_hrirs)?;
                    break;
                }
                let floats = (NUM_EARS * num_coeffs)
                    .checked_mul(length)
                    .ok_or(Error::MalformedHrtfData("Ambisonics HRIR data too large"))?;
                reader.skip_f32s(floats)?;
            }
        }

        Ok(Self {
            num_hrirs,
            num_samples,
            num_elevations,
            azimuths,
            elevations,
            hrirs,
            ambisonics_hrirs,
        })
    }

    /// Number of measurement directions (upstream `numHRIRs`).
    pub fn num_hrirs(&self) -> usize {
        self.num_hrirs
    }

    /// HRIR length in samples (upstream `numSamples`).
    pub fn num_samples(&self) -> usize {
        self.num_samples
    }

    /// HRIR of one ear (0 = left, 1 = right) for one measurement direction.
    pub fn hrir(&self, ear: usize, index: usize) -> &[f32] {
        assert!(
            ear < NUM_EARS && index < self.num_hrirs,
            "HRIR index out of range"
        );
        let start = (ear * self.num_hrirs + index) * self.num_samples;
        &self.hrirs[start..start + self.num_samples]
    }

    /// All HRIRs, `NUM_EARS × num_hrirs × num_samples` (upstream `hrtfData`).
    pub fn hrirs(&self) -> &[f32] {
        &self.hrirs
    }

    pub(crate) fn hrirs_mut(&mut self) -> &mut [f32] {
        &mut self.hrirs
    }

    /// Precomputed order-3 Ambisonics HRIRs, `NUM_EARS × 16 × num_samples`, or an empty slice if the data has none
    /// (version 2), in which case [`HrtfDatabase`](super::HrtfDatabase) projects them itself (upstream
    /// `ambisonicsData`).
    pub fn ambisonics_hrirs(&self) -> &[f32] {
        &self.ambisonics_hrirs
    }

    /// Index of the measurement nearest to `direction` (upstream `nearestHRIR`): nearest azimuth ring first, then
    /// nearest elevation within it.
    pub fn nearest_hrir(&self, direction: Vector3) -> usize {
        let interaural = InterauralSphericalVector3::from(direction);
        let phi = interaural.azimuth / DEGREES_TO_RADIANS;
        let theta = interaural.elevation / DEGREES_TO_RADIANS;

        let phi_index = nearest_neighbor(&self.azimuths, phi);
        let theta_index = nearest_neighbor(self.elevations_for(phi_index), theta);
        phi_index * self.num_elevations + theta_index
    }

    /// Up to four measurements around `direction` and their bilinear interpolation weights (upstream
    /// `interpolatedHRIRWeights`): two azimuth rings bracketing the direction, and two elevations in each.
    pub fn interpolated_hrir_weights(&self, direction: Vector3) -> HrirWeights {
        let interaural = InterauralSphericalVector3::from(direction);
        let phi = interaural.azimuth / DEGREES_TO_RADIANS;
        let theta = interaural.elevation / DEGREES_TO_RADIANS;
        let mut theta_for_phi_min = theta;
        let mut theta_for_phi_max = theta;

        let last = self.azimuths.len() - 1;
        let (mut phi_min_index, mut phi_max_index) = lower_and_upper_bound(&self.azimuths, phi);
        let mut phi_min = self.azimuths[phi_min_index];
        let mut phi_max = self.azimuths[phi_max_index];

        // Beyond the outermost ring, interpolate towards that ring's mirror image across the interaural axis.
        if phi_min_index == 0 && phi_max_index == 0 {
            phi_min += 2.0 * (-90.0 - phi_min);
            theta_for_phi_min += 2.0 * (180.0 - theta_for_phi_min);
        }
        if phi_min_index == last && phi_max_index == last {
            phi_max += 2.0 * (90.0 - phi_max);
            theta_for_phi_max += 2.0 * (180.0 - theta_for_phi_max);
        }

        if direction.z > 0.0 {
            std::mem::swap(&mut phi_min, &mut phi_max);
            std::mem::swap(&mut phi_min_index, &mut phi_max_index);
            std::mem::swap(&mut theta_for_phi_min, &mut theta_for_phi_max);
        }

        let elevations_min = self.elevations_for(phi_min_index);
        let (theta_min_index_for_phi_min, theta_max_index_for_phi_min) =
            lower_and_upper_bound(elevations_min, theta_for_phi_min);
        let theta_min_for_phi_min = elevations_min[theta_min_index_for_phi_min];
        let theta_max_for_phi_min = elevations_min[theta_max_index_for_phi_min];

        let elevations_max = self.elevations_for(phi_max_index);
        let (theta_min_index_for_phi_max, theta_max_index_for_phi_max) =
            lower_and_upper_bound(elevations_max, theta_for_phi_max);
        let theta_min_for_phi_max = elevations_max[theta_min_index_for_phi_max];
        let theta_max_for_phi_max = elevations_max[theta_max_index_for_phi_max];

        let (weight_phi_min, weight_phi_max) =
            linear_interpolation_weights(phi, phi_min, phi_max, phi_min_index, phi_max_index);
        let (weight_theta_min_for_phi_min, weight_theta_max_for_phi_min) =
            linear_interpolation_weights(
                theta_for_phi_min,
                theta_min_for_phi_min,
                theta_max_for_phi_min,
                theta_min_index_for_phi_min,
                theta_max_index_for_phi_min,
            );
        let (weight_theta_min_for_phi_max, weight_theta_max_for_phi_max) =
            linear_interpolation_weights(
                theta_for_phi_max,
                theta_min_for_phi_max,
                theta_max_for_phi_max,
                theta_min_index_for_phi_max,
                theta_max_index_for_phi_max,
            );

        let ne = self.num_elevations;
        HrirWeights {
            indices: [
                Some(phi_min_index * ne + theta_min_index_for_phi_min),
                Some(phi_min_index * ne + theta_max_index_for_phi_min),
                Some(phi_max_index * ne + theta_min_index_for_phi_max),
                Some(phi_max_index * ne + theta_max_index_for_phi_max),
                None,
                None,
                None,
                None,
            ],
            weights: [
                weight_phi_min * weight_theta_min_for_phi_min,
                weight_phi_min * weight_theta_max_for_phi_min,
                weight_phi_max * weight_theta_min_for_phi_max,
                weight_phi_max * weight_theta_max_for_phi_max,
                0.0,
                0.0,
                0.0,
                0.0,
            ],
        }
    }

    fn elevations_for(&self, azimuth_index: usize) -> &[f32] {
        let start = azimuth_index * self.num_elevations;
        &self.elevations[start..start + self.num_elevations]
    }
}

impl fmt::Debug for HrtfMap {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("HrtfMap")
            .field("num_hrirs", &self.num_hrirs)
            .field("num_samples", &self.num_samples)
            .field("num_azimuths", &self.azimuths.len())
            .field("num_elevations", &self.num_elevations)
            .field("has_ambisonics_hrirs", &!self.ambisonics_hrirs.is_empty())
            .finish_non_exhaustive()
    }
}

/// `std::upper_bound`: index of the first element greater than `needle`, by the standard's binary search, so that
/// unsorted input (elevation rings need not be sorted) gives the same result as upstream.
fn upper_bound(haystack: &[f32], needle: f32) -> usize {
    let mut first = 0;
    let mut count = haystack.len();
    while count > 0 {
        let step = count / 2;
        let mid = first + step;
        if needle < haystack[mid] {
            count = step;
        } else {
            first = mid + 1;
            count -= step + 1;
        }
    }
    first
}

/// Indices of the elements on either side of `needle`, clamped to the array (upstream `lowerAndUpperBound`).
fn lower_and_upper_bound(haystack: &[f32], needle: f32) -> (usize, usize) {
    let u = upper_bound(haystack, needle);
    let upper = u.min(haystack.len() - 1);
    let lower = u.saturating_sub(1);
    (lower, upper)
}

/// Index of the element closest to `needle`, preferring the lower one on ties (upstream `nearestNeighbor`).
fn nearest_neighbor(haystack: &[f32], needle: f32) -> usize {
    let (lower, upper) = lower_and_upper_bound(haystack, needle);
    if (needle - haystack[lower]).abs() <= (needle - haystack[upper]).abs() {
        lower
    } else {
        upper
    }
}

/// Weights `(w_min, w_max)` with `x = w_min · x_min + w_max · x_max` (upstream
/// `calculateLinearInterpolationWeights`).
fn linear_interpolation_weights(
    x: f32,
    x_min: f32,
    x_max: f32,
    x_min_index: usize,
    x_max_index: usize,
) -> (f32, f32) {
    if x_min_index == x_max_index && x_min == x_max {
        return (0.0, 1.0);
    }
    let weight_min = (x_max - x) / (x_max - x_min);
    (weight_min, 1.0 - weight_min)
}

/// Bounds-checked little-endian reader. Upstream reads through raw pointers without checks.
struct Reader<'a> {
    data: &'a [u8],
    pos: usize,
}

impl Reader<'_> {
    fn take(&mut self, n: usize) -> Result<&[u8]> {
        let end = self
            .pos
            .checked_add(n)
            .filter(|&end| end <= self.data.len())
            .ok_or(Error::MalformedHrtfData("unexpected end of data"))?;
        let bytes = &self.data[self.pos..end];
        self.pos = end;
        Ok(bytes)
    }

    fn skip(&mut self, n: usize) -> Result<()> {
        self.take(n).map(|_| ())
    }

    fn skip_f32s(&mut self, count: usize) -> Result<()> {
        let n = count
            .checked_mul(4)
            .ok_or(Error::MalformedHrtfData("unexpected end of data"))?;
        self.skip(n)
    }

    fn i32(&mut self) -> Result<i32> {
        let bytes = self.take(4)?;
        Ok(i32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]))
    }

    /// A non-negative `i32` count.
    fn count(&mut self) -> Result<usize> {
        usize::try_from(self.i32()?).map_err(|_| Error::MalformedHrtfData("negative count"))
    }

    fn f32s(&mut self, out: &mut [f32]) -> Result<()> {
        let n = out
            .len()
            .checked_mul(4)
            .ok_or(Error::MalformedHrtfData("unexpected end of data"))?;
        let bytes = self.take(n)?;
        for (o, b) in out.iter_mut().zip(bytes.chunks_exact(4)) {
            *o = f32::from_le_bytes([b[0], b[1], b[2], b[3]]);
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn upper_bound_matches_std() {
        let sorted = [1.0, 2.0, 2.0, 3.0];
        assert_eq!(upper_bound(&sorted, 0.0), 0);
        assert_eq!(upper_bound(&sorted, 2.0), 3);
        assert_eq!(upper_bound(&sorted, 5.0), 4);
        assert_eq!(upper_bound(&sorted, f32::NAN), 4);
    }

    #[test]
    fn built_in_data_loads_at_each_sampling_rate() {
        for (rate, samples) in [(44100, 200), (48000, 218), (24000, 109)] {
            let map = HrtfMap::new(rate).unwrap();
            assert_eq!(map.num_hrirs(), 1250);
            assert_eq!(map.num_samples(), samples);
            assert_eq!(map.azimuths.len(), 25);
            assert_eq!(map.num_elevations, 50);
            assert_eq!(map.ambisonics_hrirs().len(), NUM_EARS * 16 * samples);
        }
    }

    #[test]
    fn invalid_data_is_rejected() {
        let data = crate::hrtf::BUILTIN_HRTF_DATA;
        assert_eq!(
            HrtfMap::new(22050).unwrap_err(),
            Error::UnsupportedSamplingRate(22050)
        );

        let mut v4 = data[..64].to_vec();
        v4[4] = 4;
        assert_eq!(
            HrtfMap::from_bytes(&v4, 44100).unwrap_err(),
            Error::UnsupportedHrtfVersion(4)
        );

        assert!(matches!(
            HrtfMap::from_bytes(&data[..100_000], 44100),
            Err(Error::MalformedHrtfData(_))
        ));
        assert!(matches!(
            HrtfMap::from_bytes(&data[..data.len() - 1], 24000),
            Err(Error::MalformedHrtfData(_))
        ));
    }
}
