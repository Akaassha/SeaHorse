#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/Presentation/SHHandRevealPawn.h"
#include "Gameplay/Presentation/HandRevealWidget.h"
#include "Gameplay/Components/CardsLayoutComponent.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/SHHand.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"
#include "Widgets/SViewport.h"

namespace
{
constexpr float HandRevealCameraRestoreGuardSeconds = 3.0f;

bool IsRevealPresentationReady(const ASHHandRevealPawn* Pawn, const TArray<FSHRevealedHandCard>& Cards)
{
	return IsValid(Pawn) && !Cards.IsEmpty() && Pawn->GetPresentationCards().Num() == Cards.Num() &&
		!Cards.ContainsByPredicate([](const FSHRevealedHandCard& Entry) { return !IsValid(Entry.SourceCard); });
}

bool IsComparisonPresentationReady(const ASHHandRevealPawn* Pawn,
	const TArray<FSHRevealedHandCard>& LargerCards, const TArray<FSHRevealedHandCard>& ReceivingCards)
{
	return IsValid(Pawn) && !LargerCards.IsEmpty() &&
		Pawn->GetPresentationCards().Num() == LargerCards.Num() + ReceivingCards.Num() &&
		!LargerCards.ContainsByPredicate([](const FSHRevealedHandCard& Entry) { return !IsValid(Entry.SourceCard); }) &&
		!ReceivingCards.ContainsByPredicate([](const FSHRevealedHandCard& Entry) { return !IsValid(Entry.SourceCard); });
}
}

void ASHPlayerController::AutoManageActiveCameraTarget(AActor* SuggestedTarget)
{
	// Diego temporarily possesses the selected player's private presentation pawn.
	// The replicated ClientRestart which restores their ordinary pawn can arrive
	// after ClientEndHandReveal. Keep that late restart from replacing the table
	// camera which ClientEndHandReveal has just restored.
	if (HandRevealCameraRestoreTarget.IsValid())
	{
		MaintainHandRevealCameraRestore();
		return;
	}
	// Possession/ClientRestart may arrive before the private snapshot. Preserve the
	// table camera until ClientBeginHandReveal has saved it and prepared the copies.
	if (Cast<ASHHandRevealPawn>(SuggestedTarget)) { return; }
	Super::AutoManageActiveCameraTarget(SuggestedTarget);
}

void ASHPlayerController::BeginHandRevealCameraRestore(AActor* RestoreTarget)
{
	GetWorldTimerManager().ClearTimer(HandRevealCameraRestoreTimer);
	HandRevealCameraRestoreTarget = IsValid(RestoreTarget) ? RestoreTarget : nullptr;
	MaintainHandRevealCameraRestore();
	if (HandRevealCameraRestoreTarget.IsValid())
	{
		GetWorldTimerManager().SetTimer(HandRevealCameraRestoreTimer, this,
			&ASHPlayerController::FinishHandRevealCameraRestore,
			HandRevealCameraRestoreGuardSeconds, false);
	}
}

void ASHPlayerController::MaintainHandRevealCameraRestore()
{
	if (AActor* RestoreTarget = HandRevealCameraRestoreTarget.Get();
		IsValid(RestoreTarget) && GetViewTarget() != RestoreTarget)
	{
		SetViewTarget(RestoreTarget);
	}
}

void ASHPlayerController::FinishHandRevealCameraRestore()
{
	GetWorldTimerManager().ClearTimer(HandRevealCameraRestoreTimer);
	AActor* RestoreTarget = HandRevealCameraRestoreTarget.Get();
	HandRevealCameraRestoreTarget.Reset();
	if (IsValid(RestoreTarget) && GetViewTarget() != RestoreTarget)
	{
		SetViewTarget(RestoreTarget);
	}
}

void ASHPlayerController::PreparePendingHandReveal(FGuid SessionId)
{
	if (!SessionId.IsValid() || PendingHandRevealSession == SessionId) { return; }
	FinishHandRevealCameraRestore();
	PendingHandRevealSession = SessionId;
	ViewTargetBeforeHandReveal = GetViewTarget();
	bCursorBeforeHandReveal = bShowMouseCursor;
	bAutoCameraBeforeHandReveal = bAutoManageActiveCameraTarget;
	bClickEventsBeforeHandReveal = bEnableClickEvents;
	bMouseOverBeforeHandReveal = bEnableMouseOverEvents;
}

