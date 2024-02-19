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

#include <mysofa.h>

#include "hrtf_map.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// SOFAHRTFMap
// --------------------------------------------------------------------------------------------------------------------

// An IHRTFMap that loads and queries HRTF data stored in a SOFA file.
class SOFAHRTFMap : public IHRTFMap
{
public:
    // If unsupported SOFA features are used, an error is reported. HRIR data is automatically resampled to the given
    // sampling rate.
    SOFAHRTFMap(const HRTFSettings& hrtfSettings,
                int samplingRate);

    virtual ~SOFAHRTFMap() override;

    virtual int numHRIRs() const override
    {
        return mSOFA->hrtf->M;
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
    int mSamplingRate; // Sampling rate. HRIRs are automatically resampled to this rate.
    int mNumSamples; // Number of samples in an HRIR.
    MYSOFA_EASY* mSOFA; // Handle to libmysofa API object.
    Array<float, 3> mHRIR; // HRIRs. #ears * #measurements * #samples.
    Array<float, 3> mAmbisonicsHRIR; // Ambisonics HRIRs. Always empty, since this is not stored in SOFA files.

    // Returns the coordinates of the measurement with the given index. Coordinates are in the SOFA coordinate
    // system.
    Vector3f measurementPosition(int index) const;

    float distanceToMeasurement(const Vector3f& point,
                                int index) const;

    // Returns #ears pointers to HRIR data for the measurement with the given index.
    void getHRIRsForIndex(int index,
                          const float** hrirs);

    void patchSOFANeighborhood();

    // Converts from Steam Audio coordinates to SOFA coordinates.
    static Vector3f toSOFACoordinates(const Vector3f& v);
};

}
