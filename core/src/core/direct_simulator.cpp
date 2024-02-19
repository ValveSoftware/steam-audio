//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "direct_simulator.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// DirectSimulator
// --------------------------------------------------------------------------------------------------------------------

DirectSimulator::DirectSimulator(int maxNumOcclusionSamples)
{
    if (maxNumOcclusionSamples > 1)
    {
        mSphereVolumeSamples.resize(maxNumOcclusionSamples);
        Sampling::generateSphereVolumeSamples(maxNumOcclusionSamples, mSphereVolumeSamples.data());
    }
}

void DirectSimulator::simulate(const IScene* scene,
                               DirectSimulationFlags flags,
                               const CoordinateSpace3f& source,
                               const CoordinateSpace3f& listener,
                               const DistanceAttenuationModel& distanceAttenuationModel,
                               const AirAbsorptionModel& airAbsorptionModel,
                               const Directivity& directivity,
                               OcclusionType occlusionType,
                               float occlusionRadius,
                               int numOcclusionSamples,
                               int numTransmissionRays,
                               DirectSoundPath& directSoundPath)
{
    auto distance = (source.origin - listener.origin).length();

    if (flags & CalcDistanceAttenuation)
    {
        directSoundPath.distanceAttenuation = distanceAttenuationModel.evaluate(distance);
    }
    else
    {
        directSoundPath.distanceAttenuation = 1.0f;
    }

    if (flags & CalcAirAbsorption)
    {
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            directSoundPath.airAbsorption[i] = airAbsorptionModel.evaluate(distance, i);
        }
    }
    else
    {
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            directSoundPath.airAbsorption[i] = 1.0f;
        }
    }

    if (flags & CalcDelay)
    {
        directSoundPath.delay = directPathDelay(listener.origin, source.origin);
    }
    else
    {
        directSoundPath.delay = 0.0f;
    }

    if (flags & CalcDirectivity)
    {
        directSoundPath.directivity = directivity.evaluateAt(listener.origin, source);
    }
    else
    {
        directSoundPath.directivity = 1.0f;
    }

    // Constructor for DirectSoundPath sets the default value correctly.
    if (scene && (flags & CalcOcclusion))
    {
        switch (occlusionType)
        {
        case OcclusionType::Raycast:
            directSoundPath.occlusion =
                raycastOcclusion(*scene, listener.origin, source.origin);
            break;

        case OcclusionType::Volumetric:
            directSoundPath.occlusion =
                volumetricOcclusion(*scene, listener.origin, source.origin, occlusionRadius, numOcclusionSamples);
            break;

        default:
            directSoundPath.occlusion = 0.0f;
            break;
        }
    }
    else
    {
        directSoundPath.occlusion = 1.0f;
    }

    if (scene && (flags & CalcTransmission))
    {
        transmission(*scene, listener.origin, source.origin, directSoundPath.transmission, numTransmissionRays);
    }
    else
    {
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            directSoundPath.transmission[i] = 1.0f;
        }
    }
}

float DirectSimulator::directPathDelay(const Vector3f& listener,
                                       const Vector3f& source)
{
    return (source - listener).length() / PropagationMedium::kSpeedOfSound;
}

float DirectSimulator::raycastOcclusion(const IScene& scene,
                                        const Vector3f& listenerPosition,
                                        const Vector3f& sourcePosition)
{
    return scene.isOccluded(listenerPosition, sourcePosition) ? 0.0f : 1.0f;
}

