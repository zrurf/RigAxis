#pragma once

#include "CoreMinimal.h"
#include "AnimSequences/PaperZDAnimSequence.h"
#include "RigAxisPaperZDAnimSequence.generated.h"

UCLASS(BlueprintType)
class RIGAXISPAPERZD_API URigAxisPaperZDAnimSequence : public UPaperZDAnimSequence
{
    GENERATED_BODY()

public:
    /// Animation name in the referenced RigAxis skeleton.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RigAxis")
    FName AnimationName;

    /// Playback duration in seconds.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RigAxis", meta = (ClampMin = "0.0"))
    float Duration = 1.0f;

    /// Frame rate reported to the PaperZD editor.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RigAxis", meta = (ClampMin = "0.0"))
    float FramesPerSecond = 30.0f;

    virtual float GetTotalDuration() const override { return Duration; }
    virtual float GetFramesPerSecond() const override { return FramesPerSecond; }
    virtual FName GetSequenceName() const override;
};
