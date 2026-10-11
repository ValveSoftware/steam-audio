//! Neighbouring measurements (upstream `hrtf/neighbors.c`).

use crate::lookup::{Lookup, source_positions};
use crate::spherical::{c2s, s2c};
use crate::{Error, Hrtf};

/// `MYSOFA_DEFAULT_NEIGH_STEP_ANGLE`, in degrees.
pub const DEFAULT_ANGLE_STEP: f32 = 0.5;
/// `MYSOFA_DEFAULT_NEIGH_STEP_RADIUS`, in metres.
pub const DEFAULT_RADIUS_STEP: f32 = 0.01;

/// Distance in degrees beyond which the angular searches give up (`maxNeighborSearchAngle`).
const MAX_SEARCH_ANGLE: f32 = 45.0;
/// The port's bound on the steps of one search (`docs/DEVIATIONS.md`). Upstream's radius searches run until the
/// test radius passes the largest measured one, which takes forever when the radius stops growing in `float`.
const MAX_SEARCH_STEPS: u32 = 1 << 12;

/// The neighbours of a measurement, in the order `[+azimuth, −azimuth, +elevation, −elevation, +radius, −radius]`;
/// `None` where there is none (`-1` upstream).
pub type Neighbors = [Option<usize>; 6];

/// The neighbours of every measurement (upstream `MYSOFA_NEIGHBORHOOD`): for each direction, the first other
/// measurement [`Lookup::lookup`] finds when stepping away from it in azimuth, elevation and radius.
#[derive(Debug, Clone)]
pub struct Neighborhood {
    index: Vec<Neighbors>,
}

impl Neighborhood {
    /// Finds the neighbours with the default steps (upstream `mysofa_neighborhood_init`).
    pub fn new(hrtf: &Hrtf, lookup: &Lookup) -> Result<Self, Error> {
        Self::with_steps(hrtf, lookup, DEFAULT_ANGLE_STEP, DEFAULT_RADIUS_STEP)
    }

    /// Finds the neighbours, stepping by `angle_step` degrees and `radius_step` metres (upstream
    /// `mysofa_neighborhood_init_withstepdefine`). `lookup` must have been built from `hrtf`.
    ///
    /// Azimuth and elevation are searched up to 45° away, the radius up to one step beyond the measured range; a
    /// coordinate is only searched if the measurements span a range in it. Fails with [`Error::InvalidFormat`] if
    /// the source positions are not `M` 3-D points, and with [`Error::ResourceLimit`] if a search takes more than
    /// 4096 steps.
    ///
    /// # Panics
    ///
    /// If a step is not positive and finite.
    pub fn with_steps(
        hrtf: &Hrtf,
        lookup: &Lookup,
        angle_step: f32,
        radius_step: f32,
    ) -> Result<Self, Error> {
        assert!(
            angle_step > 0.0 && angle_step.is_finite(),
            "angle step must be positive and finite, got {angle_step}"
        );
        assert!(
            radius_step > 0.0 && radius_step.is_finite(),
            "radius step must be positive and finite, got {radius_step}"
        );

        let positions = source_positions(hrtf)?;
        let mut index = vec![[None; 6]; positions.len()];
        let mut scratch = lookup.scratch();

        for (i, (&position, neighbors)) in positions.iter().zip(&mut index).enumerate() {
            let origin = c2s(position);

            // Steps `origin` with `test` until the lookup finds another measurement or `more` says to stop.
            let mut search = |slot: &mut Option<usize>,
                              step: f32,
                              test: &dyn Fn(f32) -> [f32; 3],
                              more: &dyn Fn(f32, [f32; 3]) -> bool|
             -> Result<(), Error> {
                let mut offset = step;
                for _ in 0..MAX_SEARCH_STEPS {
                    let spherical = test(offset);
                    let mut point = s2c(spherical);
                    let found = lookup.lookup_with(&mut point, &mut scratch);
                    if found != Some(i) {
                        *slot = found;
                        return Ok(());
                    }
                    offset += step;
                    if !more(offset, spherical) {
                        return Ok(());
                    }
                }
                Err(Error::ResourceLimit)
            };

            let [up, down, ..] = &mut *neighbors;
            if lookup.phi_max - lookup.phi_min > f32::MIN_POSITIVE {
                let test = |phi: f32| [origin[0] + phi, origin[1], origin[2]];
                search(up, angle_step, &test, &|phi, _| phi <= MAX_SEARCH_ANGLE)?;
                search(down, -angle_step, &test, &|phi, _| phi >= -MAX_SEARCH_ANGLE)?;
            }
            let [_, _, up, down, ..] = &mut *neighbors;
            if lookup.theta_max - lookup.theta_min > f32::MIN_POSITIVE {
                let test = |theta: f32| [origin[0], origin[1] + theta, origin[2]];
                search(up, angle_step, &test, &|theta, _| theta <= MAX_SEARCH_ANGLE)?;
                search(down, -angle_step, &test, &|theta, _| {
                    theta >= -MAX_SEARCH_ANGLE
                })?;
            }
            let [.., up, down] = &mut *neighbors;
            if lookup.radius_max - lookup.radius_min > f32::MIN_POSITIVE {
                // Upstream tests the radius just tried, before the step.
                let test = |radius: f32| [origin[0], origin[1], origin[2] + radius];
                let max = lookup.radius_max + radius_step;
                let min = lookup.radius_min - radius_step;
                search(up, radius_step, &test, &|_, tried| tried[2] <= max)?;
                search(down, -radius_step, &test, &|_, tried| tried[2] >= min)?;
            }
        }
        Ok(Self { index })
    }

    /// The neighbours of measurement `index` (upstream `mysofa_neighborhood`), `None` if it is out of range.
    pub fn get(&self, index: usize) -> Option<&Neighbors> {
        self.index.get(index)
    }

    /// The number of measurements.
    pub fn len(&self) -> usize {
        self.index.len()
    }

    pub fn is_empty(&self) -> bool {
        self.index.is_empty()
    }
}
