#include "RigAxisSceneComponent.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/Crc.h"
#include "RigAxisLog.h"
#include "RigAxisMeshBuilder.h"
#include "RigAxisRenderData.h"
#include "RigAxisSceneAsset.h"
#include "RigAxisScenePlayer.h"
#include "RigAxisSceneProxy.h"
#include "RigAxisSkeletonAsset.h"
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

struct FRigAxisSceneComponentImpl
{
    TUniquePtr<FRigAxisScenePlayer> Player;
    TUniquePtr<FRigAxisRenderData> RenderData;
    TArray<FRigAxisMeshSection> Sections;
    TSharedPtr<FRigAxisSceneState, ESPMode::ThreadSafe> SceneState;
    TWeakObjectPtr<URigAxisSceneAsset> BoundAsset;
    uint32 SceneVersion = 0;
    int32 BoundSectionCount = INDEX_NONE;
    bool bPlaying = false;
    bool bLooping = false;
    bool bShotFinished = false;
    bool bBuildErrorLogged = false;
};

URigAxisSceneComponent::URigAxisSceneComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bTickInEditor = false;
    Impl = new FRigAxisSceneComponentImpl();
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

URigAxisSceneComponent::~URigAxisSceneComponent()
{
    if (Impl)
    {
        Impl->RenderData.Reset();
        Impl->Player.Reset();
        delete Impl;
        Impl = nullptr;
    }
}

void URigAxisSceneComponent::BeginPlay()
{
    Super::BeginPlay();

    EnsureScene();
    if (Impl && Impl->Player.IsValid())
    {
        if (bAutoPlay && !DefaultShot.IsNone())
        {
            PlayShot(DefaultShot, bLoop);
        }
        else if (bAutoPlay)
        {
            Impl->bPlaying = true;
            Impl->bLooping = bLoop;
            Impl->bShotFinished = false;
        }
        UpdateScene();
    }
}

void URigAxisSceneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyScene();
    Super::EndPlay(EndPlayReason);
}

void URigAxisSceneComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    EnsureScene();
    if (!Impl || !Impl->Player.IsValid() || UpdateMode == ERigAxisUpdateMode::Manual)
    {
        return;
    }

    if (Impl->bPlaying)
    {
        const double Seconds = (UpdateMode == ERigAxisUpdateMode::Unscaled
            ? FApp::GetDeltaTime()
            : DeltaTime) * TimeScale;
        if (Seconds > 0.0)
        {
            Impl->Player->Update(Seconds);
        }
    }

    UpdateScene();
}

void URigAxisSceneComponent::EnsureScene()
{
    if (!Impl)
    {
        return;
    }

    if (!SceneAsset)
    {
        DestroyScene();
        return;
    }

    uint32 Version = SceneAsset->ManifestBytes.Num() > 0
        ? FCrc::MemCrc32(SceneAsset->ManifestBytes.GetData(), SceneAsset->ManifestBytes.Num())
        : 0;
    Version = HashCombine(Version, GetTypeHash(SceneAsset->StagePath));
    if (SceneAsset->Stage)
    {
        Version = HashCombine(Version, GetTypeHash(SceneAsset->Stage.Get()));
        Version = HashCombine(Version, GetTypeHash(SceneAsset->Stage->GetVersion()));
    }
    for (const TPair<FString, TObjectPtr<URigAxisSkeletonAsset>>& Pair : SceneAsset->ActorSkeletons)
    {
        Version = HashCombine(Version, GetTypeHash(Pair.Key));
        if (Pair.Value)
        {
            Version = HashCombine(Version, GetTypeHash(Pair.Value.Get()));
            Version = HashCombine(Version, GetTypeHash(Pair.Value->GetVersion()));
        }
        if (const FString* Path = SceneAsset->ActorSkeletonPaths.Find(Pair.Key))
        {
            Version = HashCombine(Version, GetTypeHash(*Path));
        }
    }

    if (Impl->BoundAsset.Get() == SceneAsset
        && Impl->SceneVersion == Version)
    {
        return;
    }

    DestroyScene();

    FString Error;
    Impl->Player = MakeUnique<FRigAxisScenePlayer>();
    if (!Impl->Player->Create(*SceneAsset, Error))
    {
        UE_LOG(LogRigAxis, Error, TEXT("%s: %s"), *GetPathName(), *Error);
        Impl->Player.Reset();
        Impl->BoundAsset = SceneAsset;
        Impl->SceneVersion = Version;
        return;
    }

    Impl->RenderData = MakeUnique<FRigAxisRenderData>();
    if (!Impl->RenderData->Create(Error))
    {
        UE_LOG(LogRigAxis, Error, TEXT("%s: %s"), *GetPathName(), *Error);
        Impl->RenderData.Reset();
        Impl->Player.Reset();
        Impl->BoundAsset = SceneAsset;
        Impl->SceneVersion = Version;
        return;
    }

    Impl->BoundAsset = SceneAsset;
    Impl->SceneVersion = Version;
    Impl->bShotFinished = false;
    Impl->bBuildErrorLogged = false;
}

