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

#if defined(IPL_OS_WINDOWS)
#include <codecvt>
#endif

#include "context.h"
#include "custom_scene.h"
#include "radeonrays_scene.h"
#include "scene_factory.h"
using namespace ipl;

#include "phonon.h"
#include "util.h"

#define STEAMAUDIO_SKIP_API_FUNCTIONS
#include "phonon_interfaces.h"
#include "api_context.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CScene
// --------------------------------------------------------------------------------------------------------------------

class CStaticMesh;
class CInstancedMesh;

class CScene : public IScene
{
public:
    Handle<ipl::IScene> mHandle;

    CScene(CContext* context,
           IPLSceneSettings* settings);

    CScene(CContext* context,
           IPLSceneSettings* settings,
           ISerializedObject* serializedObject);

    virtual IScene* retain() override;

    virtual void release() override;

    virtual void save(ISerializedObject* serializedObject) override;

    virtual void saveOBJ(IPLstring fileBaseName) override;

    virtual void commit() override;

    virtual IPLerror createStaticMesh(IPLStaticMeshSettings* settings,
                                      IStaticMesh** staticMesh) override;

    virtual IPLerror loadStaticMesh(ISerializedObject* serializedObject,
                                    IPLProgressCallback progressCallback,
                                    void* userData,
                                    IStaticMesh** staticMesh) override;

    virtual IPLerror createInstancedMesh(IPLInstancedMeshSettings* settings,
                                         IInstancedMesh** instancedMesh) override;
};


// --------------------------------------------------------------------------------------------------------------------
// CStaticMesh
// --------------------------------------------------------------------------------------------------------------------

class CStaticMesh : public IStaticMesh
{
public:
    Handle<ipl::IStaticMesh> mHandle;

    CStaticMesh(CScene* scene,
                IPLStaticMeshSettings* settings);

    CStaticMesh(CScene* scene,
                ISerializedObject* serializedObject);

    virtual IStaticMesh* retain() override;

    virtual void release() override;

    virtual void save(ISerializedObject* serializedObject) override;

    virtual void add(IScene* scene) override;

    virtual void remove(IScene* scene) override;
};


// --------------------------------------------------------------------------------------------------------------------
// CInstancedMesh
// --------------------------------------------------------------------------------------------------------------------

class CInstancedMesh : public IInstancedMesh
{
public:
    Handle<ipl::IInstancedMesh> mHandle;

    CInstancedMesh(CScene* scene,
                   IPLInstancedMeshSettings* settings);

    virtual IInstancedMesh* retain() override;

    virtual void release() override;

    virtual void add(IScene* scene) override;

    virtual void remove(IScene* scene) override;

    virtual void updateTransform(IScene* scene,
                                 IPLMatrix4x4 transform) override;
};

}
