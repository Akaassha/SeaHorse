#include "Gameplay/Cards/Tasks/RevealHandEffectTask.h"

#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHGameState.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Presentation/HandRevealWidget.h"
#include "Gameplay/Presentation/SHHandRevealPawn.h"
#include "Gameplay/SHHand.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASHPlayerState* URevealHandEffectTask::FindHandPlayer(ASHHand* Hand) const
{
	const ASHGameMode* Mode = GetTypedOuter<ASHGameMode>();
	const ASHGameState* State = IsValid(Mode) ? Mode->GetGameState<ASHGameState>() : nullptr;
	if (!IsValid(State) || !IsValid(Hand) || Hand->IsLogicalNPC()) { return nullptr; }
	for (APlayerState* Entry : State->PlayerArray)
	{
		ASHPlayerState* Player = Cast<ASHPlayerState>(Entry);
		if (IsValid(Player) && Player->GetHand() == Hand) { return Player; }
	}
	return nullptr;
}

bool URevealHandEffectTask::CanTargetHand(ASHHand* Hand) const
{
	const ASHGameMode* Mode = GetTypedOuter<ASHGameMode>();
	const ASHGameState* State = IsValid(Mode) ? Mode->GetGameState<ASHGameState>() : nullptr;
	if (!IsValid(State) || State->IsGameEnded() || !IsValid(GetActivatingPlayer()) ||
		!IsValid(Hand) || Hand == GetActivatingPlayer()->GetHand() || Hand->GetCardCount() == 0 ||
		Hand->IsProtectedFromCardEffects() || !State->GetParticipantHands().Contains(Hand))
	{
		return false;
	}
	if (Hand->IsLogicalNPC()) { return true; }
	ASHPlayerState* Player = FindHandPlayer(Hand);
	return IsValid(Player) && IsValid(Cast<ASHPlayerController>(Player->GetOwner()));
}

void URevealHandEffectTask::StartEffect_Implementation()
{
	if (IsFinished()) { return; }
	ASHGameMode* Mode = GetTypedOuter<ASHGameMode>();
	ASHGameState* State = IsValid(Mode) ? Mode->GetGameState<ASHGameState>() : nullptr;
	ASHPlayerController* Viewer = IsValid(GetActivatingPlayer())
		? Cast<ASHPlayerController>(GetActivatingPlayer()->GetOwner()) : nullptr;
	if (!IsValid(State) || !IsValid(Viewer)) { FinishEffect(); return; }
	TArray<ASHHand*> Candidates;
	for (ASHHand* Hand : State->GetParticipantHands())
	{
		if (CanTargetHand(Hand)) { Candidates.Add(Hand); }
	}
	if (Candidates.IsEmpty()) { FinishEffect(); return; }
	RequestParticipantSelection(Candidates, EPlayerSelectionPurpose::HandRevealTarget);
}

void URevealHandEffectTask::HandleParticipantSelected(ASHHand* Hand)
{
	if (IsFinished() || IsValid(Source)) { return; }
	if (!CanTargetHand(Hand)) { FinishEffect(); return; }
	Source = Hand;
	TargetPlayer = FindHandPlayer(Hand);
	bRevealingHuman = !Hand->IsLogicalNPC();
	PlayActivationVFX();
	ResolveAfterPresentation();
}

TArray<FSHRevealedHandCard> URevealHandEffectTask::MakeSnapshot() const
{
	TArray<FSHRevealedHandCard> Snapshot;
	if (!IsValid(Source)) { return Snapshot; }
	for (ASHCard* Card : Source->GetCards())
	{
		if (!IsValid(Card) || Card->GetOwningHand() != Source || Card->GetCardZone() != ECardZone::Hand) { continue; }
		FSHRevealedHandCard& Entry = Snapshot.AddDefaulted_GetRef();
		Entry.SourceCard = Card;
		Entry.CardDefinition = Card->GetCardDefinition();
		Entry.CardActorClass = Card->GetClass();
	}
	return Snapshot;
}

