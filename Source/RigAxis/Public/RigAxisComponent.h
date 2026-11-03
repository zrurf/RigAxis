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

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    bool Play(FName Animation, bool bInLoop = true, float MixDuration = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    bool SetAnimation(int32 Track, FName Animation, bool bInLoop, float MixDuration = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    bool QueueAnimation(int32 Track, FName Animation, bool bInLoop, float MixDuration = 0.0f, float Delay = -1.0f);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void ClearTrack(int32 Track);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void Stop();

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void SetSpeed(float Speed);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void SetTrackTime(int32 Track, float Time);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void Advance(float DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Pose")
    FVector GetBoneWorldPosition(FName Bone);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Pose")
    bool GetBoneWorldTransform(FName Bone, FTransform& OutTransform);

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
    void UpdateMesh();
    void DispatchEvents();
    void BuildPageMaterials();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> PageMaterials;

    FRigAxisComponentImpl* Impl = nullptr;
};
