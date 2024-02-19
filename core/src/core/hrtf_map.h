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

#pragma once

#include "array.h"
#include "vector.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IHRTFMap
// --------------------------------------------------------------------------------------------------------------------

enum class HRTFMapType
{
    Default,
    SOFA
};

enum class HRTFNormType
{
    None,
    RMS
};

struct HRTFSettings
{
    HRTFMapType         type            = HRTFMapType::Default;
    const uint8_t*      hrtfData        = nullptr;
    const char*         sofaFileName    = nullptr;
    const uint8_t*      sofaData        = nullptr;
    int                 sofaDataSize    = 0;
    float               volume          = 0.0f;     // Volume in dB.
    HRTFNormType        normType        = HRTFNormType::None;
};

// A data structure that stores loaded HRTF data and allows nearest-neighbor and interpolated queries. This is a base
// class; use one of its derived classes.
class IHRTFMap
{
public:
    static const int kNumEars = 2;
    static const int kMaxAmbisonicsOrder = 3;   // Limited to 3 because we use 24 virtual speakers for decoding.

public:
    virtual ~IHRTFMap()
    {}

    // 1 per measurement position. Independent of #ears.
    virtual int numHRIRs() const = 0;

    // Time-domain HRIR length. Does not include zero-padding, frame size, etc.
    virtual int numSamples() const = 0;

    // Returns HRIRs. #ears * #measurements * #samples.
    virtual Array<float, 3>& hrtfData() = 0;
    virtual const Array<float, 3>& hrtfData() const = 0;

    // Returns Ambisonics HRIRs. #ears * #coefficients * #samples. May be empty, in which case Ambisonics HRIRs/HRTFs
    // should be precomputed at load-time.
    virtual Array<float, 3>& ambisonicsData() = 0;
    virtual const Array<float, 3>& ambisonicsData() const = 0;

    // Returns the measurement index of the nearest HRIR measurement to the given direction.
    virtual int nearestHRIR(const Vector3f& direction) const = 0;

    // Returns measurement indices and corresponding weights of the HRIR measurements to interpolate for a given
    // direction. Up to 4 indices/weights may be returned.
    virtual void interpolatedHRIRWeights(const Vector3f& direction,
                                         int indices[8],
                                         float weights[8]) const = 0;
};


// --------------------------------------------------------------------------------------------------------------------
// HRTFMap
// --------------------------------------------------------------------------------------------------------------------

// An IHRTFMap that loads and queries the built-in HRTF data.
class HRTFMap : public IHRTFMap
{
public:
    // HRTF data is stored in a global variable, linked in separately. If HRIR data for the given sampling rate is not
    // found, an error is reported.
    HRTFMap(int samplingRate, const uint8_t* hrtfData = nullptr);

    virtual int numHRIRs() const override
    {
        return mNumHRIRs;
    }

    virtual int numSamples() const override
    {
        return mNumSamples;
    }

    virtual Array<float, 3>& hrtfData() override
    {
        return mHRIR;
    }

    virtual const Array<float, 3>& hrtfData() const override
    {
        return mHRIR;
    }

    virtual Array<float, 3>& ambisonicsData() override
    {
        return mAmbisonicsHRIR;
    }

    virtual const Array<float, 3>& ambisonicsData() const override
    {
        return mAmbisonicsHRIR;
    }

    virtual int nearestHRIR(const Vector3f& direction) const override;

    virtual void interpolatedHRIRWeights(const Vector3f& direction,
                                         int indices[8],
                                         float weights[8]) const override;

private:
    static const int kMinSupportedFileFormatVersion = 2;
    static const int kMaxSupportedFileFormatVersion = 3;
    static const int kMinFileFormatVersionWithSHData = 3;

    int mVersion;
    int mNumHRIRs; // Number of measurement positions.
    int mNumAzimuths; // Number of unique azimuths.
    int mNumElevations; // Number of elevations at any given azimuth.
    int mNumSamples; // Number of samples in an HRIR.
    Array<float> mAzimuths; // Azimuth values. #azimuths.
    Array<float, 2> mElevationsForAzimuth; // Elevation values, for each azimuth. #azimuths * #elevations.
    Array<float, 3> mHRIR; // HRIRs. #ears * #measurements * #samples.
    Array<float, 3> mAmbisonicsHRIR; // Ambisonics HRIRs. #ears * #coefficients * #samples.

    // Loads header information from the data, and verifies it.
    void verifyDataHeader(const byte_t*& readPointer);

    // Loads the number of HRIRs from the data.
    void loadNumHRIRs(const byte_t*& readPointer);

    // Loads directions (azimuth and elevation information) from the data.
    void loadDirections(const byte_t*& readPointer);

    // Loads the number of samples for the given sampling rate from the data, then skips ahead to the HRIR data for
    // the given sampling rate.
    int loadNumSamplesForSamplingRate(const byte_t*& readPointer,
                                       int samplingRate);

    // Loads HRIRs for the given sampling rate.
    void loadHRIRs(const byte_t*& readPointer);

    // Loads Ambisonics HRIRs for the given sampling rate. If compiled with IPL_GENERATE_AMBISONICS_HRIRS, this is
    // data is *not* loaded.
    void loadAmbisonicsHRIRs(const byte_t*& readPointer, int samplingRate);

    // Returns the index of the value in haystack that is closest to needle.
    static int nearestNeighbor(const float* haystack,
                               int size,
                               float needle);

    // Returns the indices of the two values in haystack between which needle lies.
    static std::pair<int, int> lowerAndUpperBound(const float* haystack,
                                                  int size,
                                                  float needle);

    // For xMin <= x <= xMax, returns linear interpolation weights weightMin and weightMax such that
    // x = weightMin * xMin + weightMax * xMax.
    static void calculateLinearInterpolationWeights(float x,
                                                    float xMin,
                                                    float xMax,
                                                    int xMinIndex,
                                                    int xMaxIndex,
                                                    float& weightMin,
                                                    float& weightMax);
};

}
