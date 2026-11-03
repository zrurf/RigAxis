#include "RigAxisSceneProxy.h"

#include "Components/MeshComponent.h"
#include "DynamicMeshBuilder.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "SceneManagement.h"

namespace
{
    class FRigAxisSceneProxy final : public FPrimitiveSceneProxy
    {
    public:
        FRigAxisSceneProxy(
            const UMeshComponent* Component,
            const FRigAxisSceneStateRef& InState,
            const TArray<UMaterialInterface*>& InMaterials)
            : FPrimitiveSceneProxy(Component)
            , State(InState)
            , MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetShaderPlatform()))
        {
            for (UMaterialInterface* Material : InMaterials)
            {
                Materials.Add(Material ? Material->GetRenderProxy() : nullptr);
            }
        }

        virtual SIZE_T GetTypeHash() const override
        {
            static size_t UniquePointer;
            return reinterpret_cast<SIZE_T>(&UniquePointer);
        }

        virtual void GetDynamicMeshElements(
            const TArray<const FSceneView*>& Views,
            const FSceneViewFamily& ViewFamily,
            uint32 VisibilityMap,
            FMeshElementCollector& Collector) const override
        {
            FScopeLock Lock(&State->Lock);

            for (int32 SectionIndex = 0; SectionIndex < State->Sections.Num(); ++SectionIndex)
            {
                const FRigAxisMeshSection& Section = State->Sections[SectionIndex];
                if (Section.Positions.Num() == 0 || Section.Indices.Num() < 3)
                {
                    continue;
                }

                const FMaterialRenderProxy* MaterialProxy = Materials.IsValidIndex(SectionIndex)
                    ? Materials[SectionIndex]
                    : nullptr;
                if (!MaterialProxy)
                {
                    for (FMaterialRenderProxy* Candidate : Materials)
                    {
                        if (Candidate)
                        {
                            MaterialProxy = Candidate;
                            break;
                        }
                    }
                }

                if (!MaterialProxy)
                {
                    continue;
                }

                for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
                {
                    if (!(VisibilityMap & (1 << ViewIndex)))
                    {
                        continue;
                    }

                    FDynamicMeshBuilder Builder(Collector.GetFeatureLevel());
                    Builder.ReserveVertices(Section.Positions.Num());
                    Builder.ReserveTriangles(Section.Indices.Num() / 3);

                    for (int32 Index = 0; Index < Section.Positions.Num(); ++Index)
                    {
                        FDynamicMeshVertex Vertex;
                        Vertex.Position = FVector3f(Section.Positions[Index]);
                        Vertex.TangentX = FVector3f(1.0f, 0.0f, 0.0f);
                        Vertex.TangentZ = FVector3f(State->Normal);
                        Vertex.TextureCoordinate[0] = Section.UVs.IsValidIndex(Index)
                            ? FVector2f(Section.UVs[Index])
                            : FVector2f::ZeroVector;
                        Vertex.Color = Section.Colors.IsValidIndex(Index)
                            ? Section.Colors[Index].ToFColor(true)
                            : FColor::White;
                        Builder.AddVertex(Vertex);
                    }

                    for (int32 Index = 0; Index + 2 < Section.Indices.Num(); Index += 3)
                    {
                        Builder.AddTriangle(
                            Section.Indices[Index],
                            Section.Indices[Index + 1],
                            Section.Indices[Index + 2]);
                    }

                    Builder.GetMesh(
                        GetLocalToWorld(),
                        MaterialProxy,
                        SDPG_World,
                        false,
                        false,
                        ViewIndex,
                        Collector);
                }
            }
        }

        virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
        {
            FPrimitiveViewRelevance Result;
            Result.bDrawRelevance = IsShown(View);
            Result.bShadowRelevance = IsShadowCast(View);
            Result.bDynamicRelevance = true;
            Result.bRenderInMainPass = ShouldRenderInMainPass();
            Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
            Result.bRenderCustomDepth = ShouldRenderCustomDepth();
            Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
            MaterialRelevance.SetPrimitiveViewRelevance(Result);
            Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
            return Result;
        }

        virtual bool CanBeOccluded() const override
        {
            return !MaterialRelevance.bDisableDepthTest;
        }

        virtual uint32 GetMemoryFootprint() const override
        {
            return sizeof(*this) + static_cast<uint32>(Materials.GetAllocatedSize());
        }

    private:
        FRigAxisSceneStateRef State;
        TArray<FMaterialRenderProxy*> Materials;
        FMaterialRelevance MaterialRelevance;
    };
}

FPrimitiveSceneProxy* CreateRigAxisSceneProxy(
    const UMeshComponent* Component,
    const FRigAxisSceneStateRef& State,
    const TArray<UMaterialInterface*>& Materials)
{
    return new FRigAxisSceneProxy(Component, State, Materials);
}
