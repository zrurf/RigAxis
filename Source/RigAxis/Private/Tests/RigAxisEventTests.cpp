#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "RigAxisPlayer.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const ANSICHAR* EventSkeleton = R"JSON({
      "skeleton": {"spine": "4.3"},
      "bones": [{"name": "root"}],
      "slots": [],
      "skins": [],
      "events": {
        "footstep": {"int": 7, "float": 2.5, "string": "step", "audio": "step.wav", "volume": 0.8, "balance": -0.2}
      },
      "animations": {
        "walk": {
          "events": [
            {"time": 0.5, "name": "footstep"},
            {"time": 1.0, "name": "done", "int": 9}
          ]
        }
      }
    })JSON";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigAxisEventTest, "RigAxis.Native.Events", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisEventTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URigAxisSkeletonAsset> Asset(NewObject<URigAxisSkeletonAsset>());
    Asset->SkeletonBytes.Append(reinterpret_cast<const uint8*>(EventSkeleton), FCStringAnsi::Strlen(EventSkeleton));

    TSharedPtr<FRigAxisSkeletonData> Data = Asset->GetData();
    TestTrue(TEXT("data valid"), Data.IsValid());
    if (!Data.IsValid())
    {
        AddError(FString::Printf(TEXT("asset load failed: %s"), *Asset->GetLoadError()));
        return false;
    }

    FRigAxisPlayer Player;
    FString Error;
    TestTrue(TEXT("player create"), Player.Create(Data, Error));
    if (!Player.IsValid())
    {
        AddError(FString::Printf(TEXT("player create failed: %s"), *Error));
        return false;
    }

    TestTrue(TEXT("play walk"), Player.Play(FName(TEXT("walk")), false, 0.0f));
    TestEqual(TEXT("start event count"), Player.GetEventCount(), 1);

    FRigAxisEvent Event;
    TestTrue(TEXT("start event readable"), Player.GetEvent(0, Event));
    TestEqual(TEXT("start kind"), int32(Event.Kind), 0);
    TestTrue(TEXT("start animation"), Event.Animation == FName(TEXT("walk")));
    TestTrue(TEXT("start has no event name"), Event.Name.IsNone());

    Player.Update(0.6);
    TestEqual(TEXT("key event count"), Player.GetEventCount(), 2);
    TestTrue(TEXT("key event readable"), Player.GetEvent(1, Event));
    TestEqual(TEXT("key kind"), int32(Event.Kind), 3);
    TestTrue(TEXT("key name"), Event.Name == FName(TEXT("footstep")));
    TestTrue(TEXT("key animation"), Event.Animation == FName(TEXT("walk")));
    TestEqual(TEXT("key int"), Event.IntValue, 7);
    TestTrue(TEXT("key float"), FMath::IsNearlyEqual(Event.FloatValue, 2.5f, 1e-5f));
    TestEqual(TEXT("key string"), Event.StringValue, FString(TEXT("step")));
    TestTrue(TEXT("key audio"), Event.Audio == FName(TEXT("step.wav")));
    TestTrue(TEXT("key volume"), FMath::IsNearlyEqual(Event.Volume, 0.8f, 1e-5f));
    TestTrue(TEXT("key balance"), FMath::IsNearlyEqual(Event.Balance, -0.2f, 1e-5f));
    TestTrue(TEXT("key time"), FMath::IsNearlyEqual(Event.Time, 0.5f, 1e-5f));

    Player.Update(0.5);
    TestEqual(TEXT("complete event count"), Player.GetEventCount(), 4);
    TestTrue(TEXT("done event readable"), Player.GetEvent(2, Event));
    TestEqual(TEXT("done kind"), int32(Event.Kind), 3);
    TestTrue(TEXT("done name"), Event.Name == FName(TEXT("done")));
    TestEqual(TEXT("done int"), Event.IntValue, 9);
    TestTrue(TEXT("complete readable"), Player.GetEvent(3, Event));
    TestEqual(TEXT("complete kind"), int32(Event.Kind), 2);

    Player.ClearEvents();
    TestEqual(TEXT("events cleared"), Player.GetEventCount(), 0);
    return true;
}

#endif
