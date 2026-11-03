#include "RigAxisSkeletonFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "RigAxisSkeletonAsset.h"
#include "RigAxisSkeletonData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    bool ReadFile(const FString& Path, TArray<uint8>& OutBytes, FFeedbackContext* Warn)
    {
        if (FFileHelper::LoadFileToArray(OutBytes, *Path))
        {
            return true;
        }

        Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: cannot read '%s'."), *Path);
        return false;
    }

    FString FindSkeletonFile(const FString& BasePath, const FString& BaseName)
    {
        const FString SkelFile = FPaths::Combine(BasePath, BaseName + TEXT(".skel"));
        if (FPaths::FileExists(SkelFile))
        {
            return SkelFile;
        }

        const FString JsonFile = FPaths::Combine(BasePath, BaseName + TEXT(".json"));
        return FPaths::FileExists(JsonFile) ? JsonFile : FString();
    }

    bool ResolveSourceFiles(
        const FString& Filename,
        FString& OutSkeletonFile,
        FString& OutAtlasFile,
        FFeedbackContext* Warn)
    {
        const FString Extension = FPaths::GetExtension(Filename).ToLower();
        const FString BasePath = FPaths::GetPath(Filename);
        const FString BaseName = FPaths::GetBaseFilename(Filename);

        if (Extension == TEXT("atlas"))
        {
            OutAtlasFile = Filename;
            OutSkeletonFile = FindSkeletonFile(BasePath, BaseName);
            if (OutSkeletonFile.IsEmpty())
            {
                Warn->Logf(
                    ELogVerbosity::Error,
                    TEXT("RigAxis: no .skel or .json next to '%s'."),
                    *Filename);
                return false;
            }
            return true;
        }

        OutSkeletonFile = Filename;
        OutAtlasFile = FPaths::Combine(BasePath, BaseName + TEXT(".atlas"));
        if (!FPaths::FileExists(OutAtlasFile))
        {
            Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: atlas '%s' not found."), *OutAtlasFile);
            return false;
        }
        return true;
    }

    UTexture2D* ImportTexture(
        const FString& PagePath,
        const FString& PackagePath,
        const FString& AssetName,
        FFeedbackContext* Warn)
    {
        const FString TexturePackageName = FPaths::Combine(PackagePath, AssetName);
        UTexture2D* Texture = LoadObject<UTexture2D>(
            nullptr,
            *TexturePackageName,
            nullptr,
            LOAD_NoWarn | LOAD_Quiet);
        const bool bCreated = Texture == nullptr;
        if (bCreated)
        {
            UPackage* Package = CreatePackage(*TexturePackageName);
            UTextureFactory* Factory = NewObject<UTextureFactory>();
            Factory->CompressionSettings = TC_EditorIcon;

            bool bCanceled = false;
            Texture = Cast<UTexture2D>(Factory->FactoryCreateFile(
                UTexture2D::StaticClass(),
                Package,
                FName(*AssetName),
                RF_Public | RF_Standalone,
                PagePath,
                nullptr,
                Warn,
                bCanceled));
            if (!Texture)
            {
                Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: cannot import texture '%s'."), *PagePath);
                return nullptr;
            }
            FAssetRegistryModule::AssetCreated(Texture);
        }

        Texture->SRGB = true;
        Texture->CompressionSettings = TC_EditorIcon;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->Filter = TF_Bilinear;
        Texture->AddressX = TA_Clamp;
        Texture->AddressY = TA_Clamp;
        Texture->NeverStream = true;
        Texture->LODGroup = TEXTUREGROUP_UI;
        Texture->PostEditChange();

        if (bCreated)
        {
            UPackage* Package = Texture->GetPackage();
            const FString TextureFilename = FPackageName::LongPackageNameToFilename(
                TexturePackageName,
                FPackageName::GetAssetPackageExtension());
            FSavePackageArgs SaveArgs;
            SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
            SaveArgs.Error = Warn;
            SaveArgs.bSlowTask = false;
            UPackage::SavePackage(Package, Texture, *TextureFilename, SaveArgs);
        }

        return Texture;
    }
}

