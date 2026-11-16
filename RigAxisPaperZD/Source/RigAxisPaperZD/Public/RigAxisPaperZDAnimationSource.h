#pragma once

#include "CoreMinimal.h"
#include "AnimSequences/Sources/PaperZDAnimationSource.h"
#include "RigAxisPaperZDAnimationSource.generated.h"

class UPaperZDPlaybackHandle;
class UPrimitiveComponent;

UCLASS(BlueprintType)
class RIGAXISPAPERZD_API URigAxisPaperZDAnimationSource : public UPaperZDAnimationSource
{
    GENERATED_BODY()

public:
    URigAxisPaperZDAnimationSource();

    virtual TSubclassOf<UPaperZDPlaybackHandle> GetPlaybackHandleClass() const override;
    virtual TSubclassOf<UPrimitiveComponent> GetRenderComponentClass() const override;
};
