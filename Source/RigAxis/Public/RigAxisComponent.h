#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "RigAxisTypes.h"
#include "RigAxisComponent.generated.h"

class FPrimitiveSceneProxy;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class URigAxisSkeletonAsset;
class URigAxisComponent;
struct FRigAxisComponentImpl;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FRigAxisAnimationDelegate,
    URigAxisComponent*,
    Component,
    FName,
    Animation,
    int32,
    Track);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FRigAxisAnimationEventDelegate,
    URigAxisComponent*,
    Component,
    const FRigAxisAnimationEvent&,
    Event);

/// Plays a URigAxisSkeletonAsset and renders it with a custom scene proxy.
/// PixelsPerUnrealUnit converts skeleton pixels to Unreal units; Plane selects the projection plane.
UCLASS(ClassGroup = (RigAxis), meta = (BlueprintSpawnableComponent))
class RIGAXIS_API URigAxisComponent : public UMeshComponent
{
    GENERATED_BODY()

public:
    URigAxisComponent();
    virtual ~URigAxisComponent() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RigAxis")
    TObjectPtr<URigAxisSkeletonAsset> SkeletonAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FName DefaultAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bAutoPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bLoop = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    float DefaultMix = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FLinearColor Tint = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    ERigAxisPlane Plane = ERigAxisPlane::XZ;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    float PixelsPerUnrealUnit = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bFlipX = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bFlipY = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    ERigAxisUpdateMode UpdateMode = ERigAxisUpdateMode::Scaled;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    float TimeScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    TObjectPtr<UMaterialInterface> MeshMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FRigAxisMaterialSet MaterialSet;

    UPROPERTY(BlueprintAssignable, Category = "RigAxis|Events")
    FRigAxisAnimationDelegate OnAnimationStart;

    UPROPERTY(BlueprintAssignable, Category = "RigAxis|Events")
    FRigAxisAnimationDelegate OnAnimationEnd;

    UPROPERTY(BlueprintAssignable, Category = "RigAxis|Events")
    FRigAxisAnimationDelegate OnAnimationComplete;

    UPROPERTY(BlueprintAssignable, Category = "RigAxis|Events")
    FRigAxisAnimationEventDelegate OnAnimationEvent;

    /// Starts an animation on track 0.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    bool Play(FName Animation, bool bInLoop = true, float MixDuration = 0.0f);

    /// Replaces the animation on a track. MixDuration crossfades from the previous animation.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    bool SetAnimation(int32 Track, FName Animation, bool bInLoop, float MixDuration = 0.0f);

    /// Queues an animation on a track. Delay < 0 starts it at the current animation's end.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    bool QueueAnimation(int32 Track, FName Animation, bool bInLoop, float MixDuration = 0.0f, float Delay = -1.0f);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void ClearTrack(int32 Track);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void Stop();

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void SetSpeed(float Speed);

    /// Sets the local time of a track, in seconds, and updates the pose immediately.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void SetTrackTime(int32 Track, float Time);

    /// Sets the influence of a track, clamped to 0..1, and updates the pose immediately.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    bool SetTrackAlpha(int32 Track, float Alpha);

    /// Sets the time scale of a track.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    bool SetTrackTimeScale(int32 Track, float Scale);

    /// Returns the local time of a track, in seconds.
    UFUNCTION(BlueprintPure, Category = "RigAxis|Playback")
    float GetTrackTime(int32 Track = 0);

    /// Returns the animation currently playing on a track, or None.
    UFUNCTION(BlueprintPure, Category = "RigAxis|Playback")
    FName GetCurrentAnimation(int32 Track = 0);

    /// Returns the animation duration in seconds, or 0 when the animation is missing.
    UFUNCTION(BlueprintPure, Category = "RigAxis|Playback")
    float GetAnimationDuration(FName Animation) const;

    /// Returns the duration of the animation currently playing on a track, in seconds.
    UFUNCTION(BlueprintPure, Category = "RigAxis|Playback")
    float GetCurrentAnimationDuration(int32 Track = 0);

    /// Advances playback manually. Use with UpdateMode = Manual.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void Advance(float DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Pose")
    FVector GetBoneWorldPosition(FName Bone);

    /// Returns a bone transform in world space, including the component transform.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Pose")
    bool GetBoneWorldTransform(FName Bone, FTransform& OutTransform);

    /// Selects a skin. Returns false when the skin does not exist.
    UFUNCTION(BlueprintCallable, Category = "RigAxis|Pose")
    bool SetSkin(FName Skin);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Pose")
    void ResetPhysics();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    virtual int32 GetNumMaterials() const override;
    virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
    virtual void GetUsedMaterials(
        TArray<UMaterialInterface*>& OutMaterials,
        bool bGetDebugMaterials = false) const override;

private:
    void EnsurePlayer();
    void DestroyPlayer();
    void ApplyCurrentPose();
    void UpdateMesh();
    void DispatchEvents();
    void BuildPageMaterials();
    void RebuildSectionMaterials();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> PageMaterials;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> SectionMaterials;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DarkMaterials;

    TMap<uint64, int32> DarkMaterialIndices;

    UPROPERTY(Transient)
    FRigAxisMaterialSet DefaultMaterials;

    uint32 BoundMaterialSignature = 0;
    FRigAxisComponentImpl* Impl = nullptr;
};
