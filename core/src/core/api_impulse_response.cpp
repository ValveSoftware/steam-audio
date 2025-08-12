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

#include "api_impulse_response.h"

// --------------------------------------------------------------------------------------------------------------------
// CImpulseResponse
// --------------------------------------------------------------------------------------------------------------------

namespace api {

CImpulseResponse::CImpulseResponse(CContext* context, const IPLImpulseResponseSettings* settings)
{
    if (!context || !settings)
        throw Exception(Status::Failure);

    auto _context = context->mHandle.get();
    auto _duration = settings->duration;
    auto _order = settings->order;
    auto _samplingRate = settings->samplingRate;

    if (!_context)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<ImpulseResponse>(ipl::make_shared<ImpulseResponse>(_duration, _order, _samplingRate), _context);
}

IImpulseResponse* CImpulseResponse::retain()
{
    mHandle.retain();
    return this;
}

void CImpulseResponse::release()
{
    if (mHandle.release())
    {
        this->~CImpulseResponse();
        gMemory().free(this);
    }
}

int CImpulseResponse::getNumChannels()
{
    auto _impulseResponse = mHandle.get();

    if (!_impulseResponse)
        return 0;

    return _impulseResponse->numChannels();
}

int CImpulseResponse::getNumSamples()
{
    auto _impulseResponse = mHandle.get();

    if (!_impulseResponse)
        return 0;

    return _impulseResponse->numSamples();
}

float* CImpulseResponse::getData()
{
    auto _impulseResponse = mHandle.get();

    if (!_impulseResponse)
        return nullptr;

    return (*_impulseResponse)[0];
}

float* CImpulseResponse::getChannel(int channelIndex)
{
    auto _impulseResponse = mHandle.get();

    if (!_impulseResponse)
        return nullptr;

    return (*_impulseResponse)[channelIndex];
}

void CImpulseResponse::reset()
{
    auto _impulseResponse = mHandle.get();

    if (!_impulseResponse)
        return;

    _impulseResponse->reset();
}

void CImpulseResponse::copy(IImpulseResponse* src)
{
    auto _src = static_cast<CImpulseResponse*>(src)->mHandle.get();
    auto _dst = mHandle.get();

    if (!_src || !_dst)
        return;

    ImpulseResponse::copy((*_src), (*_dst));
}

void CImpulseResponse::swap(IImpulseResponse* a)
{
    auto _a = static_cast<CImpulseResponse*>(a)->mHandle.get();
    auto _b = mHandle.get();

    if (!_a || !_b)
        return;
    ImpulseResponse::swap((*_a), (*_b));
}

void CImpulseResponse::add(IImpulseResponse* in1, IImpulseResponse* in2)
{
    auto _in1 = static_cast<CImpulseResponse*>(in1)->mHandle.get();
    auto _in2 = static_cast<CImpulseResponse*>(in2)->mHandle.get();
    auto _out = mHandle.get();

    if (!_in1 || !_in2 || !_out)
        return;

    ImpulseResponse::add((*_in1), (*_in2), (*_out));
}

void CImpulseResponse::scale(IImpulseResponse* in, float scalar)
{
    auto _in = static_cast<CImpulseResponse*>(in)->mHandle.get();
    auto _out = mHandle.get();

    if (!_in || !_out)
        return;

    ImpulseResponse::scale((*_in), scalar, (*_out));
}

void CImpulseResponse::scaleAccum(IImpulseResponse* in, float scalar)
{
    auto _in = static_cast<CImpulseResponse*>(in)->mHandle.get();
    auto _out = mHandle.get();

    if (!_in || !_out)
        return;

    ImpulseResponse::scaleAccumulate((*_in), scalar, (*_out));
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createImpulseResponse(const IPLImpulseResponseSettings* settings,
                                         IImpulseResponse** impulseResponse)
{
    if (!settings || !impulseResponse)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _impulseResponse = reinterpret_cast<CImpulseResponse*>(gMemory().allocate(sizeof(CImpulseResponse), Memory::kDefaultAlignment));
        new (_impulseResponse) CImpulseResponse(this, settings);
        *impulseResponse = _impulseResponse;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}