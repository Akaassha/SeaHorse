#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "SHAsyncLoadSteamAvatar.generated.h"

class APlayerState;
class UTexture2D;
class USHSteamAvatarSubsystem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHSteamAvatarResult, UTexture2D*, Texture, const FString&, Error);

/** Returns a cached/downloaded 128px Steam avatar. Failure leaves UI fallback selection to the caller. */
UCLASS()
class SEAHORSE_API USHAsyncLoadSteamAvatar : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="SeaHorse|Avatars",
        meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject", DisplayName="Load Steam Avatar Async"))
    static USHAsyncLoadSteamAvatar* LoadSteamAvatarAsync(const UObject* WorldContextObject, APlayerState* PlayerState);
    virtual void Activate() override;
    UPROPERTY(BlueprintAssignable) FSHSteamAvatarResult OnSuccess;
    UPROPERTY(BlueprintAssignable) FSHSteamAvatarResult OnFailure;
private:
    void Complete(UTexture2D* Texture, const FString& Error);
    TWeakObjectPtr<USHSteamAvatarSubsystem> Subsystem;
    TWeakObjectPtr<APlayerState> Player;
    bool bActivated = false;
    bool bCompleted = false;
};
