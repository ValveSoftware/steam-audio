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

#include "SteamAudioScene.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeInfo.h"
#include "Model.h"
#include "Async/Async.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "SteamAudioCommon.h"
#include "SteamAudioDynamicObjectComponent.h"
#include "SteamAudioGeometryComponent.h"
#include "SteamAudioManager.h"
#include "SteamAudioMaterial.h"
#include "SteamAudioSerializedObject.h"
#include "SteamAudioSettings.h"
#include "SteamAudioStaticMeshActor.h"

namespace SteamAudio {

// ---------------------------------------------------------------------------------------------------------------------
// Scene Export
// ---------------------------------------------------------------------------------------------------------------------

#if WITH_EDITOR

/**
 * Returns a reference to the Steam Audio Material asset to use for a given actor.
 */
static FSoftObjectPath GetMaterialAssetForActor(AActor* Actor)
{
    while (Actor)
    {
        USteamAudioGeometryComponent* GeometryComponent = Actor->FindComponentByClass<USteamAudioGeometryComponent>();
        if (GeometryComponent && GeometryComponent->Material.IsValid())
            return GeometryComponent->Material;

        Actor = Actor->GetAttachParentActor();
    }

    return nullptr;
}

/**
 * Adds a Steam Audio Material asset to the material data being prepared for export.
 */
static bool ExportMaterial(FSoftObjectPath MaterialAsset, TArray<IPLMaterial>& Materials,
    TMap<FString, int>& MaterialIndexForAsset)
{
    if (!MaterialAsset.IsValid())
    {
        UE_LOG(LogSteamAudio, Error, TEXT("No material specified for object."));
        return false;
    }

    // If the material has already been exported (because it was referenced by some other geometry), do nothing.
    if (MaterialIndexForAsset.Contains(MaterialAsset.ToString()))
        return true;

    USteamAudioMaterial* Material = Cast<USteamAudioMaterial>(MaterialAsset.TryLoad());
    if (!Material)
    {
        UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load material asset: %s."), *MaterialAsset.ToString());
        return false;
    }

    IPLMaterial SteamAudioMaterial{};
    SteamAudioMaterial.absorption[0] = Material->AbsorptionLow;
    SteamAudioMaterial.absorption[1] = Material->AbsorptionMid;
    SteamAudioMaterial.absorption[2] = Material->AbsorptionHigh;
    SteamAudioMaterial.scattering = Material->Scattering;
    SteamAudioMaterial.transmission[0] = Material->TransmissionLow;
    SteamAudioMaterial.transmission[1] = Material->TransmissionMid;
    SteamAudioMaterial.transmission[2] = Material->TransmissionHigh;

    Materials.Add(SteamAudioMaterial);
    MaterialIndexForAsset.Add(MaterialAsset.ToString(), Materials.Num() - 1);

    return true;
}

/**
 * Exports a single Static Mesh component.
 */
static bool ExportStaticMeshComponent(UStaticMeshComponent* StaticMeshComponent, TArray<IPLVector3>& Vertices,
    TArray<IPLTriangle>& Triangles, TArray<int>& MaterialIndices, TArray<IPLMaterial>& Materials,
    TMap<FString, int>& MaterialIndexForAsset, bool bRelativePositions = true)
{
    check(StaticMeshComponent);
    check(StaticMeshComponent->GetStaticMesh());
    check(StaticMeshComponent->GetStaticMesh()->GetRenderData());

    FStaticMeshLODResources& LODModel = StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0];
    check(LODModel.GetNumVertices() > 0 && LODModel.GetNumTriangles() > 0);

    int StartVertexIndex = Vertices.Num();

    const FPositionVertexBuffer& VertexBuffer = LODModel.VertexBuffers.PositionVertexBuffer;
    for (int i = 0; i < LODModel.GetNumVertices(); ++i)
    {
#if ((ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0) || (ENGINE_MAJOR_VERSION > 5))
        FVector Vertex{};
        Vertex.X = VertexBuffer.VertexPosition(i).X;
        Vertex.Y = VertexBuffer.VertexPosition(i).Y;
        Vertex.Z = VertexBuffer.VertexPosition(i).Z;
#else
        FVector Vertex = VertexBuffer.VertexPosition(i);
#endif

        if (bRelativePositions)
        {
            Vertex = StaticMeshComponent->GetComponentTransform().TransformPosition(Vertex);
        }

        Vertices.Add(SteamAudio::ConvertVector(Vertex));
    }

    FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
    for (const FStaticMeshSection& Section : LODModel.Sections)
    {
        for (uint32 i = 0; i < Section.NumTriangles; ++i)
        {
            int BaseIndex = Section.FirstIndex + i * 3;

            // todo: clarify why the triangle order is flipped here
            IPLTriangle Triangle{};
            Triangle.indices[0] = StartVertexIndex + Indices[BaseIndex + 0];
            Triangle.indices[1] = StartVertexIndex + Indices[BaseIndex + 2];
            Triangle.indices[2] = StartVertexIndex + Indices[BaseIndex + 1];

            Triangles.Add(Triangle);
        }
    }

    FSoftObjectPath MaterialAsset = GetMaterialAssetForActor(StaticMeshComponent->GetOwner());
    if (!MaterialAsset.IsValid())
    {
        MaterialAsset = GetDefault<USteamAudioSettings>()->DefaultMeshMaterial;
    }

    if (!ExportMaterial(MaterialAsset, Materials, MaterialIndexForAsset))
        return false;

    check(MaterialIndexForAsset.Contains(MaterialAsset.ToString()));
    int MaterialIndex = MaterialIndexForAsset[MaterialAsset.ToString()];
    for (int i = 0; i < LODModel.GetNumTriangles(); ++i)
    {
        MaterialIndices.Add(MaterialIndex);
    }

    return true;
}

/**
 * Exports a single Static Mesh actor.
 *
 * todo: what if there is a tree of static mesh components?
 * todo: what if static mesh components are attached to arbitrary actors (instead of static mesh actors)?
 */
static bool ExportStaticMeshComponentsForActor(AStaticMeshActor* StaticMeshActor, TArray<IPLVector3>& Vertices,
    TArray<IPLTriangle>& Triangles, TArray<int>& MaterialIndices, TArray<IPLMaterial>& Materials,
    TMap<FString, int>& MaterialIndexForAsset, bool bRelativePositions = true)
{
    check(StaticMeshActor);

    UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
    if (!StaticMeshComponent)
        return false;

    UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
    if (!StaticMesh || !StaticMesh->HasValidRenderData())
        return false;

    return ExportStaticMeshComponent(StaticMeshComponent, Vertices, Triangles, MaterialIndices, Materials,
        MaterialIndexForAsset, bRelativePositions);
}

/**
 * Exports a single Landscape (terrain) actor.
 *
 * todo: non-default materials for terrain
 */
static bool ExportLandscapeActor(ALandscape* LandscapeActor, TArray<IPLVector3>& Vertices,
    TArray<IPLTriangle>& Triangles, TArray<int>& MaterialIndices, TArray<IPLMaterial>& Materials,
    TMap<FString, int>& MaterialIndexForAsset)
{
    check(LandscapeActor);

    int InitialNumTriangles = Triangles.Num();

    ULandscapeInfo* LandscapeInfo = LandscapeActor->GetLandscapeInfo();
    if (!LandscapeInfo)
        return false;

    for (auto ComponentIt = LandscapeInfo->XYtoComponentMap.CreateIterator(); ComponentIt; ++ComponentIt)
    {
        ULandscapeComponent* Component = ComponentIt.Value();
        check(Component);

        FLandscapeComponentDataInterface CDI(Component);

        for (int y = 0; y < Component->ComponentSizeQuads; ++y)
        {
            for (int x = 0; x < Component->ComponentSizeQuads; ++x)
            {
                int StartIndex = Vertices.Num();

                Vertices.Add(ConvertVector(CDI.GetWorldVertex(x, y)));
                Vertices.Add(ConvertVector(CDI.GetWorldVertex(x, y + 1)));
                Vertices.Add(ConvertVector(CDI.GetWorldVertex(x + 1, y + 1)));
                Vertices.Add(ConvertVector(CDI.GetWorldVertex(x + 1, y)));

                IPLTriangle Triangle{};

                Triangle.indices[0] = StartIndex;
                Triangle.indices[1] = StartIndex + 2;
                Triangle.indices[2] = StartIndex + 3;
                Triangles.Add(Triangle);

                Triangle.indices[0] = StartIndex;
                Triangle.indices[1] = StartIndex + 1;
                Triangle.indices[2] = StartIndex + 2;
                Triangles.Add(Triangle);
            }
        }
    }

    FSoftObjectPath MaterialAsset = GetDefault<USteamAudioSettings>()->DefaultLandscapeMaterial;
    if (!ExportMaterial(MaterialAsset, Materials, MaterialIndexForAsset))
        return false;

    check(MaterialIndexForAsset.Contains(MaterialAsset.ToString()));
    int MaterialIndex = MaterialIndexForAsset[MaterialAsset.ToString()];
    int NumTriangles = Triangles.Num() - InitialNumTriangles;
    for (int i = 0; i < NumTriangles; ++i)
    {
        MaterialIndices.Add(MaterialIndex);
    }

    return true;
}

