#include "RigAxisCharacterActor.h"

#include "RigAxisComponent.h"

ARigAxisCharacterActor::ARigAxisCharacterActor()
{
    RigAxis = CreateDefaultSubobject<URigAxisComponent>(TEXT("RigAxis"));
    RootComponent = RigAxis;
}
