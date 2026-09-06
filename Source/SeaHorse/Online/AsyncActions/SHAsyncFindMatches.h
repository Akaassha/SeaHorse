#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Online/SHSessionTypes.h"
#include "SHAsyncFindMatches.generated.h"

class USHSessionSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHAsyncFindMatchesResult,
	const TArray<FSHSessionResult>&, Results, const FString&, Error);

/** Searches through the active online service and returns results when the search finishes. */
UCLASS()
class SEAHORSE_API USHAsyncFindMatches : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="SeaHorse|Sessions",
		meta=(BlueprintInternalUseOnly="true", WorldContext="WorldContextObject", DisplayName="Find Matches Async", Keywords="search rooms sessions lobby"))
	static USHAsyncFindMatches* FindMatchesAsync(const UObject* WorldContextObject, bool bLAN = false, int32 MaxResults = 50);
	virtual void Activate() override;
	// Success with an empty Results array means no available rooms were found.
	UPROPERTY(BlueprintAssignable) FSHAsyncFindMatchesResult OnSuccess;
	// Failed requests return an empty Results array and an explanation in Error.
	UPROPERTY(BlueprintAssignable) FSHAsyncFindMatchesResult OnFailure;
private:
	void Complete(bool bSuccess, const TArray<FSHSessionResult>& Results, const FString& Error);
	TWeakObjectPtr<USHSessionSubsystem> Subsystem;
	bool bSearchLAN = false;
	int32 SearchLimit = 50;
	bool bActivated = false;
	bool bCompleted = false;
};
