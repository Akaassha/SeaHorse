// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Board/VictoryStack.h"
#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/SHCard.h"
#include "SeaHorse/Gameplay/Board/SHTable.h"
#include "Kismet/GameplayStatics.h"
#include "SeaHorse/Gameplay/Core/SHGameMode.h"
#include "SeaHorse/Gameplay/Components/TurnComponent.h"
#include "SeaHorse/Gameplay/Components/CardsLayoutComponent.h"
#include "SeaHorse/Gameplay/Player/SHPlayerRepresentation.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "TimerManager.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "Components/MeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Gameplay/Presentation/CardReactionPrompt.h"
#include "Gameplay/Presentation/CardSelectionPrompt.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"

ASHPlayerController::ASHPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ASHPlayerController::ClientOfferCardReaction_Implementation(int32 OfferId, ASHCard* ReactionCard, ASHCard* TargetCard, TSubclassOf<UCardReactionPrompt> WidgetClass)
{
	if (!IsLocalController()) { return; }
	CloseCardInfo();
	ClientCloseCardReaction_Implementation(ActiveReactionOfferId);
	ActiveReactionOfferId = OfferId;
	bCursorBeforeReaction = bShowMouseCursor;
	if (!GetWorld()->GetGameViewport()) { return; }
	if (!WidgetClass || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogTemp, Warning, TEXT("Reaction offer %d has no concrete PromptWidgetClass; declining."), OfferId);
		ServerRespondToCardReaction(OfferId, false);
		return;
	}
	ActiveReactionPrompt = CreateWidget<UCardReactionPrompt>(this, WidgetClass);
	if (!ActiveReactionPrompt) { ServerRespondToCardReaction(OfferId, false); return; }
	ActiveReactionPrompt->InitializeOffer(OfferId, ReactionCard, TargetCard);
	ActiveReactionPrompt->AddToViewport(200);
	ActiveReactionPrompt->OnOfferPresented();
	if (!ActiveReactionPrompt || ActiveReactionOfferId != OfferId) { return; }
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(ActiveReactionPrompt->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ASHPlayerController::ClientCloseCardReaction_Implementation(int32 OfferId)
{
	if (OfferId != ActiveReactionOfferId || ActiveReactionOfferId == INDEX_NONE) { return; }
	if (ActiveReactionPrompt) { ActiveReactionPrompt->RemoveFromParent(); ActiveReactionPrompt = nullptr; }
	ActiveReactionOfferId = INDEX_NONE;
	bShowMouseCursor = bCursorBeforeReaction;
	// Match the table's GameAndUI mode: permanent capture breaks card drag and HUD clicks.
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		InputMode.SetWidgetToFocus(Viewport->GetGameViewportWidget());
	}
	SetInputMode(InputMode);
}

void ASHPlayerController::ServerRespondToCardReaction_Implementation(int32 OfferId, bool bAccept)
{
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>()) { Mode->RespondToCardReaction(GetPlayerState<ASHPlayerState>(), OfferId, bAccept); }
}

void ASHPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ValidateCardInfoAccess();
	KeepDraggedCardAboveOtherCards();
	UpdateLocalActivatablePairHover();
	UpdatePairTargetingIndicator();
}

void ASHPlayerController::StartPairTargetingIndicator(
	ASHCard* CardA, ASHCard* CardB, FName EffectPresentationId)
{
	StopPairTargetingIndicator();
	if (!IsLocalController() || !IsValid(CardA) || !IsValid(CardB))
	{
		return;
	}

	TargetingSourceCardA = CardA;
	TargetingSourceCardB = CardB;
	CurrentTargetingEffectPresentationId = EffectPresentationId;
	SetCardHoverSuppressedForTargeting(true);
	TSubclassOf<APairTargetingIndicator> IndicatorClass = PairTargetingIndicatorClass;
	if (!IndicatorClass)
	{
		IndicatorClass = APairTargetingIndicator::StaticClass();
	}
	PairTargetingIndicator = GetWorld()->SpawnActor<APairTargetingIndicator>(
		IndicatorClass, FTransform::Identity);
	if (IsValid(PairTargetingIndicator))
	{
		FPairTargetingIndicatorStyle ResolvedStyle = DefaultPairTargetingStyle;
		if (const FPairTargetingIndicatorStyle* EffectStyle = PairTargetingStyles.Find(EffectPresentationId))
		{
			ResolvedStyle = *EffectStyle;
			// Asset references left empty in an effect override inherit from the default style.
			ResolvedStyle.BodyMesh = IsValid(ResolvedStyle.BodyMesh)
				? ResolvedStyle.BodyMesh : DefaultPairTargetingStyle.BodyMesh;
			ResolvedStyle.ArrowHeadMesh = IsValid(ResolvedStyle.ArrowHeadMesh)
				? ResolvedStyle.ArrowHeadMesh : DefaultPairTargetingStyle.ArrowHeadMesh;
			ResolvedStyle.ValidMaterial = IsValid(ResolvedStyle.ValidMaterial)
				? ResolvedStyle.ValidMaterial : DefaultPairTargetingStyle.ValidMaterial;
			ResolvedStyle.InvalidMaterial = IsValid(ResolvedStyle.InvalidMaterial)
				? ResolvedStyle.InvalidMaterial : DefaultPairTargetingStyle.InvalidMaterial;
		}
		UE_LOG(LogTemp, Log,
			TEXT("[SH_TARGETING_INDICATOR] EffectId=%s BodyMesh=%s ArrowMesh=%s ValidMaterial=%s InvalidMaterial=%s"),
			*EffectPresentationId.ToString(), *GetNameSafe(ResolvedStyle.BodyMesh),
			*GetNameSafe(ResolvedStyle.ArrowHeadMesh), *GetNameSafe(ResolvedStyle.ValidMaterial),
			*GetNameSafe(ResolvedStyle.InvalidMaterial));
		PairTargetingIndicator->InitializeIndicator(ResolvedStyle, EffectPresentationId);
		UpdatePairTargetingIndicator();
	}
}

void ASHPlayerController::StopPairTargetingIndicator()
{
	SetCurrentValidEffectTarget(nullptr);
	RestoreEffectTargetOutlines();
	// The snapshot can predate activation availability changing during selection.
	// Never restore the source pair's old "ready to activate" glow after using it.
	if (IsValid(TargetingSourceCardA)) { TargetingSourceCardA->ClearInteractionHighlight(); }
	if (IsValid(TargetingSourceCardB)) { TargetingSourceCardB->ClearInteractionHighlight(); }
	if (IsValid(PairTargetingIndicator))
	{
		PairTargetingIndicator->Destroy();
	}
	PairTargetingIndicator = nullptr;
	TargetingSourceCardA = nullptr;
	TargetingSourceCardB = nullptr;
	CurrentTargetingEffectPresentationId = NAME_None;
	SetCardHoverSuppressedForTargeting(false);
}

void ASHPlayerController::SetCardHoverSuppressedForTargeting(bool bSuppressed)
{
	if (bCardHoverSuppressedForTargeting == bSuppressed)
	{
		return;
	}
	bCardHoverSuppressedForTargeting = bSuppressed;
	if (bSuppressed)
	{
		bSavedEnableMouseOverEvents = bEnableMouseOverEvents;
		bEnableMouseOverEvents = false;
	}
	else
	{
		bEnableMouseOverEvents = bSavedEnableMouseOverEvents;
	}

	for (TActorIterator<ASHCard> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			It->OnNormalHoverSuppressionChanged(bSuppressed);
		}
	}

	// BP_Hand traces the cursor and sets focus every tick independently of
	// bEnableMouseOverEvents. Block that focus at the native layout source.
	for (TActorIterator<ASHHand> It(GetWorld()); It; ++It)
	{
		ASHHand* Hand = *It;
		if (!IsValid(Hand))
		{
			continue;
		}
		TArray<USHHandCardsLayoutComponent*> LayoutComponents;
		Hand->GetComponents<USHHandCardsLayoutComponent>(LayoutComponents);
		for (USHHandCardsLayoutComponent* Layout : LayoutComponents)
		{
			if (IsValid(Layout))
			{
				Layout->SetTargetingFocusSuppressed(bSuppressed);
			}
		}
		Hand->UpdateCardPositions();
		if (!bSuppressed) { Hand->RefreshPairActivationAvailability(true); }
	}
}

void ASHPlayerController::SetCurrentValidEffectTarget(AActor* NewTarget)
{
	if (CurrentValidEffectTarget == NewTarget)
	{
		return;
	}
	if (ASHCard* PreviousCard = Cast<ASHCard>(CurrentValidEffectTarget))
	{
		PreviousCard->OnEffectTargetHoverChanged(false, CurrentTargetingEffectPresentationId);
	}
	else if (ASHPlayerRepresentation* PreviousPlayer =
		Cast<ASHPlayerRepresentation>(CurrentValidEffectTarget))
	{
		PreviousPlayer->OnEffectTargetHoverChanged(false, CurrentTargetingEffectPresentationId);
	}

	CurrentValidEffectTarget = NewTarget;
	if (ASHCard* NewCard = Cast<ASHCard>(CurrentValidEffectTarget))
	{
		NewCard->OnEffectTargetHoverChanged(true, CurrentTargetingEffectPresentationId);
	}
	else if (ASHPlayerRepresentation* NewPlayer =
		Cast<ASHPlayerRepresentation>(CurrentValidEffectTarget))
	{
		NewPlayer->OnEffectTargetHoverChanged(true, CurrentTargetingEffectPresentationId);
	}
}

