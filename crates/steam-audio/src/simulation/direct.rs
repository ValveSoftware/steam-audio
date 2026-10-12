//! The direct sound path from a source to the listener (upstream `DirectSimulator`): distance attenuation, air
//! absorption, propagation delay, directivity, occlusion and transmission through the scene.

use std::ops::{BitOr, BitOrAssign};

use crate::air_absorption::AirAbsorptionModel;
use crate::bands::NUM_BANDS;
use crate::directivity::Directivity;
use crate::distance_attenuation::DistanceAttenuationModel;
use crate::geometry::{Ray, Sphere};
use crate::math::{CoordinateSpace3, Vector3};
use crate::propagation_medium::SPEED_OF_SOUND;
use crate::sampling;
use crate::scene::RayTracer;

/// Parameters of the direct sound path from a source to the listener (upstream `DirectSoundPath`), as computed by
/// [`DirectSimulator`] or by the caller, and applied by [`DirectEffect`](crate::effects::DirectEffect).
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub struct DirectSoundPath {
    /// Gain from distance attenuation.
    pub distance_attenuation: f32,
    /// Gain per band from air absorption.
    pub air_absorption: [f32; NUM_BANDS],
    /// Propagation delay, in seconds. Not used by [`DirectEffect`](crate::effects::DirectEffect).
    pub delay: f32,
    /// Fraction of the source that is visible (1: not occluded).
    pub occlusion: f32,
    /// Fraction of occluded sound transmitted through geometry, per band.
    pub transmission: [f32; NUM_BANDS],
    /// Gain from the source's directivity.
    pub directivity: f32,
}

/// Which parts of a [`DirectSoundPath`] a [`DirectSimulator`] computes (upstream `DirectSimulationFlags`). The
/// others get their neutral value (1, or a delay of 0).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct DirectSimulationFlags(u32);

impl DirectSimulationFlags {
    pub const NONE: Self = Self(0);
    pub const DISTANCE_ATTENUATION: Self = Self(1 << 0);
    pub const AIR_ABSORPTION: Self = Self(1 << 1);
    pub const DIRECTIVITY: Self = Self(1 << 2);
    pub const OCCLUSION: Self = Self(1 << 3);
    pub const TRANSMISSION: Self = Self(1 << 4);
    pub const DELAY: Self = Self(1 << 5);

    /// Flags from their bits, as in `phonon.h` (unknown bits are kept and ignored).
    pub const fn from_bits(bits: u32) -> Self {
        Self(bits)
    }

    pub const fn bits(self) -> u32 {
        self.0
    }

    /// Whether every flag of `other` is set.
    pub const fn contains(self, other: Self) -> bool {
        self.0 & other.0 == other.0
    }
}

impl BitOr for DirectSimulationFlags {
    type Output = Self;

    fn bitor(self, rhs: Self) -> Self {
        Self(self.0 | rhs.0)
    }
}

impl BitOrAssign for DirectSimulationFlags {
    fn bitor_assign(&mut self, rhs: Self) {
        self.0 |= rhs.0;
    }
}

/// How occlusion is computed (upstream `OcclusionType`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub enum OcclusionType {
    /// One ray from the listener to the source's centre: the source is either visible (1) or not (0).
    #[default]
    Raycast,
    /// The fraction of points inside the source's sphere that the listener can see.
    Volumetric,
}

/// Inputs of [`DirectSimulator::simulate`] (the arguments of upstream `simulate`).
#[derive(Debug, Clone, Copy)]
pub struct DirectSimulationParams<'a> {
    pub flags: DirectSimulationFlags,
    pub source: CoordinateSpace3,
    pub listener: CoordinateSpace3,
    pub distance_attenuation_model: &'a DistanceAttenuationModel,
    pub air_absorption_model: &'a AirAbsorptionModel,
    pub directivity: &'a Directivity,
    pub occlusion_type: OcclusionType,
    /// Radius of the source's sphere, for [`OcclusionType::Volumetric`].
    pub occlusion_radius: f32,
    /// Points sampled in the source's sphere, for [`OcclusionType::Volumetric`]; at most the simulator's maximum.
    pub num_occlusion_samples: usize,
    /// Maximum number of surfaces traced through for transmission. Must be positive when transmission is computed.
    pub num_transmission_rays: usize,
}

