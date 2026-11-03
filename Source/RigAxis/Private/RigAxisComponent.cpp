#include "RigAxisComponent.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "RigAxisLog.h"
#include "RigAxisMeshBuilder.h"
#include "RigAxisPlayer.h"
#include "RigAxisRenderData.h"
#include "RigAxisSceneProxy.h"
#include "RigAxisSkeletonAsset.h"
#include "RigAxisSkeletonData.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    UMaterialInterface* SelectBlendMaterial(
        const FRigAxisMaterialSet& Set,
        const FRigAxisMaterialSet& Defaults,
        UMaterialInterface* Fallback,
        ERigAxisBlend Blend)
    {
        UMaterialInterface* Material = nullptr;
        UMaterialInterface* Default = nullptr;
        switch (Blend)
        {
        case ERigAxisBlend::Normal:
            Material = Set.Normal;
            Default = Defaults.Normal;
            break;
        case ERigAxisBlend::Additive:
            Material = Set.Additive;
            Default = Defaults.Additive;
            break;
        case ERigAxisBlend::Multiply:
            Material = Set.Multiply;
            Default = Defaults.Multiply;
            break;
        case ERigAxisBlend::Screen:
            Material = Set.Screen;
            Default = Defaults.Screen;
            break;
        }
        return Material ? Material : (Fallback ? Fallback : Default);
    }
}

struct FRigAxisComponentImpl
{
    TSharedPtr<FRigAxisSkeletonData> Data;
    TUniquePtr<FRigAxisPlayer> Player;
    TUniquePtr<FRigAxisRenderData> RenderData;
    TArray<FRigAxisMeshSection> Sections;
    TSharedPtr<FRigAxisSceneState, ESPMode::ThreadSafe> SceneState;
    TWeakObjectPtr<URigAxisSkeletonAsset> BoundAsset;
    TWeakObjectPtr<UMaterialInterface> BoundMaterial;
    int32 AssetVersion = INDEX_NONE;
    int32 BoundSectionCount = INDEX_NONE;
};