void URigAxisSceneComponent::DestroyScene()
{
    if (!Impl)
    {
        return;
    }
    if (!Impl->Player.IsValid()
        && !Impl->RenderData.IsValid()
        && Impl->Sections.Num() == 0
        && SectionMaterials.Num() == 0)
    {
        Impl->BoundAsset.Reset();
        Impl->SceneVersion = 0;
        return;
    }

    Impl->RenderData.Reset();
    Impl->Player.Reset();
    Impl->Sections.Reset();
    {
        FScopeLock Lock(&Impl->SceneState->Lock);
        Impl->SceneState->Sections.Reset();
    }
    Impl->BoundAsset.Reset();
    Impl->SceneVersion = 0;
    Impl->BoundSectionCount = INDEX_NONE;
    Impl->bPlaying = false;
    Impl->bLooping = false;
    Impl->bShotFinished = false;
    Impl->bBuildErrorLogged = false;

    SectionMaterials.Reset();
    BaseMaterials.Reset();
    BaseMaterialIndices.Reset();
    DarkMaterials.Reset();
    DarkMaterialIndices.Reset();
    BoundMaterialSignature = 0;
    BoundMaterialSetSignature = 0;
    MarkRenderStateDirty();
}

void URigAxisSceneComponent::UpdateScene()
{
    if (!Impl || !Impl->Player.IsValid() || !Impl->RenderData.IsValid())
    {
        return;
    }

    FString BuildError;
    if (!Impl->RenderData->BuildScene(Impl->Player->GetScene(), BuildError))
    {
        if (!Impl->bBuildErrorLogged)
        {
            Impl->bBuildErrorLogged = true;
            UE_LOG(LogRigAxis, Error, TEXT("%s: %s"), *GetPathName(), *BuildError);
        }
        return;
    }
    Impl->bBuildErrorLogged = false;

    FRigAxisMeshBuilder::Build(
        *Impl->RenderData,
        Plane,
        PixelsPerUnrealUnit,
        false,
        false,
        Impl->Sections);

    const FVector Normal = Plane == ERigAxisPlane::XZ
        ? FVector(0.0f, -1.0f, 0.0f)
        : FVector(0.0f, 0.0f, 1.0f);
    {
        FScopeLock Lock(&Impl->SceneState->Lock);
        Impl->SceneState->Sections = Impl->Sections;
        Impl->SceneState->Normal = Normal;
    }

    uint32 SetSignature = GetTypeHash(MeshMaterial.Get());
    SetSignature = HashCombine(SetSignature, GetTypeHash(MaterialSet.Normal.Get()));
    SetSignature = HashCombine(SetSignature, GetTypeHash(MaterialSet.Additive.Get()));
    SetSignature = HashCombine(SetSignature, GetTypeHash(MaterialSet.Multiply.Get()));
    SetSignature = HashCombine(SetSignature, GetTypeHash(MaterialSet.Screen.Get()));
    if (SetSignature != BoundMaterialSetSignature)
    {
        BoundMaterialSetSignature = SetSignature;
        BaseMaterials.Reset();
        BaseMaterialIndices.Reset();
        DarkMaterials.Reset();
        DarkMaterialIndices.Reset();
        SectionMaterials.Reset();
    }

    RebuildSectionMaterials();

    uint32 Signature = GetTypeHash(Impl->Sections.Num());
    for (const TObjectPtr<UMaterialInstanceDynamic>& Material : SectionMaterials)
    {
        Signature = HashCombine(Signature, GetTypeHash(Material.Get()));
    }
    const bool bSignatureChanged = Signature != BoundMaterialSignature;
    BoundMaterialSignature = Signature;

    if (bSignatureChanged || Impl->Sections.Num() != Impl->BoundSectionCount)
    {
        Impl->BoundSectionCount = Impl->Sections.Num();
        MarkRenderStateDirty();
    }
    UpdateBounds();

    if (Impl->bPlaying && !Impl->bLooping && !Impl->bShotFinished)
    {
        const double Duration = Impl->Player->GetDuration();
        if (Duration > 0.0 && Impl->Player->GetTime() >= Duration - KINDA_SMALL_NUMBER)
        {
            Impl->bShotFinished = true;
            OnShotFinished.Broadcast(this, FName(*Impl->Player->GetCurrentShot()));
        }
    }
}

