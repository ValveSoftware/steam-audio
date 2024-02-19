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
#include "api_serialized_object.h"
#include "api_embree_device.h"
#include "api_radeonrays_device.h"
#include "api_scene.h"

namespace api {

// --------------------------------------------------------------------------------------------------------------------
// CScene
// --------------------------------------------------------------------------------------------------------------------

CScene::CScene(CContext* context,
               IPLSceneSettings* settings)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    SceneType _sceneType = static_cast<SceneType>(settings->type);
    auto _closestHitCallback = reinterpret_cast<ClosestHitCallback>(settings->closestHitCallback);
    auto _anyHitCallback = reinterpret_cast<AnyHitCallback>(settings->anyHitCallback);
    auto _batchedClosestHitCallback = reinterpret_cast<BatchedClosestHitCallback>(settings->batchedClosestHitCallback);
    auto _batchedAnyHitCallback = reinterpret_cast<BatchedAnyHitCallback>(settings->batchedAnyHitCallback);
    auto _embree = (settings->type == IPL_SCENETYPE_EMBREE && settings->embreeDevice) ? reinterpret_cast<CEmbreeDevice*>(settings->embreeDevice)->mHandle.get() : nullptr;
    auto _radeonRays = (settings->type == IPL_SCENETYPE_RADEONRAYS && settings->radeonRaysDevice) ? reinterpret_cast<CRadeonRaysDevice*>(settings->radeonRaysDevice)->mHandle.get() : nullptr;

    new (&mHandle) Handle<ipl::IScene>(shared_ptr<ipl::IScene>(SceneFactory::create(_sceneType, _closestHitCallback, _anyHitCallback, _batchedClosestHitCallback, _batchedAnyHitCallback, settings->userData, _embree, _radeonRays)), _context);
}

CScene::CScene(CContext* context,
               IPLSceneSettings* settings,
               ISerializedObject* serializedObject)
{
    auto _context = context->mHandle.get();
    if (!_context)
        throw Exception(Status::Failure);

    SceneType _sceneType = static_cast<SceneType>(settings->type);
    auto _embree = (settings->embreeDevice) ? reinterpret_cast<CEmbreeDevice*>(settings->embreeDevice)->mHandle.get() : nullptr;
    auto _radeonRays = (settings->radeonRaysDevice) ? reinterpret_cast<CRadeonRaysDevice*>(settings->radeonRaysDevice)->mHandle.get() : nullptr;

    auto _serializedObject = static_cast<CSerializedObject*>(serializedObject)->mHandle.get();
    if (!_serializedObject)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<ipl::IScene>(shared_ptr<ipl::IScene>(SceneFactory::create(_sceneType, _embree, _radeonRays, *_serializedObject)), _context);
}

IScene* CScene::retain()
{
    mHandle.retain();
    return this;
}

void CScene::release()
{
    if (mHandle.release())
    {
        this->~CScene();
        gMemory().free(this);
    }
}

void CScene::save(ISerializedObject* serializedObject)
{
    if (!serializedObject)
        return;

    auto _scene = mHandle.get();
    auto _serializedObject = static_cast<CSerializedObject*>(serializedObject)->mHandle.get();
    if (!_scene || !_serializedObject)
        return;

    static_cast<Scene*>(_scene.get())->serializeAsRoot(*_serializedObject);
}

void CScene::saveOBJ(IPLstring fileBaseName)
{
    if (!fileBaseName)
        return;

    auto _scene = mHandle.get();
    if (!_scene)
        return;

    _scene->dumpObj(fileBaseName);
}

void CScene::commit()
{
    auto _scene = mHandle.get();
    if (!_scene)
        return;

    _scene->commit();
}

