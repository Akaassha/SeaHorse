#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHPlayerRepresentation.generated.h"

class ASHHand;
class ASHPlayerState;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerRepresentationChanged, ASHPlayerState*, PlayerState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerPickerStateChanged, bool, bSelectable);

/**
 * World-space, clickable representation of the human player displayed by an SHHand.
 * A Blueprint subclass can provide the mesh/widget and react to the events below;
 * selection and player identity remain in native multiplayer-safe code.
 */
UCLASS(Blueprintable)
class SEAHORSE_API ASHPlayerRepresentation : public AActor
{
	GENERATED_BODY()

public:
	ASHPlayerRepresentation();

	virtual void NotifyActorOnClicked(FKey ButtonPressed = EKeys::LeftMouseButton) override;

	void BindToHand(ASHHand* InVisualHand);
	void RefreshFromHand();
	void SetSelectable(bool bInSelectable);

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	ASHHand* GetVisualHand() const { return VisualHand; }

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	ASHPlayerState* GetRepresentedPlayerState() const { return RepresentedPlayerState; }

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	FText GetPlayerDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	UTexture2D* GetPlayerAvatar() const;

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	bool IsPlayerSelectionEnabled() const { return bSelectable; }

	UPROPERTY(BlueprintAssignable, Category = "Player Representation")
	FOnPlayerRepresentationChanged OnRepresentationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Player Representation")
	FOnPlayerPickerStateChanged OnPickerStateChanged;

protected:
	/** Used in PIE/offline and until an online avatar provider returns a texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Representation")
	TObjectPtr<UTexture2D> FallbackAvatar;

	/** Optional editor/offline label; PlayerState name takes priority when available. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Representation")
	FText OfflineDisplayName;

	/** Extension point for Steam or another online avatar provider. */
	UFUNCTION(BlueprintNativeEvent, Category = "Player Representation")
	UTexture2D* ResolvePlayerAvatar(ASHPlayerState* PlayerState) const;
	virtual UTexture2D* ResolvePlayerAvatar_Implementation(ASHPlayerState* PlayerState) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<ASHHand> VisualHand;

	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> RepresentedPlayerState;

	bool bSelectable = false;
};
