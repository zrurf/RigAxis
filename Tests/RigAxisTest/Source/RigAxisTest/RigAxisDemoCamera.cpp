#include "RigAxisDemoCamera.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ARigAxisDemoCamera::ARigAxisDemoCamera()
{
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    RootComponent = Camera;
    Camera->ProjectionMode = ECameraProjectionMode::Perspective;
    Camera->FieldOfView = 45.0f;
    Camera->bAutoActivate = true;
}

void ARigAxisDemoCamera::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->bAutoManageActiveCameraTarget = false;
        PlayerController->SetViewTarget(this);
    }
}
