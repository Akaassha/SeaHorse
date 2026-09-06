#pragma once

#include "CoreMinimal.h"
#include "SHSessionTypes.generated.h"

UENUM(BlueprintType)
enum class ESHSessionOperation : uint8 { None, Create, Find, Join, Leave, Start, Travel };

USTRUCT(BlueprintType)
struct SEAHORSE_API FSHSessionResult
{
	GENERATED_BODY()
	// Opaque handle into the latest search. Old results cannot join a different server.
	UPROPERTY(BlueprintReadOnly) FGuid ResultId;
	UPROPERTY(BlueprintReadOnly) FString ServerName;
	UPROPERTY(BlueprintReadOnly) FString HostName;
	UPROPERTY(BlueprintReadOnly) int32 CurrentPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayers = 0;
	UPROPERTY(BlueprintReadOnly) int32 PingMilliseconds = 0;
	UPROPERTY(BlueprintReadOnly) bool bIsLAN = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSHSessionOperationComplete, ESHSessionOperation, Operation, bool, bSuccess, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHSessionSearchComplete, bool, bSuccess, const TArray<FSHSessionResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHSessionStateChanged, ESHSessionOperation, Operation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHConnectionError, const FString&, Error);