bool ASHPlayerController::IsValidEffectTarget(const AActor* Actor) const
{
	if (!IsValid(Actor)) { return false; }
	if (const ASHPlayerRepresentation* Picker = Cast<ASHPlayerRepresentation>(Actor))
	{
		return (IsValid(Picker->GetRepresentedPlayerState()) &&
			LocalPlayerSelectionCandidates.Contains(Picker->GetRepresentedPlayerState())) ||
			(IsValid(Picker->GetRepresentedHand()) &&
			LocalParticipantSelectionCandidates.Contains(Picker->GetRepresentedHand()));
	}
	if (const ASHCard* Card = Cast<ASHCard>(Actor))
	{
		return LocalHandCardSelectionCandidates.Contains(Card) || LocalActivationPairSelectionCandidates.Contains(Card) ||
			(IsValid(Card->GetOwningHand()) && Card->GetOwningHand()->IsLogicalNPC() &&
			LocalParticipantSelectionCandidates.Contains(Card->GetOwningHand()));
	}
	return false;
}

bool ASHPlayerController::CancelEffectTargeting()
{
	if (!IsLocalController() || !IsValid(TargetingSourceCardA) || !IsValid(TargetingSourceCardB))
	{
		UE_LOG(LogTemp, Log, TEXT("[SH_CANCEL_TARGET] No local targeting session: PC=%s Local=%d A=%s B=%s"),
			*GetName(), IsLocalController(), *GetNameSafe(TargetingSourceCardA), *GetNameSafe(TargetingSourceCardB));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[SH_CANCEL_TARGET] Request: PC=%s A=%s B=%s"),
		*GetName(), *GetNameSafe(TargetingSourceCardA), *GetNameSafe(TargetingSourceCardB));
	ServerCancelEffectTargeting(TargetingSourceCardA, TargetingSourceCardB);
	return true;
}

void ASHPlayerController::ServerCancelEffectTargeting_Implementation(ASHCard* CardA, ASHCard* CardB)
{
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>())
	{
		const bool bCancelled = Mode->CancelEffectTargetSelection(GetPlayerState<ASHPlayerState>(), CardA, CardB);
		UE_LOG(LogTemp, Log, TEXT("[SH_CANCEL_TARGET] Server result: PC=%s Cancelled=%d A=%s B=%s"),
			*GetName(), bCancelled, *GetNameSafe(CardA), *GetNameSafe(CardB));
	}
}

void ASHPlayerController::RestoreEffectTargetOutlines()
{
	for (const auto& Entry : EffectOutlineMeshes)
	{
		if (UMeshComponent* Mesh = Entry.Key.Get())
		{
			Mesh->SetCustomDepthStencilValue(Entry.Value.StencilValue);
			Mesh->SetCustomDepthStencilWriteMask(Entry.Value.WriteMask);
			Mesh->SetRenderCustomDepth(Entry.Value.bRenderCustomDepth);
			Mesh->SetCustomPrimitiveDataFloat(20, Entry.Value.CardHighlightState);
		}
	}
	EffectOutlineMeshes.Reset();
}

void ASHPlayerController::RefreshSelectionPrompt()
{
	if (SelectionPromptWidget && (!SelectionPromptClass || LocalHandCardSelectionCandidates.IsEmpty() ||
		SelectionPromptWidget->GetClass() != SelectionPromptClass.Get()))
	{
		UCardSelectionPrompt* Previous = SelectionPromptWidget;
		SelectionPromptWidget = nullptr;
		Previous->RemoveFromParent();
	}
	if (!SelectionPromptClass || LocalHandCardSelectionCandidates.IsEmpty() || !IsLocalController() ||
		!GetWorld() || !GetWorld()->GetGameViewport() || SelectionPromptClass->HasAnyClassFlags(CLASS_Abstract)) { return; }
	if (!SelectionPromptWidget)
	{
		SelectionPromptWidget = CreateWidget<UCardSelectionPrompt>(this, SelectionPromptClass);
		if (!SelectionPromptWidget) { return; }
		SelectionPromptWidget->AddToViewport(100);
	}
	if (SelectionPromptWidget) { SelectionPromptWidget->OnSelectionChanged(); }
}

void ASHPlayerController::ConfirmEffectCardSelection()
{
	if (LocalHandCardSelectionCandidates.IsEmpty() || LocallySelectedEffectCards.Num() < LocalSelectionMin ||
		LocallySelectedEffectCards.Num() > LocalSelectionMax) { return; }
	TArray<ASHCard*> Cards;
	for (ASHCard* Card : LocallySelectedEffectCards) { Cards.Add(Card); }
	ServerSubmitHandCardsSelection(Cards);
}

void ASHPlayerController::ToggleEffectCardSelection(ASHCard* Card)
{
	if (!IsValid(Card) || !LocalHandCardSelectionCandidates.Contains(Card)) { return; }
	if (LocalSelectionMax == 1) { ServerSubmitHandCardSelection(Card); return; }
	if (LocallySelectedEffectCards.Contains(Card)) { LocallySelectedEffectCards.Remove(Card); }
	else if (LocallySelectedEffectCards.Num() < LocalSelectionMax) { LocallySelectedEffectCards.Add(Card); }
	const int32 Serial = LocalCardSelectionSerial;
	RefreshSelectionPrompt();
	// A Blueprint callback may confirm synchronously and open another selection step.
	if (Serial == LocalCardSelectionSerial && LocallySelectedEffectCards.Num() == LocalSelectionMax)
	{
		ConfirmEffectCardSelection();
	}
}

void ASHPlayerController::UpdateEffectTargetOutlines(AActor* HoveredActor)
{
	// These reserved stencil IDs are decoded by PP_SoftOutline. All state is local
	// presentation; the candidate lists and accepted selection still come from the server.
	TSet<TWeakObjectPtr<UMeshComponent>> CurrentMeshes;
	auto ApplyActor = [&](AActor* Actor)
	{
		const bool bValidTarget = IsValidEffectTarget(Actor);
		const bool bSelected = LocallySelectedEffectCards.Contains(Cast<ASHCard>(Actor));
		const int32 Stencil = bSelected ? 251 : (Actor == HoveredActor ? (bValidTarget ? 251 : 252) : 250);
		TInlineComponentArray<UMeshComponent*> Meshes(Actor);
		for (UMeshComponent* Mesh : Meshes)
		{
			// Widget quads and collision shapes are not the visible target silhouette.
			if (!IsValid(Mesh) || Mesh->IsA<UWidgetComponent>()) { continue; }
			CurrentMeshes.Add(Mesh);
			if (!EffectOutlineMeshes.Contains(Mesh))
			{
				EffectOutlineMeshes.Add(Mesh, {Mesh->bRenderCustomDepth != 0,
					Mesh->CustomDepthStencilValue, Mesh->CustomDepthStencilWriteMask,
					Mesh->GetCustomPrimitiveData().Data.IsValidIndex(20) ? Mesh->GetCustomPrimitiveData().Data[20] : 0.0f});
			}
			Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
			Mesh->SetCustomDepthStencilValue(Stencil);
			Mesh->SetRenderCustomDepth(bValidTarget || Actor == HoveredActor);
			// M_Card reads slot 20 without replacing its Blueprint-owned dynamic
			// material (card textures and ordinary hover continue to update normally).
			// 0=ordinary, 1=suppressed, 2=white, 3=green hover, 4=red hover.
			Mesh->SetCustomPrimitiveDataFloat(20,
				bSelected ? 3.0f : (Actor == HoveredActor ? (bValidTarget ? 3.0f : 4.0f) : (bValidTarget ? 2.0f : 1.0f)));
		}
	};
	// Also suppress ordinary interaction outlines on invalid targets during selection.
	for (TActorIterator<ASHCard> It(GetWorld()); It; ++It) { ApplyActor(*It); }
	for (TActorIterator<ASHPlayerRepresentation> It(GetWorld()); It; ++It) { ApplyActor(*It); }
	for (auto It = EffectOutlineMeshes.CreateIterator(); It; ++It)
	{
		if (!CurrentMeshes.Contains(It.Key()))
		{
			if (UMeshComponent* Mesh = It.Key().Get())
			{
				Mesh->SetCustomDepthStencilValue(It.Value().StencilValue);
				Mesh->SetCustomDepthStencilWriteMask(It.Value().WriteMask);
				Mesh->SetRenderCustomDepth(It.Value().bRenderCustomDepth);
				Mesh->SetCustomPrimitiveDataFloat(20, It.Value().CardHighlightState);
			}
			It.RemoveCurrent();
		}
	}
}

