#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "RigAxisSkeletonAsset.h"
#include "RigAxisSkeletonFactory.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRigAxisImporterTest,
    "RigAxis.Editor.Import",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisImporterTest::RunTest(const FString& Parameters)
{
    const FString FixtureDir = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("../Fixtures")));
    const FString WorkDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RigAxisImporterTest"));
    const FString Guid = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
    const FString BaseName = FString::Printf(TEXT("rigaxis_import_%s"), *Guid);
    const FString AssetName = FString::Printf(TEXT("ImporterTest_%s"), *Guid);
    const FString PackageName = FString::Printf(TEXT("/Game/RigAxisTest/%s"), *AssetName);

    IFileManager::Get().DeleteDirectory(*WorkDir, false, true);
    IFileManager::Get().MakeDirectory(*WorkDir, true);

    const FString JsonPath = FPaths::Combine(WorkDir, BaseName + TEXT(".json"));
    const FString AtlasPath = FPaths::Combine(WorkDir, BaseName + TEXT(".atlas"));
    const FString PageName = BaseName + TEXT(".png");
    const FString PagePath = FPaths::Combine(WorkDir, PageName);

    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(FixtureDir, TEXT("hero.json")))
        || !FFileHelper::SaveArrayToFile(Bytes, *JsonPath)
        || !FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(FixtureDir, TEXT("hero.png")))
        || !FFileHelper::SaveArrayToFile(Bytes, *PagePath))
    {
        AddError(TEXT("cannot copy fixture files"));
        return false;
    }

    FString AtlasText;
    if (!FFileHelper::LoadFileToString(AtlasText, *FPaths::Combine(FixtureDir, TEXT("hero.atlas"))))
    {
        AddError(TEXT("cannot read hero.atlas"));
        return false;
    }
    AtlasText.ReplaceInline(TEXT("hero.png"), *PageName);
    if (!FFileHelper::SaveStringToFile(AtlasText, *AtlasPath))
    {
        AddError(TEXT("cannot write atlas"));
        return false;
    }

    UPackage* Package = CreatePackage(*PackageName);
    URigAxisSkeletonFactory* Factory = NewObject<URigAxisSkeletonFactory>();
    bool bCanceled = false;
    UObject* Imported = Factory->FactoryCreateFile(
        URigAxisSkeletonAsset::StaticClass(),
        Package,
        FName(*AssetName),
        RF_Public | RF_Standalone,
        JsonPath,
        nullptr,
        GWarn,
        bCanceled);

    URigAxisSkeletonAsset* Asset = Cast<URigAxisSkeletonAsset>(Imported);
    TestFalse(TEXT("import canceled"), bCanceled);
    TestNotNull(TEXT("imported asset"), Asset);
    if (Asset)
    {
        TestEqual(TEXT("atlas pages"), Asset->AtlasPages.Num(), 1);
        TestTrue(TEXT("skeleton data"), Asset->GetData().IsValid());

        const int32 Version = Asset->GetVersion();
        FString JsonText;
        if (FFileHelper::LoadFileToString(JsonText, *JsonPath))
        {
            JsonText.ReplaceInline(TEXT("\"move\""), TEXT("\"walk\""));
            FFileHelper::SaveStringToFile(JsonText, *JsonPath);
        }

        const EReimportResult::Type Result = Factory->Reimport(Asset);
        TestEqual(
            TEXT("reimport result"),
            static_cast<int32>(Result),
            static_cast<int32>(EReimportResult::Succeeded));
        TestTrue(TEXT("version bumped"), Asset->GetVersion() > Version);
        TestTrue(TEXT("skeleton data after reimport"), Asset->GetData().IsValid());
    }

    TArray<UTexture2D*> Pages;
    if (Asset)
    {
        for (const TObjectPtr<UTexture2D>& Page : Asset->AtlasPages)
        {
            Pages.Add(Page);
        }

        ObjectTools::DeleteSingleObject(Asset, false);
    }

    for (UTexture2D* Page : Pages)
    {
        const FString TextureFilename = Page
            ? FPackageName::LongPackageNameToFilename(
                Page->GetPackage()->GetName(),
                FPackageName::GetAssetPackageExtension())
            : FString();
        if (Page)
        {
            ObjectTools::DeleteSingleObject(Page, false);
        }
        if (!TextureFilename.IsEmpty())
        {
            IFileManager::Get().Delete(*TextureFilename, false, true, true);
        }
    }

    Package->MarkAsGarbage();
    IFileManager::Get().DeleteDirectory(*WorkDir, false, true);
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

    return true;
}

#endif
