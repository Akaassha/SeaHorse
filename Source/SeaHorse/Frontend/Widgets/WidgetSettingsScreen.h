#pragma once

#include "CoreMinimal.h"
#include "Frontend/Widgets/WidgetActivatableBase.h"
#include "WidgetSettingsScreen.generated.h"

/** Language controls for the designer's settings screen. Preferences live in SHGameUserSettings. */
UCLASS(Abstract, Blueprintable)
class SEAHORSE_API UWidgetSettingsScreen : public UWidgetActivatableBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Settings|Language")
	void RefreshLanguageOptions();
	UFUNCTION(BlueprintCallable, Category = "Settings|Language")
	void SelectPreviousLanguage();
	UFUNCTION(BlueprintCallable, Category = "Settings|Language")
	void SelectNextLanguage();
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ApplyLocalSettings();
	UFUNCTION(BlueprintPure, Category = "Settings|Language")
	TArray<FString> GetAvailableCultureCodes() const { return AvailableCultureCodes; }
	UFUNCTION(BlueprintPure, Category = "Settings|Language")
	FString GetSelectedCultureCode() const { return SelectedCultureCode; }
	UFUNCTION(BlueprintPure, Category = "Settings|Language")
	FText GetSelectedLanguageName() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	/** Existing TextBlock/CommonTextBlock to update; no layout changes are required. */
	UPROPERTY(EditDefaultsOnly, Category = "Settings|Language")
	FName LanguageNameWidgetName = TEXT("CommonTextBlock_0");

private:
	void ChangeLanguage(int32 Offset);
	UPROPERTY(Transient)
	TArray<FString> AvailableCultureCodes;
	UPROPERTY(Transient)
	FString SelectedCultureCode;
};
