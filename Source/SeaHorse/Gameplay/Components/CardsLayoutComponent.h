#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SeaHorse/Gameplay/SHHand.h"
#include "CardsLayoutComponent.generated.h"

class ASHCard;
class ASHHand;
class USceneComponent;
class USplineComponent;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(SeaHorse), meta=(BlueprintSpawnableComponent))
class SEAHORSE_API USHCardsLayoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USHCardsLayoutComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	virtual void Initialize(const TArray<ASHCard*>& Cards, USplineComponent* Spline,
		USceneComponent* TableCenterDirection);

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	virtual void UpdateCardsPositions(const TArray<ASHCard*>& Cards);

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	virtual void MoveCardsToDesiredPositions(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	virtual void UpdateSingleCardPosition(ASHCard* Card, int32 Index, int32 CardsAmount);

	UFUNCTION(BlueprintPure, Category="Cards Layout")
	virtual double CalculateCardDistanceAtSpline(int32 CardsAmount, int32 ArrayIndex) const;

protected:
	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	TObjectPtr<ASHHand> OwningHand;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	TObjectPtr<USplineComponent> OwnerSpline;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	TObjectPtr<USceneComponent> TableCenterDirectionComponent;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	bool bInitialized = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	TMap<TObjectPtr<ASHCard>, FTransform> CardsTransforms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards Layout", meta=(ClampMin="0.0"))
	double PreferredCardSpacing = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards Layout")
	double CardDepthSpacing = 0.2;
};

UCLASS(Blueprintable, BlueprintType, ClassGroup=(SeaHorse), meta=(BlueprintSpawnableComponent))
class SEAHORSE_API USHHandCardsLayoutComponent : public USHCardsLayoutComponent
{
	GENERATED_BODY()

public:
	virtual void UpdateCardsPositions(const TArray<ASHCard*>& Cards) override;
	virtual void MoveCardsToDesiredPositions(float DeltaTime) override;
	virtual void UpdateSingleCardPosition(ASHCard* Card, int32 Index, int32 CardsAmount) override;

	UFUNCTION(BlueprintPure, Category="Cards Layout")
	double CalculateFocusOffset(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void SetFocusedCardIndex(int32 FocusedCardIndex);

	/** Disables the normal hand hover lift/spread while a local effect is choosing a target. */
	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void SetTargetingFocusSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void SetSelectedCardIndex(int32 SelectedCardIndex);

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void RemoveCardFromLayout(ASHCard* Card);

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void SetDraggedCard(ASHCard* Card);

	/** Compatibility wrapper for existing Blueprint assets. */
	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void SetDreggedCard(ASHCard* Card);

	double GetDistanceToLayout(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void UpdateNPCCardsPositions(const TArray<ASHCard*>& Cards);

	/** Compatibility wrapper for existing Blueprint assets. */
	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void UpdateNPCCardsPositons(const TArray<ASHCard*>& Cards);

protected:
	bool CanLayoutHandCard(const ASHCard* Card) const;
	bool GetExternalCardDropPreview(ASHCard*& OutDraggedCard, int32& OutInsertIndex);
	int32 CalculateDropInsertIndex(const ASHCard* PreviewCard, int32 CurrentCardCount) const;
	bool IsLocallyDraggedCard(const ASHCard* Card) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards Layout")
	float ForwardFocusedOffser = 2.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	int32 FocusedCardIndexValue = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	int32 SelectedCardIndexValue = INDEX_NONE;

	/** Prevents hover focus from fighting the temporary gap shown for a dragged card. */
	bool bSuppressFocusedCardPresentation = false;

	/** Presentation-only suppression controlled locally while choosing an effect target. */
	bool bSuppressFocusedCardForTargeting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards Layout")
	double FocusSpreadDistance = 0.0;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	TMap<TObjectPtr<ASHCard>, FTransform> CardsTransforms_WithNoOffsets;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	TObjectPtr<ASHCard> DraggedCard;

	UPROPERTY(Transient)
	TObjectPtr<ASHCard> PreviewTrackingCard;

	int32 StablePreviewInsertIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cards Layout", meta=(ClampMin="0.0", Units="cm"))
	double DropPreviewSwitchHysteresis = 0.35;
};

UCLASS(Blueprintable, BlueprintType, ClassGroup=(SeaHorse), meta=(BlueprintSpawnableComponent))
class SEAHORSE_API USHActivatableCardsLayoutComponent : public USHCardsLayoutComponent
{
	GENERATED_BODY()

public:
	virtual void UpdateCardsPositions(const TArray<ASHCard*>& Cards) override;
	virtual void MoveCardsToDesiredPositions(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void UpdatePairPositions(const FActivatedPair& Pair, int32 Index, int32 CardsAmount);

	UFUNCTION(BlueprintCallable, Category="Cards Layout")
	void MovePairsToDesiredPositions(float DeltaTime);

	UFUNCTION(BlueprintPure, Category="Cards Layout", meta=(DisplayName="Is Pair Valid?"))
	bool IsPairValid(const FActivatedPair& Pair) const;

protected:
	void SetCardActivatable(ASHCard* Card, bool bActivatable) const;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	TMap<FActivatedPair, FTransform> PairsTransform;

	UPROPERTY(BlueprintReadOnly, Transient, Category="Cards Layout")
	TArray<FActivatedPair> Pairs;
};
