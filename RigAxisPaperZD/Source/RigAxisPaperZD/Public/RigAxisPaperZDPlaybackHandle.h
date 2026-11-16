#pragma once

#include "CoreMinimal.h"
#include "AnimSequences/Players/PaperZDPlaybackHandle.h"
#include "RigAxisPaperZDPlaybackHandle.generated.h"

class UPaperZDAnimSequence;
class UPaperZDAnimationSkin;
class UPrimitiveComponent;
struct FPaperZDAnimationPlaybackData;

/// Prototype handle: uses the highest-weight sequence and drives one RigAxis track.
/// Weighted blending, layers, skins, and notifies are not mapped yet.
UCLASS()
class RIGAXISPAPERZD_API URigAxisPaperZDPlaybackHandle : public UPaperZDPlaybackHandle
{
    GENERATED_BODY()

public:
    virtual void ConfigureRenderComponent(UPrimitiveComponent* RenderComponent, bool bIsPreviewPlayback = false) override;
    virtual void UpdateRenderPlayback(
        UPrimitiveComponent* RenderComponent,
        const FPaperZDAnimationPlaybackData& PlaybackData,
        bool bIsPreviewPlayback = false,
        int32 LayerIndex = 0,
        UPaperZDAnimationSkin* SkinOverride = nullptr) override;

private:
    TWeakObjectPtr<const UPaperZDAnimSequence> CurrentSequence;
};
