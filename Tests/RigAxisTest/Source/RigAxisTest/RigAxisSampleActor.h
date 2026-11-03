#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RigAxisSampleActor.generated.h"

class UCameraComponent;
class URigAxisComponent;

UCLASS()
class ARigAxisSampleActor : public AActor
{
    GENERATED_BODY()

public:
    ARigAxisSampleActor();
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RigAxis")
    TObjectPtr<URigAxisComponent> RigAxis;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RigAxis")
    TObjectPtr<UCameraComponent> Camera;
};