bool ASHPlayerController::ResolveTargetingCursor(FVector& OutLocation, bool& bOutValidTarget,
	AActor*& OutValidTargetActor) const
{
	bOutValidTarget = false;
	OutValidTargetActor = nullptr;
	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, true, HitResult))
	{
		OutLocation = HitResult.ImpactPoint;
		AActor* HitActor = HitResult.GetActor();
		bOutValidTarget = IsValidEffectTarget(HitActor);
		OutValidTargetActor = HitActor;

		if (IsValid(HitActor))
		{
			FVector BoundsOrigin;
			FVector BoundsExtent;
			HitActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
			OutLocation.Z = FMath::Max(OutLocation.Z, BoundsOrigin.Z + BoundsExtent.Z);
		}
		return true;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		const float PlaneZ = IsValid(TargetingSourceCardA)
			? TargetingSourceCardA->GetActorLocation().Z : 0.0f;
		const FPlane CursorPlane(FVector(0.0f, 0.0f, PlaneZ), FVector::UpVector);
		OutLocation = FMath::LinePlaneIntersection(
			RayOrigin, RayOrigin + RayDirection * HALF_WORLD_MAX, CursorPlane);
		return true;
	}
	return false;
}

void ASHPlayerController::UpdatePairTargetingIndicator()
{
	if (!IsValid(PairTargetingIndicator))
	{
		return;
	}
	if (!IsValid(TargetingSourceCardA) || !IsValid(TargetingSourceCardB))
	{
		StopPairTargetingIndicator();
		return;
	}

	FVector End;
	bool bValidTarget = false;
	AActor* ValidTargetActor = nullptr;
	if (!ResolveTargetingCursor(End, bValidTarget, ValidTargetActor))
	{
		SetCurrentValidEffectTarget(nullptr);
		UpdateEffectTargetOutlines(nullptr);
		return;
	}
	SetCurrentValidEffectTarget(bValidTarget ? ValidTargetActor : nullptr);
	UpdateEffectTargetOutlines(ValidTargetActor);
	const FVector Start = (TargetingSourceCardA->GetActorLocation() +
		TargetingSourceCardB->GetActorLocation()) * 0.5f;
	PairTargetingIndicator->UpdateIndicator(Start, End, bValidTarget);
}

void ASHPlayerController::UpdateLocalActivatablePairHover()
{
	if (!IsLocalController())
	{
		return;
	}
	if (IsValid(PairTargetingIndicator))
	{
		if (IsValid(LastActivatableHoverCard))
		{
			if (ASHHand* PreviousVisualHand =
				FindVisualHandForLogicalHand(LastActivatableHoverCard->GetOwningHand()))
			{
				PreviousVisualHand->SetLocalActivatableCardHovered(
					LastActivatableHoverCard, false);
			}
			LastActivatableHoverCard = nullptr;
		}
		return;
	}

	FHitResult HitResult;
	GetHitResultUnderCursor(ECC_Visibility, true, HitResult);
	ASHCard* CardUnderCursor = Cast<ASHCard>(HitResult.GetActor());
	ASHHand* HoverVisualHand = IsValid(CardUnderCursor)
		? FindVisualHandForLogicalHand(CardUnderCursor->GetOwningHand()) : nullptr;
	if (!IsValid(HoverVisualHand) || !HoverVisualHand->CanLocalPlayerActivatePair(CardUnderCursor))
	{
		CardUnderCursor = nullptr;
		HoverVisualHand = nullptr;
	}
	if (CardUnderCursor == LastActivatableHoverCard)
	{
		return;
	}

	if (IsValid(LastActivatableHoverCard))
	{
		if (ASHHand* PreviousVisualHand =
			FindVisualHandForLogicalHand(LastActivatableHoverCard->GetOwningHand()))
		{
			PreviousVisualHand->SetLocalActivatableCardHovered(
				LastActivatableHoverCard, false);
		}
	}

	LastActivatableHoverCard = CardUnderCursor;
	if (IsValid(CardUnderCursor))
	{
		if (IsValid(HoverVisualHand))
		{
			HoverVisualHand->SetLocalActivatableCardHovered(CardUnderCursor, true);
		}
	}
}

void ASHPlayerController::KeepDraggedCardAboveOtherCards()
{
	if (!IsLocalController() || !IsValid(LocallyDraggedCard))
	{
		return;
	}

	// NPC stack cards inherit the stack actor scale. Once a card is being
	// dragged, present it at the same neutral scale used by a player's hand.
	// The destination/source layout takes the scale back over after release.
	LocallyDraggedCard->SetActorScale3D(FVector::OneVector);

	double HighestOtherCardZ = -TNumericLimits<double>::Max();
	for (TActorIterator<ASHCard> It(GetWorld()); It; ++It)
	{
		const ASHCard* OtherCard = *It;
		if (IsValid(OtherCard) && OtherCard != LocallyDraggedCard)
		{
			HighestOtherCardZ = FMath::Max(HighestOtherCardZ,
				static_cast<double>(OtherCard->GetActorLocation().Z));
		}
	}

	if (HighestOtherCardZ > -TNumericLimits<double>::Max())
	{
		FVector DragLocation = LocallyDraggedCard->GetActorLocation();
		DragLocation.Z = HighestOtherCardZ + DraggedCardZClearance;
		LocallyDraggedCard->SetActorLocation(DragLocation);
	}
}

void ASHPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		GetWorldTimerManager().SetTimer(
			TableSetupRetryTimer, this, &ASHPlayerController::TrySetupTableView, 0.1f, true, 0.0f);
	}
}

void ASHPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseCardInfo();
	ClientCloseCardReaction_Implementation(ActiveReactionOfferId);
	ClearLocalEffectSelectionState();
	StopPairTargetingIndicator();
	GetWorldTimerManager().ClearTimer(TableSetupRetryTimer);
	GetWorldTimerManager().ClearTimer(RotatedHandsReconcileTimer);
	Super::EndPlay(EndPlayReason);
}

bool ASHPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (IsLocalController() && Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed)
	{
		// Only clicks that reached the game arrive here. Close without consuming
		// the event: the same press must still select/drag/activate its world target.
		CloseCardInfo();
		ASHCard* Pressed = nullptr;
		FVector Position;
		GetCardInteractionUnderCursor(Pressed, Position);
		PointerPressedCard = Pressed;
		PointerPressedLocation = Position;
	}
	if (IsLocalController() && Params.Key == EKeys::Escape && Params.Event == IE_Pressed && ActiveCardInfoWidget)
	{
		CloseCardInfo();
		return true;
	}
	if (ActiveReactionOfferId != INDEX_NONE) { return true; }
	if (IsLocalController() && Params.Key == EKeys::RightMouseButton)
	{
		if (Params.Event == IE_Pressed)
		{
			ASHCard* Card = nullptr;
			FVector Location;
			GetCardInteractionUnderCursor(Card, Location);
			if (!ShowCardInfo(Card)) { CloseCardInfo(); }
		}
		// Do not let right-click trigger Blueprint card dragging or effect selection.
		return true;
	}
	if (IsLocalController() && Params.Key == EKeys::Enter && Params.Event == IE_Pressed && LocalSelectionMax > 1 && !LocalHandCardSelectionCandidates.IsEmpty())
	{
		ConfirmEffectCardSelection();
		return true;
	}
	if (IsLocalController() && Params.Key == EKeys::Escape && Params.Event == IE_Pressed && CancelEffectTargeting())
	{
		return true;
	}
	if (IsLocalController() && Params.Key == EKeys::LeftMouseButton &&
		Params.Event == IE_Released && bConsumeEffectSelectionRelease)
	{
		bConsumeEffectSelectionRelease = false;
		return true;
	}
	if (IsLocalController() && Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed &&
		(!LocalPlayerSelectionCandidates.IsEmpty() || !LocalParticipantSelectionCandidates.IsEmpty() ||
			!LocalHandCardSelectionCandidates.IsEmpty() || !LocalActivationPairSelectionCandidates.IsEmpty() || bAwaitingPlayerSelectionResponse))
	{
		// Own the whole gesture, including release. A listen-server selection can
		// synchronously open the next step before this press finishes dispatching.
		bConsumeEffectSelectionRelease = true;
		FHitResult HitResult;
		GetHitResultUnderCursor(ECC_Visibility, true, HitResult);
		TryHandleEffectSelectionClick(HitResult.GetActor());
		return true;
	}

	return Super::InputKey(Params);
}