void ASHPlayerController::ClientBeginHandReveal_Implementation(FGuid SessionId, ASHHand* SourceHand,
	ASHHandRevealPawn* RevealPawn, const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder,
	bool bCanFinish, TSubclassOf<UHandRevealWidget> WidgetClass)
{
	if (!IsLocalController() || !GetLocalPlayer() || !SessionId.IsValid()) { return; }
	if (ActiveHandRevealSession == SessionId && !IsValid(RevealPawn)) { return; }
	if (ActiveHandRevealSession == SessionId && ActiveHandRevealPawn == RevealPawn)
	{
		ClientUpdateHandReveal_Implementation(SessionId, Cards, bCanReorder);
		if (ActiveHandRevealSession == SessionId && IsRevealPresentationReady(ActiveHandRevealPawn, Cards))
		{ ServerAcknowledgeHandReveal(SessionId); }
		return;
	}
	if (ActiveHandRevealSession.IsValid()) { ClientEndHandReveal_Implementation(ActiveHandRevealSession); }
	if (PendingHandRevealSession.IsValid() && PendingHandRevealSession != SessionId)
	{
		ClientEndHandReveal_Implementation(PendingHandRevealSession);
	}
	PreparePendingHandReveal(SessionId);
	// Actor references on an RPC may arrive before the new owner-only pawn's
	// actor channel. Remember the session before returning so its later close can
	// still restore possession and the table camera if every retry remains unmapped.
	if (!IsValid(RevealPawn)) { return; }
	CloseCardInfo();
	ClearLocalEffectSelectionState();
	StopPairTargetingIndicator();
	ResetHandCursorHover();
	PointerPressedCard.Reset();
	bConsumeEffectSelectionRelease = false;
	// Let the existing Blueprint release path close its drag gate. Clear the
	// native drag immediately so that this release cannot commit a table drop.
	if (PlayerInput && IsValid(LocallyDraggedCard))
	{
		PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.f));
	}
	LocallyDraggedCard = nullptr;
	LastPreviewCard = nullptr;
	LastPreviewInsertIndex = INDEX_NONE;
	for (TActorIterator<ASHHand> It(GetWorld()); It; ++It)
	{
		if (auto* Layout = It->FindComponentByClass<USHHandCardsLayoutComponent>())
		{
			Layout->SetDraggedCard(nullptr);
			Layout->SetSelectedCardIndex(INDEX_NONE);
			Layout->SetFocusedCardIndex(INDEX_NONE);
		}
	}
	ActiveHandRevealSession = SessionId;
	ActiveHandRevealPawn = RevealPawn;
	bCanFinishHandReveal = bCanFinish;
	bAutoManageActiveCameraTarget = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	bShowMouseCursor = true;
	RevealPawn->InitializePresentation(this, SessionId, Cards, bCanReorder, bCanFinish);
	if (ActiveHandRevealSession != SessionId || !IsValid(ActiveHandRevealPawn)) { return; }
	SetViewTarget(RevealPawn);
	if (GetWorld()->GetGameViewport())
	{
		if (!WidgetClass || WidgetClass->HasAnyClassFlags(CLASS_Abstract)) { WidgetClass = UHandRevealWidget::StaticClass(); }
		UHandRevealWidget* CreatedWidget = CreateWidget<UHandRevealWidget>(this, WidgetClass);
		if (ActiveHandRevealSession != SessionId) { return; }
		ActiveHandRevealWidget = CreatedWidget;
		if (ActiveHandRevealWidget)
		{
			ActiveHandRevealWidget->InitializeReveal(RevealPawn, SourceHand, bCanFinish);
			if (ActiveHandRevealSession != SessionId || ActiveHandRevealWidget != CreatedWidget) { return; }
			ActiveHandRevealWidget->AddToViewport(250);
		}
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	if (ActiveHandRevealSession == SessionId && IsRevealPresentationReady(ActiveHandRevealPawn, Cards))
	{ ServerAcknowledgeHandReveal(SessionId); }
}