IPLerror CScene::createStaticMesh(IPLStaticMeshSettings* settings,
                                  IStaticMesh** staticMesh)
{
    if (!settings || !staticMesh)
        return IPL_STATUS_FAILURE;

    if (settings->numVertices <= 0 || settings->numTriangles <= 0 || settings->numMaterials <= 0)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _staticMesh = reinterpret_cast<CStaticMesh*>(gMemory().allocate(sizeof(CStaticMesh), Memory::kDefaultAlignment));
        new (_staticMesh) CStaticMesh(this, settings);
        *staticMesh = _staticMesh;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLerror CScene::loadStaticMesh(ISerializedObject* serializedObject,
                                IPLProgressCallback progressCallback,
                                void* userData,
                                IStaticMesh** staticMesh)
{
    if (!serializedObject || !staticMesh)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _staticMesh = reinterpret_cast<CStaticMesh*>(gMemory().allocate(sizeof(CStaticMesh), Memory::kDefaultAlignment));
        new (_staticMesh) CStaticMesh(this, serializedObject);
        *staticMesh = _staticMesh;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLerror CScene::createInstancedMesh(IPLInstancedMeshSettings* settings,
                                     IInstancedMesh** instancedMesh)
{
    if (!settings || !settings->subScene || !instancedMesh)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _instancedMesh = reinterpret_cast<CInstancedMesh*>(gMemory().allocate(sizeof(CInstancedMesh), Memory::kDefaultAlignment));
        new (_instancedMesh) CInstancedMesh(this, settings);
        *instancedMesh = _instancedMesh;
    }
    catch (Exception exception)
    {
        return (IPLerror) exception.status();
    }

    return IPL_STATUS_SUCCESS;
}


// --------------------------------------------------------------------------------------------------------------------
// CStaticMesh
// --------------------------------------------------------------------------------------------------------------------

CStaticMesh::CStaticMesh(CScene* scene,
                         IPLStaticMeshSettings* settings)
{
    auto _context = scene->mHandle.context();
    if (!_context)
        throw Exception(Status::Failure);

    auto _scene = scene->mHandle.get();
    if (!_scene)
        throw Exception(Status::Failure);

    auto _vertices = reinterpret_cast<Vector3f*>(settings->vertices);
    auto _triangles = reinterpret_cast<Triangle*>(settings->triangles);
    auto _materials = reinterpret_cast<Material*>(settings->materials);

    new (&mHandle) Handle<ipl::IStaticMesh>(_scene->createStaticMesh(
        settings->numVertices, settings->numTriangles,
        settings->numMaterials, _vertices, _triangles,
        settings->materialIndices, _materials), _context);
}

CStaticMesh::CStaticMesh(CScene* scene,
                         ISerializedObject* serializedObject)
{
    auto _context = scene->mHandle.context();
    if (!_context)
        throw Exception(Status::Failure);

    auto _scene = scene->mHandle.get();
    if (!_scene)
        throw Exception(Status::Failure);

    auto _serializedObject = reinterpret_cast<CSerializedObject*>(serializedObject)->mHandle.get();
    if (!_serializedObject)
        throw Exception(Status::Failure);

    new (&mHandle) Handle<ipl::IStaticMesh>(_scene->createStaticMesh(*_serializedObject), _context);
}

IStaticMesh* CStaticMesh::retain()
{
    mHandle.retain();
    return this;
}

void CStaticMesh::release()
{
    if (mHandle.release())
    {
        this->~CStaticMesh();
        gMemory().free(this);
    }
}

void CStaticMesh::save(ISerializedObject* serializedObject)
{
    if (!serializedObject)
        return;

    auto _staticMesh = mHandle.get();
    auto _serializedObject = static_cast<CSerializedObject*>(serializedObject)->mHandle.get();
    if (!_staticMesh || !_serializedObject)
        return;

    static_cast<StaticMesh*>(_staticMesh.get())->serializeAsRoot(*_serializedObject);
}

void CStaticMesh::add(IScene* scene)
{
    if (!scene)
        return;

    auto _scene = static_cast<CScene*>(scene)->mHandle.get();
    auto _staticMesh = mHandle.get();
    if (!_scene || !_staticMesh)
        return;

    _scene->addStaticMesh(_staticMesh);
}

void CStaticMesh::remove(IScene* scene)
{
    if (!scene)
        return;

    auto _scene = static_cast<CScene*>(scene)->mHandle.get();
    auto _staticMesh = mHandle.get();
    if (!_scene || !_staticMesh)
        return;

    _scene->removeStaticMesh(_staticMesh);
}


// --------------------------------------------------------------------------------------------------------------------
// CInstancedMesh
// --------------------------------------------------------------------------------------------------------------------

CInstancedMesh::CInstancedMesh(CScene* scene,
                               IPLInstancedMeshSettings* settings)
{
    auto _context = scene->mHandle.context();
    if (!_context)
        throw Exception(Status::Failure);

    auto _scene = scene->mHandle.get();
    auto _subScene = reinterpret_cast<CScene*>(settings->subScene)->mHandle.get();
    if (!_scene || !_subScene)
        throw Exception(Status::Failure);

    auto& _transform = reinterpret_cast<Matrix4x4f&>(settings->transform);

    new (&mHandle) Handle<ipl::IInstancedMesh>(_scene->createInstancedMesh(_subScene, _transform), _context);
}

IInstancedMesh* CInstancedMesh::retain()
{
    mHandle.retain();
    return this;
}

void CInstancedMesh::release()
{
    if (mHandle.release())
    {
        this->~CInstancedMesh();
        gMemory().free(this);
    }
}

void CInstancedMesh::add(IScene* scene)
{
    if (!scene)
        return;

    auto _scene = static_cast<CScene*>(scene)->mHandle.get();
    auto _instancedMesh = mHandle.get();
    if (!_scene || !_instancedMesh)
        return;

    _scene->addInstancedMesh(_instancedMesh);
}

void CInstancedMesh::remove(IScene* scene)
{
    if (!scene)
        return;

    auto _scene = static_cast<CScene*>(scene)->mHandle.get();
    auto _instancedMesh = mHandle.get();
    if (!_scene || !_instancedMesh)
        return;

    _scene->removeInstancedMesh(_instancedMesh);
}

void CInstancedMesh::updateTransform(IScene* scene,
                                     IPLMatrix4x4 transform)
{
    if (!scene)
        return;

    auto _scene = static_cast<CScene*>(scene)->mHandle.get();
    auto _instancedMesh = mHandle.get();
    if (!_scene || !_instancedMesh)
        return;

    auto& _transform = reinterpret_cast<Matrix4x4f&>(transform);

    _instancedMesh->updateTransform(*_scene, _transform);
}


// --------------------------------------------------------------------------------------------------------------------
// CContext
// --------------------------------------------------------------------------------------------------------------------

IPLerror CContext::createScene(IPLSceneSettings* settings,
                               IScene** scene)
{
    if (!settings || !scene)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _scene = reinterpret_cast<CScene*>(gMemory().allocate(sizeof(CScene), Memory::kDefaultAlignment));
        new (_scene) CScene(this, settings);
        *scene = _scene;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
}

IPLerror CContext::loadScene(IPLSceneSettings* settings,
                             ISerializedObject* serializedObject,
                             IPLProgressCallback progressCallback,
                             void* userData,
                             IScene** scene)
{
    if (!settings || !serializedObject || !scene)
        return IPL_STATUS_FAILURE;

    try
    {
        auto _scene = reinterpret_cast<CScene*>(gMemory().allocate(sizeof(CScene), Memory::kDefaultAlignment));
        new (_scene) CScene(this, settings, serializedObject);
        *scene = _scene;
    }
    catch (Exception exception)
    {
        return static_cast<IPLerror>(exception.status());
    }

    return IPL_STATUS_SUCCESS;
}

}