/**
 * Exports every actor in the given list of actors.
 *
 * todo: is it safe to assume that only static mesh actors and landscape actors will be exported? what about random
 *       actors with static mesh components?
 */
static bool ExportActors(const TArray<AActor*>& Actors, TArray<IPLVector3>& Vertices,
    TArray<IPLTriangle>& Triangles, TArray<int>& MaterialIndices, TArray<IPLMaterial>& Materials,
    TMap<FString, int>& MaterialIndexForAsset, bool bRelativePositions = true)
{
    for (AActor* Actor : Actors)
    {
        if (Actor->IsA<AStaticMeshActor>())
        {
            if (!ExportStaticMeshComponentsForActor(Cast<AStaticMeshActor>(Actor), Vertices, Triangles, MaterialIndices,
                Materials, MaterialIndexForAsset, bRelativePositions))
            {
                return false;
            }
        }
        else if (Actor->IsA<ALandscape>())
        {
            if (!ExportLandscapeActor(Cast<ALandscape>(Actor), Vertices, Triangles, MaterialIndices, Materials,
                MaterialIndexForAsset))
            {
                return false;
            }
        }
    }

    return true;
}

/**
 * Exports all BSP geometry in the given world.
 *
 * todo: does not understand sublevels?
 */
static bool ExportBSPGeometry(UWorld* World, ULevel* Level, TArray<IPLVector3>& Vertices,
    TArray<IPLTriangle>& Triangles, TArray<int>& MaterialIndices, TArray<IPLMaterial>& Materials,
    TMap<FString, int>& MaterialIndexForAsset)
{
    check(World);
    check(Level);
    check(World->GetModel());

    int InitialNumVertices = Vertices.Num();
    int InitialNumTriangles = Triangles.Num();

    // Gather and convert all world vertices to Steam Audio coords
#if ((ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 0) || (ENGINE_MAJOR_VERSION > 5))
    for (const FVector3f& WorldVertexf : World->GetModel()->Points)
    {
        FVector WorldVertex{};
        WorldVertex.X = WorldVertexf.X;
        WorldVertex.Y = WorldVertexf.Y;
        WorldVertex.Z = WorldVertexf.Z;

        Vertices.Add(SteamAudio::ConvertVector(WorldVertex));
    }
#else
    for (const FVector& WorldVertex : World->GetModel()->Points)
    {
        Vertices.Add(SteamAudio::ConvertVector(WorldVertex));
    }
#endif

    // Gather vertex indices for all faces ("nodes" are faces)
    for (const FBspNode& WorldNode : World->GetModel()->Nodes)
    {
        // Ignore degenerate faces
        if (WorldNode.NumVertices <= 2)
            continue;

        // Faces are organized as triangle fans
        int Index0 = World->GetModel()->Verts[WorldNode.iVertPool + 0].pVertex;
        int Index1 = World->GetModel()->Verts[WorldNode.iVertPool + 1].pVertex;
        int Index2;

        for (int v = 2; v < WorldNode.NumVertices; ++v)
        {
            Index2 = World->GetModel()->Verts[WorldNode.iVertPool + v].pVertex;

            IPLTriangle Triangle{};
            Triangle.indices[0] = Index0 + InitialNumVertices;
            Triangle.indices[1] = Index2 + InitialNumVertices;
            Triangle.indices[2] = Index1 + InitialNumVertices;
            Triangles.Add(Triangle);

            Index1 = Index2;
        }
    }

    FSoftObjectPath MaterialAsset = GetDefault<USteamAudioSettings>()->DefaultBSPMaterial;
    if (!ExportMaterial(MaterialAsset, Materials, MaterialIndexForAsset))
        return false;

    check(MaterialIndexForAsset.Contains(MaterialAsset.ToString()));
    int MaterialIndex = MaterialIndexForAsset[MaterialAsset.ToString()];
    int NumTriangles = Triangles.Num() - InitialNumTriangles;
    for (int i = 0; i < NumTriangles; ++i)
    {
        MaterialIndices.Add(MaterialIndex);
    }

    return true;
}

