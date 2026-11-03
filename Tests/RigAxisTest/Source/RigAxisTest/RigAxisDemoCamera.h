#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RigAxisDemoCamera.generated.h"

class UCameraComponent;

UCLASS()
class ARigAxisDemoCamera : public AActor
{
    GENERATED_BODY()

public:
    ARigAxisDemoCamera();
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RigAxis")
    TObjectPtr<UCameraComponent> Camera;
};
