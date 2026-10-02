#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Internationalization/Internationalization.h"
#include "UObject/UnrealType.h"
#include "Frontend/Widgets/WidgetSettingsScreen.h"
#include "GameSettings/SHGameUserSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHSettingsLanguageTest,
	"SeaHorse.Frontend.SettingsLanguage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSHSettingsLanguageTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
#endif
	UClass* WidgetClass = LoadClass<UWidgetSettingsScreen>(nullptr,
		TEXT("/Game/SeaHorse/Frontend/Settings/WBP_WAB_Settings.WBP_WAB_Settings_C"));
	if (!TestNotNull(TEXT("Authored settings widget inherits the language-aware base"), WidgetClass)) { return false; }
	USHGameUserSettings* Settings = USHGameUserSettings::Get();
	const FString OriginalCulture = Settings->GetCurrentCulture();
	FStrProperty* SavedCulture = FindFProperty<FStrProperty>(Settings->GetClass(), TEXT("CurrentCulture"));
	const FString OriginalPreference = SavedCulture->GetPropertyValue_InContainer(Settings);
	UObject* Defaults = Settings->GetClass()->GetDefaultObject();
	const FString OriginalDefaultPreference = SavedCulture->GetPropertyValue_InContainer(Defaults);
	// Button presses exercise real persistence without changing the user's settings files.
	const FString TestDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
		TEXT("Automation/SettingsLanguage") / FGuid::NewGuid().ToString());
	IFileManager::Get().MakeDirectory(*TestDirectory, true);
	const FString TestGameIni = TestDirectory / TEXT("GameUserSettings.ini");
	const FString TestEditorIni = TestDirectory / TEXT("EditorSettings.ini");
	// SaveSettings reloads the GameUserSettings config path internally. Redirect the
	// existing branch's output instead of overriding that global, and restore its cache.
	FConfigBranch* GameBranch = GConfig->FindBranch(TEXT("GameUserSettings"), GGameUserSettingsIni);
	if (!TestNotNull(TEXT("Loaded game settings config branch"), GameBranch)) { return false; }
	TGuardValue<FString> GameOutputGuard(GameBranch->IniPath, TestGameIni);
	TGuardValue<FConfigFile> GameCacheGuard(GameBranch->InMemoryFile, GameBranch->InMemoryFile);
	TGuardValue<FConfigCommandStream> GameSavedLayerGuard(GameBranch->SavedLayer, GameBranch->SavedLayer);
	TGuardValue<FString> EditorIniGuard(GEditorSettingsIni, TestEditorIni);
	GConfig->AddNewBranch(TestEditorIni);
	ON_SCOPE_EXIT
	{
		Settings->SetCurrentCulture(OriginalCulture);
		SavedCulture->SetPropertyValue_InContainer(Settings, OriginalPreference);
		SavedCulture->SetPropertyValue_InContainer(Defaults, OriginalDefaultPreference);
		GConfig->UnloadFile(TestGameIni); GConfig->UnloadFile(TestEditorIni);
		IFileManager::Get().Delete(*TestGameIni); IFileManager::Get().Delete(*TestEditorIni);
	};
	const TArray<FString> Available = Settings->GetAvailableGameCultures();
	TestTrue(TEXT("Compiled Polish localization is discoverable"), Available.Contains(TEXT("pl")));
	TestTrue(TEXT("Compiled English localization is discoverable"), Available.Contains(TEXT("en")));
	TestFalse(TEXT("Unsupported current cultures are not advertised as translations"), Available.Contains(TEXT("de")));
	Settings->SetCurrentCulture(TEXT("pl-PL"));
	TestEqual(TEXT("Regional culture selects the available Polish language"), Settings->GetSelectedGameCulture(), FString(TEXT("pl")));
	const FText SettingsTitle = FText::FromStringTable(TEXT("/Game/SeaHorse/Localization/ST_Frontend.ST_Frontend"), TEXT("SETTINGS"));
	// The editor deliberately loads native game texts. The packaged client must also
	// exercise real translation changes, including switching back to the native language.
	if (!GIsEditor) { TestEqual(TEXT("Polish menu translation is loaded"), SettingsTitle.ToString(), FString(TEXT("Ustawienia"))); }

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UGameInstance* Instance = NewObject<UGameInstance>();
	World->SetGameInstance(Instance);
	ON_SCOPE_EXIT
	{
		World->SetGameInstance(nullptr); World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	UWidgetSettingsScreen* Widget = CreateWidget<UWidgetSettingsScreen>(World, WidgetClass);
	if (!TestNotNull(TEXT("Actual designer settings widget can be instantiated"), Widget)) { return false; }
	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	Widget->ActivateWidget();
	UTextBlock* Label = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("CommonTextBlock_0")));
	if (!TestNotNull(TEXT("Existing language label is preserved"), Label)) { return false; }
	TestEqual(TEXT("Opening settings selects the current language"), Widget->GetSelectedCultureCode(), FString(TEXT("pl")));
	TestEqual(TEXT("Opening settings does not overwrite the regional culture"), Settings->GetCurrentCulture(), FString(TEXT("pl-PL")));
	TestTrue(TEXT("The designer's label immediately displays Polish"), Label->GetText().ToString().Equals(TEXT("Polski"), ESearchCase::IgnoreCase));
	auto Click = [this, Widget](const FName Name)
	{
		UWidget* Button = Widget->WidgetTree->FindWidget(Name);
		if (!TestNotNull(TEXT("Authored button exists"), Button)) { return; }
		const auto* Delegate = FindFProperty<FMulticastDelegateProperty>(Button->GetClass(), TEXT("OnClicked"));
		if (!TestNotNull(TEXT("Authored button click dispatcher"), Delegate)) { return; }
		const FMulticastScriptDelegate* Clicked = Delegate->GetMulticastDelegate(Delegate->ContainerPtrToValuePtr<void>(Button));
		if (!TestNotNull(TEXT("Button delegate storage"), Clicked)) { return; }
		TestTrue(TEXT("Button is connected to its settings action"), Clicked->IsBound());
		Clicked->ProcessMulticastDelegate<UObject>(nullptr);
	};
	Click(TEXT("WBP_LobbyButton_266"));
	TestEqual(TEXT("Next arrow wraps from Polish to English"), Settings->GetCurrentCulture(), FString(TEXT("en")));
	TestEqual(TEXT("Changing language updates the existing text widget immediately"), Label->GetText().ToString(), FString(TEXT("English")));
	if (!GIsEditor) { TestEqual(TEXT("Switching to English updates existing string table texts"), SettingsTitle.ToString(), FString(TEXT("Settings"))); }
	// Read the real file, then reload it into the cache to simulate startup.
	FConfigFile PersistedSettings;
	PersistedSettings.Read(TestGameIni);
	FString PersistedCulture;
	TestTrue(TEXT("Language selection is saved to disk"), PersistedSettings.GetString(TEXT("/Script/SeaHorse.SHGameUserSettings"), TEXT("CurrentCulture"), PersistedCulture));
	TestEqual(TEXT("Saved value is the culture code, not its display name"), PersistedCulture, FString(TEXT("en")));
	Settings->SetCurrentCulture(TEXT("pl"));
	GConfig->LoadFile(TestGameIni);
	Settings->LoadConfig(nullptr, *TestGameIni);
	Settings->ApplyNonResolutionSettings();
	TestEqual(TEXT("Loading saved settings restores the selected language"), Settings->GetCurrentCulture(), FString(TEXT("en")));
	TestEqual(TEXT("Restoring settings refreshes the already-open label"), Label->GetText().ToString(), FString(TEXT("English")));
	Click(TEXT("WBP_LobbyButton_138"));
	TestEqual(TEXT("Previous arrow switches to Polish"), Settings->GetCurrentCulture(), FString(TEXT("pl")));
	Click(TEXT("WBP_LobbyButton"));
	TestEqual(TEXT("Apply keeps the chosen language"), Settings->GetCurrentCulture(), FString(TEXT("pl")));
	Click(TEXT("MainMenu_Button"));
	TestFalse(TEXT("Existing Back action still closes settings"), Widget->IsActivated());
	Widget->ActivateWidget();
	TestEqual(TEXT("Reopening settings retains the chosen language"), Widget->GetSelectedCultureCode(), FString(TEXT("pl")));
	TestEqual(TEXT("Reopening does not duplicate options"), Widget->GetAvailableCultureCodes().Num(), Available.Num());
	FInternationalization::Get().SetCurrentCulture(TEXT("en-US"));
	TestEqual(TEXT("External culture change refreshes an open settings screen"), Widget->GetSelectedCultureCode(), FString(TEXT("en")));
	TestEqual(TEXT("External culture change refreshes the visible name"), Label->GetText().ToString(), FString(TEXT("English")));
	Settings->SetCurrentCulture(TEXT("de"));
	Widget->RefreshLanguageOptions();
	TestEqual(TEXT("Unavailable locale uses the native language as a display fallback"), Widget->GetSelectedCultureCode(), FString(TEXT("en")));
	TestEqual(TEXT("Fallback display does not overwrite the preference on opening"), Settings->GetCurrentCulture(), FString(TEXT("de")));
	Widget->DeactivateWidget();
	return true;
}

#endif
