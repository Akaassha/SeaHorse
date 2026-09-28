#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HandRevealWidget.generated.h"

class ASHHand;
class ASHHandRevealPawn;
class UButton;
class UTextBlock;

/** Designer-extensible controls. The reveal session and permissions live outside the widget. */
UCLASS(Blueprintable)
class SEAHORSE_API UHandRevealWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeReveal(ASHHandRevealPawn* Pawn, ASHHand* SourceHand, bool bCanFinish);
	void InitializeComparison(ASHHandRevealPawn* Pawn, ASHHand* LargerHand, ASHHand* ReceivingHand);
	void RefreshReveal();
	UFUNCTION(BlueprintPure, Category = "Hand Reveal")
	ASHHandRevealPawn* GetRevealPawn() const;
	UFUNCTION(BlueprintPure, Category = "Hand Reveal")
	ASHHand* GetSourceHand() const;
	UFUNCTION(BlueprintPure, Category = "Hand Comparison")
	ASHHand* GetReceivingHand() const;
	UFUNCTION(BlueprintPure, Category = "Hand Reveal")
	bool CanFinishViewing() const;
	UFUNCTION(BlueprintCallable, Category = "Hand Reveal")
	void FinishViewing();
	UFUNCTION(BlueprintImplementableEvent, Category = "Hand Reveal")
	void OnRevealChanged();

protected:
	virtual void NativeOnInitialized() override;
	/** Optional Designer button. Its click is wired automatically. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Hand Reveal")
	TObjectPtr<UButton> FinishButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Hand Reveal")
	TObjectPtr<UTextBlock> InformationText;

private:
	UPROPERTY(Transient)
	TObjectPtr<ASHHandRevealPawn> RevealPawn;
	UPROPERTY(Transient)
	TObjectPtr<ASHHand> RevealedHand;
	UPROPERTY(Transient)
	TObjectPtr<ASHHand> ComparisonReceivingHand;
	bool bShowFinishButton = false;
};