bool ASHPlayerController::TryHandleEffectSelectionClick(AActor* HitActor)
{
	if (!LocalActivationPairSelectionCandidates.IsEmpty())
	{
		if (ASHCard* Card = Cast<ASHCard>(HitActor); LocalActivationPairSelectionCandidates.Contains(Card)) { ServerActivateStoredPair(Card); }
		return true;
	}
	if (!LocalHandCardSelectionCandidates.IsEmpty())
	{
		// Overlapping BN cards represent one stack; resolve any hit to its offered top.
		if (const ASHCard* HitCard = Cast<ASHCard>(HitActor); IsValid(HitCard) && IsValid(HitCard->GetOwningHand()) && HitCard->GetOwningHand()->IsLogicalNPC())
		{
			for (ASHCard* Candidate : LocalHandCardSelectionCandidates)
			{
				if (IsValid(Candidate) && Candidate->GetOwningHand() == HitCard->GetOwningHand()) { ServerSubmitHandCardSelection(Candidate); return true; }
			}
		}
		if (ASHCard* Card = Cast<ASHCard>(HitActor); LocalHandCardSelectionCandidates.Contains(Card))
		{
			ToggleEffectCardSelection(Card);
		}
		return true;
	}
	if (bAwaitingPlayerSelectionResponse)
	{
		return true;
	}
	if (!LocalPlayerSelectionCandidates.IsEmpty())
	{
		// Match ResolveTargetingCursor: player choices use the represented player,
		// not a card-only BP lookup which returns None for a player picker.
		const ASHPlayerRepresentation* Picker = Cast<ASHPlayerRepresentation>(HitActor);
		return TrySubmitPlayerSelectionForPicker(IsValid(Picker) ? Picker->GetRepresentedPlayerState() : nullptr);
	}
	if (!LocalParticipantSelectionCandidates.IsEmpty())
	{
		if (const ASHPlayerRepresentation* Picker = Cast<ASHPlayerRepresentation>(HitActor))
		{
			return TrySubmitParticipantSelectionForHand(Picker->GetRepresentedHand());
		}
		TrySubmitParticipantSelectionForCard(Cast<ASHCard>(HitActor));
		return true;
	}
	return false;
}

void ASHPlayerController::TrySetupTableView()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][PC:%s] TryInitialTableSetup BEGIN | Initialized=%d"),
        GetWorld()->GetTimeSeconds(),
        *GetNameSafe(this),
        bTableViewInitialized);

    if (bTableViewInitialized)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> SKIP: already initialized"));
        return;
    }

    if (!IsLocalController())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> WAIT: not local controller"));
        return;
    }

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> WAIT: no GameState"));
        return;
    }

    if (!SHGameState->IsMatchReady())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] -> WAIT: MatchReady=false"));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT] MatchReady=true | Players=%d | ExpectedCards=%d"),
        SHGameState->PlayerArray.Num(),
        SHGameState->GetInitialDealtCardCount());

    ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();

    if (!IsValid(LocalPlayerState) ||
        LocalPlayerState->GetSeatIndex() == INDEX_NONE)
    {
        return;
    }

    if (SHGameState->PlayerArray.IsEmpty())
    {
        return;
    }

	const TArray<ASHHand*> ParticipantHands = SHGameState->GetParticipantHands();
	if (ParticipantHands.Num() < 2 || ParticipantHands.Num() > 6)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SH_INIT] -> WAIT: expected 2-6 participant hands, received %d"),
			ParticipantHands.Num());
		return;
	}

	TSet<ASHHand*> HumanHands;
	for (APlayerState* State : SHGameState->PlayerArray)
	{
		ASHPlayerState* ReplicatedPlayerState = Cast<ASHPlayerState>(State);
		if (!IsValid(ReplicatedPlayerState) || ReplicatedPlayerState->GetSeatIndex() == INDEX_NONE ||
			!IsValid(ReplicatedPlayerState->GetHand()))
		{
			UE_LOG(LogTemp, Warning, TEXT("[SH_INIT] -> WAIT: incomplete replicated player/hand assignment"));
			return;
		}
		HumanHands.Add(ReplicatedPlayerState->GetHand());
	}

	for (ASHHand* Hand : ParticipantHands)
	{
		if (IsValid(Hand) && !Hand->IsLogicalNPC() && !HumanHands.Contains(Hand))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[SH_INIT] -> WAIT: human hand %s has no replicated PlayerState"), *GetNameSafe(Hand));
			return;
		}
	}

    int32 ReceivedCardCount = 0;

    for (ASHHand* Hand : ParticipantHands)
    {
        if (!IsValid(Hand))
        {
            UE_LOG(LogTemp, Warning, TEXT("[SH_INIT] -> WAIT: invalid participant hand"));
            return;
        }

        int32 ValidCards = 0;

        for (ASHCard* Card : Hand->GetCards())
        {
            if (IsValid(Card))
            {
                ++ValidCards;
            }
        }


        UE_LOG(LogTemp, Warning,
            TEXT("[SH_INIT] Hand=%s LogicalSeat=%d IsNPC=%d Cards=%d Valid=%d"),
            *GetNameSafe(Hand),
            Hand->GetLayoutSeatIndex(),
            Hand->IsLogicalNPC(),
            Hand->GetCardCount(),
            ValidCards);


        if (ValidCards != Hand->GetCardCount())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[SH_INIT] -> WAIT: unresolved Card actors"));
            return;
        }

        ReceivedCardCount += Hand->GetCardCount();
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT] ReceivedCards=%d ExpectedCards=%d"),
        ReceivedCardCount,
        SHGameState->GetInitialDealtCardCount());

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f] >>> READY - calling SetupTableView"),
        GetWorld()->GetTimeSeconds());

    SetupTableView();
    bTableViewInitialized = true;
	// Initialize UI only after the retry loop has resolved the local player and table references.
	if (!bLocalMatchUIInitialized)
	{
		bLocalMatchUIInitialized = true;
		OnLocalMatchUIReady(LocalPlayerState, SHGameState);
	}
	GetWorldTimerManager().ClearTimer(TableSetupRetryTimer);

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f] <<< TABLE INITIALIZED"),
        GetWorld()->GetTimeSeconds());
}

bool ASHPlayerController::IsMatchHost() const
{
    return HasAuthority() && IsLocalController();
}

void ASHPlayerController::ServerRestartMatch_Implementation()
{
    if (!IsMatchHost())
    {
        return;
    }

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();
    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();
    ASHGameMode* SHGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
    if (!IsValid(SHGameMode) || !IsValid(SHGameState) || !SHGameState->IsGameEnded() ||
        !IsValid(SHPlayerState) || !SHGameState->PlayerArray.Contains(SHPlayerState))
    {
        return;
    }

    // RestartGame performs server travel and ignores duplicate requests while leaving the map.
    SHGameMode->RestartGame();
}

void ASHPlayerController::ServerSkipCurrentPhase_Implementation()
{
    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();

    if (!IsValid(SHPlayerState))
    {
        return;
    }

    ASHGameMode* SHGameMode =
        GetWorld()->GetAuthGameMode<ASHGameMode>();

    if (!IsValid(SHGameMode) || SHGameMode->IsWaitingForPlayerSelection())
    {
        return;
    }

    UTurnComponent* TurnComponent = SHGameMode->GetTurnComponent();
    if (IsValid(TurnComponent))
    {
        TurnComponent->SkipCurrentPhase(SHPlayerState);
    }
}

void ASHPlayerController::ClientRequestPlayerSelection_Implementation(
    const TArray<ASHPlayerState*>& Candidates,
    EPlayerSelectionPurpose Purpose)
{
	bAwaitingPlayerSelectionResponse = false;
	ClearLocalPlayerSelection();
	for (ASHPlayerState* Candidate : Candidates)
	{
		if (IsValid(Candidate))
		{
			LocalPlayerSelectionCandidates.AddUnique(Candidate);
		}
	}

	const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
	if (IsValid(GameState))
	{
		for (ASHHand* LogicalHand : GameState->GetParticipantHands())
		{
			ASHHand* VisualHand = FindVisualHandForLogicalHand(LogicalHand);
			ASHPlayerRepresentation* Picker = IsValid(VisualHand) ? VisualHand->GetPlayerPicker() : nullptr;
			ASHPlayerState* RepresentedPlayer = IsValid(VisualHand)
				? VisualHand->GetRepresentedPlayerState()
				: nullptr;
			if (IsValid(Picker))
			{
				Picker->SetSelectable(LocalPlayerSelectionCandidates.Contains(RepresentedPlayer));
			}
		}
	}

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_SELECTION][CLIENT_REQUEST] Controller=%s Purpose=%s CandidateCount=%d"),
        *GetNameSafe(this),
        *UEnum::GetValueAsString(Purpose),
        Candidates.Num());

    for (const ASHPlayerState* Candidate : Candidates)
    {
        ASHHand* CandidateHand = IsValid(Candidate) ? Candidate->GetHand() : nullptr;

        UE_LOG(LogTemp, Warning,
            TEXT("[SH_SELECTION][CLIENT_CANDIDATE] Player=%s Hand=%s Cards=%d"),
            *GetNameSafe(Candidate),
            *GetNameSafe(CandidateHand),
            IsValid(CandidateHand) ? CandidateHand->GetCardCount() : INDEX_NONE);
    }

    // Native world-space pickers own this interaction. The legacy BP event arms
    // a second, card-based selector (IsSelectingPlayer), which submits None for
    // picker clicks and can remain armed after the native selector completes.
}

