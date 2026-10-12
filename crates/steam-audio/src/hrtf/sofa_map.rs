//! HRTFs loaded from SOFA (AES69) files (upstream `SOFAHRTFMap`), through the `sofa` crate (ADR 0001).
//!
//! SOFA positions use their own coordinate system: x ahead, y to the left, z up. A Steam Audio direction
//! `(x, y, z)` (x to the right, y up, z behind) is `(-z, -x, y)` there.

use std::fmt;
use std::path::Path;

use sofa::{Easy, Lookup, LookupScratch, Neighbors, c2s, s2c};

use super::{HrirWeights, NUM_EARS};
use crate::error::{Error, Result};
use crate::math::{Vector3, cpp_max, cpp_min};

/// HRIRs measured at the source positions of a SOFA file, resampled to the renderer's sampling rate, with
/// nearest-neighbour and interpolated direction queries (upstream `SOFAHRTFMap`, the `IHRTFMap` for SOFA files).
///
/// Queries take `&mut self` because the nearest-measurement search uses internal scratch space; they do not
/// allocate.
pub struct SofaHrtfMap {
    num_samples: usize,
    /// Cartesian source positions in SOFA coordinates.
    positions: Vec<[f32; 3]>,
    lookup: Lookup,
    scratch: LookupScratch,
    /// The neighbours of every measurement, with missing azimuth neighbours filled in by wrapping around.
    neighbors: Vec<Neighbors>,
    /// `NUM_EARS × num_hrirs × num_samples`.
    hrirs: Vec<f32>,
}

impl SofaHrtfMap {
    /// Loads a SOFA file held in memory and resamples its HRIRs to `sampling_rate` (upstream `SOFAHRTFMap` with
    /// `sofaData`, which calls `mysofa_open_data_no_norm`).
    ///
    /// Fails with [`Error::Sofa`] if the file does not load, with [`Error::UnsupportedSofaDelay`] if it has
    /// non-zero delays, and with [`Error::MalformedHrtfData`] if its HRIRs are empty.
    pub fn from_bytes(data: &[u8], sampling_rate: u32) -> Result<Self> {
        Self::from_easy(Easy::load_no_norm(data, sampling_rate as f32)?)
    }

    /// Reads a SOFA file and loads it like [`SofaHrtfMap::from_bytes`] (upstream `SOFAHRTFMap` with
    /// `sofaFileName`, which calls `mysofa_open_no_norm`).
    pub fn from_file(path: impl AsRef<Path>, sampling_rate: u32) -> Result<Self> {
        Self::from_easy(Easy::load_file_no_norm(path, sampling_rate as f32)?)
    }

    fn from_easy(easy: Easy) -> Result<Self> {
        let Easy {
            hrtf,
            lookup,
            neighborhood,
        } = easy;

        // Data.Delay values are not supported by Steam Audio.
        let delays = hrtf.data_delay.values.as_deref().unwrap_or_default();
        if delays.iter().any(|d| d.abs() > 1e-3) {
            return Err(Error::UnsupportedSofaDelay);
        }

        // `Easy` has checked that there are M > 0 measurements, R = 2 receivers and C = 3 coordinates, and that the
        // IRs have M × R × N values.
        let num_hrirs = hrtf.m as usize;
        let num_samples = hrtf.n as usize;
        if num_samples == 0 {
            return Err(Error::MalformedHrtfData("empty HRIRs"));
        }
        let positions: Vec<[f32; 3]> = hrtf
            .source_position
            .values
            .as_deref()
            .unwrap_or_default()
            .chunks_exact(3)
            .map(|p| [p[0], p[1], p[2]])
            .collect();
        let neighbors = (0..num_hrirs)
            .map(|i| {
                *neighborhood
                    .get(i)
                    .expect("one neighbourhood per measurement")
            })
            .collect();

        let ir = hrtf.data_ir.values.as_deref().unwrap_or_default();
        let mut hrirs = vec![0.0f32; NUM_EARS * num_hrirs * num_samples];
        for (i, measurement) in ir.chunks_exact(NUM_EARS * num_samples).enumerate() {
            for (ear, hrir) in measurement.chunks_exact(num_samples).enumerate() {
                let start = (ear * num_hrirs + i) * num_samples;
                hrirs[start..start + num_samples].copy_from_slice(hrir);
            }
        }

        let scratch = lookup.scratch();
        let mut map = Self {
            num_samples,
            positions,
            lookup,
            scratch,
            neighbors,
            hrirs,
        };
        map.patch_neighbors();
        Ok(map)
    }

