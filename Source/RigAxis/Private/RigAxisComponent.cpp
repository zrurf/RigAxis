#include "RigAxisComponent.h"

#include "Misc/App.h"
#include "ProceduralMeshComponent.h"
#include "RigAxisLog.h"
#include "RigAxisMeshBuilder.h"
#include "RigAxisPlayer.h"
#include "RigAxisRenderData.h"
#include "RigAxisSkeletonAsset.h"
#include "RigAxisSkeletonData.h"

struct FRigAxisComponentImpl
{
    TSharedPtr<FRigAxisSkeletonData> Data;
    TUniquePtr<FRigAxisPlayer> Player;
    TUniquePtr<FRigAxisRenderData> RenderData;
    TArray<FRigAxisMeshSection> Sections;
    TWeakObjectPtr<URigAxisSkeletonAsset> BoundAsset;
    int32 AssetVersion = INDEX_NONE;
};

URigAxisComponent::URigAxisComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bTickInEditor = false;
    Impl = new FRigAxisComponentImpl();

    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    SetMobility(EComponentMobility::Movable);

    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(this);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCastShadow(false);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->bUseAsyncCooking = false;
}

URigAxisComponent::~URigAxisComponent()
{
    DestroyPlayer();
    delete Impl;
    Impl = nullptr;
}

void URigAxisComponent::BeginPlay()
{
    Super::BeginPlay();

    EnsurePlayer();

    if (bAutoPlay && !DefaultAnimation.IsNone())
    {
        Play(DefaultAnimation, bLoop, DefaultMix);
    }

    if (Impl && Impl->Player.IsValid() && Impl->RenderData.IsValid() && Impl->Data.IsValid())
    {
        Impl->Player->Apply();
        DispatchEvents();
        if (Impl->RenderData->Build(*Impl->Data, *Impl->Player, Tint))
        {
            UpdateMesh();
        }
    }
}

void URigAxisComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyPlayer();
    Super::EndPlay(EndPlayReason);
}

void URigAxisComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (UpdateMode == ERigAxisUpdateMode::Manual)
    {
        return;
    }

    EnsurePlayer();
    if (!Impl || !Impl->Player.IsValid() || !Impl->RenderData.IsValid() || !Impl->Data.IsValid())
    {
        return;
    }

    const double Seconds = (UpdateMode == ERigAxisUpdateMode::Unscaled
        ? FApp::GetDeltaTime()
        : DeltaTime) * TimeScale;
    if (Seconds <= 0.0)
    {
        return;
    }

    Impl->Player->Update(Seconds);
    Impl->Player->Apply();
    DispatchEvents();
    if (Impl->RenderData->Build(*Impl->Data, *Impl->Player, Tint))
    {
        UpdateMesh();
    }
}

void URigAxisComponent::EnsurePlayer()
{
    if (!Impl)
    {
        return;
    }

    if (!SkeletonAsset)
    {
        DestroyPlayer();
        return;
    }

    const int32 Version = SkeletonAsset->GetVersion();
    if (Impl->Player.IsValid()
        && Impl->BoundAsset.Get() == SkeletonAsset
        && Impl->AssetVersion == Version)
    {
        return;
    }

    DestroyPlayer();

    Impl->Data = SkeletonAsset->GetData();
    if (!Impl->Data.IsValid())
    {
        UE_LOG(LogRigAxis, Error, TEXT("%s: %s"), *GetPathName(), *SkeletonAsset->GetLoadError());
        return;
    }

    FString Error;
    Impl->Player = MakeUnique<FRigAxisPlayer>();
    if (!Impl->Player->Create(Impl->Data, Error))
    {
        UE_LOG(LogRigAxis, Error, TEXT("%s: %s"), *GetPathName(), *Error);
        Impl->Player.Reset();
        Impl->Data.Reset();
        return;
    }

    Impl->RenderData = MakeUnique<FRigAxisRenderData>();
    if (!Impl->RenderData->Create(Error))
    {
        UE_LOG(LogRigAxis, Error, TEXT("%s: %s"), *GetPathName(), *Error);
        Impl->RenderData.Reset();
        Impl->Player.Reset();
        Impl->Data.Reset();
        return;
    }

    Impl->BoundAsset = SkeletonAsset;
    Impl->AssetVersion = SkeletonAsset->GetVersion();
}

