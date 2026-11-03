#include "RigAxisSceneFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "RigAxisSceneAsset.h"
#include "RigAxisSkeletonAsset.h"
#include "RigAxisSkeletonFactory.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    void SaveAssetPackage(UObject* Asset, FFeedbackContext* Warn)
    {
        if (!Asset)
        {
            return;
        }

        UPackage* Package = Asset->GetPackage();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            Package->GetName(),
            FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.Error = Warn;
        Args.bSlowTask = false;
        UPackage::SavePackage(Package, Asset, *Filename, Args);
    }

    URigAxisSkeletonAsset* ImportSkeleton(
        const FString& FullPath,
        const FString& PackagePath,
        FFeedbackContext* Warn,
        TArray<FString>& OutSources)
    {
        const FString BaseName = FPaths::GetBaseFilename(FullPath);
        const FString AssetPath = FPaths::Combine(PackagePath, BaseName);
        URigAxisSkeletonAsset* Skeleton = LoadObject<URigAxisSkeletonAsset>(
            nullptr,
            *AssetPath,
            nullptr,
            LOAD_NoWarn | LOAD_Quiet);
        if (Skeleton)
        {
            for (const FString& Source : Skeleton->AdditionalSourceFiles)
            {
                OutSources.AddUnique(Source);
            }
            return Skeleton;
        }

        UPackage* Package = CreatePackage(*AssetPath);
        URigAxisSkeletonFactory* Factory = NewObject<URigAxisSkeletonFactory>();
        bool bCanceled = false;
        Skeleton = Cast<URigAxisSkeletonAsset>(Factory->FactoryCreateFile(
            URigAxisSkeletonAsset::StaticClass(),
            Package,
            FName(*BaseName),
            RF_Public | RF_Standalone,
            FullPath,
            nullptr,
            Warn,
            bCanceled));
        if (!Skeleton)
        {
            return nullptr;
        }

        FAssetRegistryModule::AssetCreated(Skeleton);
        SaveAssetPackage(Skeleton, Warn);
        for (const FString& Source : Skeleton->AdditionalSourceFiles)
        {
            OutSources.AddUnique(Source);
        }
        return Skeleton;
    }
}

URigAxisSceneFactory::URigAxisSceneFactory()
{
    bCreateNew = false;
    bEditorImport = true;
    bText = false;
    ImportPriority = 2;
    SupportedClass = URigAxisSceneAsset::StaticClass();
    Formats.Add(TEXT("json;Rigby2D scene manifest"));
}

bool URigAxisSceneFactory::FactoryCanImport(const FString& Filename)
{
    return Filename.EndsWith(TEXT(".scene.json"), ESearchCase::IgnoreCase);
}

UObject* URigAxisSceneFactory::FactoryCreateFile(
    UClass* InClass,
    UObject* InParent,
    FName InName,
    EObjectFlags Flags,
    const FString& Filename,
    const TCHAR* Parms,
    FFeedbackContext* Warn,
    bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;

    TArray<uint8> ManifestBytes;
    if (!FFileHelper::LoadFileToArray(ManifestBytes, *Filename))
    {
        Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: cannot read '%s'."), *Filename);
        return nullptr;
    }

    FString ManifestText;
    if (!FFileHelper::LoadFileToString(ManifestText, *Filename))
    {
        Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: cannot read '%s'."), *Filename);
        return nullptr;
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->HasField(TEXT("rigbyScene")))
    {
        Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: '%s' is not a scene manifest."), *Filename);
        return nullptr;
    }

    FString StagePath;
    if (!Root->TryGetStringField(TEXT("stage"), StagePath) || StagePath.IsEmpty())
    {
        Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: scene manifest names no stage."));
        return nullptr;
    }

    UPackage* Package = Cast<UPackage>(InParent);
    if (!Package)
    {
        Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: import target is not a package."));
        return nullptr;
    }

    const FString BaseDir = FPaths::GetPath(Filename);
    const FString PackagePath = FPackageName::GetLongPackagePath(Package->GetName());
    TArray<FString> Sources;
    Sources.Add(Filename);

    URigAxisSkeletonAsset* Stage = ImportSkeleton(
        FPaths::ConvertRelativePathToFull(FPaths::Combine(BaseDir, StagePath)),
        PackagePath,
        Warn,
        Sources);
    if (!Stage)
    {
        return nullptr;
    }

    TMap<FString, TObjectPtr<URigAxisSkeletonAsset>> ActorSkeletons;
    TMap<FString, FString> ActorSkeletonPaths;
    const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
    if (Root->TryGetArrayField(TEXT("actors"), Actors) && Actors)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Actors)
        {
            TSharedPtr<FJsonObject> Actor;
            if (Value.IsValid())
            {
                Actor = Value->AsObject();
            }
            if (!Actor.IsValid())
            {
                continue;
            }

            FString ActorId;
            FString SkeletonPath;
            if (!Actor->TryGetStringField(TEXT("id"), ActorId)
                || ActorId.IsEmpty()
                || !Actor->TryGetStringField(TEXT("skeleton"), SkeletonPath)
                || SkeletonPath.IsEmpty()
                || ActorSkeletons.Contains(ActorId))
            {
                continue;
            }

            URigAxisSkeletonAsset* Skeleton = ImportSkeleton(
                FPaths::ConvertRelativePathToFull(FPaths::Combine(BaseDir, SkeletonPath)),
                PackagePath,
                Warn,
                Sources);
            if (!Skeleton)
            {
                return nullptr;
            }

            ActorSkeletons.Add(ActorId, Skeleton);
            ActorSkeletonPaths.Add(ActorId, SkeletonPath);
        }
    }

    const FString AssetName = InName.ToString();
    URigAxisSceneAsset* Asset = FindObject<URigAxisSceneAsset>(Package, *AssetName);
    const bool bCreated = Asset == nullptr;
    if (bCreated)
    {
        Asset = NewObject<URigAxisSceneAsset>(Package, InName, Flags);
    }

    Asset->ManifestBytes = ManifestBytes;
    Asset->Stage = Stage;
    Asset->StagePath = StagePath;
    Asset->ActorSkeletons = ActorSkeletons;
    Asset->ActorSkeletonPaths = ActorSkeletonPaths;
    Asset->AdditionalSourceFiles = Sources;

    if (bCreated)
    {
        FAssetRegistryModule::AssetCreated(Asset);
    }
    Package->MarkPackageDirty();

    return Asset;
}
