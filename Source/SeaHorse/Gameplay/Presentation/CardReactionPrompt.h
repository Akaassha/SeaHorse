#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CardReactionPrompt.generated.h"

class ASHCard;
class UButton;
class UTextBlock;

/** Presentation only. Accept/Decline forward an offer ID to the owning controller. */
UCLASS(Abstract, Blueprintable)
class SEAHORSE_API UCardReactionPrompt : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeOffer(int32 InOfferId, ASHCard* InReactionCard, ASHCard* InTargetCard);
	UPROPERTY(BlueprintReadOnly, Category = "Reaction")
	TObjectPtr<ASHCard> ReactionCard;
	UPROPERTY(BlueprintReadOnly, Category = "Reaction")
	TObjectPtr<ASHCard> TargetCard;
	UFUNCTION(BlueprintCallable, Category = "Reaction")
	void AcceptReaction();
	UFUNCTION(BlueprintCallable, Category = "Reaction")
	void DeclineReaction();
	UFUNCTION(BlueprintImplementableEvent, Category = "Reaction")
	void OnOfferPresented();
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	/** Optional Designer controls. Custom layouts can call Accept/Decline directly instead. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Reaction")
	TObjectPtr<UButton> AcceptButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Reaction")
	TObjectPtr<UButton> DeclineButton;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Reaction")
	TObjectPtr<UTextBlock> QuestionText;
private:
	int32 OfferId = INDEX_NONE;
};
