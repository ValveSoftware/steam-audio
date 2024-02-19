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

#if defined(IPL_USES_RADEONRAYS)

#include "opencl_energy_field.h"
#include "opencl_impulse_response.h"
#include "opencl_kernel.h"
#include "radeonrays_device.h"
#include "reconstructor.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OpenCLReconstructor
// --------------------------------------------------------------------------------------------------------------------

class OpenCLReconstructor : public IReconstructor
{
public:
    OpenCLReconstructor(shared_ptr<RadeonRaysDevice> radeonRays,
                        float maxDuration,
                        int maxOrder,
                        int samplingRate);

    virtual void reconstruct(int numIRs,
                             const EnergyField* const* energyFields,
                             const float* const* distanceAttenuationCorrectionCurves,
                             const AirAbsorptionModel* airAbsorptionModels,
                             ImpulseResponse* const* impulseResponses,
                             ReconstructionType type,
                             float duration,
                             int order) override;

private:
    static const int kBatchSize = 8;

    shared_ptr<RadeonRaysDevice> mRadeonRays;

    int mNumChannels;
    int mNumSamples;
    int mSamplingRate;

    OpenCLBuffer mAirAbsorption;
    OpenCLBuffer mBandFilters;
    OpenCLBuffer mWhiteNoise;
    OpenCLBuffer mBatchedBandIRs;
    OpenCLBuffer mBatchedIR;

    OpenCLKernel mReconstruct;
    OpenCLKernel mApplyIIR;
    OpenCLKernel mCombine;

    void reconstruct(const OpenCLEnergyField& energyField,
                     int index);

    void applyIIR(int numBins,
                  int batchSize);

    void combine(int numBins,
                 int batchSize);
};

}

#endif
