#include "Online/AsyncActions/SHAsyncLoadSteamAvatar.h"
#include "Online/SHSteamAvatarSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

USHAsyncLoadSteamAvatar* USHAsyncLoadSteamAvatar::LoadSteamAvatarAsync(const UObject* WorldContextObject, APlayerState* PlayerState)
{
    auto* Node = NewObject<USHAsyncLoadSteamAvatar>();
    Node->Player = PlayerState;
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (UGameInstance* Instance = World ? World->GetGameInstance() : nullptr)
    {
        Node->RegisterWithGameInstance(Instance);
        Node->Subsystem = Instance->GetSubsystem<USHSteamAvatarSubsystem>();
    }
    return Node;
}

void USHAsyncLoadSteamAvatar::Activate()
{
    if (bActivated || bCompleted) { return; }
    bActivated = true;
    if (!Subsystem.IsValid()) { Complete(nullptr, TEXT("Avatar subsystem is unavailable.")); return; }
    TWeakObjectPtr<USHAsyncLoadSteamAvatar> WeakThis(this);
    Subsystem->RequestAvatar(Player.Get(), [WeakThis](UTexture2D* Texture, const FString& Error)
    {
        if (WeakThis.IsValid()) { WeakThis->Complete(Texture, Error); }
    });
}

void USHAsyncLoadSteamAvatar::Complete(UTexture2D* Texture, const FString& Error)
{
    if (bCompleted) { return; }
    bCompleted = true;
    if (Texture) { OnSuccess.Broadcast(Texture, Error); }
    else { OnFailure.Broadcast(nullptr, Error); }
    SetReadyToDestroy();
}
