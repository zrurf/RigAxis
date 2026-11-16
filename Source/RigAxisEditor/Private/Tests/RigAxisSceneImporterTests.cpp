#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "RigAxisSceneAsset.h"
#include "RigAxisSceneComponent.h"
#include "RigAxisSceneFactory.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRigAxisSceneImporterTest,
    "RigAxis.Editor.SceneImport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisSceneImporterTest::RunTest(const FString& Parameters)
{
    const FString FixtureDir = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("../Fixtures/scene")));
    const FString WorkDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RigAxisSceneImporterTest"));
    const FString Guid = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
    const FString StageBase = FString::Printf(TEXT("stage_%s"), *Guid);
    const FString HeroBase = FString::Printf(TEXT("hero_%s"), *Guid);
    const FString AssetName = FString::Printf(TEXT("SceneTest_%s"), *Guid);
    const FString PackageName = FString::Printf(TEXT("/Game/RigAxisTest/%s"), *AssetName);

    IFileManager::Get().DeleteDirectory(*WorkDir, false, true);
    IFileManager::Get().MakeDirectory(*WorkDir, true);

    auto CopyFile = [&](const FString& Source, const FString& Target)
    {
        TArray<uint8> Bytes;
        return FFileHelper::LoadFileToArray(Bytes, *FPaths::Combine(FixtureDir, Source))
            && FFileHelper::SaveArrayToFile(Bytes, *FPaths::Combine(WorkDir, Target));
    };

    auto CopyReplacing = [&](const FString& Source, const FString& Target, const FString& From, const FString& To)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(FixtureDir, Source)))
        {
            return false;
        }
        Text.ReplaceInline(*From, *To);
        return FFileHelper::SaveStringToFile(Text, *FPaths::Combine(WorkDir, Target));
    };

    const FString StageJson = StageBase + TEXT(".json");
    const FString StageAtlas = StageBase + TEXT(".atlas");
    const FString StagePage = StageBase + TEXT(".png");
    const FString HeroJson = HeroBase + TEXT(".json");
    const FString HeroAtlas = HeroBase + TEXT(".atlas");
    const FString HeroPage = HeroBase + TEXT(".png");
    const FString SceneJson = AssetName + TEXT(".scene.json");

    const bool bCopied = CopyFile(TEXT("stage.json"), StageJson)
        && CopyReplacing(TEXT("stage.atlas"), StageAtlas, TEXT("stage.png"), StagePage)
        && CopyFile(TEXT("stage.png"), StagePage)
        && CopyFile(TEXT("hero.json"), HeroJson)
        && CopyReplacing(TEXT("hero.atlas"), HeroAtlas, TEXT("hero.png"), HeroPage)
        && CopyFile(TEXT("hero.png"), HeroPage);
    if (!bCopied)
    {
        AddError(TEXT("cannot copy scene fixtures"));
        return false;
    }

    FString SceneText;
    if (!FFileHelper::LoadFileToString(
            SceneText,
            *FPaths::Combine(FixtureDir, TEXT("scene.scene.json"))))
    {
        AddError(TEXT("cannot read scene manifest"));
        return false;
    }
    SceneText.ReplaceInline(TEXT("stage.json"), *StageJson);
    SceneText.ReplaceInline(TEXT("hero.json"), *HeroJson);
    if (!FFileHelper::SaveStringToFile(SceneText, *FPaths::Combine(WorkDir, SceneJson)))
    {
        AddError(TEXT("cannot write scene manifest"));
        return false;
    }

    UPackage* Package = CreatePackage(*PackageName);
    URigAxisSceneFactory* Factory = NewObject<URigAxisSceneFactory>();
    bool bCanceled = false;
    UObject* Imported = Factory->FactoryCreateFile(
        URigAxisSceneAsset::StaticClass(),
        Package,
        FName(*AssetName),
        RF_Public | RF_Standalone,
        FPaths::Combine(WorkDir, SceneJson),
        nullptr,
        GWarn,
        bCanceled);

    URigAxisSceneAsset* Asset = Cast<URigAxisSceneAsset>(Imported);
    TestFalse(TEXT("import canceled"), bCanceled);
    TestNotNull(TEXT("imported scene"), Asset);

    TArray<UObject*> Objects;
    TArray<FString> Files;
    if (Asset)
    {
        Objects.Add(Asset);
        TestTrue(TEXT("manifest bytes"), Asset->ManifestBytes.Num() > 0);
        TestEqual(TEXT("stage path"), Asset->StagePath, StageJson);
        TestNotNull(TEXT("stage"), Asset->Stage.Get());
        URigAxisSkeletonAsset* Hero = Asset->FindActorSkeleton(TEXT("hero"));
        TestNotNull(TEXT("actor skeleton"), Hero);
        TestEqual(
            TEXT("actor path"),
            Asset->ActorSkeletonPaths.FindRef(TEXT("hero")),
            HeroJson);

        if (Asset->Stage)
        {
            Objects.Add(Asset->Stage);
            TestTrue(TEXT("stage data"), Asset->Stage->GetData().IsValid());
        }
        if (Hero)
        {
            Objects.Add(Hero);
            TestTrue(TEXT("hero data"), Hero->GetData().IsValid());
        }

        TStrongObjectPtr<URigAxisSceneComponent> Component(NewObject<URigAxisSceneComponent>());
        Component->SceneAsset = Asset;
        Component->UpdateMode = ERigAxisUpdateMode::Manual;
        Component->ResetPhysics();
        TestEqual(TEXT("actor count"), Component->GetActorCount(), 1);
        TestEqual(TEXT("actor id"), Component->GetActorId(0), FName(TEXT("hero")));
        TestEqual(TEXT("actor name"), Component->GetActorName(0), FName(TEXT("Hero")));
    }

    for (int32 Index = 0; Index < Objects.Num(); ++Index)
    {
        URigAxisSkeletonAsset* Skeleton = Cast<URigAxisSkeletonAsset>(Objects[Index]);
        if (Skeleton)
        {
            for (const TObjectPtr<UTexture2D>& Page : Skeleton->AtlasPages)
            {
                if (Page)
                {
                    Objects.AddUnique(Page.Get());
                }
            }
        }
    }

    for (UObject* Object : Objects)
    {
        if (Object)
        {
            const FString Filename = FPackageName::LongPackageNameToFilename(
                Object->GetPackage()->GetName(),
                FPackageName::GetAssetPackageExtension());
            Files.Add(Filename);
        }
    }
    for (UObject* Object : Objects)
    {
        if (Object)
        {
            ObjectTools::DeleteSingleObject(Object, false);
        }
    }
    for (const FString& Filename : Files)
    {
        IFileManager::Get().Delete(*Filename, false, true, true);
    }

    Package->MarkAsGarbage();
    IFileManager::Get().DeleteDirectory(*WorkDir, false, true);
    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    return true;
}

#endif