/**
 * Returns true if either a) a Steam Audio Geometry component is attached to the given actor, or b) a Steam Audio
 * Geometry component is attached to some ancestor of this actor and Export All Children is checked on that component.
 */
static bool IsSteamAudioGeometry(AActor* Actor)
{
    AActor* CurrentActor = Actor;
    while (CurrentActor)
    {
        USteamAudioGeometryComponent* GeometryComponent = CurrentActor->FindComponentByClass<USteamAudioGeometryComponent>();
        if (GeometryComponent)
            return (CurrentActor == Actor || GeometryComponent->bExportAllChildren);

        CurrentActor = CurrentActor->GetAttachParentActor();
    }

    return false;
}

/**
 * Returns true if either a) a Steam Audio Dynamic Object component is attached to the given actor, or b) a Steam Audio
 * Dynamic Object component is attached to some ancestor of this actor.
 */
static bool IsSteamAudioDynamicObject(AActor* Actor)
{
    AActor* CurrentActor = Actor;
    while (CurrentActor)
    {
        USteamAudioDynamicObjectComponent* DynamicObjectComponent = Actor->FindComponentByClass<USteamAudioDynamicObjectComponent>();
        if (DynamicObjectComponent)
            return true;

        CurrentActor = CurrentActor->GetAttachParentActor();
    }

    return false;
}

/**
 * Finds all actors in the given (sub)level that are tagged for export as part of the level's static geometry.
 */
static void GetActorsForStaticGeometryExport(UWorld* World, ULevel* Level, TArray<AActor*>& Actors)
{
    check(World);
    check(Level);

    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        if (It->GetLevel() != Level)
            continue;

        if (!IsSteamAudioGeometry(*It) || IsSteamAudioDynamicObject(*It))
            continue;

        // Ignore static meshes that are marked as Movable.
        UStaticMeshComponent* StaticMeshComponent = It->GetStaticMeshComponent();
        if (!StaticMeshComponent || StaticMeshComponent->Mobility == EComponentMobility::Movable)
            continue;

        Actors.Add(*It);
    }

    if (GetDefault<USteamAudioSettings>()->bExportLandscapeGeometry)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            if (It->GetLevel() != Level)
                continue;

            if (!IsSteamAudioGeometry(*It) || IsSteamAudioDynamicObject(*It))
                continue;

            Actors.Add(*It);
        }
    }
}

/**
 * Returns true if the given actor should be exported as part of the given Steam Audio Dynamic Object component. This
 * is used to ensure that an actor is only exported as part of the Steam Audio Dynamic Object which is its closest
 * ancestor.
 */
static bool DoesDynamicObjectContainActor(USteamAudioDynamicObjectComponent* DynamicObjectComponent, AActor* Actor)
{
    check(DynamicObjectComponent);
    check(Actor);

    const AActor* DynamicObjectActor = DynamicObjectComponent->GetOwner();

    while (Actor)
    {
        const USteamAudioDynamicObjectComponent* Component = Actor->FindComponentByClass<USteamAudioDynamicObjectComponent>();
        if (Component && Component == DynamicObjectComponent)
            return true;

        if (Actor == DynamicObjectActor)
            break;

        Actor = Actor->GetAttachParentActor();
    }

    return false;
}

/**
 * Finds all actors that should be exported as part of the given Steam Audio Dynamic Object.
 */