URigAxisSkeletonAsset* URigAxisSceneComponent::FindSourceSkeleton(uint32 Source) const
{
    if (!SceneAsset)
    {
        return nullptr;
    }
    if (Source == 0)
    {
        return SceneAsset->Stage;
    }
    if (!Impl || !Impl->Player.IsValid())
    {
        return nullptr;
    }
    return SceneAsset->FindActorSkeleton(Impl->Player->GetSourceId(Source));
}

void URigAxisSceneComponent::RebuildSectionMaterials()
{
    SectionMaterials.Reset();
    SectionMaterials.Reserve(Impl->Sections.Num());

    for (const FRigAxisMeshSection& Section : Impl->Sections)
    {
        URigAxisSkeletonAsset* Skeleton = FindSourceSkeleton(Section.Source);
        if (!Skeleton)
        {
            SectionMaterials.Add(nullptr);
            continue;
        }

        const int32 Page = static_cast<int32>(Section.Page);
        const int32 Blend = static_cast<int32>(Section.Blend);
        const uint64 BaseKey = (static_cast<uint64>(Section.Source) << 56)
            | (static_cast<uint64>(Page) << 40)
            | (static_cast<uint64>(Blend) << 36);

        auto CreateInstance = [this, Skeleton, Page, &Section]() -> UMaterialInstanceDynamic*
        {
            UMaterialInterface* Base = SelectMaterial(Section.Blend);
            if (!Base)
            {
                return nullptr;
            }
            UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, this);
            if (Skeleton->AtlasPages.IsValidIndex(Page) && Skeleton->AtlasPages[Page])
            {
                Instance->SetTextureParameterValue(TEXT("SpriteTexture"), Skeleton->AtlasPages[Page]);
            }
            if (Skeleton->bPremultipliedAlpha)
            {
                Instance->SetScalarParameterValue(TEXT("UsePMA"), 1.0f);
            }
            return Instance;
        };

        UMaterialInstanceDynamic* Material = nullptr;
        if (Section.Dark.A <= KINDA_SMALL_NUMBER)
        {
            if (const int32* Existing = BaseMaterialIndices.Find(BaseKey))
            {
                if (BaseMaterials.IsValidIndex(*Existing))
                {
                    Material = BaseMaterials[*Existing];
                }
            }
            if (!Material)
            {
                Material = CreateInstance();
                if (Material)
                {
                    BaseMaterialIndices.Add(BaseKey, BaseMaterials.Add(Material));
                }
            }
        }
        else
        {
            const uint64 Key = BaseKey
                | static_cast<uint64>(Section.Dark.ToFColor(true).ToPackedARGB());
            if (const int32* Existing = DarkMaterialIndices.Find(Key))
            {
                if (DarkMaterials.IsValidIndex(*Existing))
                {
                    Material = DarkMaterials[*Existing];
                }
            }
            if (!Material)
            {
                Material = CreateInstance();
                if (Material)
                {
                    Material->SetVectorParameterValue(TEXT("DarkColor"), Section.Dark);
                    DarkMaterialIndices.Add(Key, DarkMaterials.Add(Material));
                }
            }
        }
        SectionMaterials.Add(Material);
    }
}

