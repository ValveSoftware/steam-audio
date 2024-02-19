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

#include "hrtf_map.h"

#include "error.h"
#include "log.h"
#include "math_functions.h"
#include "polar_vector.h"
#include "sh.h"

extern ipl::byte_t gDefaultHrtfData[];

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// HRTFMap
// --------------------------------------------------------------------------------------------------------------------

// The layout of the HRTF data is as follows:
//
//  HEADER
//  FOURCC identifier ('HRTF')          int32_t
//  File format version                 int32_t
//
//  METADATA
//  # HRIR sampling directions          int32_t
//  For each direction:
//      Elevation (canonical coords)    float
//      Azimuth (canonical coords)      float
//  # sampling rates                    int32_t
//  For each sampling rate:
//      Sampling rate (Hz)              int32_t
//      # HRIR samples (N)              int32_t
//
//  HRIR DATA
//  For each sampling rate:
//      For each direction:
//          Left-ear HRIR               N x float
//          Right-ear HRIR              N x float
//
//  This class is designed with the CIPIC database in mind. (Particularly, the coordinate system.) For more
//  details, see:
//
//  https://web.archive.org/web/20170916053150/http://interface.cipic.ucdavis.edu/sound/hrtf.html
//
//  The .hrtf files are generated from SOFA files using sofa2hrtf. For more information on the SOFA format, see:
//
//  http://www.sofaconventions.org/mediawiki/index.php/Main_Page

HRTFMap::HRTFMap(int samplingRate, const uint8_t* hrtfData /* = nullptr */)
{
    const byte_t* readPointer = (hrtfData) ? hrtfData : gDefaultHrtfData;

    verifyDataHeader(readPointer);
    loadNumHRIRs(readPointer);
    loadDirections(readPointer);

    auto bytesToSkipAfter = loadNumSamplesForSamplingRate(readPointer, samplingRate);
    loadHRIRs(readPointer);
    readPointer += bytesToSkipAfter;

    loadAmbisonicsHRIRs(readPointer, samplingRate);
}

int HRTFMap::nearestHRIR(const Vector3f& direction) const
{
    // The input direction is in canonical spherical coordinates, but the HRTF data was measured in interaural
    // spherical coordinates. So we convert.
    auto interauralDirection = InterauralSphericalVector3f(direction);

    // Convert the azimuth and elevation from radians to degrees.
    auto phi = interauralDirection.azimuth / Math::kDegreesToRadians;
    auto theta = interauralDirection.elevation / Math::kDegreesToRadians;

    // Calculate the index of the HRTF to use.
    auto phiIndex = nearestNeighbor(mAzimuths.data(), mNumAzimuths, phi);
    auto thetaIndex = nearestNeighbor(mElevationsForAzimuth[phiIndex], mNumElevations, theta);
    auto index = phiIndex * mNumElevations + thetaIndex;

    return index;
}