URigAxisComponent::URigAxisComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bTickInEditor = false;
    Impl = new FRigAxisComponentImpl();
    Impl->SceneState = MakeShared<FRigAxisSceneState, ESPMode::ThreadSafe>();

    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCastShadow(false);
    SetMobility(EComponentMobility::Movable);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> NormalMaterial(
        TEXT("/RigAxis/Materials/M_RigAxisNormal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> AdditiveMaterial(
        TEXT("/RigAxis/Materials/M_RigAxisAdditive"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> MultiplyMaterial(
        TEXT("/RigAxis/Materials/M_RigAxisMultiply"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ScreenMaterial(
        TEXT("/RigAxis/Materials/M_RigAxisScreen"));
    DefaultMaterials.Normal = NormalMaterial.Object;
    DefaultMaterials.Additive = AdditiveMaterial.Object;
    DefaultMaterials.Multiply = MultiplyMaterial.Object;
    DefaultMaterials.Screen = ScreenMaterial.Object;
}

URigAxisComponent::~URigAxisComponent()
{
    if (Impl)
    {
        Impl->RenderData.Reset();
        Impl->Player.Reset();
        Impl->Data.Reset();
        delete Impl;
        Impl = nullptr;
    }
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

FPrimitiveSceneProxy* URigAxisComponent::CreateSceneProxy()
{
    if (!Impl || Impl->Sections.Num() == 0)
    {
        return nullptr;
    }

    TArray<UMaterialInterface*> Materials;
    Materials.Reserve(PageMaterials.Num());
    bool bHasMaterial = false;
    for (const TObjectPtr<UMaterialInstanceDynamic>& Material : PageMaterials)
    {
        Materials.Add(Material);
        bHasMaterial |= Material != nullptr;
    }
    if (!bHasMaterial)
    {
        return nullptr;
    }

    const FVector Normal = Plane == ERigAxisPlane::XZ
        ? FVector(0.0f, -1.0f, 0.0f)
        : FVector(0.0f, 0.0f, 1.0f);
    {
        FScopeLock Lock(&Impl->SceneState->Lock);
        Impl->SceneState->Sections = Impl->Sections;
        Impl->SceneState->Normal = Normal;
    }
    return CreateRigAxisSceneProxy(this, Impl->SceneState.ToSharedRef(), Materials);
}

FBoxSphereBounds URigAxisComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    FBox Box(ForceInit);
    if (Impl)
    {
        for (const FRigAxisMeshSection& Section : Impl->Sections)
        {
            for (const FVector& Position : Section.Positions)
            {
                Box += Position;
            }
        }
    }

    if (Box.IsValid)
    {
        return FBoxSphereBounds(Box).TransformBy(LocalToWorld);
    }

    return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0f);
}

int32 URigAxisComponent::GetNumMaterials() const
{
    return PageMaterials.Num();
}

UMaterialInterface* URigAxisComponent::GetMaterial(int32 ElementIndex) const
{
    return PageMaterials.IsValidIndex(ElementIndex) ? PageMaterials[ElementIndex] : nullptr;
}

void URigAxisComponent::GetUsedMaterials(
    TArray<UMaterialInterface*>& OutMaterials,
    bool bGetDebugMaterials) const
{
    for (const TObjectPtr<UMaterialInstanceDynamic>& Material : PageMaterials)
    {
        if (Material)
        {
            OutMaterials.Add(Material);
        }
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
    BuildPageMaterials();
    Impl->BoundMaterial = MeshMaterial;
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
    {
        FScopeLock Lock(&Impl->SceneState->Lock);
        Impl->SceneState->Sections.Reset();
    }
    Impl->BoundAsset.Reset();
    Impl->BoundMaterial.Reset();
    Impl->AssetVersion = INDEX_NONE;
    Impl->BoundSectionCount = INDEX_NONE;
    PageMaterials.Reset();
    MarkRenderStateDirty();
}

void URigAxisComponent::BuildPageMaterials()
{
    PageMaterials.Reset();
    if (!SkeletonAsset)
    {
        return;
    }

    const int32 PageCount = SkeletonAsset->AtlasPages.Num();
    PageMaterials.Reserve(PageCount * 4);
    for (int32 Page = 0; Page < PageCount; ++Page)
    {
        for (int32 Blend = 0; Blend < 4; ++Blend)
        {
            UMaterialInterface* Base = SelectBlendMaterial(
                MaterialSet,
                DefaultMaterials,
                MeshMaterial,
                static_cast<ERigAxisBlend>(Blend));
            UMaterialInstanceDynamic* Instance = Base
                ? UMaterialInstanceDynamic::Create(Base, this)
                : nullptr;
            if (Instance)
            {
                if (SkeletonAsset->AtlasPages[Page])
                {
                    Instance->SetTextureParameterValue(
                        TEXT("SpriteTexture"),
                        SkeletonAsset->AtlasPages[Page]);
                }
                if (SkeletonAsset->bPremultipliedAlpha)
                {
                    Instance->SetScalarParameterValue(TEXT("UsePMA"), 1.0f);
                }
            }
            PageMaterials.Add(Instance);
        }
    }
}

void URigAxisComponent::UpdateMesh()
{
    if (!Impl || !Impl->RenderData.IsValid())
    {
        return;
    }

    const int32 PageCount = SkeletonAsset ? SkeletonAsset->AtlasPages.Num() : 0;
    const bool bMaterialsChanged = Impl->BoundMaterial.Get() != MeshMaterial.Get()
        || PageMaterials.Num() != PageCount * 4;
    if (bMaterialsChanged)
    {
        BuildPageMaterials();
        Impl->BoundMaterial = MeshMaterial;
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
    {
        FScopeLock Lock(&Impl->SceneState->Lock);
        Impl->SceneState->Sections = Impl->Sections;
        Impl->SceneState->Normal = Normal;
    }

    if (bMaterialsChanged || Impl->Sections.Num() != Impl->BoundSectionCount)
    {
        Impl->BoundSectionCount = Impl->Sections.Num();
        MarkRenderStateDirty();
    }
    UpdateBounds();
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
    FTransform Transform;
    if (GetBoneWorldTransform(Bone, Transform))
    {
        return Transform.GetLocation();
    }
    return GetComponentLocation();
}

bool URigAxisComponent::GetBoneWorldTransform(FName Bone, FTransform& OutTransform)
{
    EnsurePlayer();
    if (!Impl || !Impl->Player.IsValid() || !Impl->Data.IsValid())
    {
        return false;
    }

    RigAxisAffine Affine = {};
    if (!Impl->Player->GetBoneWorldTransform(Bone, Affine))
    {
        return false;
    }

    const float Scale = PixelsPerUnrealUnit > KINDA_SMALL_NUMBER
        ? 1.0f / PixelsPerUnrealUnit
        : 1.0f;
    const double Rotation = FMath::RadiansToDegrees(FMath::Atan2(Affine.b, Affine.a));
    const double ScaleX = FMath::Sqrt(Affine.a * Affine.a + Affine.b * Affine.b);
    const double ScaleY = FMath::Sqrt(Affine.c * Affine.c + Affine.d * Affine.d)
        * ((Affine.a * Affine.d - Affine.b * Affine.c) < 0.0 ? -1.0 : 1.0);
    const double X = Affine.tx * Scale * (bFlipX ? -1.0 : 1.0);
    const double Y = Affine.ty * Scale * (bFlipY ? -1.0 : 1.0);

    FTransform Local;
    if (Plane == ERigAxisPlane::XZ)
    {
        Local = FTransform(
            FRotator(static_cast<float>(Rotation), 0.0f, 0.0f),
            FVector(static_cast<float>(X), 0.0f, static_cast<float>(Y)),
            FVector(static_cast<float>(ScaleX), 1.0f, static_cast<float>(ScaleY)));
    }
    else
    {
        Local = FTransform(
            FRotator(0.0f, static_cast<float>(Rotation), 0.0f),
            FVector(static_cast<float>(X), static_cast<float>(Y), 0.0f),
            FVector(static_cast<float>(ScaleX), static_cast<float>(ScaleY), 1.0f));
    }

    OutTransform = Local * GetComponentTransform();
    return true;
}