UMaterialInterface* URigAxisSceneComponent::SelectMaterial(ERigAxisBlend Blend) const
{
    return SelectBlendMaterial(MaterialSet, DefaultMaterials, MeshMaterial, Blend);
}

FPrimitiveSceneProxy* URigAxisSceneComponent::CreateSceneProxy()
{
    if (!Impl || Impl->Sections.Num() == 0 || SectionMaterials.Num() != Impl->Sections.Num())
    {
        return nullptr;
    }

    TArray<UMaterialInterface*> Materials;
    Materials.Reserve(SectionMaterials.Num());
    bool bHasMaterial = false;
    for (const TObjectPtr<UMaterialInstanceDynamic>& Material : SectionMaterials)
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

FBoxSphereBounds URigAxisSceneComponent::CalcBounds(const FTransform& LocalToWorld) const
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

int32 URigAxisSceneComponent::GetNumMaterials() const
{
    return SectionMaterials.Num();
}

UMaterialInterface* URigAxisSceneComponent::GetMaterial(int32 ElementIndex) const
{
    return SectionMaterials.IsValidIndex(ElementIndex) ? SectionMaterials[ElementIndex] : nullptr;
}

void URigAxisSceneComponent::GetUsedMaterials(
    TArray<UMaterialInterface*>& OutMaterials,
    bool bGetDebugMaterials) const
{
    for (const TObjectPtr<UMaterialInstanceDynamic>& Material : SectionMaterials)
    {
        if (Material)
        {
            OutMaterials.Add(Material);
        }
    }
}

void URigAxisSceneComponent::PlayShot(FName Shot, bool bInLoop)
{
    EnsureScene();
    if (!Impl || !Impl->Player.IsValid())
    {
        return;
    }

    Impl->Player->Play(Shot.ToString(), bInLoop);
    Impl->bPlaying = true;
    Impl->bLooping = bInLoop;
    Impl->bShotFinished = false;
    UpdateScene();
}

void URigAxisSceneComponent::Stop()
{
    EnsureScene();
    if (!Impl || !Impl->Player.IsValid())
    {
        return;
    }

    Impl->bPlaying = false;
    Impl->Player->SetTime(0.0);
    Impl->bShotFinished = false;
    UpdateScene();
}

void URigAxisSceneComponent::SetTime(float Time)
{
    EnsureScene();
    if (!Impl || !Impl->Player.IsValid())
    {
        return;
    }

    Impl->Player->SetTime(Time);
    Impl->bShotFinished = false;
    UpdateScene();
}

void URigAxisSceneComponent::Advance(float DeltaSeconds)
{
    if (DeltaSeconds <= 0.0f)
    {
        return;
    }

    EnsureScene();
    if (!Impl || !Impl->Player.IsValid())
    {
        return;
    }

    Impl->Player->Update(static_cast<double>(DeltaSeconds) * TimeScale);
    UpdateScene();
}

void URigAxisSceneComponent::ResetPhysics()
{
    EnsureScene();
    if (!Impl || !Impl->Player.IsValid())
    {
        return;
    }

    Impl->Player->ResetPhysics();
    UpdateScene();
}

float URigAxisSceneComponent::GetDuration() const
{
    return Impl && Impl->Player.IsValid()
        ? static_cast<float>(Impl->Player->GetDuration())
        : 0.0f;
}

float URigAxisSceneComponent::GetTime() const
{
    return Impl && Impl->Player.IsValid()
        ? static_cast<float>(Impl->Player->GetTime())
        : 0.0f;
}

FName URigAxisSceneComponent::GetCurrentShot() const
{
    return Impl && Impl->Player.IsValid()
        ? FName(*Impl->Player->GetCurrentShot())
        : NAME_None;
}

int32 URigAxisSceneComponent::GetActorCount() const
{
    return Impl && Impl->Player.IsValid()
        ? static_cast<int32>(Impl->Player->GetActorCount())
        : 0;
}

FName URigAxisSceneComponent::GetActorId(int32 Actor) const
{
    if (!Impl || !Impl->Player.IsValid() || Actor < 0)
    {
        return NAME_None;
    }
    const FString Id = Impl->Player->GetActorId(static_cast<uint32>(Actor));
    return Id.IsEmpty() ? NAME_None : FName(*Id);
}

FName URigAxisSceneComponent::GetActorName(int32 Actor) const
{
    if (!Impl || !Impl->Player.IsValid() || Actor < 0)
    {
        return NAME_None;
    }
    const FString Name = Impl->Player->GetActorName(static_cast<uint32>(Actor));
    return Name.IsEmpty() ? NAME_None : FName(*Name);
}

int32 URigAxisSceneComponent::FindActorIndex(FName Actor) const
{
    if (!Impl || !Impl->Player.IsValid())
    {
        return INDEX_NONE;
    }

    const FString Name = Actor.ToString();
    const uint32 Count = Impl->Player->GetActorCount();
    for (uint32 Index = 0; Index < Count; ++Index)
    {
        if (Impl->Player->GetActorId(Index) == Name)
        {
            return static_cast<int32>(Index);
        }
    }
    return INDEX_NONE;
}

FTransform URigAxisSceneComponent::AffineToWorld(const RigAxisAffine& Affine) const
{
    const float Scale = PixelsPerUnrealUnit > KINDA_SMALL_NUMBER
        ? 1.0f / PixelsPerUnrealUnit
        : 1.0f;
    const double Rotation = FMath::RadiansToDegrees(FMath::Atan2(Affine.b, Affine.a));
    const double ScaleX = FMath::Sqrt(Affine.a * Affine.a + Affine.b * Affine.b);
    const double ScaleY = FMath::Sqrt(Affine.c * Affine.c + Affine.d * Affine.d)
        * ((Affine.a * Affine.d - Affine.b * Affine.c) < 0.0 ? -1.0 : 1.0);
    const float X = static_cast<float>(Affine.tx) * Scale;
    const float Y = static_cast<float>(Affine.ty) * Scale;

    FTransform Local;
    if (Plane == ERigAxisPlane::XZ)
    {
        Local = FTransform(
            FRotator(static_cast<float>(Rotation), 0.0f, 0.0f),
            FVector(X, 0.0f, Y),
            FVector(static_cast<float>(ScaleX), 1.0f, static_cast<float>(ScaleY)));
    }
    else
    {
        Local = FTransform(
            FRotator(0.0f, static_cast<float>(Rotation), 0.0f),
            FVector(X, Y, 0.0f),
            FVector(static_cast<float>(ScaleX), static_cast<float>(ScaleY), 1.0f));
    }
    return Local * GetComponentTransform();
}

FTransform URigAxisSceneComponent::GetCameraWorldTransform() const
{
    if (!Impl || !Impl->Player.IsValid())
    {
        return GetComponentTransform();
    }

    RigAxisAffine Affine = {};
    if (!Impl->Player->GetCameraWorld(Affine))
    {
        return GetComponentTransform();
    }
    return AffineToWorld(Affine);
}

float URigAxisSceneComponent::GetCameraZoom() const
{
    if (!Impl || !Impl->Player.IsValid())
    {
        return 1.0f;
    }

    RigAxisAffine Affine = {};
    if (!Impl->Player->GetCameraWorld(Affine))
    {
        return 1.0f;
    }
    return static_cast<float>(FMath::Sqrt(Affine.a * Affine.a + Affine.b * Affine.b));
}

bool URigAxisSceneComponent::GetActorBoneWorldTransform(
    FName Actor,
    FName Bone,
    FTransform& OutTransform) const
{
    if (!Impl || !Impl->Player.IsValid())
    {
        return false;
    }

    const int32 ActorIndex = FindActorIndex(Actor);
    if (ActorIndex == INDEX_NONE)
    {
        return false;
    }

    RigAxisAffine Affine = {};
    if (!Impl->Player->GetActorBoneWorld(static_cast<uint32>(ActorIndex), Bone, Affine))
    {
        return false;
    }
    OutTransform = AffineToWorld(Affine);
    return true;
}