void HRTFMap::interpolatedHRIRWeights(const Vector3f& direction,
                                      int indices[8],
                                      float weights[8]) const
{
    // The input direction is in canonical spherical coordinates, but the HRTF data was measured in interaural
    // spherical coordinates. So we convert.
    auto interauralDirection = InterauralSphericalVector3f(direction);

    // Convert the azimuth and elevation from radians to degrees.
    auto phi = interauralDirection.azimuth / Math::kDegreesToRadians;
    auto theta = interauralDirection.elevation / Math::kDegreesToRadians;
    auto thetaForPhiMin = theta;
    auto thetaForPhiMax = theta;

    // The input azimuth (phi) lies between two azimuth values at which the HRTFs were measured. Calculate the
    // indices in mAzimuths where these two azimuth values (phiMin and phiMax) occur, as well as their values
    // themselves.
    auto phiIndices = lowerAndUpperBound(mAzimuths.data(), mNumAzimuths, phi);
    auto phiMinIndex = phiIndices.first;
    auto phiMaxIndex = phiIndices.second;
    auto phiMin = mAzimuths[phiMinIndex];
    auto phiMax = mAzimuths[phiMaxIndex];

    if (phiMinIndex == 0 && phiMaxIndex == 0)
    {
        phiMin += 2 * (-90.0f - phiMin);
        thetaForPhiMin += 2 * (180.0f - thetaForPhiMin);
    }
    if (phiMinIndex == mNumAzimuths - 1 && phiMaxIndex == mNumAzimuths - 1)
    {
        phiMax += 2 * (90.0f - phiMax);
        thetaForPhiMax += 2 * (180.0f - thetaForPhiMax);
    }

    if (direction.z() > 0.0f)
    {
        std::swap(phiMin, phiMax);
        std::swap(phiMinIndex, phiMaxIndex);
        std::swap(thetaForPhiMin, thetaForPhiMax);
    }

    // The input elevation (theta) lies between two elevation values at which the HRTFs were measured, with azimuth
    // set to phiMin. Calculate the indices in mElevationsForAzimuth[phiMinIndex] at which these two elevation
    // values (thetaMinForPhiMin, thetaMaxForPhiMin) occur, as well as their values themselves.
    auto thetaIndicesForPhiMin = lowerAndUpperBound(mElevationsForAzimuth[phiMinIndex], mNumElevations, thetaForPhiMin);
    auto thetaMinIndexForPhiMin = thetaIndicesForPhiMin.first;
    auto thetaMaxIndexForPhiMin = thetaIndicesForPhiMin.second;
    auto thetaMinForPhiMin = mElevationsForAzimuth[phiMinIndex][thetaMinIndexForPhiMin];
    auto thetaMaxForPhiMin = mElevationsForAzimuth[phiMinIndex][thetaMaxIndexForPhiMin];

    // The input elevation (theta) lies between two elevation values at which the HRTFs were measured, with azimuth
    // set to phiMax. Calculate the indices in mElevationsForAzimuth[phiMaxIndex] at which these two elevation
    // values (thetaMinForPhiMax, thetaMaxForPhiMax) occur, as well as their values themselves.
    auto thetaIndicesForPhiMax = lowerAndUpperBound(mElevationsForAzimuth[phiMaxIndex], mNumElevations, thetaForPhiMax);
    auto thetaMinIndexForPhiMax = thetaIndicesForPhiMax.first;
    auto thetaMaxIndexForPhiMax = thetaIndicesForPhiMax.second;
    auto thetaMinForPhiMax = mElevationsForAzimuth[phiMaxIndex][thetaMinIndexForPhiMax];
    auto thetaMaxForPhiMax = mElevationsForAzimuth[phiMaxIndex][thetaMaxIndexForPhiMax];

    // Calculate linear interpolation weights between azimuth values.
    auto weightPhiMin = 0.0f, weightPhiMax = 0.0f;
    calculateLinearInterpolationWeights(phi, phiMin, phiMax, phiMinIndex, phiMaxIndex, weightPhiMin, weightPhiMax);

    // Calculate linear interpolation weights between elevation values, for both phiMin and phiMax.
    auto weightThetaMinForPhiMin = 0.0f, weightThetaMaxForPhiMin = 0.0f;
    auto weightThetaMinForPhiMax = 0.0f, weightThetaMaxForPhiMax = 0.0f;
    calculateLinearInterpolationWeights(thetaForPhiMin, thetaMinForPhiMin, thetaMaxForPhiMin,
                                        thetaMinIndexForPhiMin, thetaMaxIndexForPhiMin,
                                        weightThetaMinForPhiMin, weightThetaMaxForPhiMin);
    calculateLinearInterpolationWeights(thetaForPhiMax, thetaMinForPhiMax, thetaMaxForPhiMax,
                                        thetaMinIndexForPhiMax, thetaMaxIndexForPhiMax,
                                        weightThetaMinForPhiMax, weightThetaMaxForPhiMax);

    // Calculate bilinear interpolation weights between the four sample values (phiMin, thetaMinForPhiMin),
    // (phiMin, thetaMaxForPhiMin), (phiMax, thetaMinForPhiMax), and (phiMax, thetaMaxForPhiMax).
    weights[0] = weightPhiMin * weightThetaMinForPhiMin;
    weights[1] = weightPhiMin * weightThetaMaxForPhiMin;
    weights[2] = weightPhiMax * weightThetaMinForPhiMax;
    weights[3] = weightPhiMax * weightThetaMaxForPhiMax;
    weights[4] = 0.0f;
    weights[5] = 0.0f;
    weights[6] = 0.0f;
    weights[7] = 0.0f;

    // Calculate row-major linear indices within mLeftHrtf and mRightHrtf for the four HRTFs we will be
    // interpolating.
    indices[0] = phiMinIndex * mNumElevations + thetaMinIndexForPhiMin;
    indices[1] = phiMinIndex * mNumElevations + thetaMaxIndexForPhiMin;
    indices[2] = phiMaxIndex * mNumElevations + thetaMinIndexForPhiMax;
    indices[3] = phiMaxIndex * mNumElevations + thetaMaxIndexForPhiMax;
    indices[4] = -1;
    indices[5] = -1;
    indices[6] = -1;
    indices[7] = -1;
}

void HRTFMap::verifyDataHeader(const byte_t*& readPointer)
{
    // Skip the FOURCC identifier.
    readPointer += sizeof(int32_t);

    // Read the version, and verify that it is a supported version.
    mVersion = *reinterpret_cast<const int32_t*>(readPointer);
    if (mVersion < kMinSupportedFileFormatVersion || kMaxSupportedFileFormatVersion < mVersion)
    {
        gLog().message(MessageSeverity::Error, "%s: Unsupported HRTF data format version: %d.", __FUNCTION__, mVersion);
        throw Exception(Status::Initialization);
    }

    readPointer += sizeof(int32_t);
}