    /// Number of measurements (upstream `numHRIRs`).
    pub fn num_hrirs(&self) -> usize {
        self.positions.len()
    }

    /// HRIR length in samples, after resampling (upstream `numSamples`).
    pub fn num_samples(&self) -> usize {
        self.num_samples
    }

    /// HRIR of one ear (0 = left, 1 = right) for one measurement.
    pub fn hrir(&self, ear: usize, index: usize) -> &[f32] {
        assert!(
            ear < NUM_EARS && index < self.num_hrirs(),
            "HRIR index out of range"
        );
        let start = (ear * self.num_hrirs() + index) * self.num_samples;
        &self.hrirs[start..start + self.num_samples]
    }

    /// All HRIRs, `NUM_EARS × num_hrirs × num_samples` (upstream `hrtfData`).
    pub fn hrirs(&self) -> &[f32] {
        &self.hrirs
    }

    pub(crate) fn hrirs_mut(&mut self) -> &mut [f32] {
        &mut self.hrirs
    }

    /// The Cartesian source positions, in SOFA coordinates (x ahead, y left, z up).
    pub fn positions(&self) -> &[[f32; 3]] {
        &self.positions
    }

    /// The nearest-measurement search over [`positions`](Self::positions).
    pub fn lookup(&self) -> &Lookup {
        &self.lookup
    }

    /// The neighbours of a measurement in the order `[+azimuth, −azimuth, +elevation, −elevation, +radius,
    /// −radius]`: those of [`sofa::Neighborhood`], except that a missing azimuth neighbour is replaced by the first
    /// other measurement found when stepping around the full circle in 0.5° steps (upstream
    /// `patchSOFANeighborhood`).
    pub fn neighbors(&self, index: usize) -> &Neighbors {
        &self.neighbors[index]
    }

    /// Index of the measurement nearest to `direction` (upstream `nearestHRIR`, which calls `mysofa_lookup`).
    /// `direction` need not be normalized: like upstream, it is scaled onto the range of measured radii.
    pub fn nearest_hrir(&mut self, direction: Vector3) -> usize {
        let mut point = to_sofa_coordinates(direction);
        self.lookup
            .lookup_with(&mut point, &mut self.scratch)
            .expect("a SOFA HRTF has measurements")
    }

