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

#include "api_energy_field.h"

// --------------------------------------------------------------------------------------------------------------------
// CEnergyField
// --------------------------------------------------------------------------------------------------------------------

namespace api {

CEnergyField::CEnergyField(CContext* context, const IPLEnergyFieldSettings* settings)
{
    if (!context || !settings)
        throw Exception(Status::Failure);

    auto _context = context->mHandle.get();
    auto _duration = settings->duration;
    auto _order = settings->order;

    if (!_context)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<EnergyField>(ipl::make_shared<EnergyField>(_duration, _order), _context);
}

CEnergyField* CEnergyField::retain()
{
    mHandle.retain();
    return this;
}

void CEnergyField::release()
{
    if (mHandle.release())
    {
        this->~CEnergyField();
        gMemory().free(this);
    }
}

int CEnergyField::getNumChannels()
{
    auto _energyField = mHandle.get();

    if (!_energyField)
        return 0;

    return _energyField->numChannels();
}

int CEnergyField::getNumBins()
{
    auto _energyField = mHandle.get();

    if (!_energyField)
        return 0;

    return _energyField->numBins();
}

float* CEnergyField::getData()
{
    auto _energyField = mHandle.get();

    if (!_energyField)
        return nullptr;

    return _energyField->flatData();
}

float* CEnergyField::getChannel(int channelIndex)
{
    auto _energyField = mHandle.get();

    if (!_energyField)
        return nullptr;

    return (*mHandle.get())[channelIndex][0];
}

float* CEnergyField::getBand(int channelIndex, int bandIndex)
{
    auto _energyField = mHandle.get();

    if (!_energyField)
        return nullptr;

    return (*mHandle.get())[channelIndex][bandIndex];
}

void CEnergyField::reset()
{
    auto _energyField = mHandle.get();

    if (!_energyField)
        return;

    _energyField->reset();
}

void CEnergyField::copy(IEnergyField* src)
{
    auto _src = static_cast<CEnergyField*>(src)->mHandle.get();
    auto _dst = mHandle.get();

    if (!_src || !_dst)
        return;

    _dst->copyFrom(*_src);
}

void CEnergyField::swap(IEnergyField* other)
{
    auto _a = static_cast<CEnergyField*>(other)->mHandle.get();
    auto _b = mHandle.get();

    if (!_a || !_b)
        return;

    EnergyField::swap(*_a, *_b);
}

void CEnergyField::add(IEnergyField* in1, IEnergyField* in2)
{
    auto _in1 = static_cast<CEnergyField*>(in1)->mHandle.get();
    auto _in2 = static_cast<CEnergyField*>(in2)->mHandle.get();
    auto _out = mHandle.get();

    if (!_in1 || !_in2 || !_out)
        return;

    EnergyField::add(*_in1, *_in2, *_out);
}

void CEnergyField::scale(IEnergyField* in, float scalar)
{
    auto _in = static_cast<CEnergyField*>(in)->mHandle.get();
    auto _out = mHandle.get();

    if (!_in || !_out)
        return;

    EnergyField::scale(*_in, scalar, *_out);
}

void CEnergyField::scaleAccum(IEnergyField* in, float scalar)
{
    auto _in = static_cast<CEnergyField*>(in)->mHandle.get();
    auto _out = mHandle.get();

    if (!_in || !_out)
        return;

    EnergyField::scaleAccumulate(*_in, scalar, *_out);
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createEnergyField(const IPLEnergyFieldSettings* settings,
                                     IEnergyField** energyField)
{
    if (!settings || !energyField)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _energyField = reinterpret_cast<CEnergyField*>(gMemory().allocate(sizeof(CEnergyField), Memory::kDefaultAlignment));
        new (_energyField) CEnergyField(this, settings);
        *energyField = _energyField;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
