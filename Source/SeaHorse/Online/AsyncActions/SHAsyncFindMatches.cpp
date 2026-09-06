#include "Online/AsyncActions/SHAsyncFindMatches.h"
#include "Online/SHSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

USHAsyncFindMatches* USHAsyncFindMatches::FindMatchesAsync(const UObject* WorldContextObject, bool bLAN, int32 MaxResults)
{
	USHAsyncFindMatches* Node = NewObject<USHAsyncFindMatches>();
	Node->bSearchLAN = bLAN;
	Node->SearchLimit = MaxResults;
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (UGameInstance* Instance = World ? World->GetGameInstance() : nullptr)
	{
		Node->RegisterWithGameInstance(Instance);
		Node->Subsystem = Instance->GetSubsystem<USHSessionSubsystem>();
	}
	return Node;
}

void USHAsyncFindMatches::Activate()
{
	if (bActivated || bCompleted) { return; }
	bActivated = true;
	if (!Subsystem.IsValid())
	{
		Complete(false, {}, TEXT("Session subsystem is unavailable. Run this node in a game world."));
		return;
	}
	TWeakObjectPtr<USHAsyncFindMatches> WeakThis(this);
	Subsystem->FindMatchesWithCallback(bSearchLAN, SearchLimit,
		[WeakThis](bool bSuccess, const TArray<FSHSessionResult>& Results, const FString& Error)
		{
			if (WeakThis.IsValid()) { WeakThis->Complete(bSuccess, Results, Error); }
		});
}

void USHAsyncFindMatches::Complete(bool bSuccess, const TArray<FSHSessionResult>& Results, const FString& Error)
{
	if (bCompleted) { return; }
	bCompleted = true;
	if (bSuccess) { OnSuccess.Broadcast(Results, Error); }
	else { OnFailure.Broadcast(Results, Error); }
	SetReadyToDestroy();
}