    /// Up to eight measurements around `direction` and their trilinear interpolation weights in azimuth, elevation
    /// and radius (upstream `interpolatedHRIRWeights`).
    ///
    /// Every slot has an index: unused slots repeat the nearest measurement with a weight of 0. If `direction` is
    /// within 1e-5 of a measured position (unnormalized), that measurement gets weight 1, and the other slots hold
    /// index 0 with weight 0, the values upstream's `interpolatedHRTF` initializes them to.
    pub fn interpolated_hrir_weights(&mut self, direction: Vector3) -> HrirWeights {
        const NONE: f32 = f32::MAX;

        let sofa_direction = to_sofa_coordinates(direction);
        let nearest = self.nearest_hrir(direction);

        let d = self.distance_to_measurement(sofa_direction, nearest);
        if d.abs() < 1e-5 {
            let mut weights = [0.0; 8];
            weights[0] = 1.0;
            let mut indices = [Some(0); 8];
            indices[0] = Some(nearest);
            return HrirWeights { indices, weights };
        }

        let neighbors = self.neighbors[nearest];

        let [mut phi, theta, r] = c2s(sofa_direction);
        let nearest_position = c2s(self.positions[nearest]);
        let neighbor_positions = neighbors.map(|n| n.map_or([0.0; 3], |n| c2s(self.positions[n])));

        // Only the azimuth of the ±azimuth neighbours matters, and so on.
        let mut neighbor_coordinates = [
            neighbor_positions[0][0],
            neighbor_positions[1][0],
            neighbor_positions[2][1],
            neighbor_positions[3][1],
            neighbor_positions[4][2],
            neighbor_positions[5][2],
        ];

        // Unwrap the ±azimuth neighbours relative to the nearest measurement.
        for c in &mut neighbor_coordinates[..2] {
            if *c > nearest_position[0] {
                while *c - nearest_position[0] >= 180.0 {
                    *c -= 360.0;
                }
            } else {
                while *c - nearest_position[0] <= -180.0 {
                    *c += 360.0;
                }
            }
        }

        // Distance along each axis from the nearest measurement to each neighbour, or `FLT_MAX` if the neighbour is
        // on the other side from the query.
        let query = [phi, phi, theta, theta, r, r];
        let axis = [0, 0, 1, 1, 2, 2];
        let neighbor_distances: [f32; 6] = std::array::from_fn(|k| {
            let n = nearest_position[axis[k]];
            if (neighbor_coordinates[k] - n) * (query[k] - n) < 0.0 {
                NONE
            } else {
                (neighbor_coordinates[k] - n).abs()
            }
        });

        // The direction to interpolate in along each axis, if both neighbours exist (or either, for the radius,
        // which is not continuous).
        let closer = |a: usize| -> Option<usize> {
            (neighbors[a].is_some() && neighbors[a + 1].is_some()).then(|| {
                if neighbor_distances[a] <= neighbor_distances[a + 1] {
                    a
                } else {
                    a + 1
                }
            })
        };
        let phi_index = closer(0);
        let theta_index = closer(2);
        let r_index = closer(4).or(if neighbors[4].is_some() {
            Some(4)
        } else if neighbors[5].is_some() {
            Some(5)
        } else {
            None
        });

        // Neighbour `k` of measurement `index`, as `mysofa_neighborhood(index)[k]`. Upstream dereferences a null
        // pointer when `index` is missing (only possible for the (r, theta, phi) neighbour).
        let neighbor_of = |index: Option<usize>, k: Option<usize>| -> Option<usize> {
            self.neighbors[index?][k?]
        };

        // 0: nearest; 1: phi neighbour; 2: theta neighbour; 3: r neighbour; 4: (theta, phi); 5: (r, phi);
        // 6: (r, theta); 7: (r, theta, phi).
        let mut indices = [None; 8];
        indices[0] = Some(nearest);
        indices[1] = phi_index.and_then(|k| neighbors[k]);
        indices[2] = theta_index.and_then(|k| neighbors[k]);
        indices[3] = r_index.and_then(|k| neighbors[k]);
        if phi_index.is_some() && theta_index.is_some() {
            indices[4] = neighbor_of(indices[2], phi_index);
        }
        if phi_index.is_some() && r_index.is_some() {
            indices[5] = neighbor_of(indices[3], phi_index);
        }
        if theta_index.is_some() && r_index.is_some() {
            indices[6] = neighbor_of(indices[3], theta_index);
        }
        if phi_index.is_some() && theta_index.is_some() && r_index.is_some() {
            indices[7] = neighbor_of(indices[6], phi_index);
        }

        // Remove duplicates.
        for i in 0..8 {
            if (0..i).any(|j| indices[i] == indices[j]) {
                indices[i] = None;
            }
        }

        // Coordinates of each point, with azimuths unwrapped relative to the nearest measurement.
        let mut phi_self = [NONE; 8];
        let mut theta_self = [NONE; 8];
        let mut r_self = [NONE; 8];
        for i in 0..8 {
            if let Some(index) = indices[i] {
                let p = c2s(self.positions[index]);
                phi_self[i] = p[0];
                theta_self[i] = p[1];
                r_self[i] = p[2];
                if (phi_self[i] - phi_self[0]).abs() >= 180.0 {
                    if phi_self[i] > phi_self[0] {
                        phi_self[i] -= 360.0;
                    } else {
                        phi_self[i] += 360.0;
                    }
                }
            }
        }

        // The slot across each axis from each slot.
        const PHI_NEIGHBORS: [usize; 8] = [1, 0, 4, 5, 2, 3, 7, 6];
        const THETA_NEIGHBORS: [usize; 8] = [2, 4, 0, 6, 1, 7, 3, 5];
        const R_NEIGHBORS: [usize; 8] = [3, 5, 6, 0, 7, 1, 2, 4];
        let other = |values: &[f32; 8], across: &[usize; 8]| -> [f32; 8] {
            std::array::from_fn(|i| {
                if indices[across[i]].is_none() {
                    values[i]
                } else {
                    values[across[i]]
                }
            })
        };
        let phi_other = other(&phi_self, &PHI_NEIGHBORS);
        let theta_other = other(&theta_self, &THETA_NEIGHBORS);
        let r_other = other(&r_self, &R_NEIGHBORS);

        // Unwrap the query's azimuth relative to the nearest measurement.
        if (phi - phi_self[0]).abs() >= 180.0 {
            if phi > phi_self[0] {
                phi -= 360.0;
            } else {
                phi += 360.0;
            }
        }

        // Linear interpolation term between `own` and `other`; azimuth and elevation terms outside [0, 1] are moved
        // by 1 first, in case they wrapped around.
        let term = |other: f32, own: f32, x: f32, wraps: bool| {
            let mut t = if (other - own).abs() < 1e-5 {
                1.0
            } else {
                (other - x) / (other - own)
            };
            if wraps {
                if t < 0.0 {
                    t += 1.0;
                } else if t > 1.0 {
                    t -= 1.0;
                }
            }
            cpp_max(0.0, cpp_min(t, 1.0))
        };

        let mut weights = [0.0f32; 8];
        let mut total_weight = 0.0f32;
        for i in 0..8 {
            if indices[i].is_some() {
                let phi_term = term(phi_other[i], phi_self[i], phi, true);
                let theta_term = term(theta_other[i], theta_self[i], theta, true);
                let r_term = term(r_other[i], r_self[i], r, false);
                weights[i] = phi_term * theta_term * r_term;
            } else {
                indices[i] = Some(nearest);
                weights[i] = 0.0;
            }
            total_weight += weights[i];
        }

        for w in &mut weights {
            *w /= total_weight;
        }

        HrirWeights { indices, weights }
    }