bool ASHPlayerController::TrySubmitPlayerSelectionForPicker(ASHPlayerState* SelectedPlayer)
{
	if (!LocalParticipantSelectionCandidates.IsEmpty())
	{
		return TrySubmitParticipantSelectionForHand(IsValid(SelectedPlayer) ? SelectedPlayer->GetHand() : nullptr);
	}
	if (LocalPlayerSelectionCandidates.IsEmpty())
	{
		return false;
	}

	if (IsValid(SelectedPlayer) && LocalPlayerSelectionCandidates.Contains(SelectedPlayer))
	{
		ClearLocalPlayerSelection();
		bAwaitingPlayerSelectionResponse = true;
		ServerSubmitPlayerSelection(SelectedPlayer);
	}

	return true;
}

void ASHPlayerController::ClearLocalPlayerSelection()
{
	LocalPlayerSelectionCandidates.Reset();
	const ASHGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ASHGameState>() : nullptr;
	if (!IsValid(GameState))
	{
		return;
	}

	for (ASHHand* LogicalHand : GameState->GetParticipantHands())
	{
		ASHHand* VisualHand = FindVisualHandForLogicalHand(LogicalHand);
		if (ASHPlayerRepresentation* Picker =
			IsValid(VisualHand) ? VisualHand->GetPlayerPicker() : nullptr)
		{
			Picker->SetSelectable(false);
		}
	}
}

void ASHPlayerController::ClearLocalEffectSelectionState()
{
	++LocalCardSelectionSerial;
	LocalHandCardSelectionCandidates.Reset();
	LocallySelectedEffectCards.Reset();
	LocalSelectionMin = LocalSelectionMax = 1;
	SelectionPromptClass = nullptr;
	RefreshSelectionPrompt();
	bAwaitingPlayerSelectionResponse = false;
	ClearLocalPlayerSelection();
	LocalParticipantSelectionCandidates.Reset();
	LocalActivationPairSelectionCandidates.Reset();
}

void ASHPlayerController::ClientRequestAdditionalCardDraw_Implementation(const TArray<ASHPlayerState*>& ValidSources)
{
    OnAdditionalCardDrawRequested(ValidSources);
}

void ASHPlayerController::ClientUpdateCardDrawGuidance_Implementation(
    const TArray<ASHPlayerState*>& ValidSources,
    ECardDrawGuidanceType GuidanceType)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[SH_DRAW_GUIDANCE][CLIENT] Controller=%s Type=%s SourceCount=%d"),
        *GetNameSafe(this),
        *UEnum::GetValueAsString(GuidanceType),
        ValidSources.Num());

    OnCardDrawGuidanceUpdated(ValidSources, GuidanceType);
}

void ASHPlayerController::ClientSetGuidedDrawHands_Implementation(const TArray<ASHHand*>& ValidHands)
{
    LocalGuidedDrawHands.Reset();
    for (ASHHand* Hand : ValidHands)
    {
        if (IsValid(Hand))
        {
            LocalGuidedDrawHands.AddUnique(Hand);
        }
    }
}

void ASHPlayerController::ClientReconcileRotatedHands_Implementation()
{
	RemainingRotatedHandsReconciles = 10;
	ReconcileRotatedHandsPresentation();
	GetWorldTimerManager().SetTimer(
		RotatedHandsReconcileTimer,
		this,
		&ASHPlayerController::ReconcileRotatedHandsPresentation,
		0.1f,
		true);
}

void ASHPlayerController::ClientNotifyPairPresentation_Implementation(
	ASHHand* LogicalHand, ASHCard* CardA, ASHCard* CardB, bool bEffectActivation)
{
	FPendingPairPresentationEvent Event;
	Event.LogicalHand = LogicalHand;
	Event.CardA = CardA;
	Event.CardB = CardB;
	Event.bEffectActivation = bEffectActivation;

	if (!TryRoutePairPresentation(Event))
	{
		PendingPairPresentationEvents.Add(Event);
	}
}

bool ASHPlayerController::TryRoutePairPresentation(const FPendingPairPresentationEvent& Event)
{
	if (!IsValid(Event.LogicalHand) ||
		!IsValid(Event.CardA) || !IsValid(Event.CardB))
	{
		return false;
	}

	ASHHand* VisualHand = FindVisualHandForLogicalHand(Event.LogicalHand);
	if (!IsValid(VisualHand))
	{
		return false;
	}

	if (Event.bEffectActivation)
	{
		VisualHand->PresentStoredPairActivated(Event.CardA, Event.CardB);
	}
	else
	{
		VisualHand->PresentPairCreated(Event.CardA, Event.CardB);
	}
	return true;
}

void ASHPlayerController::FlushPendingPairPresentationEvents()
{
	for (int32 Index = PendingPairPresentationEvents.Num() - 1; Index >= 0; --Index)
	{
		if (TryRoutePairPresentation(PendingPairPresentationEvents[Index]))
		{
			PendingPairPresentationEvents.RemoveAt(Index);
		}
	}
}

void ASHPlayerController::ReconcileRotatedHandsPresentation()
{
	const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();
	if (IsValid(GameState))
	{
		for (ASHHand* LogicalHand : GameState->GetParticipantHands())
		{
			ASHHand* VisualHand = FindVisualHandForLogicalHand(LogicalHand);
			if (!IsValid(LogicalHand) || !IsValid(VisualHand))
			{
				continue;
			}

			if (LogicalHand->IsLogicalNPC())
			{
				VisualHand->LayoutNPCStack(LogicalHand);
			}
			else
			{
				VisualHand->RefreshCardsPresentation();
				VisualHand->UpdateCardPositions();
			}
		}
	}

	if (--RemainingRotatedHandsReconciles <= 0)
	{
		GetWorldTimerManager().ClearTimer(RotatedHandsReconcileTimer);
	}
}

void ASHPlayerController::ServerSubmitPlayerSelection_Implementation(
	ASHPlayerState* SelectedPlayer)
{
    ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
    ASHPlayerState* SelectingPlayer = GetPlayerState<ASHPlayerState>();

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_SELECTION][SERVER_SUBMIT] Controller=%s SelectingPlayer=%s SelectedPlayer=%s"),
        *GetNameSafe(this),
        *GetNameSafe(SelectingPlayer),
        *GetNameSafe(SelectedPlayer));

    if (IsValid(GameMode) && IsValid(SelectingPlayer))
    {
		GameMode->SubmitPlayerSelection(SelectingPlayer, SelectedPlayer);
    }
}

void ASHPlayerController::ClientRequestParticipantSelection_Implementation(
    const TArray<ASHHand*>& Candidates,
    EPlayerSelectionPurpose Purpose)
{
    ClearLocalEffectSelectionState();
    LocalParticipantSelectionCandidates.Reset();
    for (ASHHand* Candidate : Candidates)
    {
        if (IsValid(Candidate))
        {
            LocalParticipantSelectionCandidates.AddUnique(Candidate);
        }
    }

    if (const ASHGameState* State = GetWorld()->GetGameState<ASHGameState>())
    {
        for (ASHHand* LogicalHand : State->GetParticipantHands())
        {
            ASHHand* VisualHand = FindVisualHandForLogicalHand(LogicalHand);
            ASHPlayerRepresentation* Picker = IsValid(VisualHand) ? VisualHand->GetPlayerPicker() : nullptr;
            if (IsValid(Picker))
            {
                Picker->SetSelectable(LocalParticipantSelectionCandidates.Contains(LogicalHand));
            }
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_PARTICIPANT_SELECTION][CLIENT_REQUEST] Controller=%s Purpose=%s CandidateCount=%d"),
        *GetNameSafe(this), *UEnum::GetValueAsString(Purpose),
        LocalParticipantSelectionCandidates.Num());
}

bool ASHPlayerController::TrySubmitParticipantSelectionForCard(const ASHCard* Card)
{
    if (LocalParticipantSelectionCandidates.IsEmpty())
    {
        // A forced/additional draw only restricts the source. It still uses the
        // ordinary drag preview, release decision and authoritative draw checks.
        return false;
    }

    ASHHand* SelectedHand = IsValid(Card) ? Card->GetOwningHand() : nullptr;
    return TrySubmitParticipantSelectionForHand(
        IsValid(SelectedHand) && SelectedHand->IsLogicalNPC() ? SelectedHand : nullptr);
}

bool ASHPlayerController::TrySubmitParticipantSelectionForHand(ASHHand* SelectedHand)
{
    if (LocalParticipantSelectionCandidates.IsEmpty())
    {
        return false;
    }

    // Consume invalid targets as well; the authoritative candidate list determines eligibility.
    if (IsValid(SelectedHand) && LocalParticipantSelectionCandidates.Contains(SelectedHand))
    {
        LocalParticipantSelectionCandidates.Reset();
        ClearLocalPlayerSelection();
        bAwaitingPlayerSelectionResponse = true;
        ServerSubmitParticipantSelection(SelectedHand);
    }
    return true;
}

