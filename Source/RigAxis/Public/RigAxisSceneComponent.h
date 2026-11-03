#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "RigAxis/RigAxis.h"
#include "RigAxisTypes.h"
#include "RigAxisSceneComponent.generated.h"

class FPrimitiveSceneProxy;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class URigAxisSceneAsset;
class URigAxisSkeletonAsset;
class URigAxisSceneComponent;
struct FRigAxisSceneComponentImpl;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FRigAxisShotFinishedDelegate,
    URigAxisSceneComponent*,
    Component,
    FName,
    Shot);

UCLASS(ClassGroup = (RigAxis), meta = (BlueprintSpawnableComponent))
class RIGAXIS_API URigAxisSceneComponent : public UMeshComponent
{
    GENERATED_BODY()

public:
    URigAxisSceneComponent();
    virtual ~URigAxisSceneComponent() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RigAxis")
    TObjectPtr<URigAxisSceneAsset> SceneAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FName DefaultShot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bAutoPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bLoop = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    float TimeScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    float PixelsPerUnrealUnit = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    ERigAxisPlane Plane = ERigAxisPlane::XZ;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    TObjectPtr<UMaterialInterface> MeshMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FRigAxisMaterialSet MaterialSet;

    UPROPERTY(BlueprintAssignable, Category = "RigAxis|Events")
    FRigAxisShotFinishedDelegate OnShotFinished;

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void PlayShot(FName Shot, bool bInLoop = false);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void Stop();

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void SetTime(float Time);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    void Advance(float DeltaSeconds);

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    float GetDuration() const;

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    float GetTime() const;

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Playback")
    FName GetCurrentShot() const;

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Camera")
    FTransform GetCameraWorldTransform() const;

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Camera")
    float GetCameraZoom() const;

    UFUNCTION(BlueprintCallable, Category = "RigAxis|Pose")
    bool GetActorBoneWorldTransform(FName Actor, FName Bone, FTransform& OutTransform) const;

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
    void EnsureScene();
    void DestroyScene();
    void UpdateScene();
    void RebuildSectionMaterials();
    UMaterialInterface* SelectMaterial(ERigAxisBlend Blend) const;
    URigAxisSkeletonAsset* FindSourceSkeleton(uint32 Source) const;
    int32 FindActorIndex(FName Actor) const;
    FTransform AffineToWorld(const RigAxisAffine& Affine) const;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> SectionMaterials;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> BaseMaterials;

    TMap<uint64, int32> BaseMaterialIndices;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DarkMaterials;

    TMap<uint64, int32> DarkMaterialIndices;

    UPROPERTY(Transient)
    FRigAxisMaterialSet DefaultMaterials;

    uint32 BoundMaterialSignature = 0;
    uint32 BoundMaterialSetSignature = 0;
    FRigAxisSceneComponentImpl* Impl = nullptr;
};