    fn distance_to_measurement(&self, point: [f32; 3], index: usize) -> f32 {
        let [x, y, z] = point;
        let [px, py, pz] = self.positions[index];
        (Vector3::new(x, y, z) - Vector3::new(px, py, pz)).length()
    }

    /// Fills in missing ±azimuth neighbours by stepping around the circle until another measurement is nearest
    /// (upstream `patchSOFANeighborhood`). None is found at ±90° elevation, where the azimuth has no effect.
    fn patch_neighbors(&mut self) {
        for i in 0..self.positions.len() {
            let measurement = c2s(self.positions[i]);
            for (slot, sign) in [(0, 1.0f32), (1, -1.0)] {
                if self.neighbors[i][slot].is_some() {
                    continue;
                }
                let mut d_phi = 0.5f32;
                while d_phi <= 360.0 {
                    let mut point = measurement;
                    if sign > 0.0 {
                        point[0] += d_phi;
                        if point[0] > 360.0 {
                            point[0] -= 360.0;
                        }
                    } else {
                        point[0] -= d_phi;
                        if point[0] < 0.0 {
                            point[0] += 360.0;
                        }
                    }

                    let mut point = s2c(point);
                    let nearest = self.lookup.lookup_with(&mut point, &mut self.scratch);
                    if nearest != Some(i) {
                        self.neighbors[i][slot] = nearest;
                        break;
                    }
                    d_phi += 0.5;
                }
            }
        }
    }
}

impl Clone for SofaHrtfMap {
    fn clone(&self) -> Self {
        Self {
            num_samples: self.num_samples,
            positions: self.positions.clone(),
            lookup: self.lookup.clone(),
            scratch: self.lookup.scratch(),
            neighbors: self.neighbors.clone(),
            hrirs: self.hrirs.clone(),
        }
    }
}

impl fmt::Debug for SofaHrtfMap {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("SofaHrtfMap")
            .field("num_hrirs", &self.num_hrirs())
            .field("num_samples", &self.num_samples)
            .finish_non_exhaustive()
    }
}

