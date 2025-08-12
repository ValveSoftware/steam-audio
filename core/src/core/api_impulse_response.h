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

#include "impulse_response.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"

// --------------------------------------------------------------------------------------------------------------------
// CImpulseResponse
// --------------------------------------------------------------------------------------------------------------------

namespace api {

class CImpulseResponse : public IImpulseResponse
{
public:
    Handle<ImpulseResponse> mHandle;

    CImpulseResponse(CContext* context, const IPLImpulseResponseSettings* settings);
    
    virtual IImpulseResponse* retain() override;

    virtual void release() override;

    virtual int getNumChannels() override;

    virtual int getNumSamples() override;

    virtual float* getData() override;

    virtual float* getChannel(int channelIndex) override;

    virtual void reset() override;

    virtual void copy(IImpulseResponse* src) override;

    virtual void swap(IImpulseResponse* a) override;

    virtual void add(IImpulseResponse* in1, IImpulseResponse* in2) override;

    virtual void scale(IImpulseResponse* in, float scalar) override;

    virtual void scaleAccum(IImpulseResponse* in, float scalar) override;
};

}
