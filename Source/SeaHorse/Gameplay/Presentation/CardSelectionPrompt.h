#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CardSelectionPrompt.generated.h"

class ASHCard;
class ASHPlayerController;

/** Optional Designer-authored presentation. Selection and validation remain in controller/GameMode. */
UCLASS(Abstract, Blueprintable)
class SEAHORSE_API UCardSelectionPrompt : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Card Selection")
	int32 GetMinimumCards() const;
	UFUNCTION(BlueprintPure, Category = "Card Selection")
	int32 GetMaximumCards() const;
	UFUNCTION(BlueprintPure, Category = "Card Selection")
	TArray<ASHCard*> GetSelectedCards() const;
	UFUNCTION(BlueprintPure, Category = "Card Selection")
	int32 GetSelectedCardCount() const;
	UFUNCTION(BlueprintPure, Category = "Card Selection")
	TArray<ASHCard*> GetCandidateCards() const;
	UFUNCTION(BlueprintPure, Category = "Card Selection")
	bool CanConfirmSelection() const;
	UFUNCTION(BlueprintCallable, Category = "Card Selection")
	void ConfirmSelection();
	UFUNCTION(BlueprintCallable, Category = "Card Selection")
	void ToggleCardSelection(ASHCard* Card);
	UFUNCTION(BlueprintCallable, Category = "Card Selection")
	void ClearSelection();
	/** Called after opening the panel and after each selection change. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Card Selection")
	void OnSelectionChanged();
private:
	ASHPlayerController* GetSelectionController() const;
};
