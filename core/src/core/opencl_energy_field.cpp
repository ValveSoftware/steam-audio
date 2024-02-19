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

#if defined(IPL_USES_RADEONRAYS)

#include "opencl_energy_field.h"

#include "bands.h"
#include "radeonrays_reflection_simulator.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OpenCLEnergyField
// --------------------------------------------------------------------------------------------------------------------

const int OpenCLEnergyField::kMaxBins = 256;

OpenCLEnergyField::OpenCLEnergyField(shared_ptr<OpenCLDevice> openCL, float duration, int order)
    : EnergyField(duration, order)
    , mOpenCL(openCL)
    , mBuffer(*openCL, numChannels() * Bands::kNumBands * kMaxBins * sizeof(cl_float))
{}

void OpenCLEnergyField::reset()
{
    EnergyField::reset();

    auto zero = 0.0f;
    clEnqueueFillBuffer(mOpenCL->irUpdateQueue(), mBuffer.buffer(), &zero, sizeof(cl_float), 0, mBuffer.size(),
                        0, nullptr, nullptr);
}

void OpenCLEnergyField::copyDeviceToHost()
{
    auto _buffer = reinterpret_cast<int*>(clEnqueueMapBuffer(mOpenCL->irUpdateQueue(), mBuffer.buffer(), CL_TRUE,
                                          CL_MAP_READ, 0, mBuffer.size(), 0, nullptr, nullptr, nullptr));

    for (auto i = 0, index = 0; i < numChannels(); ++i)
    {
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            for (auto k = 0; k < std::min(numBins(), kMaxBins); ++k, ++index)
            {
                mData[i][j][k] = _buffer[index] / RadeonRaysReflectionSimulator::kHistogramScale;
            }

            index += std::max(0, kMaxBins - numBins());
        }
    }

    clEnqueueUnmapMemObject(mOpenCL->irUpdateQueue(), mBuffer.buffer(), _buffer, 0, nullptr, nullptr);
}

void OpenCLEnergyField::copyHostToDevice()
{
    auto _buffer = reinterpret_cast<int*>(clEnqueueMapBuffer(mOpenCL->irUpdateQueue(), mBuffer.buffer(), CL_TRUE,
                                          CL_MAP_WRITE_INVALIDATE_REGION, 0, mBuffer.size(), 0, nullptr, nullptr,
                                          nullptr));

    for (auto i = 0, index = 0; i < numChannels(); ++i)
    {
        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            for (auto k = 0; k < std::min(numBins(), kMaxBins); ++k, ++index)
            {
                _buffer[index] = convertIntSat(floorf(RadeonRaysReflectionSimulator::kHistogramScale * mData[i][j][k]));
            }

            index += std::max(0, kMaxBins - numBins());
        }
    }

    clEnqueueUnmapMemObject(mOpenCL->irUpdateQueue(), mBuffer.buffer(), _buffer, 0, nullptr, nullptr);
}

int OpenCLEnergyField::convertIntSat(float x)
{
    auto out = 0;

    if (x < INT32_MIN)
    {
        out = INT32_MIN;
    }
    else if (x > INT32_MAX)
    {
        out = UINT32_MAX;
    }
    else
    {
        out = static_cast<int32_t>(x);
    }

    return out;
}

}

#endif
