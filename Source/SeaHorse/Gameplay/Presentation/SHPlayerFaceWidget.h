#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SHPlayerFaceWidget.generated.h"

class ASHPlayerRepresentation;
class UImage;
class UTextBlock;
class UTexture2D;

/** Local, read-only presentation rendered onto a mesh. Never added to the viewport. */
UCLASS(Blueprintable)
class SEAHORSE_API USHPlayerFaceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void UpdatePresentation(ASHPlayerRepresentation* InRepresentation);

	UFUNCTION(BlueprintPure, Category = "Player Face")
	ASHPlayerRepresentation* GetRepresentation() const;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Player Face")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Player Face")
	TObjectPtr<UTexture2D> AvatarTexture;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Player Face")
	bool bSelectable = false;

	/** Update custom controls here. Called after Construct and before drawing the texture. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Player Face")
	void OnPresentationUpdated();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Optional Designer controls; these names opt into automatic text/image updates. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Player Face")
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Player Face")
	TObjectPtr<UImage> AvatarImage;

private:
	void BuildDefaultLayout();

	UPROPERTY(Transient)
	TWeakObjectPtr<ASHPlayerRepresentation> Representation;
};
