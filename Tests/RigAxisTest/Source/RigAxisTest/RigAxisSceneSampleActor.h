#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RigAxisSceneSampleActor.generated.h"

class UCameraComponent;
class URigAxisSceneComponent;

UCLASS()
class ARigAxisSceneSampleActor : public AActor
{
    GENERATED_BODY()

public:
    ARigAxisSceneSampleActor();
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RigAxis")
    TObjectPtr<URigAxisSceneComponent> Scene;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RigAxis")
    TObjectPtr<UCameraComponent> Camera;
};