void HRTFMap::loadNumHRIRs(const byte_t*& readPointer)
{
    mNumHRIRs = *reinterpret_cast<const int32_t*>(readPointer);
    readPointer += sizeof(int32_t);
}

void HRTFMap::loadDirections(const byte_t*& readPointer)
{
    // Read the azimuth and elevation values from the data file. These will be stored in canonical spherical
    // coordinates, although the data was measured by performing regular sampling in interaural polar coordinates.
    vector<float> canonicalDirections(2 * mNumHRIRs);
    memcpy(canonicalDirections.data(), readPointer, canonicalDirections.size() * sizeof(float));
    readPointer += canonicalDirections.size() * sizeof(float);

    // Convert all the loaded angles from degrees to radians.
    for (auto i = 0U; i < canonicalDirections.size(); ++i)
    {
        canonicalDirections[i] *= Math::kDegreesToRadians;
    }

    // Convert all the loaded angles from canonical spherical coordinates to interaural spherical coordinates.
    vector<int> interauralAzimuths(mNumHRIRs);
    vector<int> interauralElevations(mNumHRIRs);
    for (auto i = 0; i < mNumHRIRs; ++i)
    {
        auto canonicalCoordinates = SphericalVector3f(1.0f, canonicalDirections[2 * i + 0],
                                                      canonicalDirections[2 * i + 1]);
        auto interauralCoordinates = InterauralSphericalVector3f(canonicalCoordinates);

        interauralAzimuths[i] = static_cast<int>(roundf(interauralCoordinates.azimuth
                                                 / Math::kDegreesToRadians));
        interauralElevations[i] = static_cast<int>(roundf(interauralCoordinates.elevation
                                                   / Math::kDegreesToRadians));
    }

    // The data is assumed to be measured at N distinct azimuth "rings", with each ring containing M distinct
    // measurements at different elevations, giving a total of NM measurements. We want to extract the N azimuth
    // values. The interauralAzimuths array contains the azimuths for each measurement, so each azimuth will be
    // repeated M times in that array. So we sort it, then permute it so that all the unique values are at the
    // start of the array, then copy the N unique azimuths into mAzimuths.
    std::sort(std::begin(interauralAzimuths), std::end(interauralAzimuths));
    auto uniqueIterator = std::unique(std::begin(interauralAzimuths), std::end(interauralAzimuths));
    mNumAzimuths = static_cast<int>(std::distance(std::begin(interauralAzimuths), uniqueIterator));
    mAzimuths.resize(mNumAzimuths);
    for (auto i = 0; i < mNumAzimuths; ++i)
    {
        mAzimuths[i] = static_cast<float>(interauralAzimuths[i]);
    }

    // As explained above, for each of the N azimuth rings in which measurements were taken, there are M
    // measurements at different elevations. Although each azimuth ring contains the same _number_ of measurements,
    // these need not be at the same elevation _values_. So we create an array of N arrays, one for each azimuth
    // ring. The ith array contains M elements: the M elevation values used when measuring HRTFs for the ith
    // azimuth value. The code below assumes that measurements occur in the HRTF database in "row-major" order:
    // first you have all the elevation values for the first azimuth, then all the elevation values for the second
    // azimuth, and so on.
    mNumElevations = mNumHRIRs / mNumAzimuths;
    mElevationsForAzimuth.resize(mNumAzimuths, mNumElevations);
    for (auto i = 0, index = 0; i < mNumAzimuths; ++i)
    {
        for (auto j = 0; j < mNumElevations; ++j)
        {
            mElevationsForAzimuth[i][j] = static_cast<float>(interauralElevations[index++]);
        }
    }
}

