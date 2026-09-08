#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Online/SHSteamAvatarSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHAvatarLifecycleTest, "SeaHorse.Avatars.RequestLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHAvatarLifecycleTest::RunTest(const FString& Parameters)
{
    auto* Instance = NewObject<UGameInstance>();
    auto* Service = NewObject<USHSteamAvatarSubsystem>(Instance);
    int32 InvalidCalls = 0;
    Service->RequestAvatar(nullptr, [this, &InvalidCalls](UTexture2D* Texture, const FString& Error)
    {
        ++InvalidCalls;
        TestNull(TEXT("Missing player returns no texture"), Texture);
        TestFalse(TEXT("Missing player provides an error"), Error.IsEmpty());
    });
    TestEqual(TEXT("Invalid requests finish immediately exactly once"), InvalidCalls, 1);

    // Model outstanding downloads without requiring Steam or downloading external images.
    int32 Completed = 0;
    int32 ReentrantCompleted = 0;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto& Request = Service->Requests.AddDefaulted_GetRef();
        Request.Completion = [this, Service, &Completed, &ReentrantCompleted](UTexture2D* Texture, const FString& Error)
        {
            ++Completed;
            TestNull(TEXT("Shutdown does not return an unfinished image"), Texture);
            TestFalse(TEXT("Shutdown provides a failure reason"), Error.IsEmpty());
            Service->RequestAvatar(nullptr, [&ReentrantCompleted](UTexture2D*, const FString&) { ++ReentrantCompleted; });
        };
    }
    Service->Deinitialize();
    TestEqual(TEXT("Every pending download completes on shutdown"), Completed, 2);
    TestEqual(TEXT("Requests from shutdown callbacks are rejected safely"), ReentrantCompleted, 2);
    TestTrue(TEXT("No requests retained after shutdown"), Service->Requests.IsEmpty());
    TestFalse(TEXT("Ticker is released"), Service->TickHandle.IsValid());
    return true;
}
#endif
