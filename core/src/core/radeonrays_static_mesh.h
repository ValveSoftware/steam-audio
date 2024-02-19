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

#if defined(IPL_USES_RADEONRAYS)

#include "radeonrays_scene.h"
#include "static_mesh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// RadeonRaysStaticMesh
// --------------------------------------------------------------------------------------------------------------------

class RadeonRaysStaticMesh : public IStaticMesh
{
public:
    RadeonRaysStaticMesh(shared_ptr<RadeonRaysDevice> radeonRays,
                         int numVertices,
                         int numTriangles,
                         int numMaterials,
                         const Vector3f* vertices,
                         const Triangle* triangles,
                         const int* materialIndices,
                         const Material* materials);

    RadeonRaysStaticMesh(shared_ptr<RadeonRaysDevice> radeonRays,
                         const Serialized::StaticMesh* serializedObject);

    RadeonRaysStaticMesh(shared_ptr<RadeonRaysDevice> radeonRays,
                         SerializedObject& serializedObject);

    ~RadeonRaysStaticMesh();

    virtual int numVertices() const override
    {
        return mNumVertices;
    }

    virtual int numTriangles() const override
    {
        return mNumTriangles;
    }

    virtual int numMaterials() const override
    {
        return mNumMaterials;
    }

    const RadeonRays::Shape* shape() const
    {
        return mShape;
    }

    cl_mem& normals() const
    {
        return mNormals->buffer();
    }

    cl_mem& materialIndices() const
    {
        return mMaterialIndices->buffer();
    }

    cl_mem& materials() const
    {
        return mMaterials->buffer();
    }

    shared_ptr<IStaticMesh> cpuStaticMesh()
    {
        return mCPUStaticMesh;
    }

private:
    shared_ptr<RadeonRaysDevice> mRadeonRays;
    RadeonRays::Shape* mShape;
    int mNumVertices;
    int mNumTriangles;
    int mNumMaterials;
    unique_ptr<OpenCLBuffer> mNormals;
    unique_ptr<OpenCLBuffer> mMaterialIndices;
    unique_ptr<OpenCLBuffer> mMaterials;
    shared_ptr<IStaticMesh> mCPUStaticMesh;

    void initialize(const Vector3f* vertices,
                    const Triangle* triangles,
                    const int* materialIndices,
                    const Material* materials);

    void calcNormals(const Vector3f* vertices,
                     const Triangle* triangles);
};

}

#endif
