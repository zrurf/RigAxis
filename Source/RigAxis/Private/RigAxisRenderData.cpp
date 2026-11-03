#include "RigAxisRenderData.h"

#include "RigAxisPlayer.h"
#include "RigAxisSkeletonData.h"

namespace
{
    FString LastError()
    {
        const char* Text = rigaxis_last_error();
        return Text ? FString(UTF8_TO_TCHAR(Text)) : FString();
    }
}

FRigAxisRenderData::~FRigAxisRenderData()
{
    Destroy();
}

bool FRigAxisRenderData::Create(FString& OutError)
{
    check(IsInGameThread());
    Destroy();

    RigAxisStatus Status = rigaxis_render_create(&Render);
    if (Status != RIGAXIS_OK)
    {
        OutError = LastError();
        Render = nullptr;
        return false;
    }

    return true;
}

void FRigAxisRenderData::Destroy()
{
    if (Render)
    {
        rigaxis_render_free(Render);
        Render = nullptr;
    }
    Batches.Reset();
}

bool FRigAxisRenderData::Build(
    const FRigAxisSkeletonData& Data,
    const FRigAxisPlayer& Player,
    const FLinearColor& Tint)
{
    if (!IsValid() || !Player.IsValid())
    {
        return false;
    }

    const float TintArray[4] = { Tint.R, Tint.G, Tint.B, Tint.A };
    const RigAxisStatus Status = rigaxis_render_build(
        Render,
        Data.GetSkeleton(),
        Player.GetPose(),
        Data.GetLookup(),
        TintArray);
    if (Status != RIGAXIS_OK)
    {
        return false;
    }

    const uint32 Count = rigaxis_render_batch_count(Render);
    Batches.SetNum(static_cast<int32>(Count));
    for (uint32 BatchIndex = 0; BatchIndex < Count; ++BatchIndex)
    {
        RigAxisBatch NativeBatch = {};
        if (!rigaxis_render_batch_at(Render, BatchIndex, &NativeBatch))
        {
            return false;
        }

        FRigAxisRenderBatch& Batch = Batches[BatchIndex];
        Batch.Source = NativeBatch.source;
        Batch.Page = NativeBatch.page;
        Batch.Blend = static_cast<ERigAxisBlend>(NativeBatch.blend);

        Batch.Vertices.SetNumUninitialized(static_cast<int32>(NativeBatch.vertex_count));
        for (uint32 Index = 0; Index < NativeBatch.vertex_count; ++Index)
        {
            const RigAxisVertex& Source = NativeBatch.vertices[Index];
            FRigAxisVertex& Target = Batch.Vertices[Index];
            Target.Position = FVector2f(Source.x, Source.y);
            Target.UV = FVector2f(Source.u, Source.v);
            Target.Light = FLinearColor(Source.light_r, Source.light_g, Source.light_b, Source.light_a);
            Target.Dark = FLinearColor(Source.dark_r, Source.dark_g, Source.dark_b, Source.dark_a);
        }

        Batch.Indices.SetNumUninitialized(static_cast<int32>(NativeBatch.index_count));
        if (NativeBatch.index_count > 0)
        {
            FMemory::Memcpy(
                Batch.Indices.GetData(),
                NativeBatch.indices,
                NativeBatch.index_count * sizeof(uint32));
        }
    }

    return true;
}