void URevealHandEffectTask::ResolveAbility()
{
	if (IsFinished() || bOpened || bClosed) { return; }
	ViewerController = IsValid(GetActivatingPlayer()) ? Cast<ASHPlayerController>(GetActivatingPlayer()->GetOwner()) : nullptr;
	if (!IsValid(ViewerController) || !CanTargetHand(Source)) { FinishEffect(); return; }
	// A disconnect during the activation animation must not silently switch a human reveal into a BN reveal.
	if (bRevealingHuman && (!IsValid(TargetPlayer) || Source->IsLogicalNPC() || FindHandPlayer(Source) != TargetPlayer)) { FinishEffect(); return; }
	TargetController = IsValid(TargetPlayer) ? Cast<ASHPlayerController>(TargetPlayer->GetOwner()) : nullptr;
	if (!Source->IsLogicalNPC() && !IsValid(TargetController)) { FinishEffect(); return; }

	const URevealHandEffectFragment* Fragment = IsValid(GetCardA())
		? Cast<URevealHandEffectFragment>(UCardDefinition::FindFragmentByClass(
			GetCardA()->GetCardDefinition(), URevealHandEffectFragment::StaticClass())) : nullptr;
	TSubclassOf<ASHHandRevealPawn> PawnClass = IsValid(Fragment) && Fragment->RevealPawnClass
		? Fragment->RevealPawnClass.Get() : ASHHandRevealPawn::StaticClass();
	SessionWidgetClass = IsValid(Fragment) && Fragment->RevealWidgetClass
		? Fragment->RevealWidgetClass.Get() : UHandRevealWidget::StaticClass();

	auto SpawnPrivatePawn = [this, PawnClass](ASHPlayerController* Controller)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
		FActorSpawnParameters Params;
		Params.Owner = Controller;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASHHandRevealPawn* Pawn = GetWorld()->SpawnActor<ASHHandRevealPawn>(PawnClass,
			CameraLocation, CameraRotation, Params);
		if (IsValid(Pawn))
		{
			// Enforce privacy even if a designer changes inherited defaults on a Blueprint subclass.
			Pawn->bAlwaysRelevant = false;
			Pawn->bOnlyRelevantToOwner = true;
			Pawn->SetReplicates(true);
			Pawn->SetReplicateMovement(false);
			Pawn->ForceNetUpdate();
		}
		return Pawn;
	};

	ViewerPawn = SpawnPrivatePawn(ViewerController);
	if (!IsValid(ViewerPawn)) { CompleteSession(); return; }
	if (IsValid(TargetController))
	{
		TargetPawn = SpawnPrivatePawn(TargetController);
		if (!IsValid(TargetPawn)) { CompleteSession(); return; }
	}

	SessionId = FGuid::NewGuid();
	bOpened = true;
	LastSnapshot = MakeSnapshot();
	bLastCanReorder = IsValid(TargetController) && Source->HasSeaHorseCard();
	bTargetReady = !bRevealingHuman;
	// A controller's reliable RPC can reach a client before the pawn's actor
	// channel. Retain the session and retry until both private views acknowledge it.
	GetWorld()->GetTimerManager().SetTimer(SessionWatchTimer, this, &URevealHandEffectTask::RefreshSession, 0.2f, true);
	GetWorld()->GetTimerManager().SetTimer(PresentationTimeoutTimer, this, &URevealHandEffectTask::OnPresentationTimeout, 15.f, false);
	TryPresentParticipants();
}

bool URevealHandEffectTask::IsSessionFor(ASHPlayerState* Player, FGuid InSessionId) const
{
	return !IsFinished() && bOpened && !bClosed && SessionId.IsValid() && InSessionId == SessionId &&
		IsValid(Player) && (Player == GetActivatingPlayer() || Player == TargetPlayer);
}

void URevealHandEffectTask::AcknowledgePresentation(ASHPlayerState* Player, FGuid InSessionId)
{
	if (!IsSessionFor(Player, InSessionId)) { return; }
	if (Player == GetActivatingPlayer())
	{
		bViewerReady = true;
		bShownToViewer = true;
	}
	else if (Player == TargetPlayer) { bTargetReady = true; }
	if (bViewerReady && bTargetReady)
	{
		GetWorld()->GetTimerManager().ClearTimer(PresentationTimeoutTimer);
	}
}

void URevealHandEffectTask::TryPresentParticipants()
{
	if (IsFinished() || !bOpened || bClosed) { return; }
	if (!bViewerReady && IsValid(ViewerController) && IsValid(ViewerPawn))
	{
		ViewerController->ClientBeginHandReveal(SessionId, Source, ViewerPawn, LastSnapshot, false, true, SessionWidgetClass);
	}
	if (bClosed || IsFinished()) { return; }
	if (!bTargetReady && IsValid(TargetController) && IsValid(TargetPawn))
	{
		TargetController->ClientBeginHandReveal(SessionId, Source, TargetPawn, LastSnapshot, bLastCanReorder, false, SessionWidgetClass);
	}
}

void URevealHandEffectTask::OnPresentationTimeout()
{
	if (!IsFinished() && !bClosed && (!bViewerReady || !bTargetReady))
	{
		if (!bShownToViewer && IsValid(ViewerController))
		{
			ViewerController->ClientShowCardEffectMessage(FText::FromString(
				TEXT("Nie udało się otworzyć podglądu kart. Spróbuj ponownie.")));
		}
		CompleteSession();
	}
}

void URevealHandEffectTask::FinishViewing(ASHPlayerState* Player, FGuid InSessionId)
{
	if (IsSessionFor(Player, InSessionId) && Player == GetActivatingPlayer()) { CompleteSession(); }
}

