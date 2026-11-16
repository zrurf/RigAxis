#include "RigAxisPaperZDAnimSequence.h"

FName URigAxisPaperZDAnimSequence::GetSequenceName() const
{
    return AnimationName.IsNone() ? Super::GetSequenceName() : AnimationName;
}
