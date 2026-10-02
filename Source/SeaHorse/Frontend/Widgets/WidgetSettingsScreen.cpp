#include "Frontend/Widgets/WidgetSettingsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "GameSettings/SHGameUserSettings.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/KismetInternationalizationLibrary.h"

void UWidgetSettingsScreen::NativeConstruct()
{
	Super::NativeConstruct();
	FInternationalization::Get().OnCultureChanged().RemoveAll(this);
	FInternationalization::Get().OnCultureChanged().AddUObject(this, &ThisClass::RefreshLanguageOptions);
	RefreshLanguageOptions();
}

void UWidgetSettingsScreen::NativeDestruct()
{
	FInternationalization::Get().OnCultureChanged().RemoveAll(this);
	Super::NativeDestruct();
}

void UWidgetSettingsScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshLanguageOptions();
}

void UWidgetSettingsScreen::RefreshLanguageOptions()
{
	AvailableCultureCodes = USHGameUserSettings::GetAvailableGameCultures();
	SelectedCultureCode = USHGameUserSettings::Get()->GetSelectedGameCulture();
	if (WidgetTree)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(WidgetTree->FindWidget(LanguageNameWidgetName)))
		{
			Label->SetText(GetSelectedLanguageName());
		}
	}
}

FText UWidgetSettingsScreen::GetSelectedLanguageName() const
{
	if (SelectedCultureCode.IsEmpty()) { return FText::GetEmpty(); }
	FString Name = UKismetInternationalizationLibrary::GetCultureDisplayName(SelectedCultureCode, true);
	if (!Name.IsEmpty()) { Name[0] = FChar::ToUpper(Name[0]); }
	return FText::FromString(Name);
}

void UWidgetSettingsScreen::ChangeLanguage(int32 Offset)
{
	RefreshLanguageOptions();
	if (AvailableCultureCodes.Num() < 2) { return; }
	const int32 Index = AvailableCultureCodes.IndexOfByKey(SelectedCultureCode);
	const int32 NextIndex = (FMath::Max(Index, 0) + Offset + AvailableCultureCodes.Num()) % AvailableCultureCodes.Num();
	USHGameUserSettings* Settings = USHGameUserSettings::Get();
	// OnCultureChanged can rebuild the array during SetCurrentCulture; do not pass an element by reference.
	const FString NextCulture = AvailableCultureCodes[NextIndex];
	Settings->SetCurrentCulture(NextCulture);
	// The culture notification refreshes labels immediately; save the actual accepted value.
	Settings->SaveSettings();
	RefreshLanguageOptions();
}

void UWidgetSettingsScreen::SelectPreviousLanguage() { ChangeLanguage(-1); }
void UWidgetSettingsScreen::SelectNextLanguage() { ChangeLanguage(1); }

void UWidgetSettingsScreen::ApplyLocalSettings()
{
	USHGameUserSettings::Get()->ApplySettings(false);
	RefreshLanguageOptions();
}
