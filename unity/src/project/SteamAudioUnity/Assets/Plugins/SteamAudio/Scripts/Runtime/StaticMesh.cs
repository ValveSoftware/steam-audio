//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Runtime.InteropServices;

namespace SteamAudio
{
    public class StaticMesh
    {
        Context mContext = null;
        IntPtr mStaticMesh = IntPtr.Zero;

        public StaticMesh(Context context, Scene scene, Vector3[] vertices, Triangle[] triangles, int[] materialIndices, Material[] materials)
        {
            mContext = context;

            var verticesBuffer = Marshal.AllocHGlobal(vertices.Length * Marshal.SizeOf(typeof(Vector3)));
            var trianglesBuffer = Marshal.AllocHGlobal(triangles.Length * Marshal.SizeOf(typeof(Triangle)));
            var materialIndicesBuffer = Marshal.AllocHGlobal(materialIndices.Length * Marshal.SizeOf(typeof(int)));
            var materialsBuffer = Marshal.AllocHGlobal(materials.Length * Marshal.SizeOf(typeof(Material)));

            for (var i = 0; i < vertices.Length; ++i)
            {
                Marshal.StructureToPtr(vertices[i], new IntPtr(verticesBuffer.ToInt64() + i * Marshal.SizeOf(typeof(Vector3))), false);
            }

            for (var i = 0; i < triangles.Length; ++i)
            {
                Marshal.StructureToPtr(triangles[i], new IntPtr(trianglesBuffer.ToInt64() + i * Marshal.SizeOf(typeof(Triangle))), false);
            }

            Marshal.Copy(materialIndices, 0, materialIndicesBuffer, triangles.Length);

            for (var i = 0; i < materials.Length; ++i)
            {
                Marshal.StructureToPtr(materials[i], new IntPtr(materialsBuffer.ToInt64() + i * Marshal.SizeOf(typeof(Material))), false);
            }

            var staticMeshSettings = new StaticMeshSettings { };
            staticMeshSettings.numVertices = vertices.Length;
            staticMeshSettings.numTriangles = triangles.Length;
            staticMeshSettings.numMaterials = materials.Length;
            staticMeshSettings.vertices = verticesBuffer;
            staticMeshSettings.triangles = trianglesBuffer;
            staticMeshSettings.materialIndices = materialIndicesBuffer;
            staticMeshSettings.materials = materialsBuffer;

            var status = API.iplStaticMeshCreate(scene.Get(), ref staticMeshSettings, out mStaticMesh);
            if (status != Error.Success)
            {
                throw new Exception(string.Format("Unable to create static mesh for export ({0} vertices, {1} triangles, {2} materials): [{3}]",
                    staticMeshSettings.numVertices.ToString(), staticMeshSettings.numTriangles.ToString(), staticMeshSettings.numMaterials.ToString(), 
                    status.ToString()));
            }

            Marshal.FreeHGlobal(verticesBuffer);
            Marshal.FreeHGlobal(trianglesBuffer);
            Marshal.FreeHGlobal(materialIndicesBuffer);
            Marshal.FreeHGlobal(materialsBuffer);
        }

        public StaticMesh(Context context, Scene scene, SerializedData dataAsset)
        {
            mContext = context;

            var serializedObject = new SerializedObject(context, dataAsset);

            var status = API.iplStaticMeshLoad(scene.Get(), serializedObject.Get(), null, IntPtr.Zero, out mStaticMesh);
            if (status != Error.Success)
                throw new Exception(string.Format("Unable to load static mesh ({0}). [{1}]", dataAsset.name, status));
        }

        public StaticMesh(StaticMesh staticMesh)
        {
            mContext = staticMesh.mContext;

            mStaticMesh = API.iplStaticMeshRetain(staticMesh.mStaticMesh);
        }

        ~StaticMesh()
        {
            API.iplStaticMeshRelease(ref mStaticMesh);

            mContext = null;
        }

        public IntPtr Get()
        {
            return mStaticMesh;
        }

        public void Save(SerializedData dataAsset)
        {
            var serializedObject = new SerializedObject(mContext);
            API.iplStaticMeshSave(mStaticMesh, serializedObject.Get());
            serializedObject.WriteToFile(dataAsset);
        }

        public void AddToScene(Scene scene)
        {
            API.iplStaticMeshAdd(mStaticMesh, scene.Get());
            scene.NotifyAddObject();
        }

        public void RemoveFromScene(Scene scene)
        {
            API.iplStaticMeshRemove(mStaticMesh, scene.Get());
            scene.NotifyRemoveObject();
        }
    }
}
