#include "RigAxisMeshBuilder.h"

#include "RigAxisRenderData.h"

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

    for (const FRigAxisRenderBatch& Batch : RenderData.GetBatches())
    {
        FRigAxisMeshSection& Section = OutSections.AddDefaulted_GetRef();
        Section.Source = Batch.Source;
        Section.Page = Batch.Page;
        Section.Blend = Batch.Blend;

        Section.Positions.Reserve(Batch.Vertices.Num());
        Section.UVs.Reserve(Batch.Vertices.Num());
        Section.Colors.Reserve(Batch.Vertices.Num());
        for (const FRigAxisVertex& Vertex : Batch.Vertices)
        {
            const float X = (bFlipX ? -Vertex.Position.X : Vertex.Position.X) * Scale;
            const float Y = (bFlipY ? -Vertex.Position.Y : Vertex.Position.Y) * Scale;

            Section.Positions.Add(
                Plane == ERigAxisPlane::XZ
                    ? FVector(X, 0.0f, Y)
                    : FVector(X, Y, 0.0f));
            Section.UVs.Add(FVector2D(Vertex.UV.X, Vertex.UV.Y));
            Section.Colors.Add(Vertex.Light);
        }

        const bool bReverseWinding = bFlipX == bFlipY;
        Section.Indices.Reserve(Batch.Indices.Num());
        for (int32 Index = 0; Index + 2 < static_cast<int32>(Batch.Indices.Num()); Index += 3)
        {
            const int32 A = static_cast<int32>(Batch.Indices[Index]);
            const int32 B = static_cast<int32>(Batch.Indices[Index + 1]);
            const int32 C = static_cast<int32>(Batch.Indices[Index + 2]);
            Section.Indices.Add(A);
            Section.Indices.Add(bReverseWinding ? C : B);
            Section.Indices.Add(bReverseWinding ? B : C);
        }
    }
}
