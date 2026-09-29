#include "Gameplay/Cards/Tasks/CompareHandsEffectTask.h"

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

ASHPlayerState* UCompareHandsEffectTask::FindHandPlayer(ASHHand* Hand) const
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

bool UCompareHandsEffectTask::CanTargetHand(ASHHand* Hand) const
{
	const ASHGameMode* Mode = GetTypedOuter<ASHGameMode>();
	const ASHGameState* State = IsValid(Mode) ? Mode->GetGameState<ASHGameState>() : nullptr;
	ASHHand* ActivatorHand = IsValid(GetActivatingPlayer()) ? GetActivatingPlayer()->GetHand() : nullptr;
	if (!IsValid(State) || State->IsGameEnded() || !IsValid(ActivatorHand) || !IsValid(Hand) ||
		Hand == ActivatorHand || Hand->IsLogicalNPC() || Hand->IsProtectedFromCardEffects() ||
		!State->GetParticipantHands().Contains(Hand))
	{
		return false;
	}
	ASHPlayerState* Player = FindHandPlayer(Hand);
	return IsValid(Player) && IsValid(Cast<ASHPlayerController>(Player->GetOwner()));
}

void UCompareHandsEffectTask::StartEffect_Implementation()
{
	if (IsFinished()) { return; }
	ASHGameMode* Mode = GetTypedOuter<ASHGameMode>();
	ASHGameState* State = IsValid(Mode) ? Mode->GetGameState<ASHGameState>() : nullptr;
	ASHPlayerController* Controller = IsValid(GetActivatingPlayer())
		? Cast<ASHPlayerController>(GetActivatingPlayer()->GetOwner()) : nullptr;
	if (!IsValid(State) || !IsValid(Controller)) { FinishEffect(); return; }

	TArray<ASHHand*> Candidates;
	for (ASHHand* Hand : State->GetParticipantHands())
	{
		if (CanTargetHand(Hand)) { Candidates.Add(Hand); }
	}
	if (Candidates.IsEmpty()) { FinishEffect(); return; }
	RequestParticipantSelection(Candidates, EPlayerSelectionPurpose::HandComparisonTarget);
}

void UCompareHandsEffectTask::HandleParticipantSelected(ASHHand* Hand)
{
	if (IsFinished() || IsValid(SelectedPlayer)) { return; }
	if (!CanTargetHand(Hand)) { FinishEffect(); return; }
	SelectedPlayer = FindHandPlayer(Hand);
	PlayActivationVFX();
	ResolveAfterPresentation();
}

TArray<FSHRevealedHandCard> UCompareHandsEffectTask::MakeSnapshot(const ASHHand* Hand) const
{
	TArray<FSHRevealedHandCard> Snapshot;
	if (!IsValid(Hand)) { return Snapshot; }
	for (ASHCard* Card : const_cast<ASHHand*>(Hand)->GetCards())
	{
		if (!IsValid(Card) || Card->GetOwningHand() != Hand || Card->GetCardZone() != ECardZone::Hand) { continue; }
		FSHRevealedHandCard& Entry = Snapshot.AddDefaulted_GetRef();
		Entry.SourceCard = Card;
		Entry.CardDefinition = Card->GetCardDefinition();
		Entry.CardActorClass = Card->GetClass();
	}
	return Snapshot;
}

void UCompareHandsEffectTask::ResolveAbility()
{
	if (IsFinished() || bOpened || bClosed) { return; }
	ASHHand* ActivatorHand = IsValid(GetActivatingPlayer()) ? GetActivatingPlayer()->GetHand() : nullptr;
	ASHHand* SelectedHand = IsValid(SelectedPlayer) ? SelectedPlayer->GetHand() : nullptr;
	ActivatorController = IsValid(GetActivatingPlayer())
		? Cast<ASHPlayerController>(GetActivatingPlayer()->GetOwner()) : nullptr;
	SelectedController = IsValid(SelectedPlayer) ? Cast<ASHPlayerController>(SelectedPlayer->GetOwner()) : nullptr;
	if (!IsValid(ActivatorController) || !IsValid(SelectedController) || !CanTargetHand(SelectedHand) ||
		!IsValid(ActivatorHand) || ActivatorHand->IsLogicalNPC())
	{
		FinishEffect();
		return;
	}

	const int32 ActivatorCount = ActivatorHand->GetCardCount();
	const int32 SelectedCount = SelectedHand->GetCardCount();
	if (ActivatorCount == SelectedCount)
	{
		bConsumePair = true;
		FinishEffect();
		return;
	}
	LargerHand = ActivatorCount > SelectedCount ? ActivatorHand : SelectedHand;
	ReceivingHand = ActivatorCount < SelectedCount ? ActivatorHand : SelectedHand;
	DrawingPlayer = ReceivingHand == ActivatorHand ? GetActivatingPlayer() : SelectedPlayer.Get();
	RemainingTransfers = FMath::Abs(ActivatorCount - SelectedCount);

	const UCompareHandsEffectFragment* Fragment = IsValid(GetCardA())
		? Cast<UCompareHandsEffectFragment>(UCardDefinition::FindFragmentByClass(
			GetCardA()->GetCardDefinition(), UCompareHandsEffectFragment::StaticClass())) : nullptr;
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
			Pawn->bAlwaysRelevant = false;
			Pawn->bOnlyRelevantToOwner = true;
			Pawn->SetReplicates(true);
			Pawn->SetReplicateMovement(false);
			Pawn->ForceNetUpdate();
		}
		return Pawn;
	};

	ActivatorPawn = SpawnPrivatePawn(ActivatorController);
	SelectedPawn = SpawnPrivatePawn(SelectedController);
	if (!IsValid(ActivatorPawn) || !IsValid(SelectedPawn)) { CompleteSession(); return; }

	SessionId = FGuid::NewGuid();
	bOpened = true;
	LastLargerSnapshot = MakeSnapshot(LargerHand);
	LastReceivingSnapshot = MakeSnapshot(ReceivingHand);
	GetWorld()->GetTimerManager().SetTimer(SessionWatchTimer, this,
		&UCompareHandsEffectTask::RefreshSession, 0.2f, true);
	GetWorld()->GetTimerManager().SetTimer(PresentationTimeoutTimer, this,
		&UCompareHandsEffectTask::OnPresentationTimeout, 15.f, false);
	TryPresentParticipants();
}

