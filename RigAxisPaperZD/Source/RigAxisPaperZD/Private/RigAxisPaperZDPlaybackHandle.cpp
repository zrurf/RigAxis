#include "RigAxisPaperZDPlaybackHandle.h"

#include "AnimSequences/PaperZDAnimSequence.h"
#include "AnimSequences/Players/PaperZDAnimationPlaybackData.h"
#include "RigAxisComponent.h"
#include "RigAxisPaperZDAnimSequence.h"

void URigAxisPaperZDPlaybackHandle::ConfigureRenderComponent(
    UPrimitiveComponent* RenderComponent,
    bool bIsPreviewPlayback)
{
    if (URigAxisComponent* RigAxis = Cast<URigAxisComponent>(RenderComponent))
    {
        RigAxis->UpdateMode = ERigAxisUpdateMode::Manual;
        RigAxis->bAutoPlay = false;
    }
    CurrentSequence.Reset();
}

void URigAxisPaperZDPlaybackHandle::UpdateRenderPlayback(
    UPrimitiveComponent* RenderComponent,
    const FPaperZDAnimationPlaybackData& PlaybackData,
    bool bIsPreviewPlayback,
    int32 LayerIndex,
    UPaperZDAnimationSkin* SkinOverride)
{
    URigAxisComponent* RigAxis = Cast<URigAxisComponent>(RenderComponent);
    if (!RigAxis)
    {
        return;
    }

    const URigAxisPaperZDAnimSequence* Selected = nullptr;
    float PlaybackTime = 0.0f;
    float BestWeight = -1.0f;
    for (const FPaperZDWeightedAnimation& Weighted : PlaybackData.WeightedAnimations)
    {
        const URigAxisPaperZDAnimSequence* Candidate =
            Cast<URigAxisPaperZDAnimSequence>(Weighted.AnimSequencePtr.Get());
        if (!Candidate
            || Candidate->AnimationName.IsNone()
            || Weighted.Weight <= BestWeight)
        {
            continue;
        }

        Selected = Candidate;
        PlaybackTime = Weighted.PlaybackTime;
        BestWeight = Weighted.Weight;
    }

    if (!Selected)
    {
        CurrentSequence.Reset();
        return;
    }

    if (CurrentSequence.Get() != Selected)
    {
        RigAxis->Play(Selected->AnimationName, true, 0.0f);
        CurrentSequence = Selected;
    }
    RigAxis->SetTrackTime(0, PlaybackTime);
}
