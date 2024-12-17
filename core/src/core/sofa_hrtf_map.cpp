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

#if defined(IPL_OS_WINDOWS) || defined(IPL_OS_LINUX) || defined(IPL_OS_MACOSX) || defined(IPL_OS_ANDROID) || defined(IPL_OS_IOS) || defined(IPL_OS_WASM)

#include "error.h"
#include "log.h"
#include "sofa_hrtf_map.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SOFAHRTFMap
// --------------------------------------------------------------------------------------------------------------------

SOFAHRTFMap::SOFAHRTFMap(const HRTFSettings& hrtfSettings,
                         int samplingRate)
    : mSamplingRate(samplingRate)
{
    int status = MYSOFA_OK;

    if (hrtfSettings.sofaFileName)
    {
        mSOFA = mysofa_open(hrtfSettings.sofaFileName, static_cast<float>(samplingRate), &mNumSamples, &status);
        if (status != MYSOFA_OK)
        {
            gLog().message(MessageSeverity::Error, "Unable to load SOFA file: %s. [%d]", hrtfSettings.sofaFileName, status);
            throw Exception(Status::Failure);
        }
    }
    else if (hrtfSettings.sofaData)
    {
        mSOFA = mysofa_open_data(reinterpret_cast<const char*>(hrtfSettings.sofaData), hrtfSettings.sofaDataSize,
            static_cast<float>(samplingRate), &mNumSamples, &status);
        if (status != MYSOFA_OK)
        {
            gLog().message(MessageSeverity::Error, "Unable to load SOFA data. [%d]", status);
            throw Exception(Status::Initialization);
        }
    }
    else
    {
        gLog().message(MessageSeverity::Error, "Attempted to create SOFA HRTF without either a file name or a data buffer.");
        throw Exception(Status::Initialization);
    }

    // DataDelay values are not currently supported by Steam Audio.
    for (auto i = 0u; i < mSOFA->hrtf->DataDelay.elements; ++i)
    {
        if (fabsf(mSOFA->hrtf->DataDelay.values[i]) > 1e-3f)
        {
            gLog().message(MessageSeverity::Error,
                           "SOFA file %s contains non-zero values for Data.Delay; this is not currently supported.",
                           hrtfSettings.sofaFileName);

            throw Exception(Status::Failure);
        }
    }

    patchSOFANeighborhood();

    mHRIR.resize(kNumEars, numHRIRs(), mNumSamples);

    for (auto i = 0; i < numHRIRs(); ++i)
    {
        const float* hrirData[kNumEars] = { nullptr, nullptr };
        getHRIRsForIndex(i, hrirData);
        for (auto j = 0; j < kNumEars; ++j)
        {
            memcpy(mHRIR[j][i], hrirData[j], mNumSamples * sizeof(float));
        }
    }
}

SOFAHRTFMap::~SOFAHRTFMap()
{
    mysofa_close(mSOFA);
}

int SOFAHRTFMap::nearestHRIR(const Vector3f& direction) const
{
    auto sofaDirection = toSOFACoordinates(direction);
    auto index = mysofa_lookup(mSOFA->lookup, sofaDirection.elements);
    return index;
}

