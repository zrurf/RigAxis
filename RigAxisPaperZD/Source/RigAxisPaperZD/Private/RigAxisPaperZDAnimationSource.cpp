#include "RigAxisPaperZDAnimationSource.h"

#include "AnimSequences/Players/PaperZDPlaybackHandle.h"
#include "Components/PrimitiveComponent.h"
#include "RigAxisComponent.h"
#include "RigAxisPaperZDAnimSequence.h"
#include "RigAxisPaperZDPlaybackHandle.h"

URigAxisPaperZDAnimationSource::URigAxisPaperZDAnimationSource()
{
    SupportedAnimSequenceClass = URigAxisPaperZDAnimSequence::StaticClass();
    bSupportsBlending = false;
    bSupportsBlendLayers = false;
    bSupportsCompositeAnimationLayers = false;
}

TSubclassOf<UPaperZDPlaybackHandle> URigAxisPaperZDAnimationSource::GetPlaybackHandleClass() const
{
    return URigAxisPaperZDPlaybackHandle::StaticClass();
}

TSubclassOf<UPrimitiveComponent> URigAxisPaperZDAnimationSource::GetRenderComponentClass() const
{
    return URigAxisComponent::StaticClass();
}