bool UCompareHandsEffectTask::IsSessionFor(ASHPlayerState* Player, FGuid InSessionId) const
{
	return !IsFinished() && bOpened && !bClosed && SessionId.IsValid() && InSessionId == SessionId &&
		IsValid(Player) && (Player == GetActivatingPlayer() || Player == SelectedPlayer);
}

void UCompareHandsEffectTask::AcknowledgePresentation(ASHPlayerState* Player, FGuid InSessionId)
{
	if (!IsSessionFor(Player, InSessionId)) { return; }
	if (Player == GetActivatingPlayer()) { bActivatorReady = true; }
	if (Player == SelectedPlayer) { bSelectedReady = true; }
	if (bActivatorReady && bSelectedReady)
	{
		GetWorld()->GetTimerManager().ClearTimer(PresentationTimeoutTimer);
	}
}

void UCompareHandsEffectTask::TryPresentParticipants()
{
	if (IsFinished() || !bOpened || bClosed) { return; }
	if (!bActivatorReady && IsValid(ActivatorController) && IsValid(ActivatorPawn))
	{
		ActivatorController->ClientBeginHandComparison(SessionId, LargerHand, ReceivingHand, ActivatorPawn,
			LastLargerSnapshot, LastReceivingSnapshot, RemainingTransfers,
			DrawingPlayer == GetActivatingPlayer(), SessionWidgetClass);
	}
	if (bClosed || IsFinished()) { return; }
	if (!bSelectedReady && IsValid(SelectedController) && IsValid(SelectedPawn))
	{
		SelectedController->ClientBeginHandComparison(SessionId, LargerHand, ReceivingHand, SelectedPawn,
			LastLargerSnapshot, LastReceivingSnapshot, RemainingTransfers,
			DrawingPlayer == SelectedPlayer, SessionWidgetClass);
	}
}

void UCompareHandsEffectTask::OnPresentationTimeout()
{
	if (!IsFinished() && !bClosed && (!bActivatorReady || !bSelectedReady))
	{
		ASHPlayerController* DrawingController = DrawingPlayer == GetActivatingPlayer()
			? ActivatorController.Get() : SelectedController.Get();
		if (IsValid(DrawingController))
		{
			DrawingController->ClientShowCardEffectMessage(FText::FromString(
				TEXT("Nie udało się otworzyć porównania talii. Spróbuj ponownie.")));
		}
		CompleteSession();
	}
}

void UCompareHandsEffectTask::TransferCard(ASHPlayerState* Player, FGuid InSessionId,
	ASHCard* Card, int32 InsertIndex)
{
	if (!IsSessionFor(Player, InSessionId) || !bActivatorReady || !bSelectedReady ||
		Player != DrawingPlayer || !IsValid(LargerHand) || !IsValid(ReceivingHand) ||
		RemainingTransfers <= 0 || !IsValid(Card) || Card->GetOwningHand() != LargerHand ||
		Card->GetCardZone() != ECardZone::Hand || !LargerHand->ContainsCard(Card) ||
		InsertIndex < 0 || InsertIndex > ReceivingHand->GetCardCount())
	{
		return;
	}

	ASHPlayerController* DrawingController = Cast<ASHPlayerController>(Player->GetOwner());
	ASHHandRevealPawn* DrawingPawn = Player == GetActivatingPlayer() ? ActivatorPawn.Get() : SelectedPawn.Get();
	if (!IsValid(DrawingController) || !IsValid(DrawingPawn))
	{
		return;
	}

	LargerHand->RemoveCard(Card);
	ReceivingHand->AddCard(Card, InsertIndex);
	--RemainingTransfers;
	bConsumePair = true;
	PublishSnapshot(true);
	if (RemainingTransfers == 0) { CompleteSession(); }
}

