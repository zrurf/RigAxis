#include "RigAxisSceneSampleActor.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RigAxisSceneComponent.h"

ARigAxisSceneSampleActor::ARigAxisSceneSampleActor()
{
    Scene = CreateDefaultSubobject<URigAxisSceneComponent>(TEXT("Scene"));
    RootComponent = Scene;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Scene);
    Camera->SetRelativeLocation(FVector(0.0f, -500.0f, 0.0f));
    Camera->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
    Camera->OrthoWidth = 200.0f;
    Camera->bAutoActivate = true;
}

void ARigAxisSceneSampleActor::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->bAutoManageActiveCameraTarget = false;
        PlayerController->SetViewTarget(this);
    }
}
