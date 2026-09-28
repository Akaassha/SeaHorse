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
#include "Widgets/SViewport.h"

namespace
{
bool IsRevealPresentationReady(const ASHHandRevealPawn* Pawn, const TArray<FSHRevealedHandCard>& Cards)
{
	return IsValid(Pawn) && !Cards.IsEmpty() && Pawn->GetPresentationCards().Num() == Cards.Num() &&
		!Cards.ContainsByPredicate([](const FSHRevealedHandCard& Entry) { return !IsValid(Entry.SourceCard); });
}
}

void ASHPlayerController::AutoManageActiveCameraTarget(AActor* SuggestedTarget)
{
	// Possession/ClientRestart may arrive before the private snapshot. Preserve the
	// table camera until ClientBeginHandReveal has saved it and prepared the copies.
	if (Cast<ASHHandRevealPawn>(SuggestedTarget)) { return; }
	Super::AutoManageActiveCameraTarget(SuggestedTarget);
}

void ASHPlayerController::ClientBeginHandReveal_Implementation(FGuid SessionId, ASHHand* SourceHand,
	ASHHandRevealPawn* RevealPawn, const TArray<FSHRevealedHandCard>& Cards, bool bCanReorder,
	bool bCanFinish, TSubclassOf<UHandRevealWidget> WidgetClass)
{
	if (!IsLocalController() || !GetLocalPlayer() || !SessionId.IsValid()) { return; }
	// Actor references on an RPC may arrive before the new owner-only pawn's
	// actor channel. The server retries until this client acknowledges it.
	if (!IsValid(RevealPawn)) { return; }
	if (ActiveHandRevealSession == SessionId && ActiveHandRevealPawn == RevealPawn)
	{
		ClientUpdateHandReveal_Implementation(SessionId, Cards, bCanReorder);
		if (ActiveHandRevealSession == SessionId && IsRevealPresentationReady(ActiveHandRevealPawn, Cards))
		{ ServerAcknowledgeHandReveal(SessionId); }
		return;
	}
	ClientEndHandReveal_Implementation(ActiveHandRevealSession);
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
	ViewTargetBeforeHandReveal = GetViewTarget();
	bCursorBeforeHandReveal = bShowMouseCursor;
	bAutoCameraBeforeHandReveal = bAutoManageActiveCameraTarget;
	bClickEventsBeforeHandReveal = bEnableClickEvents;
	bMouseOverBeforeHandReveal = bEnableMouseOverEvents;
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

void ASHPlayerController::ClientEndHandReveal_Implementation(FGuid SessionId)
{
	if (!SessionId.IsValid() || SessionId != ActiveHandRevealSession) { return; }
	ActiveHandRevealSession.Invalidate();
	bCanFinishHandReveal = false;
	if (ActiveHandRevealWidget) { ActiveHandRevealWidget->RemoveFromParent(); ActiveHandRevealWidget = nullptr; }
	if (IsValid(ActiveHandRevealPawn)) { ActiveHandRevealPawn->ClearPresentation(); }
	ActiveHandRevealPawn = nullptr;
	bAutoManageActiveCameraTarget = bAutoCameraBeforeHandReveal;
	bShowMouseCursor = bCursorBeforeHandReveal;
	bEnableClickEvents = bClickEventsBeforeHandReveal;
	bEnableMouseOverEvents = bMouseOverBeforeHandReveal;
	if (ViewTargetBeforeHandReveal.IsValid()) { SetViewTarget(ViewTargetBeforeHandReveal.Get()); }
	else if (GetPawn()) { SetViewTarget(GetPawn()); }
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
