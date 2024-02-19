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

#include "context.h"
#include "error.h"
#include "memory_allocator.h"
#include "overlap_save_convolution_effect.h"
using namespace ipl;

#include "phonon.h"

template <typename T>
class Handle
{
private:
    shared_ptr<T> mPointer;
    std::atomic<int> mRefCount;
    shared_ptr<Context> mContext; // Okay to set to nullptr if this is the handle to the Context itself.

public:
    Handle()
        : mPointer(nullptr)
        , mRefCount(0)
    {}

    Handle(shared_ptr<T> pointer,
           shared_ptr<Context> context)
        : mPointer(pointer)
        , mRefCount(1)
        , mContext(context)
    {}

    Handle(const Handle<T>&) = delete;
    Handle(Handle<T>&&) = delete;

    ~Handle() = default;

    Handle<T>& operator=(const Handle<T>&) = delete;
    Handle<T>& operator=(Handle<T>&&) = delete;

    shared_ptr<T>& get()
    {
        return mPointer;
    }

    void retain()
    {
        ++mRefCount;
    }

    bool release()
    {
        if (--mRefCount == 0)
        {
            mPointer.reset();
            return true;
        }
        else
        {
            return false;
        }
    }

    shared_ptr<Context> context()
    {
        return mContext;
    }
};

template <typename T>
struct HandleTraits
{
    typedef void opaque_type;
};

#define DEFINE_OPAQUE_HANDLE(x, y)  namespace ipl { class y; } template <> struct HandleTraits<ipl::y> { typedef x opaque_type; }

DEFINE_OPAQUE_HANDLE(IPLContext, Context);
DEFINE_OPAQUE_HANDLE(IPLSerializedObject, SerializedObject);
DEFINE_OPAQUE_HANDLE(IPLEmbreeDevice, EmbreeDevice);
DEFINE_OPAQUE_HANDLE(IPLOpenCLDeviceList, OpenCLDeviceList);
DEFINE_OPAQUE_HANDLE(IPLOpenCLDevice, OpenCLDevice);
DEFINE_OPAQUE_HANDLE(IPLRadeonRaysDevice, RadeonRaysDevice);
DEFINE_OPAQUE_HANDLE(IPLTrueAudioNextDevice, TANDevice);
DEFINE_OPAQUE_HANDLE(IPLScene, IScene);
DEFINE_OPAQUE_HANDLE(IPLStaticMesh, IStaticMesh);
DEFINE_OPAQUE_HANDLE(IPLInstancedMesh, IInstancedMesh);
DEFINE_OPAQUE_HANDLE(IPLHRTF, HRTFDatabase);
DEFINE_OPAQUE_HANDLE(IPLPanningEffect, PanningEffect);
DEFINE_OPAQUE_HANDLE(IPLBinauralEffect, BinauralEffect);
DEFINE_OPAQUE_HANDLE(IPLVirtualSurroundEffect, VirtualSurroundEffect);
DEFINE_OPAQUE_HANDLE(IPLAmbisonicsEncodeEffect, AmbisonicsEncodeEffect);
DEFINE_OPAQUE_HANDLE(IPLAmbisonicsPanningEffect, AmbisonicsPanningEffect);
DEFINE_OPAQUE_HANDLE(IPLAmbisonicsBinauralEffect, AmbisonicsBinauralEffect);
DEFINE_OPAQUE_HANDLE(IPLAmbisonicsRotationEffect, AmbisonicsRotateEffect);
DEFINE_OPAQUE_HANDLE(IPLAmbisonicsDecodeEffect, AmbisonicsDecodeEffect);
DEFINE_OPAQUE_HANDLE(IPLDirectEffect, DirectEffect);
DEFINE_OPAQUE_HANDLE(IPLReflectionEffect, IndirectEffect);
DEFINE_OPAQUE_HANDLE(IPLReflectionMixer, IndirectMixer);
DEFINE_OPAQUE_HANDLE(IPLPathEffect, PathEffect);
DEFINE_OPAQUE_HANDLE(IPLProbeArray, ProbeArray);
DEFINE_OPAQUE_HANDLE(IPLProbeBatch, ProbeBatch);
DEFINE_OPAQUE_HANDLE(IPLSource, SimulationData);
DEFINE_OPAQUE_HANDLE(IPLSimulator, SimulationManager);

template <> struct HandleTraits<ipl::TripleBuffer<ipl::OverlapSaveFIR>> { typedef IPLReflectionEffectIR opaque_type; };

template <typename T>
typename HandleTraits<T>::opaque_type createHandle(shared_ptr<Context> context,
                                                   shared_ptr<T> sharedPointer)
{
    Handle<T>* handle = reinterpret_cast<Handle<T>*>(gMemory().allocate(sizeof(Handle<T>), Memory::kDefaultAlignment));
    new (handle) Handle<T>(shared_ptr<T>(sharedPointer), context);
    return reinterpret_cast<typename HandleTraits<T>::opaque_type>(handle);
}

template <typename T>
typename HandleTraits<T>::opaque_type retainHandle(typename HandleTraits<T>::opaque_type handle)
{
    if (!handle)
        return nullptr;

    reinterpret_cast<Handle<T>*&>(handle)->retain();
    return handle;
}

template <typename T>
void releaseHandle(typename HandleTraits<T>::opaque_type& handle)
{
    if (!handle)
        return;

    if (reinterpret_cast<Handle<T>*&>(handle)->release())
    {
        reinterpret_cast<Handle<T>*&>(handle)->~Handle();
        gMemory().free(handle);
    }

    handle = nullptr;
}

template <typename T>
shared_ptr<T> derefHandle(typename HandleTraits<T>::opaque_type handle)
{
    if (!handle)
        return nullptr;

    return reinterpret_cast<Handle<T>*&>(handle)->get();
}

template <typename T>
shared_ptr<Context> contextFromHandle(typename HandleTraits<T>::opaque_type handle)
{
    if (!handle)
        return nullptr;

    return reinterpret_cast<Handle<T>*&>(handle)->context();
}