/// Computes [`DirectSoundPath`]s (upstream `DirectSimulator`).
#[derive(Debug, Clone)]
pub struct DirectSimulator {
    sphere_volume_samples: Vec<Vector3>,
}

impl DirectSimulator {
    /// A simulator for volumetric occlusion with up to `max_num_occlusion_samples` points.
    ///
    /// As upstream, a maximum of 1 keeps no points at all, so volumetric occlusion then always gives 0.
    pub fn new(max_num_occlusion_samples: usize) -> Self {
        let mut sphere_volume_samples = Vec::new();
        if max_num_occlusion_samples > 1 {
            sphere_volume_samples.resize(max_num_occlusion_samples, Vector3::ZERO);
            sampling::sphere_volume_samples(&mut sphere_volume_samples);
        }
        Self {
            sphere_volume_samples,
        }
    }

    /// The direct path from `params.source` to `params.listener`. Occlusion and transmission need a `scene`;
    /// without one they are 1.
    ///
    /// # Panics
    ///
    /// If transmission is computed with `num_transmission_rays == 0`, or `scene` reports a hit without a material.
    pub fn simulate(
        &self,
        scene: Option<&dyn RayTracer>,
        params: &DirectSimulationParams<'_>,
    ) -> DirectSoundPath {
        let flags = params.flags;
        let source = params.source.origin;
        let listener = params.listener.origin;
        let distance = (source - listener).length();

        let distance_attenuation = if flags.contains(DirectSimulationFlags::DISTANCE_ATTENUATION) {
            params.distance_attenuation_model.evaluate(distance)
        } else {
            1.0
        };

        let air_absorption = if flags.contains(DirectSimulationFlags::AIR_ABSORPTION) {
            std::array::from_fn(|band| params.air_absorption_model.evaluate(distance, band))
        } else {
            [1.0; NUM_BANDS]
        };

        let delay = if flags.contains(DirectSimulationFlags::DELAY) {
            Self::direct_path_delay(listener, source)
        } else {
            0.0
        };

        let directivity = if flags.contains(DirectSimulationFlags::DIRECTIVITY) {
            params.directivity.evaluate_at(listener, &params.source)
        } else {
            1.0
        };

        let occlusion = match scene {
            Some(scene) if flags.contains(DirectSimulationFlags::OCCLUSION) => {
                match params.occlusion_type {
                    OcclusionType::Raycast => raycast_occlusion(scene, listener, source),
                    OcclusionType::Volumetric => self.volumetric_occlusion(
                        scene,
                        listener,
                        source,
                        params.occlusion_radius,
                        params.num_occlusion_samples,
                    ),
                }
            }
            _ => 1.0,
        };

        let transmission = match scene {
            Some(scene) if flags.contains(DirectSimulationFlags::TRANSMISSION) => {
                transmission(scene, listener, source, params.num_transmission_rays)
            }
            _ => [1.0; NUM_BANDS],
        };

        DirectSoundPath {
            distance_attenuation,
            air_absorption,
            delay,
            occlusion,
            transmission,
            directivity,
        }
    }

    /// Propagation time from `source` to `listener`, in seconds (upstream `directPathDelay`).
    pub fn direct_path_delay(listener: Vector3, source: Vector3) -> f32 {
        (source - listener).length() / SPEED_OF_SOUND
    }

    /// Of the sphere's sample points the source can see (points poking through nearby geometry do not count), the
    /// fraction the listener can see too; 0 if the source sees none.
    fn volumetric_occlusion(
        &self,
        scene: &dyn RayTracer,
        listener: Vector3,
        source: Vector3,
        source_radius: f32,
        num_samples: usize,
    ) -> f32 {
        let mut occlusion = 0.0f32;
        let mut num_valid_samples = 0u32;

        let num_samples = num_samples.min(self.sphere_volume_samples.len());

        for &sample in &self.sphere_volume_samples[..num_samples] {
            let sphere = Sphere::new(source, source_radius);
            let sample = sampling::transform_sphere_volume_sample(sample, &sphere);

            if scene.is_occluded(source, sample) {
                continue;
            }

            num_valid_samples += 1;

            if !scene.is_occluded(listener, sample) {
                occlusion += 1.0;
            }
        }

        if num_valid_samples == 0 {
            return 0.0;
        }

        occlusion / num_valid_samples as f32
    }
}

fn raycast_occlusion(scene: &dyn RayTracer, listener: Vector3, source: Vector3) -> f32 {
    if scene.is_occluded(listener, source) {
        0.0
    } else {
        1.0
    }
}

