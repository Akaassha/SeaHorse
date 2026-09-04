// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHHand.generated.h"

class ASHCard;
class AVictoryStack;
class ASHPlayerState;
class ASHPlayerRepresentation;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCStackShuffled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHandCardsChanged, int32, CardCount);

UENUM(BlueprintType)
enum class EActivationPairState : uint8
{
	Creating,
	Ready,
	ClickPresentation,
	AbilityEffect,
	VictoryPresentation,
	Completed
};

USTRUCT(BlueprintType)
struct FActivatedPair
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<ASHCard> CardA;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<ASHCard> CardB;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bActivated = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EActivationPairState State = EActivationPairState::Creating;

	bool operator==(const FActivatedPair& Other) const
	{
		return CardA == Other.CardA && CardB == Other.CardB;
	}

	friend uint32 GetTypeHash(const FActivatedPair& Pair)
	{
		return HashCombine(GetTypeHash(Pair.CardA.Get()), GetTypeHash(Pair.CardB.Get()));
	}
};

UCLASS()
class SEAHORSE_API ASHHand : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASHHand();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void Initialize();

	void SetShowCardFronts(bool bShow);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPairEffectActivated(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPairCreated(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPairClicked(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPairReadyForVictory(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintImplementableEvent)
	void OnPairEffectActivated(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintImplementableEvent)
	void OnPairActivated(ASHCard* CardA, ASHCard* CardB);

	/** Presentation hook fired once when this visual hand receives a newly created pair. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cards|Activation")
	void OnPairCreated(ASHCard* CardA, ASHCard* CardB);

	/** Presentation hook fired when the stored pair's gameplay effect is activated. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cards|Activation")
	void OnStoredPairActivated(ASHCard* CardA, ASHCard* CardB);

	/** Presentation hook fired locally after both cards reach their final table transforms. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cards|Activation")
	void OnPairSettled(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintImplementableEvent, Category = "Cards|Activation")
	void OnPairClicked(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintImplementableEvent, Category = "Cards|Activation")
	void OnPairReadyForVictory(ASHCard* CardA, ASHCard* CardB);

	/** Local-only presentation hook for showing/hiding an activation indicator. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cards|Activation")
	void OnPairActivationAvailabilityChanged(ASHCard* CardA, ASHCard* CardB, bool bCanActivate);

	/** Local-only hover hook. It is emitted only while this pair can be activated locally. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cards|Activation")
	void OnActivatablePairHoverChanged(ASHCard* CardA, ASHCard* CardB, bool bHovered);

	/** Local presentation query; true only for the owning player during their turn. */
	UFUNCTION(BlueprintPure, Category = "Cards|Activation")
	bool CanLocalPlayerActivatePair(ASHCard* Card) const;
	void SetLocalActivatableCardHovered(ASHCard* Card, bool bHovered);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cards|Activation")
	virtual void AddActivationPair(ASHCard* CardA, ASHCard* CardB);
	void AddActivationPairToLogicalHand(ASHCard* CardA, ASHCard* CardB);
	void RefreshActivationPairsPresentation();
	void RefreshPairActivationAvailability();
	void NotifyPairSettled(ASHCard* CardA, ASHCard* CardB);
	void PresentPairCreated(ASHCard* CardA, ASHCard* CardB);
	void PresentStoredPairActivated(ASHCard* CardA, ASHCard* CardB);

	/** Call on the authoritative BP instance before starting an asynchronous presentation. */
	UFUNCTION(BlueprintCallable, Category = "Cards|Presentation")
	void BeginTurnBlockingEffect(FName EffectId);

	/** Releases a matching presentation lock and allows a deferred turn to finish. */
	UFUNCTION(BlueprintCallable, Category = "Cards|Presentation")
	void FinishTurnBlockingEffect(FName EffectId);

	/** Local presentation gate used by the activation-pair layout. */
	bool IsPairMovementBlocked() const { return !LocalPresentationBlocks.IsEmpty(); }

	bool RemoveActivationPair(ASHCard* CardA, ASHCard* CardB);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_Cards)
	TArray<TObjectPtr<ASHCard>> Cards;

	UPROPERTY(ReplicatedUsing = OnRep_ActivationPairs)
	TArray<FActivatedPair> ActivationPairs;
	
	bool bShowCardFronts = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Player Area")
	TObjectPtr<AVictoryStack> VictoryStack;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	virtual void AddCard(ASHCard* Card, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Hand")
	bool IsNPC() const;
	/** Authoritative mode of this logical card container, independent of local view mapping. */
	bool IsLogicalNPC() const { return bIsNPC; }
	void SetIsNPC(bool bNewIsNPC);

	UFUNCTION(BlueprintPure, Category = "Cards|NPC")
	ASHCard* GetTopCard() const;

	/** True when this logical hand contains the automatic-loss Sea Horse card. */
	UFUNCTION(BlueprintPure, Category = "Cards")
	bool HasSeaHorseCard() const;

	/** Server-only reordering used by the Sea Horse hand-management rule. */
	bool ReorderCard(ASHCard* Card, int32 InsertIndex);
	ASHCard* TakeTopCard();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cards|NPC")
	void ShuffleStack();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cards|NPC")
	void RevealStack();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<ASHCard*> GetCards();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<FActivatedPair> GetActivationPairs();

	const TArray<FActivatedPair>& GetLogicalActivationPairs() const { return ActivationPairs; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<ASHCard*> GetActivationCards();

	UFUNCTION(BlueprintCallable)
	void RemoveCard(ASHCard* Card);

	UFUNCTION()
	virtual void OnRep_Cards();

	UFUNCTION()
	void OnRep_ActivationPairs();

	UFUNCTION(BlueprintPure)
	int32 GetCardCount() const;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void UpdateCardPositions();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_LayoutSeatIndex)
	int32 LayoutSeatIndex = INDEX_NONE;

	UFUNCTION()
	void OnRep_LayoutSeatIndex();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	int32 GetLayoutSeatIndex() const;
	void SetLayoutSeatIndex(int32 NewLayoutSeatIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	const FTransform& GetLayoutTransform() const;

	void RefreshCardsPresentation();

	/** Default native presentation for an uncontrolled hand's face-down stack. */
	void LayoutNPCStack(ASHHand* LogicalNPCStack);

	bool ContainsCard(ASHCard* CardB);

	FActivatedPair* FindActivationPair(ASHCard* Card);
	void SetActivationPairState(ASHCard* CardA, ASHCard* CardB, EActivationPairState NewState);

	AVictoryStack* GetVictoryStack() const
	{
		return VictoryStack;
	}

	UFUNCTION(BlueprintPure, Category = "Player Area")
	ASHPlayerRepresentation* GetPlayerPicker() const { return PlayerPicker; }

	void SetRepresentedPlayerState(ASHPlayerState* InPlayerState);

	void SetRepresentedHand(ASHHand* InHand);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	ASHPlayerState* GetRepresentedPlayerState() const;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	ASHHand* GetRepresentedHand() const;

	UPROPERTY(BlueprintAssignable, Category = "Cards|NPC")
	FOnNPCStackShuffled OnNPCStackShuffled;
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnHandCardsChanged OnHandCardsChanged;

private:
	void SendPairPresentationToPlayerControllers(ASHCard* CardA, ASHCard* CardB,
		bool bEffectActivation) const;
	void RefreshPlayerPicker();
	void RefreshLocalCardsPresentation();

	/** World-space player representation/picker placed on the level for this visual hand slot. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Player Area", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ASHPlayerRepresentation> PlayerPicker;

	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> RepresentedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<ASHHand> RepresentedLogicalHand;

	/** Local presentation cache; prevents replication/layout refreshes from replaying pair-created effects. */
	UPROPERTY(Transient)
	TArray<FActivatedPair> PresentedActivationPairs;

	/** Local cache ensuring the layout completion hook is emitted once per pair. */
	UPROPERTY(Transient)
	TArray<FActivatedPair> SettledActivationPairs;

	/** Prevents multicast and owner RPC delivery from replaying an activation presentation. */
	UPROPERTY(Transient)
	TArray<FActivatedPair> PresentedEffectActivations;

	/** Pairs for which the local owner's BP currently shows an activation indicator. */
	UPROPERTY(Transient)
	TArray<FActivatedPair> LocallyActivatablePairs;

	UPROPERTY(Transient)
	FActivatedPair LocallyHoveredActivatablePair;
	bool bHasLocallyHoveredActivatablePair = false;

	/** Exists on every instance so client-side BP animations can pause local card movement. */
	TMap<FName, int32> LocalPresentationBlocks;
	
	FTransform LayoutTransform;

	UPROPERTY(ReplicatedUsing = OnRep_IsNPC)
	bool bIsNPC = false;
	UFUNCTION()
	void OnRep_IsNPC();
	bool ShouldShuffleForCard(const ASHCard* Card) const;

	UPROPERTY(EditDefaultsOnly, Category = "Cards|NPC", meta = (Units = "cm"))
	FVector NPCStackCardOffset = FVector(0.0, 0.0, 0.2);
};