URigAxisSkeletonFactory::URigAxisSkeletonFactory()
{
    bCreateNew = false;
    bEditorImport = true;
    bText = false;
    SupportedClass = URigAxisSkeletonAsset::StaticClass();
    Formats.Add(TEXT("skel;Rigby2D skeleton"));
    Formats.Add(TEXT("json;Rigby2D skeleton JSON"));
    Formats.Add(TEXT("atlas;Rigby2D atlas"));
}

UObject* URigAxisSkeletonFactory::FactoryCreateFile(
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

    FString SkeletonFile;
    FString AtlasFile;
    if (!ResolveSourceFiles(Filename, SkeletonFile, AtlasFile, Warn))
    {
        return nullptr;
    }

    TArray<uint8> SkeletonBytes;
    TArray<uint8> AtlasBytes;
    if (!ReadFile(SkeletonFile, SkeletonBytes, Warn) || !ReadFile(AtlasFile, AtlasBytes, Warn))
    {
        return nullptr;
    }

    FString Error;
    TSharedPtr<FRigAxisSkeletonData> Data = FRigAxisSkeletonData::Create(SkeletonBytes, AtlasBytes, Error);
    if (!Data.IsValid())
    {
        Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: %s"), *Error);
        return nullptr;
    }

    UPackage* Package = Cast<UPackage>(InParent);
    if (!Package)
    {
        Warn->Logf(ELogVerbosity::Error, TEXT("RigAxis: import target is not a package."));
        return nullptr;
    }

    const FString PackagePath = FPackageName::GetLongPackagePath(Package->GetName());
    const FString AssetName = InName.ToString();
    const FString BaseName = FPaths::GetBaseFilename(SkeletonFile);
    const TArray<FString>& PageNames = Data->GetAtlasPageNames();

    TArray<UTexture2D*> Pages;
    TArray<FString> SourceFiles;
    SourceFiles.Add(SkeletonFile);
    SourceFiles.Add(AtlasFile);

    for (int32 Index = 0; Index < PageNames.Num(); ++Index)
    {
        const FString PagePath = FPaths::Combine(FPaths::GetPath(AtlasFile), PageNames[Index]);
        if (!FPaths::FileExists(PagePath))
        {
            Warn->Logf(
                ELogVerbosity::Error,
                TEXT("RigAxis: atlas page '%s' not found."),
                *PagePath);
            return nullptr;
        }

        const FString PageBase = FPaths::GetBaseFilename(PageNames[Index]);
        const FString TextureName = FString::Printf(TEXT("T_%s_%s"), *BaseName, *PageBase);
        UTexture2D* Texture = ImportTexture(PagePath, PackagePath, TextureName, Warn);
        if (!Texture)
        {
            return nullptr;
        }

        Pages.Add(Texture);
        SourceFiles.Add(PagePath);
    }

    URigAxisSkeletonAsset* Asset = FindObject<URigAxisSkeletonAsset>(Package, *AssetName);
    const bool bCreated = Asset == nullptr;
    if (bCreated)
    {
        Asset = NewObject<URigAxisSkeletonAsset>(Package, FName(*AssetName), Flags);
    }

    Asset->SkeletonBytes = SkeletonBytes;
    Asset->AtlasBytes = AtlasBytes;
    Asset->AtlasPageNames = PageNames;
    Asset->AtlasPages.Reset();
    for (UTexture2D* Texture : Pages)
    {
        Asset->AtlasPages.Add(Texture);
    }
    Asset->AdditionalSourceFiles = SourceFiles;
    Asset->RebuildData();

    if (bCreated)
    {
        FAssetRegistryModule::AssetCreated(Asset);
    }
    Package->MarkPackageDirty();

    return Asset;
}