/// Traces alternately from the listener towards the source and from the source towards the listener, through up to
/// `num_transmission_rays` surfaces in all, and multiplies their transmission coefficients. If the listener sees
/// the source go behind an object, that object's material matters most, even with more occluders in between.
fn transmission(
    scene: &dyn RayTracer,
    listener: Vector3,
    source: Vector3,
    num_transmission_rays: usize,
) -> [f32; NUM_BANDS] {
    assert!(num_transmission_rays > 0, "at least one transmission ray");

    // Offset along the ray past a hit point before tracing on, to avoid hitting the same surface again.
    const RAY_OFFSET: f32 = 1e-2;

    let rays = [
        Ray::new(listener, (source - listener).normalize_or_zero()),
        Ray::new(source, (listener - source).normalize_or_zero()),
    ];

    let mut current_ray_index = 0;
    let mut num_hits = 0;
    let mut min_distances = [0.0f32; 2];
    let max_distance = (source - listener).length();

    // Product of the transmission coefficients of all hit points.
    let mut accumulated_transmission = [1.0f32; NUM_BANDS];

    for _ in 0..num_transmission_rays {
        let ray = &rays[current_ray_index];
        let min_distance = min_distances[current_ray_index];

        let hit = scene.closest_hit(ray, min_distance, max_distance);

        // Nothing more between the ray origin and the other end.
        if !hit.is_valid() {
            break;
        }

        num_hits += 1;

        let material = hit
            .material
            .expect("a hit reported by the scene has a material");
        for (t, m) in accumulated_transmission
            .iter_mut()
            .zip(material.transmission)
        {
            *t *= m;
        }

        // Where the next segment of this ray starts, if any.
        let min_distance = hit.distance + RAY_OFFSET;
        min_distances[current_ray_index] = min_distance;
        if min_distance >= max_distance {
            break;
        }

        // The two rays have crossed.
        if (min_distances[0] + min_distances[1]) >= max_distance {
            break;
        }

        current_ray_index = 1 - current_ray_index;
    }

    if num_hits <= 1 {
        // One material's coefficients, or 1 with no hit.
        accumulated_transmission
    } else {
        // Hits are assumed to come in pairs (both sides of a wall): the square root avoids counting each wall twice.
        accumulated_transmission.map(f32::sqrt)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn params<'a>(
        flags: DirectSimulationFlags,
        distance: &'a DistanceAttenuationModel,
        air: &'a AirAbsorptionModel,
        directivity: &'a Directivity,
    ) -> DirectSimulationParams<'a> {
        DirectSimulationParams {
            flags,
            source: CoordinateSpace3::canonical_at(Vector3::new(3.0, 4.0, 0.0)),
            listener: CoordinateSpace3::canonical_at(Vector3::ZERO),
            distance_attenuation_model: distance,
            air_absorption_model: air,
            directivity,
            occlusion_type: OcclusionType::Raycast,
            occlusion_radius: 1.0,
            num_occlusion_samples: 16,
            num_transmission_rays: 3,
        }
    }

    #[test]
    fn neutral_values_without_flags_or_scene() {
        let (d, a, dir) = Default::default();
        let simulator = DirectSimulator::new(16);
        let all = DirectSimulationFlags::from_bits(0b11_1111);
        let path = simulator.simulate(None, &params(DirectSimulationFlags::NONE, &d, &a, &dir));
        assert_eq!(
            path,
            DirectSoundPath {
                distance_attenuation: 1.0,
                air_absorption: [1.0; NUM_BANDS],
                delay: 0.0,
                occlusion: 1.0,
                transmission: [1.0; NUM_BANDS],
                directivity: 1.0,
            }
        );

        // Without a scene, occlusion and transmission stay neutral even when requested.
        let path = simulator.simulate(None, &params(all, &d, &a, &dir));
        assert_eq!(path.occlusion, 1.0);
        assert_eq!(path.transmission, [1.0; NUM_BANDS]);
        assert_eq!(path.distance_attenuation, 1.0 / 5.0);
        assert_eq!(path.delay, 5.0 / SPEED_OF_SOUND);
    }

    #[test]
    fn one_sample_keeps_no_points() {
        assert!(DirectSimulator::new(1).sphere_volume_samples.is_empty());
        assert_eq!(DirectSimulator::new(2).sphere_volume_samples.len(), 2);
    }
}