void ASHPlayerController::ClientUpdateHandReveal_Implementation(FGuid SessionId,
	const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder)
{
	if (!SessionId.IsValid() || SessionId != ActiveHandRevealSession || !IsValid(ActiveHandRevealPawn)) { return; }
	ActiveHandRevealPawn->ApplySnapshot(Cards, bCanReorder);
	if (ActiveHandRevealWidget) { ActiveHandRevealWidget->RefreshReveal(); }
}

void ASHPlayerController::ClientBeginHandComparison_Implementation(FGuid SessionId,
	ASHHand* LargerHand, ASHHand* ReceivingHand, ASHHandRevealPawn* RevealPawn,
	const TArray<FSHRevealedHandCard>& LargerCards,
	const TArray<FSHRevealedHandCard>& ReceivingCards, int32 RemainingTransfers,
	bool bCanTransfer, TSubclassOf<UHandRevealWidget> WidgetClass)
{
	if (!IsLocalController() || !GetLocalPlayer() || !SessionId.IsValid())
	{
		return;
	}
	if (ActiveHandRevealSession == SessionId && !IsValid(RevealPawn)) { return; }
	if (ActiveHandRevealSession == SessionId && ActiveHandRevealPawn == RevealPawn)
	{
		ClientUpdateHandComparison_Implementation(SessionId, LargerCards, ReceivingCards,
			RemainingTransfers, bCanTransfer);
		if (ActiveHandRevealSession == SessionId &&
			IsComparisonPresentationReady(ActiveHandRevealPawn, LargerCards, ReceivingCards))
		{
			ServerAcknowledgeHandReveal(SessionId);
		}
		return;
	}
	if (ActiveHandRevealSession.IsValid()) { ClientEndHandReveal_Implementation(ActiveHandRevealSession); }
	if (PendingHandRevealSession.IsValid() && PendingHandRevealSession != SessionId)
	{
		ClientEndHandReveal_Implementation(PendingHandRevealSession);
	}
	PreparePendingHandReveal(SessionId);
	if (!IsValid(LargerHand) || !IsValid(ReceivingHand) || !IsValid(RevealPawn)) { return; }
	CloseCardInfo();
	ClearLocalEffectSelectionState();
	StopPairTargetingIndicator();
	ResetHandCursorHover();
	PointerPressedCard.Reset();
	bConsumeEffectSelectionRelease = false;
	if (PlayerInput && IsValid(LocallyDraggedCard))
	{
		PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 0.f));
	}
	LocallyDraggedCard = nullptr;
	LastPreviewCard = nullptr;
	LastPreviewInsertIndex = INDEX_NONE;
	for (TActorIterator<ASHHand> It(GetWorld()); It; ++It)
	{
		if (auto* Layout = It->FindComponentByClass<USHHandCardsLayoutComponent>())
		{
			Layout->SetDraggedCard(nullptr);
			Layout->SetSelectedCardIndex(INDEX_NONE);
			Layout->SetFocusedCardIndex(INDEX_NONE);
		}
	}

	ActiveHandRevealSession = SessionId;
	ActiveHandRevealPawn = RevealPawn;
	bCanFinishHandReveal = false;
	bAutoManageActiveCameraTarget = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	bShowMouseCursor = true;
	RevealPawn->InitializeComparisonPresentation(this, SessionId, LargerCards, ReceivingCards,
		RemainingTransfers, bCanTransfer);
	if (ActiveHandRevealSession != SessionId || !IsValid(ActiveHandRevealPawn)) { return; }
	SetViewTarget(RevealPawn);
	if (GetWorld()->GetGameViewport())
	{
		if (!WidgetClass || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			WidgetClass = UHandRevealWidget::StaticClass();
		}
		UHandRevealWidget* CreatedWidget = CreateWidget<UHandRevealWidget>(this, WidgetClass);
		if (ActiveHandRevealSession != SessionId) { return; }
		ActiveHandRevealWidget = CreatedWidget;
		if (ActiveHandRevealWidget)
		{
			ActiveHandRevealWidget->InitializeComparison(RevealPawn, LargerHand, ReceivingHand);
			if (ActiveHandRevealSession != SessionId || ActiveHandRevealWidget != CreatedWidget) { return; }
			ActiveHandRevealWidget->AddToViewport(250);
		}
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	if (ActiveHandRevealSession == SessionId &&
		IsComparisonPresentationReady(ActiveHandRevealPawn, LargerCards, ReceivingCards))
	{
		ServerAcknowledgeHandReveal(SessionId);
	}
}

