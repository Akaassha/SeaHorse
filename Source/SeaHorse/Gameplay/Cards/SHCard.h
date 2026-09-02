// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Slate/WidgetRenderer.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardFragment.h"
#include "SHCard.generated.h"

class UCardDefinition;
class ASHHand;
class UTextureRenderTarget2D;

UENUM(BlueprintType)
enum class ECardZone : uint8
{
	None,
	Deck,
	Hand,
	Activation,
	Victory
};

UCLASS()
class SEAHORSE_API ASHCard : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASHCard();

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void Initialize();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TSubclassOf<UCardDefinition> GetCardDefinition();

	void SetCardDefinition(TSubclassOf<UCardDefinition> CardDefinition);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	ASHHand* GetOwningHand() const;

	UFUNCTION(BlueprintCallable)
	void SetFaceUp(bool bNewFaceUp);

	UFUNCTION(BlueprintImplementableEvent)
	void UpdateCardVisual(bool bShowFront);

	void Reveal();

	UFUNCTION()
	void OnRep_RevealedCardDefinition();

	void SetCardZone(ECardZone NewZone);

	UFUNCTION(BlueprintPure)
	ECardZone GetCardZone() const;

	UFUNCTION(BlueprintCallable)
	void RefreshCardFace();

	UPROPERTY(ReplicatedUsing = OnRep_CardDefinition, BlueprintReadOnly, EditAnywhere, meta = (ExposeOnSpawn = "true"))
	TSubclassOf<UCardDefinition> CardDefinition;

	void ApplyOwnerCardDefinition(TSubclassOf<UCardDefinition> InCardDefinition);

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RevealedCardDefinition)
	TSubclassOf<UCardDefinition> RevealedCardDefinition;
	
	UPROPERTY(ReplicatedUsing = OnRep_CardZone, BlueprintReadOnly)
	ECardZone CardZone = ECardZone::None;

	UFUNCTION()
	void OnRep_CardZone();

	UFUNCTION()
	void OnRep_CardDefinition();

	virtual void OnRep_Owner() override;

	UPROPERTY(EditDefaultsOnly, Category = "Card|Visual")
	TSubclassOf<UUserWidget> CardFaceWidgetClass;

	UFUNCTION(BlueprintImplementableEvent, Category = "Card|Visual")
	void OnCardFaceRendered(UTextureRenderTarget2D* RenderTarget);


	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UUserWidget> CardFaceWidget;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool bFaceUp = false;

private:

	void OnCardZoneChanged();

	TUniquePtr<FWidgetRenderer> WidgetRenderer;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> CardFaceRenderTarget;

public:

};
