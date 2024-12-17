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
#include "api_probes.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CProbeArray
// --------------------------------------------------------------------------------------------------------------------

CProbeArray::CProbeArray(CContext* context)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<ProbeArray>(ipl::make_shared<ProbeArray>(), _context);
}

IProbeArray* CProbeArray::retain()
{
    mHandle.retain();
    return this;
}

void CProbeArray::release()
{
    if (mHandle.release())
    {
        this->~CProbeArray();
        gMemory().free(this);
    }
}

void CProbeArray::generateProbes(IScene* scene,
                                 IPLProbeGenerationParams* params)
{
    if (!scene || !params)
        return;

    const auto& _transform = *reinterpret_cast<Matrix4x4f*>(&params->transform);
    auto _type = static_cast<ProbeGenerationType>(params->type);

    auto _scene = reinterpret_cast<CScene*>(scene)->mHandle.get();
    auto _probeArray = mHandle.get();
    if (!_scene || !_probeArray)
        return;

    ProbeGenerator::generateProbes(*_scene, _transform, _type, params->spacing, params->height, *_probeArray);
}

IPLint32 CProbeArray::getNumProbes()
{
    auto _probeArray = mHandle.get();
    if (!_probeArray)
        return 0;

    return static_cast<int>(_probeArray->probes.size(0));
}

IPLSphere CProbeArray::getProbe(IPLint32 index)
{
    IPLSphere sphere{};

    if (index < 0)
        return sphere;

    auto _probeArray = mHandle.get();
    if (!_probeArray)
        return sphere;

    if (index < 0 || _probeArray->numProbes() <= index)
        return sphere;

    *reinterpret_cast<Sphere*>(&sphere) = _probeArray->probes[index].influence;

    return sphere;
}

void CProbeArray::resize(IPLint32 size)
{
    if (size <= 0)
        return;

    auto _probeArray = mHandle.get();
    if (!_probeArray)
        return;

    _probeArray->probes.resize(size);
}

void CProbeArray::setProbe(IPLint32 index, IPLSphere* probe)
{
    if (index < 0 || !probe)
        return;

    auto _probeArray = mHandle.get();
    if (!_probeArray)
        return;

    const auto& _probe = *reinterpret_cast<Sphere*>(probe);

    _probeArray->probes[index].influence = _probe;
}


// --------------------------------------------------------------------------------------------------------------------
// CProbeBatch
// --------------------------------------------------------------------------------------------------------------------

CProbeBatch::CProbeBatch(CContext* context)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<ProbeBatch>(ipl::make_shared<ProbeBatch>(), _context);
}

CProbeBatch::CProbeBatch(CContext* context,
                         ISerializedObject* serializedObject)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    auto _serializedObject = static_cast<CSerializedObject*>(serializedObject)->mHandle.get();
    if (!_serializedObject)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<ProbeBatch>(ipl::make_shared<ProbeBatch>(*_serializedObject), _context);
}

IProbeBatch* CProbeBatch::retain()
{
    mHandle.retain();
    return this;
}

void CProbeBatch::release()
{
    if (mHandle.release())
    {
        this->~CProbeBatch();
        gMemory().free(this);
    }
}

void CProbeBatch::save(ISerializedObject* serializedObject)
{
    if (!serializedObject)
        return;

    auto _probeBatch = mHandle.get();
    auto _serializedObject = reinterpret_cast<CSerializedObject*>(serializedObject)->mHandle.get();
    if (!_probeBatch || !_serializedObject)
        return;

    _probeBatch->serializeAsRoot(*_serializedObject);
}

IPLint32 CProbeBatch::getNumProbes()
{
    auto _probeBatch = mHandle.get();
    if (!_probeBatch)
        return 0;

    return _probeBatch->numProbes();
}

void CProbeBatch::addProbe(IPLSphere probe)
{
    auto _probeBatch = mHandle.get();
    if (!_probeBatch)
        return;

    const auto& _probe = *reinterpret_cast<Sphere*>(&probe);

    _probeBatch->addProbe(_probe);
}

void CProbeBatch::addProbeArray(IProbeArray* probeArray)
{
    if (!probeArray)
        return;

    auto _probeBatch = mHandle.get();
    auto _probeArray = reinterpret_cast<CProbeArray*>(probeArray)->mHandle.get();
    if (!_probeBatch || !_probeArray)
        return;

    _probeBatch->addProbeArray(*_probeArray);
}

void CProbeBatch::removeProbe(IPLint32 index)
{
    if (index < 0)
        return;

    auto _probeBatch = mHandle.get();
    if (!_probeBatch)
        return;

    if (index < 0 || _probeBatch->numProbes() <= index)
        return;

    _probeBatch->removeProbe(index);
}

void CProbeBatch::commit()
{
    auto _probeBatch = mHandle.get();
    if (!_probeBatch)
        return;

    _probeBatch->commit();
}

void CProbeBatch::removeData(IPLBakedDataIdentifier* identifier)
{
    auto _probeBatch = mHandle.get();
    if (!_probeBatch)
        return;

    const auto& _identifier = *reinterpret_cast<BakedDataIdentifier*>(identifier);

    _probeBatch->removeData(_identifier);
}

IPLsize CProbeBatch::getDataSize(IPLBakedDataIdentifier* identifier)
{
    auto _probeBatch = mHandle.get();
    if (!_probeBatch)
        return 0;

    const auto& _identifier = *reinterpret_cast<BakedDataIdentifier*>(identifier);

    if (_probeBatch->hasData(_identifier))
        return static_cast<IPLsize>((*_probeBatch)[_identifier].serializedSize());
    else
        return 0;
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createProbeArray(IProbeArray** probeArray)
{
    if (!probeArray)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _probeArray = reinterpret_cast<CProbeArray*>(gMemory().allocate(sizeof(CProbeArray), Memory::kDefaultAlignment));
        new (_probeArray) CProbeArray(this);
        *probeArray = _probeArray;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLerror CContext::createProbeBatch(IProbeBatch** probeBatch)
{
    if (!probeBatch)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _probeBatch = reinterpret_cast<CProbeBatch*>(gMemory().allocate(sizeof(CProbeBatch), Memory::kDefaultAlignment));
        new (_probeBatch) CProbeBatch(this);
        *probeBatch = _probeBatch;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLerror CContext::loadProbeBatch(ISerializedObject* serializedObject,
                                  IProbeBatch** probeBatch)
{
    if (!serializedObject || !probeBatch)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _probeBatch = reinterpret_cast<CProbeBatch*>(gMemory().allocate(sizeof(CProbeBatch), Memory::kDefaultAlignment));
        new (_probeBatch) CProbeBatch(this, serializedObject);
        *probeBatch = _probeBatch;
    }
    catch (Exception e)
    {
        return static_cast<IPLerror>(e.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
