#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "CardInfoWidget.generated.h"

class ASHCard;
class UCardDefinition;
class ASHPlayerController;
class UTextureRenderTarget2D;

/** Read-only local card details. All layout is supplied by the Blueprint Designer. */
UCLASS(Abstract, Blueprintable)
class SEAHORSE_API UCardInfoWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Card Info")
	ASHCard* GetInspectedCard() const;
	UFUNCTION(BlueprintPure, Category = "Card Info")
	TSubclassOf<UCardDefinition> GetCardDefinitionClass() const;
	UFUNCTION(BlueprintPure, Category = "Card Info")
	const UCardDefinition* GetCardDefinition() const;
	/** Existing rendered face, including text, without the world mesh's material effects. May be null before rendering. */
	UFUNCTION(BlueprintPure, Category = "Card Info")
	UTextureRenderTarget2D* GetCardFaceRenderTarget() const;
	/** Ready for Image.SetBrush. Returns an empty brush when the face is unavailable or access is denied. */
	UFUNCTION(BlueprintPure, Category = "Card Info")
	FSlateBrush GetCardFaceBrush() const;
	UFUNCTION(BlueprintCallable, Category = "Card Info")
	void CloseCardInfo();
	/** Call on opening with the Designer's Visible Image and close Button. Containers must be Self Hit Test Invisible so outside clicks reach the game. */
	UFUNCTION(BlueprintCallable, Category = "Card Info")
	void SetClickOutsideTargets(UWidget* CardImage, UWidget* CloseButton);
	/** Called with valid context after the widget has been added to the viewport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Card Info")
	void OnCardInfoOpened();
protected:
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UWidget> ClickOutsideCardImage;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> ClickOutsideCloseButton;
	ASHPlayerController* GetInspectionController() const;
};
