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

#if defined(IPL_USES_TRUEAUDIONEXT)

#include <TrueAudioNext.h>

#include "audio_buffer.h"
#include "array.h"
#include "containers.h"
#include "opencl_device.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// TANDevice
// --------------------------------------------------------------------------------------------------------------------

class TANDevice
{
public:
    TANDevice(cl_command_queue convolutionQueue,
        cl_command_queue updateQueue,
        int frameSize,
        int irSize,
        int order,
        int numSources);

    int acquireSlot();

    void releaseSlot(int slot);

    void reset(int slot);

    void setDry(int slot,
                const AudioBuffer& in);

    void process(AudioBuffer& out);

    void setIR(int slot,
               const cl_mem* irChannels);

    void updateIRs();

private:
    int mFrameSize;
    int mIRSize;
    int mNumChannels;
    int mNumSources;
    priority_queue<int> mSlots;
    amf::TANContextPtr mTANContext;
    amf::TANConvolutionPtr mTANConvolution;
    Array<float, 2> mDry;
    Array<float*, 2> mDryPtrs;
    Array<float, 3> mWet;
    Array<float*, 2> mWetPtrs;
    Array<cl_mem, 2> mIR;
    Array<amf_uint32, 2> mUpdateFlags;
    Array<amf_uint32, 2> mProcessFlags;
    std::atomic<bool> mIRsUpdated;
};

}

#else

namespace ipl {

class TANDevice
{};

}

#endif