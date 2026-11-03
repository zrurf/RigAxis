#include "RigAxisSceneProxy.h"

#include "DynamicMeshBuilder.h"
#include "MaterialShared.h"
#include "PrimitiveSceneProxy.h"
#include "RigAxisComponent.h"
#include "RigAxisTypes.h"

namespace
{
    class FRigAxisSceneProxy final : public FPrimitiveSceneProxy
    {
    public:
        FRigAxisSceneProxy(
            const URigAxisComponent* Component,
            const TArray<FRigAxisMeshSection>& InSections,
            const TArray<UMaterialInterface*>& InMaterials,
            const FVector& InNormal)
            : FPrimitiveSceneProxy(Component)
            , Sections(InSections)
            , Normal(InNormal)
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
            for (const FRigAxisMeshSection& Section : Sections)
            {
                if (Section.Positions.Num() == 0 || Section.Indices.Num() < 3)
                {
                    continue;
                }

                const FMaterialRenderProxy* MaterialProxy = nullptr;
                if (Materials.IsValidIndex(static_cast<int32>(Section.Page)))
                {
                    MaterialProxy = Materials[Section.Page];
                }
                else if (Materials.Num() > 0)
                {
                    MaterialProxy = Materials[0];
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
                        Vertex.TangentZ = FVector3f(Normal);
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
            return sizeof(*this) + static_cast<uint32>(GetAllocatedSize());
        }

        SIZE_T GetAllocatedSize() const
        {
            return Sections.GetAllocatedSize() + Materials.GetAllocatedSize();
        }

    private:
        TArray<FRigAxisMeshSection> Sections;
        TArray<FMaterialRenderProxy*> Materials;
        FVector Normal;
        FMaterialRelevance MaterialRelevance;
    };
}

FPrimitiveSceneProxy* CreateRigAxisSceneProxy(
    const URigAxisComponent* Component,
    const TArray<FRigAxisMeshSection>& Sections,
    const TArray<UMaterialInterface*>& Materials,
    const FVector& Normal)
{
    return new FRigAxisSceneProxy(Component, Sections, Materials, Normal);
}