void ASHPlayerController::ClientUpdateHandComparison_Implementation(FGuid SessionId,
	const TArray<FSHRevealedHandCard>& LargerCards,
	const TArray<FSHRevealedHandCard>& ReceivingCards, int32 RemainingTransfers, bool bCanTransfer)
{
	if (!SessionId.IsValid() || SessionId != ActiveHandRevealSession || !IsValid(ActiveHandRevealPawn)) { return; }
	ActiveHandRevealPawn->ApplyComparisonSnapshot(LargerCards, ReceivingCards, RemainingTransfers, bCanTransfer);
	if (ActiveHandRevealWidget) { ActiveHandRevealWidget->RefreshReveal(); }
}

void ASHPlayerController::ClientEndHandReveal_Implementation(FGuid SessionId)
{
	if (!SessionId.IsValid() ||
		(SessionId != ActiveHandRevealSession && SessionId != PendingHandRevealSession)) { return; }
	// A delayed close for an earlier pending session must never dismiss a newer,
	// already opened presentation.
	if (ActiveHandRevealSession.IsValid() && SessionId != ActiveHandRevealSession) { return; }
	AActor* RestoreTarget = ViewTargetBeforeHandReveal.Get();
	ASHHandRevealPawn* ClosingRevealPawn = ActiveHandRevealPawn;
	if (!IsValid(RestoreTarget))
	{
		APawn* CurrentPawn = GetPawn();
		if (IsValid(CurrentPawn) && CurrentPawn != ClosingRevealPawn) { RestoreTarget = CurrentPawn; }
	}
	ActiveHandRevealSession.Invalidate();
	PendingHandRevealSession.Invalidate();
	bCanFinishHandReveal = false;
	if (ActiveHandRevealWidget) { ActiveHandRevealWidget->RemoveFromParent(); ActiveHandRevealWidget = nullptr; }
	if (IsValid(ActiveHandRevealPawn)) { ActiveHandRevealPawn->ClearPresentation(); }
	ActiveHandRevealPawn = nullptr;
	bAutoManageActiveCameraTarget = bAutoCameraBeforeHandReveal;
	bShowMouseCursor = bCursorBeforeHandReveal;
	bEnableClickEvents = bClickEventsBeforeHandReveal;
	bEnableMouseOverEvents = bMouseOverBeforeHandReveal;
	BeginHandRevealCameraRestore(RestoreTarget);
	ViewTargetBeforeHandReveal.Reset();
	bConsumeEffectSelectionRelease = false;
	ResetHandCursorHover();
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetWidgetToFocus(Viewport->GetGameViewportWidget());
		SetInputMode(InputMode);
	}
}

void ASHPlayerController::FinishHandReveal()
{
	if (ActiveHandRevealSession.IsValid() && bCanFinishHandReveal) { ServerFinishHandReveal(ActiveHandRevealSession); }
}

void ASHPlayerController::ServerFinishHandReveal_Implementation(FGuid SessionId)
{
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>())
	{
		Mode->FinishHandReveal(GetPlayerState<ASHPlayerState>(), SessionId);
	}
}

void ASHPlayerController::ServerAcknowledgeHandReveal_Implementation(FGuid SessionId)
{
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>())
	{
		Mode->AcknowledgeHandReveal(GetPlayerState<ASHPlayerState>(), SessionId);
	}
}

void ASHPlayerController::ServerReorderRevealedHand_Implementation(FGuid SessionId, ASHCard* Card, int32 InsertIndex)
{
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>())
	{
		Mode->ReorderRevealedHand(GetPlayerState<ASHPlayerState>(), SessionId, Card, InsertIndex);
	}
}

void ASHPlayerController::ServerTransferComparedHandCard_Implementation(FGuid SessionId, ASHCard* Card, int32 InsertIndex)
{
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>())
	{
		Mode->TransferComparedHandCard(GetPlayerState<ASHPlayerState>(), SessionId, Card, InsertIndex);
	}
}