static void GetActorsForDynamicObjectExport(USteamAudioDynamicObjectComponent* DynamicObjectComponent,
    TArray<AActor*>& Actors)
{
    check(DynamicObjectComponent);

    if (DynamicObjectComponent->IsInBlueprint())
    {
        const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(DynamicObjectComponent->GetOutermostObject());
        if (!BlueprintClass || !BlueprintClass->SimpleConstructionScript || !BlueprintClass->SimpleConstructionScript->GetComponentEditorActorInstance())
            return;

        Actors.Add(BlueprintClass->SimpleConstructionScript->GetComponentEditorActorInstance());
    }
    else
    {
        Actors.Add(DynamicObjectComponent->GetOwner());

        TArray<AActor*> Children;
        DynamicObjectComponent->GetOwner()->GetAllChildActors(Children);
        for (AActor* Actor : Children)
        {
            if (DoesDynamicObjectContainActor(DynamicObjectComponent, Actor))
            {
                Actors.Add(Actor);
            }
        }
    }
}

bool DoesLevelHaveStaticGeometryForExport(UWorld* World, ULevel* Level)
{
    check(World);
    check(Level);

    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
    {
        if (It->GetLevel() == Level && IsSteamAudioGeometry(*It) && !IsSteamAudioDynamicObject(*It))
            return true;
    }

    if (GetDefault<USteamAudioSettings>()->bExportLandscapeGeometry)
    {
        for (TActorIterator<ALandscape> It(World); It; ++It)
        {
            if (It->GetLevel() == Level && IsSteamAudioGeometry(*It) && !IsSteamAudioDynamicObject(*It))
                return true;
        }
    }

    if (GetDefault<USteamAudioSettings>()->bExportBSPGeometry)
    {
        if (World->GetModel() && World->GetModel()->Points.Num() > 0 && World->GetModel()->Nodes.Num() > 0)
            return true;
    }

    return false;
}