/// A Steam Audio direction in SOFA coordinates (upstream `toSOFACoordinates`).
fn to_sofa_coordinates(v: Vector3) -> [f32; 3] {
    [-v.z, -v.x, v.y]
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cipic() -> Easy {
        let path = concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/../../core/data/hrtf/cipic_124.sofa"
        );
        Easy::load_file_no_norm(path, 48000.0).unwrap()
    }

    #[test]
    fn non_zero_delays_are_rejected() {
        for (delay, ok) in [(0.0, true), (1e-3, true), (-2e-3, false), (f32::NAN, true)] {
            let mut easy = cipic();
            easy.hrtf.data_delay.values = Some(vec![0.0, delay]);
            let result = SofaHrtfMap::from_easy(easy);
            assert_eq!(result.is_ok(), ok, "delay {delay}");
            if !ok {
                assert_eq!(result.unwrap_err(), Error::UnsupportedSofaDelay);
            }
        }
    }

    /// The shipped files only lack azimuth neighbours at the poles, where none is found, so the golden data
    /// (`tools/golden/src/sofa_hrtf.cpp`) removes neighbours of Lcg-chosen measurements and patches again.
    #[test]
    fn patching_matches_upstream() {
        for (file_index, name) in ["cipic_124", "sadie_d1", "sadie_h12"]
            .into_iter()
            .enumerate()
        {
            let path = parity::data_path(&format!("hrtf/{name}.sofa"));
            let mut map = SofaHrtfMap::from_file(path, 48000).unwrap();
            let mut lcg = parity::Lcg::new(83 + file_index as u32);
            let mut chosen = Vec::new();
            for _ in 0..40 {
                let index = (lcg.next_u32() % map.num_hrirs() as u32) as usize;
                let sides = lcg.next_u32() % 3;
                if sides != 1 {
                    map.neighbors[index][0] = None;
                }
                if sides != 0 {
                    map.neighbors[index][1] = None;
                }
                chosen.push(index);
            }
            map.patch_neighbors();

            let expected = parity::golden::<i32>(&format!("sofa_hrtf_map/{name}_repatched"));
            for (&i, row) in chosen.iter().zip(expected.data.chunks_exact(3)) {
                assert_eq!(row[0], i as i32);
                for slot in 0..2 {
                    let actual = map.neighbors[i][slot].map_or(-1, |n| n as i32);
                    if actual != row[1 + slot] {
                        // Off MSVC a lookup tie may resolve differently (R17): accept another measurement only if it
                        // is as far from the point the search stopped at.
                        assert!(
                            !cfg!(all(windows, target_env = "msvc"))
                                && row[1 + slot] >= 0
                                && map.is_patch_tie(
                                    i,
                                    slot,
                                    actual as usize,
                                    row[1 + slot] as usize
                                ),
                            "{name}: measurement {i} slot {slot}: {actual}, expected {}",
                            row[1 + slot]
                        );
                    }
                }
            }
        }
    }

    impl SofaHrtfMap {
        /// Whether measurements `a` and `b` are equally far from the point where the patch search for `i` stops.
        fn is_patch_tie(&self, i: usize, slot: usize, a: usize, b: usize) -> bool {
            let measurement = c2s(self.positions[i]);
            let mut d_phi = 0.5f32;
            while d_phi <= 360.0 {
                let mut point = measurement;
                point[0] += if slot == 0 { d_phi } else { -d_phi };
                point[0] += if point[0] > 360.0 {
                    -360.0
                } else if point[0] < 0.0 {
                    360.0
                } else {
                    0.0
                };
                let mut point = s2c(point);
                if self.lookup.lookup(&mut point) != Some(i) {
                    let distance = |m: usize| {
                        let p = self.positions[m];
                        (0..3)
                            .map(|k| (f64::from(p[k]) - f64::from(point[k])).powi(2))
                            .sum::<f64>()
                            .sqrt()
                    };
                    return (distance(a) - distance(b)).abs() <= 1e-5 * distance(b).max(1e-6);
                }
                d_phi += 0.5;
            }
            false
        }
    }

    #[test]
    fn load_errors_are_reported() {
        let missing = SofaHrtfMap::from_file("no such file.sofa", 48000).unwrap_err();
        assert!(matches!(&missing, Error::Sofa(e) if matches!(e.get(), sofa::Error::Io(_))));
        assert_eq!(
            SofaHrtfMap::from_bytes(b"not a SOFA file", 48000).unwrap_err(),
            Error::Sofa(sofa::Error::InvalidFormat.into())
        );
    }
}