void ASHPlayerController::ServerSubmitParticipantSelection_Implementation(ASHHand* SelectedHand)
{
    ASHGameMode* GameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
    ASHPlayerState* SelectingPlayer = GetPlayerState<ASHPlayerState>();
    if (IsValid(GameMode) && IsValid(SelectingPlayer))
    {
        GameMode->SubmitParticipantSelection(SelectingPlayer, SelectedHand);
    }
}

void ASHPlayerController::ClientRequestActivationPairSelection_Implementation(
    const TArray<ASHCard*>& CandidateCards)
{
	ClearLocalEffectSelectionState();
	LocalActivationPairSelectionCandidates.Reset();
	for (ASHCard* Candidate : CandidateCards)
	{
		if (IsValid(Candidate))
		{
			LocalActivationPairSelectionCandidates.AddUnique(Candidate);
		}
	}
    OnActivationPairSelectionRequested(CandidateCards);
}

void ASHPlayerController::ClientRequestHandCardSelection_Implementation(const TArray<ASHCard*>& Cards)
{
	ClientRequestHandCardsSelection_Implementation(Cards, 1, 1, nullptr);
}

void ASHPlayerController::ClientRequestHandCardsSelection_Implementation(const TArray<ASHCard*>& Cards, int32 Min, int32 Max, TSubclassOf<UCardSelectionPrompt> WidgetClass)
{
	ClearLocalEffectSelectionState();
	LocalSelectionMin = Min;
	LocalSelectionMax = Max;
	SelectionPromptClass = WidgetClass;
	for (ASHCard* Card : Cards)
	{
		if (IsValid(Card)) { LocalHandCardSelectionCandidates.AddUnique(Card); }
	}
	RefreshSelectionPrompt();
}

void ASHPlayerController::ServerSubmitHandCardsSelection_Implementation(const TArray<ASHCard*>& Cards)
{
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>()) { Mode->SubmitHandCardsSelection(GetPlayerState<ASHPlayerState>(), Cards); }
}

void ASHPlayerController::ServerSubmitHandCardSelection_Implementation(ASHCard* Card)
{
	if (ASHGameMode* Mode = GetWorld()->GetAuthGameMode<ASHGameMode>())
	{
		Mode->SubmitHandCardSelection(GetPlayerState<ASHPlayerState>(), Card);
	}
}

ASHPlayerState* ASHPlayerController::FindPlayerStateForCard(const ASHCard* Card) const
{
    if (!IsValid(Card))
    {
        return nullptr;
    }

    ASHHand* CardHand = Card->GetOwningHand();
    const ASHGameState* GameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(CardHand) || !IsValid(GameState))
    {
        return nullptr;
    }

    for (APlayerState* CandidatePlayerState : GameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CandidatePlayerState);
        if (IsValid(SHPlayerState) && SHPlayerState->GetHand() == CardHand)
        {
            return SHPlayerState;
        }
    }

    return nullptr;
}

ASHHand* ASHPlayerController::FindVisualHandForLogicalHand(const ASHHand* LogicalHand) const
{
    if (!IsValid(LogicalHand))
    {
        return nullptr;
    }

    ASHGameState* GameState =
        GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(GameState))
    {
        return nullptr;
    }

    TArray<AActor*> Hands;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASHHand::StaticClass(), Hands);
    for (AActor* Actor : Hands)
    {
        ASHHand* VisualHand = Cast<ASHHand>(Actor);
        if (IsValid(VisualHand) && VisualHand->GetRepresentedHand() == LogicalHand)
        {
            return VisualHand;
        }
    }

    return nullptr;
}

void ASHPlayerController::SetupTableView()
{
    ASHGameState* SHGameState =
        GetWorld()->GetGameState<ASHGameState>();

    checkf(IsValid(SHGameState), TEXT("Invalid SHGameState"));
	SHGameState->OnTurnStateChanged.RemoveDynamic(this, &ASHPlayerController::HandleTurnStateChanged);
	SHGameState->OnTurnStateChanged.AddDynamic(this, &ASHPlayerController::HandleTurnStateChanged);

    ASHPlayerState* LocalPlayerState =
        GetPlayerState<ASHPlayerState>();

    checkf(IsValid(LocalPlayerState), TEXT("Invalid local PlayerState"));

    const int32 PlayerCount = SHGameState->GetParticipantCount();

    TArray<ASHHand*> HandsToUpdate;

	for (ASHHand* LogicalHand : SHGameState->GetParticipantHands())
    {
        const int32 VisualSeatIndex =
            GetVisualSeatIndex(
                LogicalHand->GetLayoutSeatIndex(),
                PlayerCount
            );

        ASHHand* VisualHand =
            FindLayoutHand(VisualSeatIndex);

        checkf(
            IsValid(VisualHand),
            TEXT("No Hand for VisualSeatIndex %d"),
            VisualSeatIndex
        );

        
        ASHPlayerState* ControllingPlayer = nullptr;
		for (APlayerState* State : SHGameState->PlayerArray)
		{
			ASHPlayerState* Candidate = Cast<ASHPlayerState>(State);
			if (IsValid(Candidate) && Candidate->GetHand() == LogicalHand)
			{
				ControllingPlayer = Candidate;
				break;
			}
		}
		if (IsValid(ControllingPlayer)) VisualHand->SetRepresentedPlayerState(ControllingPlayer);
		else VisualHand->SetRepresentedHand(LogicalHand);

		const bool bIsLocalPlayer = ControllingPlayer == LocalPlayerState;

        VisualHand->SetShowCardFronts(bIsLocalPlayer);

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[HAND VIEW] VisualSeat=%d Hand=%s LogicalHand=%s IsNPC=%d Player=%s%s"),
            VisualSeatIndex,
            *GetNameSafe(VisualHand),
			*GetNameSafe(LogicalHand), LogicalHand->IsLogicalNPC(),
			*GetNameSafe(ControllingPlayer),
            ControllingPlayer == LocalPlayerState
            ? TEXT(" [LOCAL]")
            : TEXT("")
        );

        HandsToUpdate.Add(VisualHand);

    }

    for (ASHHand* Hand : HandsToUpdate)
    {
        Hand->Initialize();
        Hand->UpdateCardPositions();
        Hand->RefreshActivationPairsPresentation();
    }

	// UpdateCardPositions is Blueprint-defined for regular hands. Apply the
	// native NPC stack layout afterwards so every client gets the same stacked
	// presentation at its locally rotated visual seat.
	for (ASHHand* NPCHand : SHGameState->GetNPCHands())
	{
		const int32 VisualSeatIndex = GetVisualSeatIndex(NPCHand->GetLayoutSeatIndex(), PlayerCount);
		if (ASHHand* VisualHand = FindLayoutHand(VisualSeatIndex))
		{
			VisualHand->LayoutNPCStack(NPCHand);
		}
	}

	for (APlayerState* CurrentPlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* SHPlayerState = Cast<ASHPlayerState>(CurrentPlayerState);
        ASHHand* LogicalHand = IsValid(SHPlayerState) ? SHPlayerState->GetHand() : nullptr;
        AVictoryStack* LogicalStack = IsValid(LogicalHand) ? LogicalHand->GetVictoryStack() : nullptr;

        if (IsValid(LogicalStack))
        {
            LogicalStack->RefreshCardsPresentation();
        }
    }

	FlushPendingPairPresentationEvents();

}

void ASHPlayerController::HandleTurnStateChanged(ASHPlayerState* CurrentPlayer, ETurnPhase TurnPhase)
{
	ASHPlayerState* LocalPS = GetPlayerState<ASHPlayerState>();
	if (!IsLocalController() || !IsValid(LocalPS))
	{
		return;
	}
	if (ASHHand* VisualHand = FindVisualHandForLogicalHand(LocalPS->GetHand()))
	{
		VisualHand->RefreshPairActivationAvailability();
	}
}

int32 ASHPlayerController::GetVisualSeatIndex(int32 PlayerSeatIndex, int32 PlayerCount) const
{
    ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();

    checkf(IsValid(LocalPlayerState), TEXT("Invalid local PlayerState"));

    const int32 LocalSeatIndex = LocalPlayerState->GetSeatIndex();

    return (PlayerSeatIndex - LocalSeatIndex + PlayerCount) % PlayerCount;
}

ASHHand* ASHPlayerController::FindLayoutHand(int32 LayoutSeatIndex) const
{
    TArray<AActor*> Hands;

    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(),
        ASHHand::StaticClass(),
        Hands
    );

    for (AActor* Actor : Hands)
    {
        ASHHand* Hand = Cast<ASHHand>(Actor);

        if (IsValid(Hand) && Hand->GetLayoutSeatIndex() == LayoutSeatIndex)
        {
            return Hand;
        }
    }

    return nullptr;
}