void UCompareHandsEffectTask::PublishSnapshot(bool bForce)
{
	if (IsFinished() || !bOpened || bClosed || !IsValid(LargerHand) || !IsValid(ReceivingHand)) { return; }
	const TArray<FSHRevealedHandCard> LargerSnapshot = MakeSnapshot(LargerHand);
	const TArray<FSHRevealedHandCard> ReceivingSnapshot = MakeSnapshot(ReceivingHand);
	auto Matches = [](const TArray<FSHRevealedHandCard>& A, const TArray<FSHRevealedHandCard>& B)
	{
		if (A.Num() != B.Num()) { return false; }
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].SourceCard != B[Index].SourceCard || A[Index].CardDefinition != B[Index].CardDefinition ||
				A[Index].CardActorClass != B[Index].CardActorClass) { return false; }
		}
		return true;
	};
	if (!bForce && Matches(LargerSnapshot, LastLargerSnapshot) && Matches(ReceivingSnapshot, LastReceivingSnapshot)) { return; }
	LastLargerSnapshot = LargerSnapshot;
	LastReceivingSnapshot = ReceivingSnapshot;
	if (bActivatorReady && IsValid(ActivatorController))
	{
		ActivatorController->ClientUpdateHandComparison(SessionId, LastLargerSnapshot, LastReceivingSnapshot,
			RemainingTransfers, DrawingPlayer == GetActivatingPlayer());
	}
	if (bSelectedReady && IsValid(SelectedController))
	{
		SelectedController->ClientUpdateHandComparison(SessionId, LastLargerSnapshot, LastReceivingSnapshot,
			RemainingTransfers, DrawingPlayer == SelectedPlayer);
	}
}

void UCompareHandsEffectTask::RefreshSession()
{
	if (IsFinished() || bClosed) { CloseSession(); return; }
	const ASHGameState* State = GetWorld()->GetGameState<ASHGameState>();
	if (!IsValid(State) || State->IsGameEnded() || !IsValid(GetActivatingPlayer()) || !IsValid(SelectedPlayer) ||
		!State->PlayerArray.Contains(GetActivatingPlayer()) || !State->PlayerArray.Contains(SelectedPlayer) ||
		GetActivatingPlayer()->GetHand() != (LargerHand == SelectedPlayer->GetHand() ? ReceivingHand : LargerHand) ||
		SelectedPlayer->GetHand() != (LargerHand == SelectedPlayer->GetHand() ? LargerHand : ReceivingHand) ||
		!IsValid(ActivatorController) || !IsValid(SelectedController) || !IsValid(ActivatorPawn) ||
		!IsValid(SelectedPawn))
	{
		CompleteSession();
		return;
	}
	PublishSnapshot();
	TryPresentParticipants();
}

void UCompareHandsEffectTask::CloseSession(ASHPlayerState* DepartingPlayer)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SessionWatchTimer);
		GetWorld()->GetTimerManager().ClearTimer(PresentationTimeoutTimer);
	}
	if (bClosed) { return; }
	bClosed = true;
	if (IsValid(SelectedController) && SelectedPlayer != DepartingPlayer)
	{
		if (SessionId.IsValid()) { SelectedController->ClientEndHandReveal(SessionId); }
	}
	if (IsValid(ActivatorController) && GetActivatingPlayer() != DepartingPlayer && SessionId.IsValid())
	{
		ActivatorController->ClientEndHandReveal(SessionId);
	}
	if (IsValid(SelectedPawn)) { SelectedPawn->Destroy(); }
	if (IsValid(ActivatorPawn)) { ActivatorPawn->Destroy(); }
	SelectedPawn = nullptr;
	ActivatorPawn = nullptr;
	LastLargerSnapshot.Reset();
	LastReceivingSnapshot.Reset();
	SessionWidgetClass = nullptr;
}

void UCompareHandsEffectTask::CompleteSession(ASHPlayerState* DepartingPlayer)
{
	CloseSession(DepartingPlayer);
	if (!IsFinished()) { FinishEffect(); }
}

void UCompareHandsEffectTask::AbandonEffect()
{
	CloseSession();
	Super::AbandonEffect();
}

void UCompareHandsEffectTask::HandleParticipantDisconnected(ASHPlayerState* Player)
{
	if (Player == GetActivatingPlayer())
	{
		CloseSession(Player);
		Super::AbandonEffect();
	}
	else if (Player == SelectedPlayer) { CompleteSession(Player); }
}