void SOFAHRTFMap::interpolatedHRIRWeights(const Vector3f& direction,
                                          int indices[8],
                                          float weights[8]) const
{
    auto sofaDirection = toSOFACoordinates(direction);

    // Get the index of the nearest measurement to the query position.
    auto nearest = nearestHRIR(direction);

    // If the query position is (almost) exactly one of the measurement positions, just use the corresponding
    // HRIR, without interpolation.
    auto d = distanceToMeasurement(sofaDirection, nearest);
    if (fabsf(d) < 1e-5f)
    {
        indices[0] = nearest;
        weights[0] = 1.0f;
        return;
    }

    // Initialize all the indices and weights to zero.
    for (auto i = 0; i < 8; ++i)
    {
        indices[i] = 0;
        weights[i] = 0.0f;
    }

    // Get the indices of the 6 neighbors of the nearest measurement (+/- x, +/- y, +/- z). A negative value
    // indicates that the corresponding neighbor is not present (i.e., nearest is at one of the edges of the
    // data set.
    auto neighbors = mysofa_neighborhood(mSOFA->neighborhood, nearest);

    // Calculate the coordinates of the query point.
    auto p = sofaDirection;
    mysofa_c2s(p.elements);
    auto phi = p[0];
    auto theta = p[1];
    auto r = p[2];

    // Calculate the coordinates of the nearest neighbor.
    auto nearestPosition = measurementPosition(nearest);
    mysofa_c2s(nearestPosition.elements);

    // Calculate the coordinates of the 6 neighbors of the nearest neighbor.
    Vector3f neighborPositions[6];
    for (auto i = 0; i < 6; ++i)
    {
        if (neighbors[i] >= 0)
        {
            neighborPositions[i] = measurementPosition(neighbors[i]);
            mysofa_c2s(neighborPositions[i].elements);
        }
        else
        {
            neighborPositions[i] = Vector3f{};
        }
    }

    // We only care about the phi coordinates of the +/- phi neighbors, the theta coordinates of the
    // +/- theta neighbors, and the r coordinates of the +/- r neighbors, i.e., 6 unique values.
    float neighborCoordinates[6];
    neighborCoordinates[0] = neighborPositions[0][0];
    neighborCoordinates[1] = neighborPositions[1][0];
    neighborCoordinates[2] = neighborPositions[2][1];
    neighborCoordinates[3] = neighborPositions[3][1];
    neighborCoordinates[4] = neighborPositions[4][2];
    neighborCoordinates[5] = neighborPositions[5][2];

    // Handle the case where the +phi neighbor has wrapped around in azimuth relative to the nearest neighbor.
    if (neighborCoordinates[0] > nearestPosition[0])
    {
        while (neighborCoordinates[0] - nearestPosition[0] >= 180.0f)
        {
            neighborCoordinates[0] -= 360.0f;
        }
    }
    else
    {
        while (neighborCoordinates[0] - nearestPosition[0] <= -180.0f)
        {
            neighborCoordinates[0] += 360.0f;
        }
    }

    // Handle the case where the -phi neighbor has wrapped around in azimuth relative to the nearest neighbor.
    if (neighborCoordinates[1] > nearestPosition[0])
    {
        while (neighborCoordinates[1] - nearestPosition[0] >= 180.0f)
        {
            neighborCoordinates[1] -= 360.0f;
        }
    }
    else
    {
        while (neighborCoordinates[0] - nearestPosition[0] <= -180.0f)
        {
            neighborCoordinates[1] += 360.0f;
        }
    }

    // Calculate the distances (along each axis) between the query point and the 6 neighbors of the nearest neighbor. For the +/- phi neighbors
    // we want the phi distance, etc.
    float neighborDistances[6];
    neighborDistances[0] = ((neighborCoordinates[0] - nearestPosition[0]) * (phi - nearestPosition[0]) < 0.0f) ? std::numeric_limits<float>::max() : fabsf(neighborCoordinates[0] - nearestPosition[0]);
    neighborDistances[1] = ((neighborCoordinates[1] - nearestPosition[0]) * (phi - nearestPosition[0]) < 0.0f) ? std::numeric_limits<float>::max() : fabsf(neighborCoordinates[1] - nearestPosition[0]);
    neighborDistances[2] = ((neighborCoordinates[2] - nearestPosition[1]) * (theta - nearestPosition[1]) < 0.0f) ? std::numeric_limits<float>::max() : fabsf(neighborCoordinates[2] - nearestPosition[1]);
    neighborDistances[3] = ((neighborCoordinates[3] - nearestPosition[1]) * (theta - nearestPosition[1]) < 0.0f) ? std::numeric_limits<float>::max() : fabsf(neighborCoordinates[3] - nearestPosition[1]);
    neighborDistances[4] = ((neighborCoordinates[4] - nearestPosition[2]) * (r - nearestPosition[2]) < 0.0f) ? std::numeric_limits<float>::max() : fabsf(neighborCoordinates[4] - nearestPosition[2]);
    neighborDistances[5] = ((neighborCoordinates[5] - nearestPosition[2]) * (r - nearestPosition[2]) < 0.0f) ? std::numeric_limits<float>::max() : fabsf(neighborCoordinates[5] - nearestPosition[2]);

    // If neighbors[0] is closer to the query point, phiIndex is set to 0 (interpolation occurs in the +phi direction);
    // if neighbors[1] is closer, phiIndex is set to 1 (interpolation occurs in the -phi direction); otherwise it's set
    // to -1 (no interpolation along the phi axis. The same process is repeated for theta and r.
    auto phiIndex = -1;
    if (neighbors[0] >= 0 && neighbors[1] >= 0)
    {
        phiIndex = (neighborDistances[0] <= neighborDistances[1]) ? 0 : 1;
    }

    auto thetaIndex = -1;
    if (neighbors[2] >= 0 && neighbors[3] >= 0)
    {
        thetaIndex = (neighborDistances[2] <= neighborDistances[3]) ? 2 : 3;
    }

    auto rIndex = -1;
    if (neighbors[4] >= 0 && neighbors[5] >= 0)
    {
        rIndex = (neighborDistances[4] <= neighborDistances[5]) ? 4 : 5;
    }

    // Index 0 is the nearest neighbor.
    // Index 1 is the phi-neighbor of the nearest neighbor.
    // Index 2 is the theta-neighbor.
    // Index 3 is the r-neighbor.
    // Index 4 is the (theta, phi)-neighbor, i.e. the phi neighbor of the theta neighbor.
    // Index 5 is the (r, phi)-neighbor, i.e. the phi neighbor of the r neighbor.
    // Index 6 is the (r, theta)-neighbor, i.e. the theta neighbor of the r neighbor.
    // Index 7 is the (r, theta, phi)-neighbor, i.e. the phi neighbor of the (r, theta)-neighbor.
    indices[0] = nearest;
    indices[1] = (phiIndex < 0) ? -1 : neighbors[phiIndex];
    indices[2] = (thetaIndex < 0) ? -1 : neighbors[thetaIndex];
    indices[3] = (rIndex < 0) ? -1 : neighbors[rIndex];
    indices[4] = (phiIndex < 0 || thetaIndex < 0) ? -1 : mysofa_neighborhood(mSOFA->neighborhood, indices[2])[phiIndex];
    indices[5] = (phiIndex < 0 || rIndex < 0) ? -1 : mysofa_neighborhood(mSOFA->neighborhood, indices[3])[phiIndex];
    indices[6] = (thetaIndex < 0 || rIndex < 0) ? -1 : mysofa_neighborhood(mSOFA->neighborhood, indices[3])[thetaIndex];
    indices[7] = (phiIndex < 0 || thetaIndex < 0 || rIndex < 0) ? -1 : mysofa_neighborhood(mSOFA->neighborhood, indices[6])[phiIndex];

    // Remove duplicate indices.
    for (auto i = 0; i < 8; ++i)
    {
        for (auto j = 0; j < i; ++j)
        {
            if (indices[i] == indices[j])
            {
                indices[i] = -1;
                break;
            }
        }
    }

    // Calculate the phi, theta, r coordinates of each point that we're interpolating between.
    float phiSelf[8];
    float thetaSelf[8];
    float rSelf[8];
    for (auto i = 0; i < 8; ++i)
    {
        if (indices[i] >= 0)
        {
            auto p = measurementPosition(indices[i]);
            mysofa_c2s(p.elements);

            phiSelf[i] = p[0];
            thetaSelf[i] = p[1];
            rSelf[i] = p[2];

            // If we're more than 180 degrees away from phiSelf[0], we've wrapped around in azimuth, and
            // are interpolating a point roughly in front of the listener. To prevent a discontinuity here,
            // adjust the phi coordinates by adding or subtracting 360 degrees as necessary.
            if (fabsf(phiSelf[i] - phiSelf[0]) >= 180.0f)
            {
                if (phiSelf[i] > phiSelf[0])
                {
                    phiSelf[i] -= 360.0f;
                }
                else
                {
                    phiSelf[i] += 360.0f;
                }
            }
        }
        else
        {
            // If indices[i] is negative, there's no corresponding point to interpolate, so set the
            // coordinates to FLT_MAX.
            phiSelf[i] = std::numeric_limits<float>::max();
            thetaSelf[i] = std::numeric_limits<float>::max();
            rSelf[i] = std::numeric_limits<float>::max();
        }
    }

    const auto kNearest = 0;
    const auto kPhiNeighbor = 1;
    const auto kThetaNeighbor = 2;
    const auto kRNeighbor = 3;
    const auto kThetaPhiNeighbor = 4;
    const auto kRPhiNeighbor = 5;
    const auto kRThetaNeighbor = 6;
    const auto kRThetaPhiNeighbor = 7;

    int phiNeighbors[8];
    phiNeighbors[kNearest] = kPhiNeighbor;
    phiNeighbors[kPhiNeighbor] = kNearest;
    phiNeighbors[kThetaNeighbor] = kThetaPhiNeighbor;
    phiNeighbors[kRNeighbor] = kRPhiNeighbor;
    phiNeighbors[kThetaPhiNeighbor] = kThetaNeighbor;
    phiNeighbors[kRPhiNeighbor] = kRNeighbor;
    phiNeighbors[kRThetaNeighbor] = kRThetaPhiNeighbor;
    phiNeighbors[kRThetaPhiNeighbor] = kRThetaNeighbor;

    int thetaNeighbors[8];
    thetaNeighbors[kNearest] = kThetaNeighbor;
    thetaNeighbors[kPhiNeighbor] = kThetaPhiNeighbor;
    thetaNeighbors[kThetaNeighbor] = kNearest;
    thetaNeighbors[kRNeighbor] = kRThetaNeighbor;
    thetaNeighbors[kThetaPhiNeighbor] = kPhiNeighbor;
    thetaNeighbors[kRPhiNeighbor] = kRThetaPhiNeighbor;
    thetaNeighbors[kRThetaNeighbor] = kRNeighbor;
    thetaNeighbors[kRThetaPhiNeighbor] = kRPhiNeighbor;

    int rNeighbors[8];
    rNeighbors[kNearest] = kRNeighbor;
    rNeighbors[kPhiNeighbor] = kRPhiNeighbor;
    rNeighbors[kThetaNeighbor] = kRThetaNeighbor;
    rNeighbors[kRNeighbor] = kNearest;
    rNeighbors[kThetaPhiNeighbor] = kRThetaPhiNeighbor;
    rNeighbors[kRPhiNeighbor] = kPhiNeighbor;
    rNeighbors[kRThetaNeighbor] = kThetaNeighbor;
    rNeighbors[kRThetaPhiNeighbor] = kThetaPhiNeighbor;

    float phiOther[8];
    float thetaOther[8];
    float rOther[8];
    for (auto i = 0; i < 8; ++i)
    {
        phiOther[i] = (indices[phiNeighbors[i]] < 0) ? phiSelf[i] : phiSelf[phiNeighbors[i]];
        thetaOther[i] = (indices[thetaNeighbors[i]] < 0) ? thetaSelf[i] : thetaSelf[thetaNeighbors[i]];
        rOther[i] = (indices[rNeighbors[i]] < 0) ? rSelf[i] : rSelf[rNeighbors[i]];
    }

    // Handle the case where the query point has wrapped around in azimuth relative to the nearest neighbor.
    if (fabsf(phi - phiSelf[0]) >= 180.0f)
    {
        if (phi > phiSelf[0])
        {
            phi -= 360.0f;
        }
        else
        {
            phi += 360.0f;
        }
    }

    // Calculate weights.
    auto totalWeight = 0.0f;
    for (auto i = 0; i < 8; ++i)
    {
        if (indices[i] >= 0)
        {
            // Linearly interpolate between phiSelf[i] and phiOther[i]. If this number is outside the range [0, 1], we may
            // have wrapped around, in which case add or subtract 1 to try to correct the issue.
            auto phiTerm = (fabsf(phiOther[i] - phiSelf[i]) < 1e-5f) ? 1.0f : (phiOther[i] - phi) / (phiOther[i] - phiSelf[i]);
            if (phiTerm < 0.0f)
            {
                phiTerm += 1.0f;
            }
            else if (phiTerm > 1.0f)
            {
                phiTerm -= 1.0f;
            }
            phiTerm = std::max(0.0f, std::min(phiTerm, 1.0f));

            // Same as above, but for theta.
            auto thetaTerm = (fabsf(thetaOther[i] - thetaSelf[i]) < 1e-5f) ? 1.0f : (thetaOther[i] - theta) / (thetaOther[i] - thetaSelf[i]);
            if (thetaTerm < 0.0f)
            {
                thetaTerm += 1.0f;
            }
            else if (thetaTerm > 1.0f)
            {
                thetaTerm -= 1.0f;
            }
            thetaTerm = std::max(0.0f, std::min(thetaTerm, 1.0f));

            // Same as above, but for r.
            auto rTerm = (fabsf(rOther[i] - rSelf[i]) < 1e-5f) ? 1.0f : (rOther[i] - r) / (rOther[i] - rSelf[i]);
            rTerm = std::max(0.0f, std::min(rTerm, 1.0f));

            // Weight is the product of the phi, theta, and r terms.
            weights[i] = phiTerm * thetaTerm * rTerm;
        }
        else
        {
            indices[i] = nearest;
            weights[i] = 0.0f;
        }

        totalWeight += weights[i];
    }

    // Normalize weights to sum to 1.
    for (auto i = 0; i < 8; ++i)
    {
        weights[i] /= totalWeight;
    }
}

