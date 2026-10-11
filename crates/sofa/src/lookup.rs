//! Nearest-measurement search (upstream `hrtf/lookup.c`).

use crate::kdtree::{KdTree, Stack};
use crate::spherical::{c2s, radius};
use crate::{Error, Hrtf, has_attribute};

/// Finds the measurement nearest to a direction (upstream `MYSOFA_LOOKUP`): a k-d tree over the source positions,
/// with the range of their spherical coordinates.
#[derive(Debug, Clone)]
pub struct Lookup {
    tree: KdTree,
    /// Smallest and largest radius of the source positions.
    pub radius_min: f32,
    pub radius_max: f32,
    /// Smallest and largest elevation, in degrees.
    pub theta_min: f32,
    pub theta_max: f32,
    /// Smallest and largest azimuth, in degrees.
    pub phi_min: f32,
    pub phi_max: f32,
}

impl Lookup {
    /// Builds the search structure over `hrtf`'s source positions (upstream `mysofa_lookup_init`), which must be
    /// Cartesian (see [`Hrtf::to_cartesian`]).
    ///
    /// Fails with [`Error::Internal`] (where upstream returns `NULL`) if the positions are not Cartesian, and with
    /// [`Error::InvalidFormat`] if they are not `M` 3-D points.
    ///
    /// The maxima start at `FLT_MIN`, the smallest positive `float`, as upstream: a maximum stays there when every
    /// value is smaller, for example when all elevations are negative.
    pub fn new(hrtf: &Hrtf) -> Result<Self, Error> {
        if !has_attribute(&hrtf.source_position.attributes, "Type", "cartesian") {
            return Err(Error::Internal);
        }
        let positions = source_positions(hrtf)?;

        let mut lookup = Self {
            tree: KdTree::default(),
            radius_min: f32::MAX,
            radius_max: f32::MIN_POSITIVE,
            theta_min: f32::MAX,
            theta_max: f32::MIN_POSITIVE,
            phi_min: f32::MAX,
            phi_max: f32::MIN_POSITIVE,
        };
        for &position in &positions {
            let [phi, theta, r] = c2s(position);
            let update = |min: &mut f32, max: &mut f32, value: f32| {
                if value < *min {
                    *min = value;
                }
                if value > *max {
                    *max = value;
                }
            };
            update(&mut lookup.phi_min, &mut lookup.phi_max, phi);
            update(&mut lookup.theta_min, &mut lookup.theta_max, theta);
            update(&mut lookup.radius_min, &mut lookup.radius_max, r);
        }
        for (i, &position) in positions.iter().enumerate() {
            lookup.tree.insert(position, i);
        }
        Ok(lookup)
    }

    /// The index of the measurement nearest to the Cartesian `coordinate` (upstream `mysofa_lookup`), or `None` if
    /// there are no measurements.
    ///
    /// Like upstream, `coordinate` is first scaled in place onto the range of measured radii when it lies outside
    /// it. The zero vector becomes NaN and matches whichever measurement the search visits first.
    ///
    /// Allocates scratch space for the search; [`Lookup::lookup_with`] reuses it.
    pub fn lookup(&self, coordinate: &mut [f32; 3]) -> Option<usize> {
        self.lookup_with(coordinate, &mut self.scratch())
    }

    /// Scratch space for [`Lookup::lookup_with`], sized for this tree.
    pub fn scratch(&self) -> LookupScratch {
        LookupScratch(self.tree.stack())
    }

    /// [`Lookup::lookup`] with caller-provided scratch space, which does not allocate when it comes from
    /// [`Lookup::scratch`] on this lookup.
    pub fn lookup_with(
        &self,
        coordinate: &mut [f32; 3],
        scratch: &mut LookupScratch,
    ) -> Option<usize> {
        let r = radius(coordinate);
        let scale = if r > self.radius_max {
            Some(self.radius_max / r)
        } else if r < self.radius_min {
            Some(self.radius_min / r)
        } else {
            None
        };
        if let Some(scale) = scale {
            for c in coordinate.iter_mut() {
                *c *= scale;
            }
        }
        self.tree.nearest(coordinate, &mut scratch.0)
    }
}

/// Scratch space for [`Lookup::lookup_with`].
#[derive(Default)]
pub struct LookupScratch(Stack);

impl std::fmt::Debug for LookupScratch {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("LookupScratch")
            .field("capacity", &self.0.capacity())
            .finish()
    }
}

/// The source positions as `M` points; [`Error::InvalidFormat`] unless there are exactly `M` 3-D points.
pub(crate) fn source_positions(hrtf: &Hrtf) -> Result<Vec<[f32; 3]>, Error> {
    let values = hrtf.source_position.values.as_deref().unwrap_or_default();
    if hrtf.c != 3 || values.len() as u64 != 3 * u64::from(hrtf.m) {
        return Err(Error::InvalidFormat);
    }
    Ok(values.chunks_exact(3).map(|p| [p[0], p[1], p[2]]).collect())
}