void URigAxisComponent::DestroyPlayer()
{
    if (!Impl)
    {
        return;
    }

    Impl->RenderData.Reset();
    Impl->Player.Reset();
    Impl->Data.Reset();
    Impl->Sections.Reset();
    Impl->BoundAsset.Reset();
    Impl->AssetVersion = INDEX_NONE;

    if (Mesh && IsValid(Mesh))
    {
        for (int32 Index = Mesh->GetNumSections() - 1; Index >= 0; --Index)
        {
            Mesh->ClearMeshSection(Index);
        }
    }
}

void URigAxisComponent::UpdateMesh()
{
    if (!Impl || !Mesh || !Impl->RenderData.IsValid())
    {
        return;
    }

    FRigAxisMeshBuilder::Build(
        *Impl->RenderData,
        Plane,
        PixelsPerUnrealUnit,
        bFlipX,
        bFlipY,
        Impl->Sections);

    const FVector Normal = Plane == ERigAxisPlane::XZ
        ? FVector(0.0f, -1.0f, 0.0f)
        : FVector(0.0f, 0.0f, 1.0f);
    TArray<FVector> Normals;
    TArray<FProcMeshTangent> Tangents;

    for (int32 Index = 0; Index < Impl->Sections.Num(); ++Index)
    {
        const FRigAxisMeshSection& Section = Impl->Sections[Index];
        if (Section.Positions.Num() == 0 || Section.Indices.Num() == 0)
        {
            Mesh->ClearMeshSection(Index);
            continue;
        }

        Normals.Init(Normal, Section.Positions.Num());
        Mesh->CreateMeshSection_LinearColor(
            Index,
            Section.Positions,
            Section.Indices,
            Normals,
            Section.UVs,
            Section.Colors,
            Tangents,
            false);
        Mesh->SetMaterial(Index, MeshMaterial);
    }

    for (int32 Index = Impl->Sections.Num(); Index < Mesh->GetNumSections(); ++Index)
    {
        Mesh->ClearMeshSection(Index);
    }
}

void URigAxisComponent::DispatchEvents()
{
    if (!Impl || !Impl->Player.IsValid())
    {
        return;
    }

    const int32 Count = Impl->Player->GetEventCount();
    if (Count <= 0)
    {
        return;
    }

    TArray<FRigAxisEvent> Events;
    Events.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FRigAxisEvent Event;
        if (Impl->Player->GetEvent(Index, Event))
        {
            Events.Add(MoveTemp(Event));
        }
    }
    Impl->Player->ClearEvents();

    for (const FRigAxisEvent& Native : Events)
    {
        switch (Native.Kind)
        {
        case 0:
            OnAnimationStart.Broadcast(this, Native.Animation, static_cast<int32>(Native.Track));
            break;
        case 1:
            OnAnimationEnd.Broadcast(this, Native.Animation, static_cast<int32>(Native.Track));
            break;
        case 2:
            OnAnimationComplete.Broadcast(this, Native.Animation, static_cast<int32>(Native.Track));
            break;
        case 3:
        {
            FRigAxisAnimationEvent Event;
            Event.Name = Native.Name;
            Event.Animation = Native.Animation;
            Event.Track = static_cast<int32>(Native.Track);
            Event.Time = Native.Time;
            Event.LoopIndex = Native.LoopIndex;
            Event.IntValue = Native.IntValue;
            Event.FloatValue = Native.FloatValue;
            Event.StringValue = Native.StringValue;
            Event.Audio = Native.Audio;
            Event.Volume = Native.Volume;
            Event.Balance = Native.Balance;
            OnAnimationEvent.Broadcast(this, Event);
            break;
        }
        default:
            break;
        }
    }
}