Vector3f SOFAHRTFMap::measurementPosition(int index) const
{
    auto elements = mSOFA->hrtf->SourcePosition.values + index * mSOFA->hrtf->C;
    return Vector3f(elements[0], elements[1], elements[2]);
}

float SOFAHRTFMap::distanceToMeasurement(const Vector3f& point,
                                         int index) const
{
    return (index >= 0) ? (point - measurementPosition(index)).length() : std::numeric_limits<float>::max();
}

void SOFAHRTFMap::getHRIRsForIndex(int index,
                                   const float** hrirs)
{
    auto hrirSize = mSOFA->hrtf->N;
    auto numListeners = mSOFA->hrtf->R;
    auto offset = index * numListeners * hrirSize;
    hrirs[0] = &mSOFA->hrtf->DataIR.values[offset];
    hrirs[1] = &mSOFA->hrtf->DataIR.values[offset + hrirSize];
}

void SOFAHRTFMap::patchSOFANeighborhood()
{
    auto numMeasurements = mSOFA->neighborhood->elements;
    for (auto i = 0; i < numMeasurements; ++i)
    {
        auto neighbors = &mSOFA->neighborhood->index[6 * i];

        auto measurement = measurementPosition(i);
        mysofa_c2s(measurement.elements);

        // If this measurement doesn't have a +phi neighbor, keep moving in the +phi direction until we wrap around
        // and find a measurement. Such a neighbor may not exist if we've at +/-90 degrees elevation (directly above
        // or below the listener).
        if (neighbors[0] < 0)
        {
            for (auto dPhi = 0.5f; dPhi <= 360.0f; dPhi += 0.5f)
            {
                auto point = measurement;
                point[0] += dPhi;
                if (point[0] > 360.0f)
                {
                    point[0] -= 360.0f;
                }

                mysofa_s2c(point.elements);

                auto nearest = mysofa_lookup(mSOFA->lookup, point.elements);
                if (nearest != i)
                {
                    neighbors[0] = nearest;
                    break;
                }
            }
        }

        // Repeat the above search, but in the -phi direction this time.
        if (neighbors[1] < 0)
        {
            for (auto dPhi = 0.5f; dPhi <= 360.0f; dPhi += 0.5f)
            {
                auto point = measurement;
                point[0] -= dPhi;
                if (point[0] < 0.0f)
                {
                    point[0] += 360.0f;
                }

                mysofa_s2c(point.elements);

                auto nearest = mysofa_lookup(mSOFA->lookup, point.elements);
                if (nearest != i)
                {
                    neighbors[1] = nearest;
                    break;
                }
            }
        }
    }
}

Vector3f SOFAHRTFMap::toSOFACoordinates(const Vector3f& v)
{
    return Vector3f(-v.z(), -v.x(), v.y());
}

}

#endif