bool ExportStaticGeometryForLevel(UWorld* World, ULevel* Level, FString FileName, bool bExportOBJ /* = false */)
{
    check(World);
    check(Level);

    TPromise<bool> Promise;

    Async(EAsyncExecution::Thread, [World, Level, FileName, bExportOBJ, &Promise]()
    {
        bool bResult = false;

        // Start by collecting geometry and material information from the level.
        TArray<IPLVector3> Vertices;
        TArray<IPLTriangle> Triangles;
        TArray<int> MaterialIndices;
        TArray<IPLMaterial> Materials;
        TMap<FString, int> MaterialIndexForAsset;
        TArray<AActor*> Actors;
        bool bExportSucceeded = RunInGameThread<bool>([&]()
        {
            GetActorsForStaticGeometryExport(World, Level, Actors);
            if (!ExportActors(Actors, Vertices, Triangles, MaterialIndices, Materials, MaterialIndexForAsset))
                return false;

            if (GetDefault<USteamAudioSettings>()->bExportBSPGeometry)
            {
                if (!ExportBSPGeometry(World, Level, Vertices, Triangles, MaterialIndices, Materials, MaterialIndexForAsset))
                    return false;
            }

            return true;
        });
        if (!bExportSucceeded)
        {
            Promise.SetValue(false);
            return;
        }

        // If we didn't find anything, stop here.
        if (Vertices.Num() <= 0 || Triangles.Num() <= 0 || MaterialIndices.Num() <= 0 || Materials.Num() <= 0)
        {
            UE_LOG(LogSteamAudio, Log, TEXT("No static geometry specified for level: %s"), *Level->GetOutermostObject()->GetName());
            Promise.SetValue(false);
            return;
        }

        FSteamAudioManager& Manager = FSteamAudioModule::GetManager();
        bool bInitializeSucceeded = RunInGameThread<bool>([&]()
        {
            return Manager.InitializeSteamAudio(EManagerInitReason::EXPORTING_SCENE);
        });
        if (!bInitializeSucceeded)
        {
            Promise.SetValue(false);
            return;
        }

        IPLContext Context = Manager.GetContext();
        IPLScene Scene = Manager.GetScene();

        // Create a static mesh using all the data gathered from the level.
        IPLStaticMeshSettings StaticMeshSettings{};
        StaticMeshSettings.numVertices = Vertices.Num();
        StaticMeshSettings.numTriangles = Triangles.Num();
        StaticMeshSettings.numMaterials = Materials.Num();
        StaticMeshSettings.vertices = Vertices.GetData();
        StaticMeshSettings.triangles = Triangles.GetData();
        StaticMeshSettings.materialIndices = MaterialIndices.GetData();
        StaticMeshSettings.materials = Materials.GetData();

        IPLStaticMesh StaticMesh = nullptr;
        IPLerror Status = iplStaticMeshCreate(Scene, &StaticMeshSettings, &StaticMesh);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Steam Audio static mesh for level: %s [%i]"), *Level->GetOutermostObject()->GetName(), Status);
            Manager.ShutDownSteamAudio();
            Promise.SetValue(false);
            return;
        }

        if (bExportOBJ)
        {
            // We're exporting to a .obj file, so just treat the provided file name as the name of the actual on-disk
            // file we want to save to.
            iplStaticMeshAdd(StaticMesh, Scene);
            iplSceneCommit(Scene);
            iplSceneSaveOBJ(Scene, TCHAR_TO_ANSI(*FileName));
        }
        else
        {
            // We're exporting to a .uasset file, so the provided file name is the name of an asset package (i.e.,
            // /Path/To/Thing.Thing.

            // Serialize the static mesh.
            IPLSerializedObjectSettings SerializedObjectSettings{};

            IPLSerializedObject SerializedObject = nullptr;
            Status = iplSerializedObjectCreate(Context, &SerializedObjectSettings, &SerializedObject);
            if (Status != IPL_STATUS_SUCCESS)
            {
                UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Steam Audio serialized object for level: %s [%i]"), *Level->GetOutermostObject()->GetName(), Status);
                iplStaticMeshRelease(&StaticMesh);
                Manager.ShutDownSteamAudio();
                Promise.SetValue(false);
                return;
            }

            iplStaticMeshSave(StaticMesh, SerializedObject);

            // Save the data in the IPLSerializedObject to the appropriate .uasset file.
            USteamAudioSerializedObject* Asset = RunInGameThread<USteamAudioSerializedObject*>([&]()
            {
                return USteamAudioSerializedObject::SerializeObjectToPackage(SerializedObject, FileName);
            });
            if (!Asset)
            {
                UE_LOG(LogSteamAudio, Error, TEXT("Unable to serialize mesh data for level: %s"), *Level->GetOutermostObject()->GetName());
                iplSerializedObjectRelease(&SerializedObject);
                iplStaticMeshRelease(&StaticMesh);
                Manager.ShutDownSteamAudio();
                Promise.SetValue(false);
                return;
            }

            RunInGameThread<void>([&]()
            {
                // See if there already is a Steam Audio Static Mesh actor in the level.
                ASteamAudioStaticMeshActor* SteamAudioStaticMeshActor = nullptr;
                for (TActorIterator<ASteamAudioStaticMeshActor> It(World); It; ++It)
                {
                    if (It->GetLevel() == Level)
                    {
                        SteamAudioStaticMeshActor = *It;
                        break;
                    }
                }

                if (!SteamAudioStaticMeshActor)
                {
                    // We couldn't find a Steam Audio Static Mesh actor in the level, so create one.
                    FActorSpawnParameters ActorSpawnParams{};
                    ActorSpawnParams.OverrideLevel = Level;

                    SteamAudioStaticMeshActor = World->SpawnActor<ASteamAudioStaticMeshActor>(ActorSpawnParams);
                }

                // Point the Steam Audio Static Mesh actor to the .uasset we just created.
                check(SteamAudioStaticMeshActor);
                SteamAudioStaticMeshActor->Asset = Asset;
                SteamAudioStaticMeshActor->MarkPackageDirty();
            });

            iplSerializedObjectRelease(&SerializedObject);
        }

        iplStaticMeshRelease(&StaticMesh);
        Manager.ShutDownSteamAudio();
        Promise.SetValue(true);
    });

    TFuture<bool> Value = Promise.GetFuture();
    Value.Wait();

    return Value.Get();
}