void ASHPlayerController::ClientReceiveCardDefinition_Implementation(ASHCard* Card, TSubclassOf<UCardDefinition> CardDefinition)
{
    if (!IsValid(Card) || !CardDefinition)
    {
        return;
    }

    Card->ApplyOwnerCardDefinition(CardDefinition);
    Card->SetFaceUp(true);

	if (ASHPlayerState* LocalPS = GetPlayerState<ASHPlayerState>())
	{
		if (ASHHand* VisualHand = FindVisualHandForLogicalHand(LocalPS->GetHand()))
		{
			VisualHand->RefreshPairActivationAvailability();
		}
	}
}

void ASHPlayerController::ClientSetPairTargetSelection_Implementation(
	ASHCard* CardA, ASHCard* CardB, bool bSelectingTarget,
	FName EffectPresentationId)
{
	if (!IsLocalController())
	{
		return;
	}

	// Always process teardown. An effect may move or destroy either source card
	// before this RPC is received, so invalid card references cannot block cleanup.
	if (!bSelectingTarget)
	{
		if (ASHPlayerState* LocalPS = GetPlayerState<ASHPlayerState>();
			IsValid(LocalPS) && IsValid(CardA) && IsValid(CardB))
		{
			if (ASHHand* VisualHand = FindVisualHandForLogicalHand(LocalPS->GetHand()))
			{
				VisualHand->PresentPairTargetSelection(
					CardA, CardB, false, EffectPresentationId);
			}
		}
		StopPairTargetingIndicator();
		ClearLocalEffectSelectionState();
		return;
	}

	ASHPlayerState* LocalPS = GetPlayerState<ASHPlayerState>();
	if (!IsValid(LocalPS) || !IsValid(CardA) || !IsValid(CardB))
	{
		StopPairTargetingIndicator();
		return;
	}
	if (ASHHand* VisualHand = FindVisualHandForLogicalHand(LocalPS->GetHand()))
	{
		VisualHand->PresentPairTargetSelection(
			CardA, CardB, true, EffectPresentationId);
	}
	StartPairTargetingIndicator(CardA, CardB, EffectPresentationId);
}

ASHHand* ASHPlayerController::FindVisualHandForPlayer(const ASHPlayerState* InPlayerState) const
{
    if (!IsValid(InPlayerState))
    {
        return nullptr;
    }

    TArray<AActor*> Hands;

    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(),
        ASHHand::StaticClass(),
        Hands
    );

    for (AActor* Actor : Hands)
    {
        ASHHand* Hand = Cast<ASHHand>(Actor);

        if (IsValid(Hand) &&
            Hand->GetRepresentedPlayerState() == InPlayerState)
        {
            return Hand;
        }
    }

    return nullptr;
}

void ASHPlayerController::ServerActivateStoredPair_Implementation(ASHCard* Card)
{


    if (!IsValid(Card))
    {
        return;
    }

    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();
    ASHGameMode* SHGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();
    if (IsValid(SHPlayerState) && IsValid(SHGameMode) &&
        SHGameMode->SubmitActivationPairSelection(SHPlayerState, Card))
    {
        return;
    }

	if (IsValid(SHPlayerState) && IsValid(SHGameMode))
	{
		SHGameMode->RequestStoredPairActivation(SHPlayerState, Card);
	}
}

void ASHPlayerController::ServerCreatePair_Implementation(ASHCard* CardA, ASHCard* CardB)
{
    if (!IsValid(CardA) || !IsValid(CardB) || CardA == CardB)
    {
        return;
    }

    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();
    if (!IsValid(SHPlayerState))
    {
        return;
    }

    ASHHand* Hand = SHPlayerState->GetHand();
    if (!IsValid(Hand))
    {
        return;
    }

    if (CardA->GetOwningHand() != Hand || CardB->GetOwningHand() != Hand)
    {
        return;
    }

    if (!Hand->ContainsCard(CardA) || !Hand->ContainsCard(CardB))
    {
        return;
    }

    ASHGameMode* SHGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();

    if (!IsValid(SHGameMode) || SHGameMode->IsWaitingForPlayerSelection())
    {
        return;
    }

    if (!SHGameMode->AreCardsPairCompatible(CardA, CardB))
    {
        return;
    }

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
    {
        return;
    }

    if (SHGameState->GetCurrentPlayer() != SHPlayerState)
    {
        return;
    }

    const ETurnPhase Phase = SHGameState->GetTurnPhase();

    if (Phase != ETurnPhase::FirstPairing && Phase != ETurnPhase::SecondPairing)
    {
        return;
    }

    UTurnComponent* TurnComponent = SHGameMode->GetTurnComponent();
    if (!IsValid(TurnComponent) || TurnComponent->IsPairingActionUsed())
    {
        return;
    }

	// Register before ActivatePair: adding the pair can synchronously refresh the
	// listen-server layout and report it settled.
	TurnComponent->RegisterPendingPairSettlement(CardA, CardB);
    SHGameMode->ActivatePair(SHPlayerState, CardA, CardB);

    TurnComponent->MarkPairingActionUsed();

    if (Phase == ETurnPhase::FirstPairing)
    {
        TurnComponent->CompleteCurrentPhase(ETurnPhaseEndReason::PairCreated);
    }
}

void ASHPlayerController::BeginLocalCardDrag(ASHCard* Card)
{
    if (!IsLocalController() || !IsValid(Card))
    {
        return;
    }
    LocallyDraggedCard = Card;
    LastPreviewCard = nullptr;
    LastPreviewInsertIndex = INDEX_NONE;
	bLastPreviewIsOwnHandReorder = false;
}

void ASHPlayerController::EndLocalCardDrag(ASHCard* Card)
{
    if (!IsLocalController() || (IsValid(Card) && LocallyDraggedCard != Card))
    {
        return;
    }
	const bool bOwnCardReorder = bLastPreviewIsOwnHandReorder && LastPreviewCard == Card &&
		LastPreviewInsertIndex != INDEX_NONE;
    if (bOwnCardReorder)
    {
        ServerReorderOwnCard(Card, LastPreviewInsertIndex);
    }
	else if (IsValid(Card))
	{
		ASHPlayerState* LocalPlayerState = GetPlayerState<ASHPlayerState>();
		ASHHand* TargetLogicalHand = IsValid(LocalPlayerState) ? LocalPlayerState->GetHand() : nullptr;
		ASHHand* NearestLogicalHand = FindNearestDropHand(Card);
		const bool bHasPreview = LastPreviewCard == Card && LastPreviewInsertIndex != INDEX_NONE;
		const bool bCommitDraw = bHasPreview && IsValid(TargetLogicalHand) &&
			NearestLogicalHand == TargetLogicalHand;

		UE_LOG(LogTemp, Log,
			TEXT("[SH_DROP] Card=%s Commit=%d NearestHand=%s TargetHand=%s Index=%d"),
			*GetNameSafe(Card), bCommitDraw, *GetNameSafe(NearestLogicalHand),
			*GetNameSafe(TargetLogicalHand), LastPreviewInsertIndex);
		ServerSetCardDropDecision(Card, bCommitDraw, LastPreviewInsertIndex);
		if (bCommitDraw)
		{
			ServerTakeCard(Card, LastPreviewInsertIndex);
		}
	}
    LocallyDraggedCard = nullptr;
    LastPreviewCard = nullptr;
    LastPreviewInsertIndex = INDEX_NONE;
	bLastPreviewIsOwnHandReorder = false;
}

ASHHand* ASHPlayerController::FindNearestDropHand(const ASHCard* DraggedCard) const
{
	const ASHGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ASHGameState>() : nullptr;
	if (!IsValid(GameState) || !IsValid(DraggedCard))
	{
		return nullptr;
	}

	ASHHand* NearestLogicalHand = nullptr;
	double NearestDistanceSquared = TNumericLimits<double>::Max();

	// Read the actual release point instead of the dragged actor transform. The
	// Blueprint presentation may already start snapping the card to a preview
	// slot before SetDreggedCard(nullptr) reaches EndLocalCardDrag.
	FVector DropLocation = DraggedCard->GetActorLocation();
	FVector CursorWorldOrigin;
	FVector CursorWorldDirection;
	if (DeprojectMousePositionToWorld(CursorWorldOrigin, CursorWorldDirection))
	{
		FHitResult CursorHit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CardDropHand), true);
		QueryParams.AddIgnoredActor(DraggedCard);
		const FVector TraceEnd = CursorWorldOrigin + CursorWorldDirection * HALF_WORLD_MAX;
		if (GetWorld()->LineTraceSingleByChannel(
			CursorHit, CursorWorldOrigin, TraceEnd, ECC_Visibility, QueryParams))
		{
			DropLocation = CursorHit.ImpactPoint;
			if (const ASHCard* CardUnderCursor = Cast<ASHCard>(CursorHit.GetActor()))
			{
				ASHHand* HitCardHand = CardUnderCursor->GetOwningHand();
				if (GameState->GetParticipantHands().Contains(HitCardHand))
				{
					return HitCardHand;
				}
			}
		}
	}
	for (ASHHand* LogicalHand : GameState->GetParticipantHands())
	{
		ASHHand* VisualHand = FindVisualHandForLogicalHand(LogicalHand);
		if (!IsValid(LogicalHand) || !IsValid(VisualHand))
		{
			continue;
		}

		double HandDistanceSquared = TNumericLimits<double>::Max();
		if (const USHHandCardsLayoutComponent* Layout =
			VisualHand->FindComponentByClass<USHHandCardsLayoutComponent>())
		{
			const double LayoutDistance = Layout->GetDistanceToLayout(DropLocation);
			HandDistanceSquared = LayoutDistance * LayoutDistance;
		}
		for (const ASHCard* CandidateCard : LogicalHand->GetCards())
		{
			if (IsValid(CandidateCard) && CandidateCard != DraggedCard)
			{
				HandDistanceSquared = FMath::Min(HandDistanceSquared,
					FVector::DistSquared(DropLocation, CandidateCard->GetActorLocation()));
			}
		}

		if (HandDistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = HandDistanceSquared;
			NearestLogicalHand = LogicalHand;
		}
	}
	return NearestLogicalHand;
}

