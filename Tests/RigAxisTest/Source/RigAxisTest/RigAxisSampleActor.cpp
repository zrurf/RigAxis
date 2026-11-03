#include "RigAxisSampleActor.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "RigAxisComponent.h"

ARigAxisSampleActor::ARigAxisSampleActor()
{
    RigAxis = CreateDefaultSubobject<URigAxisComponent>(TEXT("RigAxis"));
    RootComponent = RigAxis;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RigAxis);
    Camera->SetRelativeLocation(FVector(0.0f, -500.0f, 0.0f));
    Camera->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
    Camera->OrthoWidth = 200.0f;
    Camera->bAutoActivate = true;
}

void ARigAxisSampleActor::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
    {
        PlayerController->bAutoManageActiveCameraTarget = false;
        PlayerController->SetViewTarget(this);
    }
}