int HRTFMap::loadNumSamplesForSamplingRate(const byte_t*& readPointer,
                                            int samplingRate)
{
    // Read the number of sampling rates for which HRIR data is present.
    auto numSamplingRates = *reinterpret_cast<const int32_t*>(readPointer);
    readPointer += sizeof(int32_t);

    // Read the values for the sampling rates, and the corresponding HRIR lengths in samples.
    vector<int> samplingRates(numSamplingRates);
    vector<int> numSamplesForSamplingRate(numSamplingRates);
    for (auto i = 0; i < numSamplingRates; ++i)
    {
        samplingRates[i] = reinterpret_cast<const int32_t*>(readPointer)[0];
        numSamplesForSamplingRate[i] = reinterpret_cast<const int32_t*>(readPointer)[1];
        readPointer += 2 * sizeof(int32_t);
    }

    // Check to see whether HRIRs exist for the sampling rate matching that of the audio pipeline.
    auto samplingRateIterator = std::find(samplingRates.data(), samplingRates.data() + numSamplingRates,
                                          samplingRate);
    auto samplingRateIndex = static_cast<int>(std::distance(samplingRates.data(), samplingRateIterator));
    if (samplingRateIndex >= numSamplingRates)
        throw Exception(Status::Initialization);

    // Use the appropriate HRIR length based on the sampling rate.
    mNumSamples = numSamplesForSamplingRate[samplingRateIndex];

    // Skip to the HRIR data for the correct sampling rate.
    auto bytesToSkip = 0;
    for (auto i = 0; i < samplingRateIndex; ++i)
    {
        bytesToSkip += 2 * mNumHRIRs * numSamplesForSamplingRate[i] * sizeof(float);
    }

    readPointer += bytesToSkip;

    // Return the number of bytes to skip after loading HRIRs. After skipping these bytes,
    // the read pointer will be at the start of the SH data, if present.
    auto bytesToSkipAfter = 0;
    for (auto i = samplingRateIndex + 1; i < numSamplingRates; ++i)
    {
        bytesToSkipAfter += 2 * mNumHRIRs * numSamplesForSamplingRate[i] * sizeof(float);
    }

    return bytesToSkipAfter;
}

void HRTFMap::loadHRIRs(const byte_t*& readPointer)
{
    mHRIR.resize(kNumEars, mNumHRIRs, mNumSamples);
    for (auto i = 0; i < kNumEars; ++i)
    {
        for (auto j = 0; j < mNumHRIRs; ++j)
        {
            memcpy(mHRIR[i][j], readPointer, mNumSamples * sizeof(float));
            readPointer += mNumSamples * sizeof(float);
        }
    }
}

void HRTFMap::loadAmbisonicsHRIRs(const byte_t*& readPointer, int samplingRate)
{
    if (mVersion < kMinFileFormatVersionWithSHData)
        return;

    while (true)
    {
        auto order = *reinterpret_cast<const int32_t*>(readPointer);
        readPointer += sizeof(int32_t);
        if (order != kMaxAmbisonicsOrder)
        {
            gLog().message(MessageSeverity::Error, "%s: HRTF data contains Ambisonic HRIRs of unsupported order: %d.", __FUNCTION__, order);
            throw Exception(Status::Initialization);
        }

        auto numCoefficients = SphericalHarmonics::numCoeffsForOrder(order);

        auto dataSamplingRate = *reinterpret_cast<const int32_t*>(readPointer);
        readPointer += sizeof(int32_t);

        auto numSamplesForSamplingRate = *reinterpret_cast<const int32_t*>(readPointer);
        readPointer += sizeof(int32_t);

        if (dataSamplingRate == samplingRate)
        {
            mAmbisonicsHRIR.resize(kNumEars, numCoefficients, mNumSamples);
            for (auto i = 0; i < kNumEars; ++i)
            {
                for (auto j = 0; j < numCoefficients; ++j)
                {
                    memcpy(mAmbisonicsHRIR[i][j], reinterpret_cast<const float*>(readPointer), mNumSamples * sizeof(float));
                    readPointer += mNumSamples * sizeof(float);
                }
            }

            break;
        }
        else
        {
            readPointer += 2 * numCoefficients * numSamplesForSamplingRate * sizeof(float);
        }
    }
}

int HRTFMap::nearestNeighbor(const float* haystack,
                             int size,
                             float needle)
{
    auto bounds = lowerAndUpperBound(haystack, size, needle);
    return (fabsf(needle - haystack[bounds.first]) <= fabsf(needle - haystack[bounds.second])) ? bounds.first :
        bounds.second;
}

std::pair<int, int> HRTFMap::lowerAndUpperBound(const float* haystack,
                                                int size,
                                                float needle)
{
    auto u = std::distance(haystack, std::upper_bound(haystack, haystack + size, needle));
    auto upperBound = std::min(static_cast<int>(u), static_cast<int>(size - 1));
    auto lowerBound = std::max(static_cast<int>(u - 1), 0);
    return std::make_pair(lowerBound, upperBound);
}

void HRTFMap::calculateLinearInterpolationWeights(float x,
                                                  float xMin,
                                                  float xMax,
                                                  int xMinIndex,
                                                  int xMaxIndex,
                                                  float& weightMin,
                                                  float& weightMax)
{
    if (xMinIndex == xMaxIndex && xMin == xMax)
    {
        weightMin = 0.0f;
        weightMax = 1.0f;
        return;
    }

    weightMin = (xMax - x) / (xMax - xMin);
    weightMax = 1.0f - weightMin;
}

}
