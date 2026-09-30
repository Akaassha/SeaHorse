// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "SHPlayerState.generated.h"

class ASHHand;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVictoryPointsChanged, int32, NewVictoryPoints);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDisplayNameChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProtectionChanged, bool, bProtected);
/**
 * 
 */
UCLASS()
class SEAHORSE_API ASHPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRep_PlayerName() override;

	UPROPERTY(BlueprintAssignable, Category = "Player")
	FOnPlayerDisplayNameChanged OnPlayerDisplayNameChanged;

	void SetHand(ASHHand* NewHand);
	ASHHand* GetHand() const;

	UPROPERTY(ReplicatedUsing = OnRep_SeatIndex, BlueprintReadOnly)
	int32 SeatIndex = INDEX_NONE;

	UFUNCTION()
	void SetSeatIndex(int32 NewSeatIndex);

	UFUNCTION()
	int32 GetSeatIndex() const;

	UFUNCTION()
	void OnRep_SeatIndex();

	UFUNCTION(BlueprintPure, Category = "Score")
	int32 GetVictoryPoints() const { return VictoryPoints; }

	UPROPERTY(BlueprintAssignable, Category = "Score")
	FOnVictoryPointsChanged OnVictoryPointsChanged;

	void SetVictoryPoints(int32 NewVictoryPoints);

	UFUNCTION(BlueprintPure, Category = "Card Effects")
	bool IsProtectedFromCardEffects() const { return bProtectedFromCardEffects; }
	void SetProtectedFromCardEffects(bool bProtected);

	/** Local notification on both the server and clients receiving the replicated state. */
	UPROPERTY(BlueprintAssignable, Category = "Card Effects")
	FOnProtectionChanged OnProtectionChanged;
	
protected:
	UPROPERTY(ReplicatedUsing = OnRep_ProtectedFromCardEffects, BlueprintReadOnly, Category = "Card Effects")
	bool bProtectedFromCardEffects = false;

	UFUNCTION()
	void OnRep_ProtectedFromCardEffects();

	UPROPERTY(ReplicatedUsing = OnRep_Hand, BlueprintReadOnly, Category = "Hand")
	TObjectPtr<ASHHand> Hand;

	UFUNCTION()
	void OnRep_Hand();

	UPROPERTY(ReplicatedUsing = OnRep_VictoryPoints, BlueprintReadOnly, Category = "Score")
	int32 VictoryPoints = 0;

	UFUNCTION()
	void OnRep_VictoryPoints();
};
