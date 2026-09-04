#include "SeaHorse/Gameplay/Player/SHPlayerRepresentation.h"

#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "Engine/Texture2D.h"

ASHPlayerRepresentation::ASHPlayerRepresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void ASHPlayerRepresentation::NotifyActorOnClicked(FKey ButtonPressed)
{
	if (bSelectable && IsValid(RepresentedPlayerState))
	{
		ASHPlayerController* LocalController = GetWorld()
			? Cast<ASHPlayerController>(GetWorld()->GetFirstPlayerController())
			: nullptr;
		if (IsValid(LocalController) && LocalController->IsLocalController() &&
			LocalController->TrySubmitPlayerSelectionForPicker(RepresentedPlayerState))
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

void ASHPlayerRepresentation::RefreshFromHand()
{
	ASHPlayerState* NewPlayerState = IsValid(VisualHand)
		? VisualHand->GetRepresentedPlayerState()
		: nullptr;
	if (RepresentedPlayerState == NewPlayerState)
	{
		return;
	}

	RepresentedPlayerState = NewPlayerState;
	OnRepresentationChanged.Broadcast(RepresentedPlayerState);
}

void ASHPlayerRepresentation::SetSelectable(bool bInSelectable)
{
	const bool bNewSelectable = bInSelectable && IsValid(RepresentedPlayerState);
	if (bSelectable == bNewSelectable)
	{
		return;
	}

	bSelectable = bNewSelectable;
	OnPickerStateChanged.Broadcast(bSelectable);
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
