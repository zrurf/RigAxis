#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RigAxisCharacterActor.generated.h"

class URigAxisComponent;

UCLASS()
class ARigAxisCharacterActor : public AActor
{
    GENERATED_BODY()

public:
    ARigAxisCharacterActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RigAxis")
    TObjectPtr<URigAxisComponent> RigAxis;
};