void URevealHandEffectTask::Reorder(ASHPlayerState* Player, FGuid InSessionId, ASHCard* Card, int32 InsertIndex)
{
	if (!IsSessionFor(Player, InSessionId) || !bTargetReady || Player != TargetPlayer || !IsValid(TargetController) ||
		Player->GetOwner() != TargetController ||
		!IsValid(Source) || Player->GetHand() != Source || Source->IsLogicalNPC() || !Source->HasSeaHorseCard() ||
		!IsValid(Card) || Card->GetOwningHand() != Source || Card->GetCardZone() != ECardZone::Hand ||
		!Source->ContainsCard(Card) || InsertIndex < 0 || InsertIndex >= Source->GetCardCount())
	{
		return;
	}
	if (Source->ReorderCard(Card, InsertIndex)) { PublishSnapshot(); }
}

void URevealHandEffectTask::PublishSnapshot()
{
	if (IsFinished() || !bOpened || bClosed || !IsValid(Source)) { return; }
	const TArray<FSHRevealedHandCard> Snapshot = MakeSnapshot();
	const bool bCanReorder = IsValid(TargetController) && Source->HasSeaHorseCard();
	bool bChanged = Snapshot.Num() != LastSnapshot.Num() || bCanReorder != bLastCanReorder;
	for (int32 Index = 0; !bChanged && Index < Snapshot.Num(); ++Index)
	{
		bChanged = Snapshot[Index].SourceCard != LastSnapshot[Index].SourceCard ||
			Snapshot[Index].CardDefinition != LastSnapshot[Index].CardDefinition ||
			Snapshot[Index].CardActorClass != LastSnapshot[Index].CardActorClass;
	}
	if (!bChanged) { return; }
	LastSnapshot = Snapshot;
	bLastCanReorder = bCanReorder;
	if (bViewerReady && IsValid(ViewerController)) { ViewerController->ClientUpdateHandReveal(SessionId, LastSnapshot, false); }
	if (bClosed || IsFinished()) { return; }
	if (bTargetReady && IsValid(TargetController)) { TargetController->ClientUpdateHandReveal(SessionId, LastSnapshot, bCanReorder); }
}

void URevealHandEffectTask::RefreshSession()
{
	if (IsFinished() || bClosed) { CloseSession(); return; }
	const ASHGameState* State = GetWorld()->GetGameState<ASHGameState>();
	if (!IsValid(State) || State->IsGameEnded() || !IsValid(GetActivatingPlayer()) ||
		!State->PlayerArray.Contains(GetActivatingPlayer()) || !IsValid(ViewerController) ||
		GetActivatingPlayer()->GetOwner() != ViewerController || !IsValid(ViewerPawn) || !CanTargetHand(Source))
	{
		CompleteSession();
		return;
	}
	if (bRevealingHuman && (!IsValid(TargetPlayer) || !State->PlayerArray.Contains(TargetPlayer) ||
		TargetPlayer->GetHand() != Source || Source->IsLogicalNPC() || !IsValid(TargetController) ||
		TargetPlayer->GetOwner() != TargetController || !IsValid(TargetPawn)))
	{
		CompleteSession();
		return;
	}
	PublishSnapshot();
	TryPresentParticipants();
}

void URevealHandEffectTask::CloseSession(ASHPlayerState* DepartingPlayer)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SessionWatchTimer);
		GetWorld()->GetTimerManager().ClearTimer(PresentationTimeoutTimer);
	}
	if (bClosed) { return; }
	bClosed = true;
	if (IsValid(TargetController) && TargetPlayer != DepartingPlayer)
	{
		if (SessionId.IsValid()) { TargetController->ClientEndHandReveal(SessionId); }
	}
	if (IsValid(ViewerController) && GetActivatingPlayer() != DepartingPlayer && SessionId.IsValid())
	{
		ViewerController->ClientEndHandReveal(SessionId);
	}
	if (IsValid(TargetPawn)) { TargetPawn->Destroy(); }
	if (IsValid(ViewerPawn)) { ViewerPawn->Destroy(); }
	TargetPawn = nullptr;
	ViewerPawn = nullptr;
	LastSnapshot.Reset();
	SessionWidgetClass = nullptr;
}

void URevealHandEffectTask::CompleteSession(ASHPlayerState* DepartingPlayer)
{
	CloseSession(DepartingPlayer);
	if (!IsFinished()) { FinishEffect(); }
}

void URevealHandEffectTask::AbandonEffect()
{
	CloseSession();
	Super::AbandonEffect();
}

void URevealHandEffectTask::HandleParticipantDisconnected(ASHPlayerState* Player)
{
	if (Player == GetActivatingPlayer())
	{
		CloseSession(Player);
		Super::AbandonEffect();
	}
	else if (Player == TargetPlayer && IsValid(TargetPlayer)) { CompleteSession(Player); }
}