bool ExportDynamicObject(USteamAudioDynamicObjectComponent* DynamicObject, FString FileName, bool bExportOBJ /* = false */)
{
    check(DynamicObject);

    TPromise<bool> Promise;

    Async(EAsyncExecution::Thread, [DynamicObject, FileName, bExportOBJ, &Promise]()
    {
        // Start by collecting geometry and material information from the level.
        TArray<IPLVector3> Vertices;
        TArray<IPLTriangle> Triangles;
        TArray<int> MaterialIndices;
        TArray<IPLMaterial> Materials;
        TMap<FString, int> MaterialIndexForAsset;
        TArray<AActor*> Actors;
        bool bExportSucceeded = RunInGameThread<bool>([&]()
        {
            GetActorsForDynamicObjectExport(DynamicObject, Actors);
            return ExportActors(Actors, Vertices, Triangles, MaterialIndices, Materials, MaterialIndexForAsset, false);
        });
        if (!bExportSucceeded)
        {
            Promise.SetValue(false);
            return;
        }

        // If we didn't find anything, stop here.
        if (Vertices.Num() <= 0 || Triangles.Num() <= 0 || MaterialIndices.Num() <= 0 || Materials.Num() <= 0)
        {
            UE_LOG(LogSteamAudio, Log, TEXT("No geometry specified for dynamic object: %s"), *DynamicObject->GetOuter()->GetName());
            Promise.SetValue(false);
            return;
        }

        FSteamAudioManager& Manager = FSteamAudioModule::GetManager();
        bool bInitializeSucceeded = RunInGameThread<bool>([&]()
        {
            return Manager.InitializeSteamAudio(EManagerInitReason::EXPORTING_SCENE);
        });
        if (!bInitializeSucceeded)
        {
            Promise.SetValue(false);
            return;
        }

        IPLContext Context = Manager.GetContext();
        IPLScene Scene = Manager.GetScene();

        // Create a static mesh using all the data gathered from the level.
        IPLStaticMeshSettings StaticMeshSettings{};
        StaticMeshSettings.numVertices = Vertices.Num();
        StaticMeshSettings.numTriangles = Triangles.Num();
        StaticMeshSettings.numMaterials = Materials.Num();
        StaticMeshSettings.vertices = Vertices.GetData();
        StaticMeshSettings.triangles = Triangles.GetData();
        StaticMeshSettings.materialIndices = MaterialIndices.GetData();
        StaticMeshSettings.materials = Materials.GetData();

        IPLStaticMesh StaticMesh = nullptr;
        IPLerror Status = iplStaticMeshCreate(Scene, &StaticMeshSettings, &StaticMesh);
        if (Status != IPL_STATUS_SUCCESS)
        {
            UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Steam Audio static mesh for dynamic object: %s [%i]"), *DynamicObject->GetOuter()->GetName(), Status);
            Manager.ShutDownSteamAudio();
            Promise.SetValue(false);
            return;
        }

        if (bExportOBJ)
        {
            // We're exporting to a .obj file, so just treat the provided file name as the name of the actual on-disk
            // file we want to save to.
            iplStaticMeshAdd(StaticMesh, Scene);
            iplSceneCommit(Scene);
            iplSceneSaveOBJ(Scene, TCHAR_TO_ANSI(*FileName));
        }
        else
        {
            // We're exporting to a .uasset file, so the provided file name is the name of an asset package (i.e.,
            // /Path/To/Thing.Thing.

            // Serialize the static mesh.
            IPLSerializedObjectSettings SerializedObjectSettings{};

            IPLSerializedObject SerializedObject = nullptr;
            Status = iplSerializedObjectCreate(Context, &SerializedObjectSettings, &SerializedObject);
            if (Status != IPL_STATUS_SUCCESS)
            {
                UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Steam Audio serialized object for dynamic object: %s [%i]"), *DynamicObject->GetOuter()->GetName(), Status);
                iplStaticMeshRelease(&StaticMesh);
                Manager.ShutDownSteamAudio();
                Promise.SetValue(false);
                return;
            }

            iplStaticMeshSave(StaticMesh, SerializedObject);

            // Save the data in the IPLSerializedObject to the appropriate .uasset file.
            USteamAudioSerializedObject* Asset = RunInGameThread<USteamAudioSerializedObject*>([&]()
            {
                return USteamAudioSerializedObject::SerializeObjectToPackage(SerializedObject, FileName);
            });
            if (!Asset)
            {
                UE_LOG(LogSteamAudio, Error, TEXT("Unable to serialize mesh data for dynamic object: %s"), *DynamicObject->GetOuter()->GetName());
                iplSerializedObjectRelease(&SerializedObject);
                iplStaticMeshRelease(&StaticMesh);
                Manager.ShutDownSteamAudio();
                Promise.SetValue(false);
                return;
            }

            RunInGameThread<void>([&]()
            {
                // Point the Steam Audio Dynamic Object component to the .uasset we just created.
                if (DynamicObject->IsInBlueprint())
                {
                    USteamAudioDynamicObjectComponent* DefaultObject = GetMutableDefault<USteamAudioDynamicObjectComponent>();
                    if (DefaultObject)
                    {
                        DefaultObject->Asset = Asset;
                        DefaultObject->MarkPackageDirty();
                    }
                }

                DynamicObject->Asset = Asset;
                DynamicObject->MarkPackageDirty();
            });

            iplSerializedObjectRelease(&SerializedObject);
        }

        iplStaticMeshRelease(&StaticMesh);
        Manager.ShutDownSteamAudio();
        Promise.SetValue(true);
    });

    TFuture<bool> Value = Promise.GetFuture();
    Value.Wait();

    return Value.Get();
}

