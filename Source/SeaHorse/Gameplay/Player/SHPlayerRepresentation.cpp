#include "SeaHorse/Gameplay/Player/SHPlayerRepresentation.h"

#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "Engine/Texture2D.h"
#include "Components/WidgetComponent.h"

ASHPlayerRepresentation::ASHPlayerRepresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void ASHPlayerRepresentation::BeginPlay()
{
	Super::BeginPlay();
	RefreshInteractionCollision();
}

void ASHPlayerRepresentation::RefreshInteractionCollision()
{
	// The world widget's rectangular hit surface includes transparent padding
	// and can grow with its content. Use the representation mesh for selection.
	TInlineComponentArray<UWidgetComponent*> Widgets(this);
	for (UWidgetComponent* Widget : Widgets)
	{
		Widget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	}

	// Pickers and table layouts are local. Applying this locally also covers
	// engine/BP hover traces, not just the native effect-selection trace.
	SetActorEnableCollision(bSelectable);
}

void ASHPlayerRepresentation::NotifyActorOnClicked(FKey ButtonPressed)
{
	if (bSelectable && IsValid(GetRepresentedHand()))
	{
		ASHPlayerController* LocalController = GetWorld()
			? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController())
			: nullptr;
		if (IsValid(LocalController) && LocalController->IsLocalController() &&
			(LocalController->TrySubmitParticipantSelectionForHand(GetRepresentedHand()) ||
			 LocalController->TrySubmitPlayerSelectionForPicker(RepresentedPlayerState)))
		{
			return;
		}
	}

	Super::NotifyActorOnClicked(ButtonPressed);
}

void ASHPlayerRepresentation::BindToHand(ASHHand* InVisualHand)
{
	VisualHand = InVisualHand;
	RefreshFromHand();
}

ASHHand* ASHPlayerRepresentation::GetRepresentedHand() const
{
	return IsValid(VisualHand) ? VisualHand->GetRepresentedHand() : nullptr;
}

void ASHPlayerRepresentation::RefreshFromHand()
{
	ASHPlayerState* NewPlayerState = IsValid(VisualHand)
		? VisualHand->GetRepresentedPlayerState()
		: nullptr;
	if (RepresentedPlayerState == NewPlayerState)
	{
		return;
	}

	if (IsValid(RepresentedPlayerState))
	{
		RepresentedPlayerState->OnPlayerDisplayNameChanged.RemoveDynamic(this, &ThisClass::HandlePlayerDisplayNameChanged);
	}
	RepresentedPlayerState = NewPlayerState;
	if (IsValid(RepresentedPlayerState))
	{
		RepresentedPlayerState->OnPlayerDisplayNameChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerDisplayNameChanged);
	}
	OnRepresentationChanged.Broadcast(RepresentedPlayerState);
}

void ASHPlayerRepresentation::HandlePlayerDisplayNameChanged()
{
	OnRepresentationChanged.Broadcast(RepresentedPlayerState);
}

void ASHPlayerRepresentation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(RepresentedPlayerState))
	{
		RepresentedPlayerState->OnPlayerDisplayNameChanged.RemoveDynamic(this, &ThisClass::HandlePlayerDisplayNameChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void ASHPlayerRepresentation::SetSelectable(bool bInSelectable)
{
	const bool bNewSelectable = bInSelectable && IsValid(GetRepresentedHand());
	const bool bChanged = bSelectable != bNewSelectable;
	bSelectable = bNewSelectable;
	if (bChanged)
	{
		OnPickerStateChanged.Broadcast(bSelectable);
	}
	RefreshInteractionCollision();
}

FText ASHPlayerRepresentation::GetPlayerDisplayName() const
{
	if (IsValid(RepresentedPlayerState))
	{
		const FString PlayerName = RepresentedPlayerState->GetPlayerName();
		if (!PlayerName.IsEmpty())
		{
			return FText::FromString(PlayerName);
		}

		if (RepresentedPlayerState->GetSeatIndex() != INDEX_NONE)
		{
			return FText::Format(NSLOCTEXT("SeaHorse", "OfflinePlayerBySeat", "Player {0}"),
				FText::AsNumber(RepresentedPlayerState->GetSeatIndex() + 1));
		}
	}

	return OfflineDisplayName.IsEmpty()
		? NSLOCTEXT("SeaHorse", "OfflinePlayer", "Player")
		: OfflineDisplayName;
}

UTexture2D* ASHPlayerRepresentation::GetPlayerAvatar() const
{
	if (UTexture2D* OnlineAvatar = ResolvePlayerAvatar(RepresentedPlayerState))
	{
		return OnlineAvatar;
	}
	return FallbackAvatar;
}

UTexture2D* ASHPlayerRepresentation::ResolvePlayerAvatar_Implementation(ASHPlayerState* PlayerState) const
{
	return nullptr;
}