void ASHPlayerController::ServerSetCardDropDecision_Implementation(
	ASHCard* Card, bool bCommitDraw, int32 InsertIndex)
{
	PendingDropCard = nullptr;
	PendingDropInsertIndex = INDEX_NONE;
	if (!IsValid(Card))
	{
		return;
	}
	if (!bCommitDraw)
	{
		return;
	}

	ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();
	ASHHand* TargetHand = IsValid(SHPlayerState) ? SHPlayerState->GetHand() : nullptr;
	if (IsValid(TargetHand) && Card->GetOwningHand() != TargetHand &&
		InsertIndex >= 0 && InsertIndex <= TargetHand->GetCardCount())
	{
		PendingDropCard = Card;
		PendingDropInsertIndex = InsertIndex;
	}
}

void ASHPlayerController::UpdateLocalCardDropPreview(ASHCard* Card, int32 InsertIndex, bool bOwnHandReorder)
{
    if (!IsLocalController() || !IsValid(Card) || InsertIndex < 0 ||
        (LastPreviewCard == Card && LastPreviewInsertIndex == InsertIndex &&
            bLastPreviewIsOwnHandReorder == bOwnHandReorder))
    {
        return;
    }
    LastPreviewCard = Card;
    LastPreviewInsertIndex = InsertIndex;
    bLastPreviewIsOwnHandReorder = bOwnHandReorder;
    if (!bOwnHandReorder)
    {
        ServerSetCardDropPreview(Card, InsertIndex);
    }
}

void ASHPlayerController::ServerSetCardDropPreview_Implementation(ASHCard* Card, int32 InsertIndex)
{
    ASHPlayerState* PS = GetPlayerState<ASHPlayerState>();
    ASHHand* TargetHand = IsValid(PS) ? PS->GetHand() : nullptr;
    if (!IsValid(Card) || !IsValid(TargetHand) || Card->GetOwningHand() == TargetHand ||
        InsertIndex < 0 || InsertIndex > TargetHand->GetCardCount())
    {
        return;
    }
    PendingDropCard = Card;
    PendingDropInsertIndex = InsertIndex;
}

void ASHPlayerController::ServerReorderOwnCard_Implementation(ASHCard* Card, int32 InsertIndex)
{
	if (const ASHGameState* State = GetWorld()->GetGameState<ASHGameState>(); State && State->bReactionPending) { return; }
    ASHPlayerState* PS = GetPlayerState<ASHPlayerState>();
    ASHHand* Hand = IsValid(PS) ? PS->GetHand() : nullptr;
    if (IsValid(Hand))
    {
        Hand->ReorderCard(Card, InsertIndex);
    }
}

void ASHPlayerController::ServerTakeCard_Implementation(ASHCard* Card, int32 InsertIndex)
{
    if (!IsValid(Card))
    {
        return;
    }

	// BP_Hand still has a legacy unconditional ServerTakeCard call. It can be
	// sent before EndLocalCardDrag and used to bypass the local drop decision.
	// Only a draw explicitly approved by ServerSetCardDropDecision is valid.
	if (PendingDropCard != Card || PendingDropInsertIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[SH_DROP][REJECT_UNAPPROVED] Ignoring legacy/premature draw for %s"),
			*GetNameSafe(Card));
		return;
	}

	InsertIndex = PendingDropInsertIndex;
    PendingDropCard = nullptr;
    PendingDropInsertIndex = INDEX_NONE;

    ASHPlayerState* SHPlayerState = GetPlayerState<ASHPlayerState>();

    if (!IsValid(SHPlayerState))
    {
        return;
    }

    ASHHand* TargetHand = SHPlayerState->GetHand();
    ASHHand* SourceHand = Card->GetOwningHand();

    UE_LOG(LogTemp, Warning,
        TEXT("ServerTakeCard: PC=%s PS=%s TargetHand=%s IsLocalController=%s"),
        *GetNameSafe(this),
        *GetNameSafe(SHPlayerState),
        *GetNameSafe(TargetHand),
        IsLocalController() ? TEXT("TRUE") : TEXT("FALSE"));

    if (!IsValid(SourceHand) || !IsValid(TargetHand))
    {
        return;
    }

    if (SourceHand == TargetHand)
    {
        return;
    }

    if (InsertIndex < 0 || InsertIndex > TargetHand->GetCardCount())
    {
        return;
    }

    ASHGameMode* SHGameMode = GetWorld()->GetAuthGameMode<ASHGameMode>();

    if (!IsValid(SHGameMode) || SHGameMode->IsWaitingForPlayerSelection())
    {
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("BEFORE ADD: TargetHand=%s Card=%s"),
        *GetNameSafe(TargetHand),
        *GetNameSafe(Card));

    ASHGameState* SHGameState = GetWorld()->GetGameState<ASHGameState>();

    if (!IsValid(SHGameState))
    {
        return;
    }

    const bool bSourceIsNPC = SourceHand->IsLogicalNPC();
    if (bSourceIsNPC)
    {
        ASHCard* TopCard = SourceHand->GetTopCard();
        if (!IsValid(TopCard))
        {
            return;
        }

        // An NPC pile is one interaction target. Its cards overlap and a local
        // cursor trace may identify a covered card while replication/layout is
        // settling. Always resolve that click to the authoritative stack top.
        if (Card != TopCard)
        {
            UE_LOG(LogTemp, Log,
                TEXT("[SH_DRAW][NPC_TOP] Requested=%s ResolvedTop=%s Source=%s"),
                *GetNameSafe(Card), *GetNameSafe(TopCard), *GetNameSafe(SourceHand));
            Card = TopCard;
        }
    }

    if (Card->GetCardZone() != ECardZone::Hand)
    {
        return;
    }

    if (!SourceHand->ContainsCard(Card))
    {
        return;
    }

    ASHPlayerState* SourcePlayerState = nullptr;
    for (APlayerState* CandidatePlayerState : SHGameState->PlayerArray)
    {
        ASHPlayerState* Candidate = Cast<ASHPlayerState>(CandidatePlayerState);
        if (IsValid(Candidate) && Candidate->GetHand() == SourceHand)
        {
            SourcePlayerState = Candidate;
            break;
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("[SH_DRAW] Card=%s Source=%s SourceType=%s SourceCards=%d TopCard=%s Target=%s InsertIndex=%d"),
        *GetNameSafe(Card), *GetNameSafe(SourceHand),
        bSourceIsNPC ? TEXT("NPC") : TEXT("Player"),
        SourceHand->GetCardCount(),
        bSourceIsNPC ? *GetNameSafe(SourceHand->GetTopCard()) : TEXT("N/A"),
        *GetNameSafe(TargetHand), InsertIndex);

    UTurnComponent* TurnComponent = SHGameMode->GetTurnComponent();
    if (!IsValid(TurnComponent) || !TurnComponent->CanDrawCardFromHand(SHPlayerState, SourceHand))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[SH_DRAW][REJECT] Draw rules rejected Card=%s Source=%s Player=%s"),
            *GetNameSafe(Card), *GetNameSafe(SourceHand), *GetNameSafe(SHPlayerState));
        return;
    }

    if (bSourceIsNPC)
    {
        SourceHand->TakeTopCard();
    }
    else
    {
        SourceHand->RemoveCard(Card);
    }
    TargetHand->AddCard(Card, InsertIndex);

    UE_LOG(LogTemp, Log,
        TEXT("[SH_DRAW][ACCEPT] Card=%s Source=%s SourceCardsAfter=%d Target=%s TargetCardsAfter=%d"),
        *GetNameSafe(Card), *GetNameSafe(SourceHand), SourceHand->GetCardCount(),
        *GetNameSafe(TargetHand), TargetHand->GetCardCount());

    ClientReceiveCardDefinition(
        Card,
        Card->GetCardDefinition()
    );

    TurnComponent->HandleCardDrawnFromHand(SHPlayerState, SourceHand, Card);
}

