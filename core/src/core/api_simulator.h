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

#include "simulation_data.h"
#include "simulation_manager.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_radeonrays_device.h"
#include "api_tan_device.h"
#include "api_scene.h"
#include "api_probes.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CSimulator
// --------------------------------------------------------------------------------------------------------------------

class CSimulator : public ISimulator
{
public:
    Handle<SimulationManager> mHandle;

    CSimulator(CContext* context,
               IPLSimulationSettings* settings);

    virtual ISimulator* retain() override;

    virtual void release() override;

    virtual void setScene(IScene* scene) override;

    virtual void addProbeBatch(IProbeBatch* probeBatch) override;

    virtual void removeProbeBatch(IProbeBatch* probeBatch) override;

    virtual void setSharedInputs(IPLSimulationFlags flags,
                                 IPLSimulationSharedInputs* sharedInputs) override;

    virtual void commit() override;

    virtual void runDirect() override;

    virtual void runReflections() override;

    virtual void runPathing() override;

    virtual IPLerror createSource(IPLSourceSettings* settings,
                                  ISource** source) override;
};


// --------------------------------------------------------------------------------------------------------------------
// CSource
// --------------------------------------------------------------------------------------------------------------------

class CSource : public ISource
{
public:
    Handle<SimulationData> mHandle;

    CSource(CSimulator* simulator,
            IPLSourceSettings* settings);

    virtual ISource* retain() override;

    virtual void release() override;

    virtual void add(ISimulator* simulator) override;

    virtual void remove(ISimulator* simulator) override;

    virtual void setInputs(IPLSimulationFlags flags,
                           IPLSimulationInputs* inputs) override;

    virtual void getOutputs(IPLSimulationFlags flags,
                            IPLSimulationOutputs* outputs) override;
};

}
