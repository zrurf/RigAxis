#include "RigAxisMeshBuilder.h"

#include "RigAxisRenderData.h"

namespace
{
    struct FBuildGroup
    {
        int32 SectionIndex = INDEX_NONE;
        TMap<int32, int32> VertexMap;
    };
}

void FRigAxisMeshBuilder::Build(
    const FRigAxisRenderData& RenderData,
    ERigAxisPlane Plane,
    float PixelsPerUnrealUnit,
    bool bFlipX,
    bool bFlipY,
    TArray<FRigAxisMeshSection>& OutSections)
{
    OutSections.Reset();
    OutSections.Reserve(RenderData.GetBatchCount());

    const float Scale = PixelsPerUnrealUnit > KINDA_SMALL_NUMBER
        ? 1.0f / PixelsPerUnrealUnit
        : 1.0f;
    const bool bReverseWinding = bFlipX == bFlipY;

    for (const FRigAxisRenderBatch& Batch : RenderData.GetBatches())
    {
        TMap<uint32, FBuildGroup> Groups;
        for (int32 Index = 0; Index + 2 < static_cast<int32>(Batch.Indices.Num()); Index += 3)
        {
            const int32 SourceIndices[3] = {
                static_cast<int32>(Batch.Indices[Index]),
                static_cast<int32>(Batch.Indices[Index + 1]),
                static_cast<int32>(Batch.Indices[Index + 2]) };

            if (!Batch.Vertices.IsValidIndex(SourceIndices[0])
                || !Batch.Vertices.IsValidIndex(SourceIndices[1])
                || !Batch.Vertices.IsValidIndex(SourceIndices[2]))
            {
                continue;
            }

            const FLinearColor& Dark = Batch.Vertices[SourceIndices[0]].Dark;
            const uint32 DarkKey = Dark.ToFColor(true).ToPackedARGB();
            FBuildGroup* Group = Groups.Find(DarkKey);
            if (!Group)
            {
                const int32 SectionIndex = OutSections.AddDefaulted();
                FRigAxisMeshSection& NewSection = OutSections[SectionIndex];
                NewSection.Source = Batch.Source;
                NewSection.Page = Batch.Page;
                NewSection.Blend = Batch.Blend;
                NewSection.Dark = Dark;
                Group = &Groups.Add(DarkKey, FBuildGroup{ SectionIndex });
            }

            FRigAxisMeshSection& Section = OutSections[Group->SectionIndex];
            int32 TargetIndices[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
            for (int32 Corner = 0; Corner < 3; ++Corner)
            {
                const int32 SourceIndex = SourceIndices[Corner];
                if (const int32* Existing = Group->VertexMap.Find(SourceIndex))
                {
                    TargetIndices[Corner] = *Existing;
                    continue;
                }

                const FRigAxisVertex& Vertex = Batch.Vertices[SourceIndex];
                const float X = (bFlipX ? -Vertex.Position.X : Vertex.Position.X) * Scale;
                const float Y = (bFlipY ? -Vertex.Position.Y : Vertex.Position.Y) * Scale;
                const int32 TargetIndex = Section.Positions.Num();
                Section.Positions.Add(
                    Plane == ERigAxisPlane::XZ
                        ? FVector(X, 0.0f, Y)
                        : FVector(X, Y, 0.0f));
                Section.UVs.Add(FVector2D(Vertex.UV.X, Vertex.UV.Y));
                Section.Colors.Add(Vertex.Light);
                Group->VertexMap.Add(SourceIndex, TargetIndex);
                TargetIndices[Corner] = TargetIndex;
            }

            Section.Indices.Add(TargetIndices[0]);
            Section.Indices.Add(bReverseWinding ? TargetIndices[2] : TargetIndices[1]);
            Section.Indices.Add(bReverseWinding ? TargetIndices[1] : TargetIndices[2]);
        }
    }
}