// Each source has a radius, and several points are sampled within the volume
// of this sphere. To calculate a source's volumetric occlusion factor, we first
// count the number of samples that are visible to the source. (If the source is
// close to a wall or the floor, some samples may stick out through the surface,
// and these should not be counted when calculating occlusion in the next step.
// Essentially the source is shaped like a subset of the sphere's volume, where
// the subset is determined by the volumetric samples that do not cross surface
// boundaries.) For each sample that's visible to the source, we check whether
// it's also visible to the listener. The fraction of samples visible to the
// source that are also visible to the listener is then the occlusion factor.
float DirectSimulator::volumetricOcclusion(const IScene& scene,
                                           const Vector3f& listenerPosition,
                                           const Vector3f& sourcePosition,
                                           float sourceRadius,
                                           int numSamples)
{
    auto occlusion = 0.0f;
    auto numValidSamples = 0;

    numSamples = std::min(numSamples, static_cast<int>(mSphereVolumeSamples.size(0)));

    for (auto i = 0; i < numSamples; ++i)
    {
        Sphere sphere(sourcePosition, sourceRadius);
        auto sample = Sampling::transformSphereVolumeSample(mSphereVolumeSamples[i], sphere);

        if (scene.isOccluded(sourcePosition, sample))
            continue;

        ++numValidSamples;

        if (!scene.isOccluded(listenerPosition, sample))
        {
            occlusion += 1.0f;
        }
    }

    if (numValidSamples == 0)
        return .0f;

    return occlusion / numValidSamples;
}

void DirectSimulator::transmission(const IScene& scene,
                                   const Vector3f& listenerPosition,
                                   const Vector3f& sourcePosition,
                                   float* transmissionFactors,
                                   int numTransmissionRays)
{
    assert(numTransmissionRays > 0);

    // If, after finding a hit point, we want to continue tracing the ray towards the
    // source, then offset the ray origin by this distance along the ray direction, to
    // prevent self-intersection.
    constexpr auto kRayOffset = 1e-2f;

    // We will alternate between tracing a ray from the listener to the source, and from the source to the listener.
    // The motivation is that if the listener observes the source go behind an object, then that object's material is
    // most relevant in terms of the expected amount of transmitted sound, even if there are multiple other occluders
    // between the source and the listener.
    Ray rays[] = {
        Ray{listenerPosition, Vector3f::unitVector(sourcePosition - listenerPosition)},
        Ray{sourcePosition, Vector3f::unitVector(listenerPosition - sourcePosition)},
    };

    auto currentRayIndex = 0;
    auto numHits = 0;
    float minDistances[] = {0.0f, 0.0f};
    auto maxDistance = (sourcePosition - listenerPosition).length();

    // Product of the transmission coefficients of all hit points.
    float accumulatedTransmission[Bands::kNumBands] = {1.0f, 1.0f, 1.0f};

    for (auto i = 0; i < numTransmissionRays; ++i)
    {
        // Select the ray we want to trace for this iteration.
        const auto& ray = rays[currentRayIndex];
        auto& minDistance = minDistances[currentRayIndex];

        auto hit = scene.closestHit(ray, minDistance, maxDistance);

        // If there's nothing more between the ray origin and the source, stop.
        if (!hit.isValid())
            break;

        numHits++;

        // Accumulate the product of the transmission coefficients of all materials
        // encountered so far.
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            accumulatedTransmission[j] *= hit.material->transmission[j];
        }

        // Calculate the origin of the next ray segment we'll trace, if any.
        minDistance = hit.distance + kRayOffset;
        if (minDistance >= maxDistance)
            break;

        // If the total distance traveled by both rays is greater than the distance between the source and the
        // listener, then the rays have crossed, so stop.
        if ((minDistances[0] + minDistances[1]) >= maxDistance)
            break;

        // Switch to the other ray for the next iteration.
        currentRayIndex = 1 - currentRayIndex;
    }

    if (numHits <= 1)
    {
        // If we have only 1 hit, then use the transmission coefficients of that material.
        // If we have no hits, this will automatically set the transmission coefficients to
        // [1, 1, 1] (i.e., 100% transmission).
        memcpy(transmissionFactors, accumulatedTransmission, Bands::kNumBands * sizeof(float));
    }
    else
    {
        // We have more than one hit, so set the total transmission to the square root of the
        // product of the transmission coefficients of all hit points. This assumes that hit
        // points occur in pairs, e.g. both sides of a solid wall, in which case we avoid
        // double-counting the transmission due to both sides of the wall.
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            transmissionFactors[i] = sqrtf(accumulatedTransmission[i]);
        }
    }
}

}
