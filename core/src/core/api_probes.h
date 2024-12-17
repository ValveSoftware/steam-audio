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

#include "probe_generator.h"
#include "probe_batch.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"
#include "api_serialized_object.h"
#include "api_scene.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CProbeArray
// --------------------------------------------------------------------------------------------------------------------

class CProbeArray : public IProbeArray
{
public:
    Handle<ProbeArray> mHandle;

    CProbeArray(CContext* context);

    virtual IProbeArray* retain() override;

    virtual void release() override;

    virtual void generateProbes(IScene* scene,
                                IPLProbeGenerationParams* params) override;

    virtual IPLint32 getNumProbes() override;

    virtual IPLSphere getProbe(IPLint32 index) override;

    void resize(IPLint32 size);

    void setProbe(IPLint32 index, 
                  IPLSphere* probe);
};


// --------------------------------------------------------------------------------------------------------------------
// CProbeBatch
// --------------------------------------------------------------------------------------------------------------------

class CProbeBatch : public IProbeBatch
{
public:
    Handle<ProbeBatch> mHandle;

    CProbeBatch() = default;

    CProbeBatch(CContext* context);

    CProbeBatch(CContext* context,
                ISerializedObject* serializedObject);

    virtual IProbeBatch* retain() override;

    virtual void release() override;

    virtual void save(ISerializedObject* serializedObject) override;

    virtual IPLint32 getNumProbes() override;

    virtual void addProbe(IPLSphere probe) override;

    virtual void addProbeArray(IProbeArray* probeArray) override;

    virtual void removeProbe(IPLint32 index) override;

    virtual void commit() override;

    virtual void removeData(IPLBakedDataIdentifier* identifier) override;

    virtual IPLsize getDataSize(IPLBakedDataIdentifier* identifier) override;
};

}
