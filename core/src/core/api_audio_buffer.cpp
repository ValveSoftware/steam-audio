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

#include "audio_buffer.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::allocateAudioBuffer(IPLint32 numChannels,
                                       IPLint32 numSamples,
                                       IPLAudioBuffer* audioBuffer)
{
    if (numChannels <= 0 || numSamples <= 0 || !audioBuffer)
        return IPL_STATUS_FAILURE;

    try
    {
        audioBuffer->numChannels = numChannels;
        audioBuffer->numSamples = numSamples;

        auto size = (numChannels * numSamples * sizeof(float)) + (numChannels * sizeof(float*));
        auto data = reinterpret_cast<byte_t*>(gMemory().allocate(size, Memory::kDefaultAlignment));

        audioBuffer->data = reinterpret_cast<float**>(data);

        auto samples = reinterpret_cast<float*>(&data[numChannels * sizeof(float*)]);
        for (auto i = 0; i < numChannels; ++i)
        {
            audioBuffer->data[i] = &samples[i * numSamples];
        }
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

void CContext::freeAudioBuffer(IPLAudioBuffer* audioBuffer)
{
    if (!audioBuffer)
        return;

    gMemory().free(reinterpret_cast<byte_t*>(audioBuffer->data));
    audioBuffer->data = nullptr;
}

void CContext::interleaveAudioBuffer(IPLAudioBuffer* src,
                                     IPLfloat32* dst)
{
    if (!dst)
        return;

    AudioBuffer _src(src->numChannels, src->numSamples, src->data);

    _src.read(dst);
}

void CContext::deinterleaveAudioBuffer(IPLfloat32* src,
                                       IPLAudioBuffer* dst)
{
    if (!src)
        return;

    AudioBuffer _dst(dst->numChannels, dst->numSamples, dst->data);

    _dst.write(src);
}

void CContext::mixAudioBuffer(IPLAudioBuffer* in,
                              IPLAudioBuffer* mix)
{
    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _mix(mix->numChannels, mix->numSamples, mix->data);

    AudioBuffer::mix(_in, _mix);
}

void CContext::downmixAudioBuffer(IPLAudioBuffer* in,
                                  IPLAudioBuffer* out)
{
    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    AudioBuffer::downmix(_in, _out);
}

void CContext::convertAmbisonicAudioBuffer(IPLAmbisonicsType inType,
                                           IPLAmbisonicsType outType,
                                           IPLAudioBuffer* in,
                                           IPLAudioBuffer* out)
{
    auto _inType = static_cast<AmbisonicsType>(inType);
    auto _outType = static_cast<AmbisonicsType>(outType);

    AudioBuffer _in(in->numChannels, in->numSamples, in->data);
    AudioBuffer _out(out->numChannels, out->numSamples, out->data);

    AudioBuffer::convertAmbisonics(_inType, _outType, _in, _out);
}

}