bool URigAxisComponent::Play(FName Animation, bool bInLoop, float MixDuration)
{
    EnsurePlayer();
    return Impl && Impl->Player.IsValid() && Impl->Player->Play(Animation, bInLoop, MixDuration);
}

bool URigAxisComponent::SetAnimation(
    int32 Track,
    FName Animation,
    bool bInLoop,
    float MixDuration)
{
    EnsurePlayer();
    return Impl && Impl->Player.IsValid()
        && Impl->Player->SetAnimation(Track, Animation, bInLoop, MixDuration);
}

bool URigAxisComponent::QueueAnimation(
    int32 Track,
    FName Animation,
    bool bInLoop,
    float MixDuration,
    float Delay)
{
    EnsurePlayer();
    return Impl && Impl->Player.IsValid()
        && Impl->Player->QueueAnimation(Track, Animation, bInLoop, MixDuration, Delay);
}

void URigAxisComponent::ClearTrack(int32 Track)
{
    EnsurePlayer();
    if (Impl && Impl->Player.IsValid())
    {
        Impl->Player->ClearTrack(Track);
    }
}

void URigAxisComponent::Stop()
{
    EnsurePlayer();
    if (Impl && Impl->Player.IsValid())
    {
        Impl->Player->Stop();
    }
}

void URigAxisComponent::SetSpeed(float Speed)
{
    EnsurePlayer();
    if (Impl && Impl->Player.IsValid())
    {
        Impl->Player->SetSpeed(Speed);
    }
}

void URigAxisComponent::SetTrackTime(int32 Track, float Time)
{
    EnsurePlayer();
    if (Impl && Impl->Player.IsValid())
    {
        Impl->Player->SetTrackTime(Track, Time);
    }
}

void URigAxisComponent::Advance(float DeltaSeconds)
{
    EnsurePlayer();
    if (!Impl || !Impl->Player.IsValid() || !Impl->RenderData.IsValid() || !Impl->Data.IsValid())
    {
        return;
    }

    const double Seconds = static_cast<double>(DeltaSeconds) * TimeScale;
    if (Seconds <= 0.0)
    {
        return;
    }

    Impl->Player->Update(Seconds);
    Impl->Player->Apply();
    DispatchEvents();
    if (Impl->RenderData->Build(*Impl->Data, *Impl->Player, Tint))
    {
        UpdateMesh();
    }
}

bool URigAxisComponent::SetSkin(FName Skin)
{
    EnsurePlayer();
    if (!Impl || !Impl->Player.IsValid() || !Impl->Player->SetSkin(Skin))
    {
        return false;
    }

    Impl->Player->Apply();
    if (Impl->RenderData.IsValid()
        && Impl->Data.IsValid()
        && Impl->RenderData->Build(*Impl->Data, *Impl->Player, Tint))
    {
        UpdateMesh();
    }
    return true;
}

void URigAxisComponent::ResetPhysics()
{
    EnsurePlayer();
    if (Impl && Impl->Player.IsValid())
    {
        Impl->Player->ResetPhysics();
    }
}

FVector URigAxisComponent::GetBoneWorldPosition(FName Bone)
{
    EnsurePlayer();
    if (!Impl || !Impl->Player.IsValid() || !Impl->Data.IsValid())
    {
        return GetComponentLocation();
    }

    RigAxisAffine Affine = {};
    if (!Impl->Player->GetBoneWorldTransform(Bone, Affine))
    {
        return GetComponentLocation();
    }

    const float Scale = PixelsPerUnrealUnit > KINDA_SMALL_NUMBER
        ? 1.0f / PixelsPerUnrealUnit
        : 1.0f;
    const float X = static_cast<float>(bFlipX ? -Affine.tx : Affine.tx) * Scale;
    const float Y = static_cast<float>(bFlipY ? -Affine.ty : Affine.ty) * Scale;
    const FVector Local = Plane == ERigAxisPlane::XZ
        ? FVector(X, 0.0f, Y)
        : FVector(X, Y, 0.0f);
    return GetComponentTransform().TransformPosition(Local);
}
