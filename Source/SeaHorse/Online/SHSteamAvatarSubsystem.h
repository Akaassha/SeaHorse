#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHSteamAvatarSubsystem.generated.h"

class APlayerState;
class UTexture2D;

/** Local presentation cache. Each client downloads images directly from Steam. */
UCLASS()
class SEAHORSE_API USHSteamAvatarSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    using FCompletion = TFunction<void(UTexture2D*, const FString&)>;
    void RequestAvatar(APlayerState* Player, FCompletion Completion);

private:
    friend class FSHAvatarLifecycleTest;
    struct FRequest
    {
        TWeakObjectPtr<APlayerState> Player;
        double Deadline = 0;
        FCompletion Completion;
    };
    bool TickRequests(float DeltaTime);
    bool IsSteamAvailable() const;
    UTexture2D* ResolveAvatar(APlayerState* Player, bool& bFinished, FString& Error);
    UPROPERTY(Transient) TMap<FString, TObjectPtr<UTexture2D>> Cache;
    TSet<FString> RequestedUsers;
    TArray<FRequest> Requests;
    FTSTicker::FDelegateHandle TickHandle;
    bool bShuttingDown = false;
};