#endif


// ---------------------------------------------------------------------------------------------------------------------
// Scene Load/Unload
// ---------------------------------------------------------------------------------------------------------------------

IPLStaticMesh LoadStaticMeshFromAsset(FSoftObjectPath Asset, IPLContext Context, IPLScene Scene)
{
    check(Asset.IsAsset());
    check(Context);
    check(Scene);

    USteamAudioSerializedObject* AssetObject = Cast<USteamAudioSerializedObject>(Asset.TryLoad());
    if (!AssetObject)
        return nullptr;

    IPLSerializedObjectSettings SerializedObjectSettings{};
    SerializedObjectSettings.size = AssetObject->Data.Num();
    SerializedObjectSettings.data = AssetObject->Data.GetData();

    IPLSerializedObject SerializedObject = nullptr;
    IPLerror Status = iplSerializedObjectCreate(Context, &SerializedObjectSettings, &SerializedObject);
    if (Status != IPL_STATUS_SUCCESS)
    {
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to create serialized object. [%d]"), Status);
        return nullptr;
    }

    IPLStaticMesh StaticMesh = nullptr;
    Status = iplStaticMeshLoad(Scene, SerializedObject, nullptr, nullptr, &StaticMesh);
    if (Status != IPL_STATUS_SUCCESS)
    {
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to load static mesh from serialized object. [%d]"), Status);
        iplSerializedObjectRelease(&SerializedObject);
        return nullptr;
    }

    iplSerializedObjectRelease(&SerializedObject);
    return StaticMesh;
}


// ---------------------------------------------------------------------------------------------------------------------
// Baked Data Load/Unload
// ---------------------------------------------------------------------------------------------------------------------

IPLProbeBatch LoadProbeBatchFromAsset(FSoftObjectPath Asset, IPLContext Context)
{
    check(Asset.IsValid());
    check(Context);

    USteamAudioSerializedObject* AssetObject = Cast<USteamAudioSerializedObject>(Asset.TryLoad());
    if (!AssetObject)
        return nullptr;

    IPLSerializedObjectSettings SerializedObjectSettings{};
    SerializedObjectSettings.size = AssetObject->Data.Num();
    SerializedObjectSettings.data = AssetObject->Data.GetData();

    IPLSerializedObject SerializedObject = nullptr;
    IPLerror Status = iplSerializedObjectCreate(Context, &SerializedObjectSettings, &SerializedObject);
    if (Status != IPL_STATUS_SUCCESS)
    {
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to create serialized object. [%d]"), Status);
        return nullptr;
    }

    IPLProbeBatch ProbeBatch = nullptr;
    Status = iplProbeBatchLoad(Context, SerializedObject, &ProbeBatch);
    if (Status != IPL_STATUS_SUCCESS)
    {
        UE_LOG(LogSteamAudio, Error, TEXT("Unable to load probe batch from serialized object. [%d]"), Status);
        iplSerializedObjectRelease(&SerializedObject);
        return nullptr;
    }

    iplSerializedObjectRelease(&SerializedObject);
    return ProbeBatch;
}

}
