// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHHand.generated.h"

class ASHCard;
class AVictoryStack;
class ASHPlayerState;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNPCStackShuffled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHandCardsChanged, int32, CardCount);

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

	UFUNCTION(BlueprintImplementableEvent)
	void OnPairEffectActivated(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintImplementableEvent)
	void OnPairActivated(ASHCard* CardA, ASHCard* CardB);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Cards|Activation")
	virtual void AddActivationPair(ASHCard* CardA, ASHCard* CardB);
	void AddActivationPairToLogicalHand(ASHCard* CardA, ASHCard* CardB);
	void RefreshActivationPairsPresentation();

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

	AVictoryStack* GetVictoryStack() const
	{
		return VictoryStack;
	}

	void SetRepresentedPlayerState(ASHPlayerState* InPlayerState)
	{
		RepresentedPlayerState = InPlayerState;
		RepresentedLogicalHand = nullptr;
	}

	void SetRepresentedHand(ASHHand* InHand)
	{
		RepresentedPlayerState = nullptr;
		RepresentedLogicalHand = InHand;
	}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	ASHPlayerState* GetRepresentedPlayerState() const;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	ASHHand* GetRepresentedHand() const;

	UPROPERTY(BlueprintAssignable, Category = "Cards|NPC")
	FOnNPCStackShuffled OnNPCStackShuffled;
	UPROPERTY(BlueprintAssignable, Category = "Cards")
	FOnHandCardsChanged OnHandCardsChanged;

private:
	void RefreshLocalCardsPresentation();

	UPROPERTY(Transient)
	TObjectPtr<ASHPlayerState> RepresentedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<ASHHand> RepresentedLogicalHand;
	
	FTransform LayoutTransform;

	UPROPERTY(ReplicatedUsing = OnRep_IsNPC)
	bool bIsNPC = false;
	UFUNCTION()
	void OnRep_IsNPC();
	bool ShouldShuffleForCard(const ASHCard* Card) const;

	UPROPERTY(EditDefaultsOnly, Category = "Cards|NPC", meta = (Units = "cm"))
	FVector NPCStackCardOffset = FVector(0.0, 0.0, 0.2);
};
