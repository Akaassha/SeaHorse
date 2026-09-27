#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CardEffectMessageWidget.generated.h"

class UTextBlock;

/** Local informational notice. A Blueprint can provide its entire Designer layout. */
UCLASS(Blueprintable)
class SEAHORSE_API UCardEffectMessageWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category = "Card Effect Message")
	FText Message;
	void SetMessage(const FText& InMessage);
	/** Called after Message changes; use it to populate a Designer-authored layout. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Card Effect Message")
	void OnMessageSet();
protected:
	virtual void NativeOnInitialized() override;
	/** Optional Designer text block; automatically receives Message when present. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Card Effect Message")
	TObjectPtr<UTextBlock> MessageText;
};
