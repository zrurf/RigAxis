#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "RigAxisComponent.h"
#include "RigAxisSkeletonAsset.h"
#include "RigAxisSkeletonFactory.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

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
    AtlasText.ReplaceInline(TEXT("repeat: none"), TEXT("repeat: none\npma: true"));
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
        TestTrue(TEXT("premultiplied alpha"), Asset->bPremultipliedAlpha);

        TStrongObjectPtr<URigAxisComponent> Component(NewObject<URigAxisComponent>());
        Component->SkeletonAsset = Asset;
        TestTrue(TEXT("play before reimport"), Component->Play(TEXT("move")));
        Component->Advance(0.5f);

        FTransform Before;
        TestTrue(TEXT("transform before reimport"), Component->GetBoneWorldTransform(TEXT("bone"), Before));
        TestTrue(
            TEXT("position before reimport"),
            Before.GetLocation().Equals(FVector(5.0f, 0.0f, 0.0f), 0.01f));

        const int32 Version = Asset->GetVersion();
        FString JsonText;
        if (FFileHelper::LoadFileToString(JsonText, *JsonPath))
        {
            JsonText.ReplaceInline(TEXT("\"move\""), TEXT("\"walk\""));
            JsonText.ReplaceInline(TEXT("\"x\": 10"), TEXT("\"x\": 20"));
            FFileHelper::SaveStringToFile(JsonText, *JsonPath);
        }

        const EReimportResult::Type Result = Factory->Reimport(Asset);
        TestEqual(
            TEXT("reimport result"),
            static_cast<int32>(Result),
            static_cast<int32>(EReimportResult::Succeeded));
        TestTrue(TEXT("version bumped"), Asset->GetVersion() > Version);
        TestTrue(TEXT("skeleton data after reimport"), Asset->GetData().IsValid());

        Component->Advance(0.0f);
        TestTrue(TEXT("play after reimport"), Component->Play(TEXT("walk")));
        Component->Advance(0.5f);

        FTransform After;
        TestTrue(TEXT("transform after reimport"), Component->GetBoneWorldTransform(TEXT("bone"), After));
        TestTrue(
            TEXT("position after reimport"),
            After.GetLocation().Equals(FVector(10.0f, 0.0f, 0.0f), 0.01f));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRigAxisImportErrorTest,
    "RigAxis.Editor.ImportErrors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisImportErrorTest::RunTest(const FString& Parameters)
{
    AddExpectedError(TEXT("RigAxis:"), EAutomationExpectedErrorFlags::Contains, 2);

    const FString FixtureDir = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("../Fixtures")));
    const FString WorkDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RigAxisImportErrorTest"));
    IFileManager::Get().DeleteDirectory(*WorkDir, false, true);
    IFileManager::Get().MakeDirectory(*WorkDir, true);

    auto WriteAtlas = [&WorkDir](const FString& Name, const FString& Page)
    {
        const FString Text = FString::Printf(
            TEXT("%s\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\nimage\n  bounds: 0,0,32,32\n"),
            *Page);
        FFileHelper::SaveStringToFile(Text, *FPaths::Combine(WorkDir, Name));
    };

    const FString MissingJson = FPaths::Combine(WorkDir, TEXT("missing.json"));
    TArray<uint8> Bytes;
    FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(FixtureDir, TEXT("hero.json")));
    FFileHelper::SaveArrayToFile(Bytes, *MissingJson);
    WriteAtlas(TEXT("missing.atlas"), TEXT("missing.png"));

    const FString BrokenJson = FPaths::Combine(WorkDir, TEXT("broken.json"));
    FFileHelper::SaveStringToFile(TEXT("{ invalid"), *BrokenJson);
    WriteAtlas(TEXT("broken.atlas"), TEXT("broken.png"));

    const FString Guid = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
    UPackage* Package = CreatePackage(
        *FString::Printf(TEXT("/Game/RigAxisTest/ImporterErrorTest_%s"), *Guid));
    URigAxisSkeletonFactory* Factory = NewObject<URigAxisSkeletonFactory>();

    bool bCanceled = false;
    UObject* Missing = Factory->FactoryCreateFile(
        URigAxisSkeletonAsset::StaticClass(),
        Package,
        FName(TEXT("MissingPage")),
        RF_Public | RF_Standalone,
        MissingJson,
        nullptr,
        GWarn,
        bCanceled);
    TestNull(TEXT("missing atlas page"), Missing);

    UObject* Broken = Factory->FactoryCreateFile(
        URigAxisSkeletonAsset::StaticClass(),
        Package,
        FName(TEXT("BrokenSkeleton")),
        RF_Public | RF_Standalone,
        BrokenJson,
        nullptr,
        GWarn,
        bCanceled);
    TestNull(TEXT("invalid skeleton"), Broken);

    Package->MarkAsGarbage();
    IFileManager::Get().DeleteDirectory(*WorkDir, false, true);
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

    return true;
}

#endif
