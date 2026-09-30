#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHPlayerRepresentation.generated.h"

class ASHHand;
class ASHPlayerState;
class UTexture2D;
class UTextureRenderTarget2D;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USHPlayerFaceWidget;
class FWidgetRenderer;

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

	virtual void BeginPlay() override;
	virtual void NotifyActorOnClicked(FKey ButtonPressed = EKeys::LeftMouseButton) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

	void BindToHand(ASHHand* InVisualHand);
	void RefreshFromHand();
	void SetSelectable(bool bInSelectable);

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	ASHHand* GetVisualHand() const { return VisualHand; }

	/** Logical participant represented at this seat, including NPCs without a PlayerState. */
	UFUNCTION(BlueprintPure, Category = "Player Representation")
	ASHHand* GetRepresentedHand() const;

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	ASHPlayerState* GetRepresentedPlayerState() const { return RepresentedPlayerState; }

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	FText GetPlayerDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	UTexture2D* GetPlayerAvatar() const;

	UFUNCTION(BlueprintPure, Category = "Player Representation")
	bool IsPlayerSelectionEnabled() const { return bSelectable; }

	/** Redraw after changing custom presentation (e.g. status icons). No continuous rendering. */
	UFUNCTION(BlueprintCallable, Category = "Player Representation|Face")
	void RefreshPlayerFace();

	/** Optional explicit mesh selection. Otherwise the unique mesh with FaceMaterialSlot is used. */
	UFUNCTION(BlueprintCallable, Category = "Player Representation|Face")
	bool SetPlayerFaceMesh(UStaticMeshComponent* Mesh);

	/** Retry an online avatar request, or reload after a custom provider changes. */
	UFUNCTION(BlueprintCallable, Category = "Player Representation|Face")
	void ReloadPlayerAvatar();

	UFUNCTION(BlueprintPure, Category = "Player Representation|Face")
	USHPlayerFaceWidget* GetPlayerFaceWidget() const { return PlayerFaceWidget; }

	UFUNCTION(BlueprintPure, Category = "Player Representation|Face")
	UTextureRenderTarget2D* GetPlayerFaceRenderTarget() const { return PlayerFaceRenderTarget; }

	UFUNCTION(BlueprintPure, Category = "Player Representation|Face")
	UMaterialInstanceDynamic* GetPlayerFaceMaterial() const { return PlayerFaceMaterial; }

	/** Local presentation hook emitted while this representation is the valid target under the arrow. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Player Representation|Targeting")
	void OnEffectTargetHoverChanged(bool bTargeted, FName EffectPresentationId);

	UPROPERTY(BlueprintAssignable, Category = "Player Representation")
	FOnPlayerRepresentationChanged OnRepresentationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Player Representation")
	FOnPlayerPickerStateChanged OnPickerStateChanged;

protected:
	/** Opt-in: choose SHPlayerFaceWidget or a Widget Blueprint derived from it. None preserves legacy visuals. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Representation|Face")
	TSubclassOf<USHPlayerFaceWidget> PlayerFaceWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Representation|Face", meta = (ClampMin = "64", ClampMax = "2048"))
	FIntPoint FaceRenderSize = FIntPoint(512, 512);

	/** Match the Widget Designer's Custom preview size. Render resolution can then change without reflow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Representation|Face", meta = (ClampMin = "1", ClampMax = "4096"))
	FIntPoint FaceDesignSize = FIntPoint(512, 512);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Representation|Face")
	FName FaceMaterialSlot = TEXT("PlayerFace");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Representation|Face")
	FName FaceTextureParameter = TEXT("PlayerFaceTexture");

	/** Optional override; otherwise use the material already assigned to the mesh slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player Representation|Face")
	TObjectPtr<UMaterialInterface> FaceBaseMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player Representation|Face")
	bool bLoadSteamAvatar = true;

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
	void RefreshInteractionCollision();
	bool BindFaceMaterial();
	void RestoreFaceMaterial();
	void ReleaseFaceResources();
	void CompleteAvatarRequest(uint64 RequestId, ASHPlayerState* ExpectedPlayer, UTexture2D* Texture);

	friend class FSHPlayerFacePresentationTest;

	UPROPERTY(Transient)
	TObjectPtr<USHPlayerFaceWidget> PlayerFaceWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PlayerFaceRenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlayerFaceMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalFaceMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedAvatar;

	UPROPERTY(Transient)
	TWeakObjectPtr<UStaticMeshComponent> ExplicitFaceMesh;

	UPROPERTY(Transient)
	TWeakObjectPtr<UStaticMeshComponent> BoundFaceMesh;

	FWidgetRenderer* FaceRenderer = nullptr;
	int32 BoundFaceSlotIndex = INDEX_NONE;
	uint64 AvatarRequestId = 0;
	bool bRenderingFace = false;
	bool bPresentationEnded = false;

	UFUNCTION()
	void HandlePlayerDisplayNameChanged();

	UPROPERTY(Transient)
	TObjectPtr<ASHHand> VisualHand;

	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> RepresentedPlayerState;

	bool bSelectable = false;
};
