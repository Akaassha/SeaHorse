#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Frontend/FrontendSubsystem.h"
#include "Frontend/FrontendGameplayTags.h"
#include "Frontend/Widgets/Options/OptionsDataRegistry.h"
#include "Frontend/Widgets/Options/DataObjects/ListDataObjectCollection.h"
#include "Frontend/Widgets/Options/DataObjects/MyListDataObjectString.h"
#include "GameSettings/SHGameUserSettings.h"
#include "PropertyPathHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHFrontendSettingsTest, "SeaHorse.Frontend.SettingsAndRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHFrontendSettingsTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Engine uses SeaHorse settings from config"), Cast<USHGameUserSettings>(UGameUserSettings::GetGameUserSettings()));
	USHGameUserSettings* Settings = NewObject<USHGameUserSettings>();
	TestTrue(TEXT("Reflected volume setter is callable by options"), PropertyPathHelpers::SetPropertyValueFromString(Settings, FCachedPropertyPath(TEXT("SetOverallVolume")), TEXT("0.35")));
	TestEqual(TEXT("Reflected setter changes the preference"), Settings->GetOverallVolume(), 0.35f);
	FString Value;
	TestTrue(TEXT("Reflected getter is callable by options"), PropertyPathHelpers::GetPropertyValueAsString(Settings, FCachedPropertyPath(TEXT("GetOverallVolume")), Value));
	TestEqual(TEXT("Reflected getter returns the same preference"), FCString::Atof(*Value), 0.35f);
	Settings->SetOverallVolume(-1.f);
	TestEqual(TEXT("Volume cannot be negative"), Settings->GetOverallVolume(), 0.f);

	UOptionsDataRegistry* Registry = NewObject<UOptionsDataRegistry>();
	Registry->InitOptionsDataRegistry(nullptr);
	const int32 TabCount = Registry->GetRegisteredOptionsTabCollections().Num();
	Registry->InitOptionsDataRegistry(nullptr);
	TestEqual(TEXT("Repeated initialization does not duplicate tabs"), Registry->GetRegisteredOptionsTabCollections().Num(), TabCount);
	for (UListDataObjectCollection* Tab : Registry->GetRegisteredOptionsTabCollections())
	{
		TestFalse(TEXT("Tab text resolves without old assets"), Tab->GetDataDisplayName().ToString().Contains(TEXT("MISSING")));
		for (UListDataObjectBase* Item : Registry->GetListSourceItemsBySelectedTabID(Tab->GetDataID()))
		{
			TestNotEqual(TEXT("Local preferences do not expose match difficulty"), Item->GetDataID(), FName(TEXT("GameDifficulty")));
		}
	}
	UListDataObjectStringInteger* Quality = NewObject<UListDataObjectStringInteger>();
	Quality->AddIntegerOption(2, FText::FromString(TEXT("High")));
	Quality->SetDefaultValueFromString(TEXT("2"));
	Quality->InitDataObject();
	TestEqual(TEXT("Known quality is not labeled Custom"), Quality->GetCurrentDisplayText().ToString(), FString(TEXT("High")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHFrontendMissingLayoutTest, "SeaHorse.Frontend.MissingLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHFrontendMissingLayoutTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>();
	UFrontendSubsystem* Subsystem = NewObject<UFrontendSubsystem>(Instance);
	int32 CallbackCount = 0;
	Subsystem->PushSoftWidgetToStackAsync(FrontendGameplayTags::Frontend_WidgetStack_Modal, {},
		[this, &CallbackCount](EAsyncPushWdgetState State, UWidgetActivatableBase* Widget)
		{
			++CallbackCount;
			TestTrue(TEXT("Missing configuration reports failure"), State == EAsyncPushWdgetState::Failed);
			TestNull(TEXT("No widget was created"), Widget);
		});
	TestEqual(TEXT("Failure completes exactly once"), CallbackCount, 1);
	int32 ConfirmCount = 0;
	Subsystem->PushConfirmScreenToModalStackAsync(EConfirmScreenType::YesNo, FText::GetEmpty(), FText::GetEmpty(),
		[this, &ConfirmCount](EConfirmScreenButtonType Button)
		{
			++ConfirmCount;
			TestTrue(TEXT("Missing confirmation cannot approve an action"), Button == EConfirmScreenButtonType::Canceled);
		});
	TestEqual(TEXT("Confirmation completes exactly once"), ConfirmCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHFrontendEditConditionsTest, "SeaHorse.Frontend.EditConditionsRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHFrontendEditConditionsTest::RunTest(const FString& Parameters)
{
	UListDataObjectCollection* Data = NewObject<UListDataObjectCollection>();
	bool bCanEdit = false;
	FOptionsDataEditConditionDescriptor Condition;
	Condition.SetEditConditionFunc([&bCanEdit]() { return bCanEdit; });
	Condition.SetDisabledRichReason(TEXT("Unavailable while another setting is enabled."));
	Data->AddEditCondition(Condition);
	TestFalse(TEXT("Unmet condition disables the setting"), Data->IsDataCurrentlyEditable());
	TestFalse(TEXT("Disabled setting explains why"), Data->GetDisabledRichText().IsEmpty());
	bCanEdit = true;
	TestTrue(TEXT("Changed condition enables the setting"), Data->IsDataCurrentlyEditable());
	TestTrue(TEXT("Enabled setting clears the stale disabled reason"), Data->GetDisabledRichText().IsEmpty());
	Data->AddEditDependencyData(nullptr);
	Data->AddEditDependencyData(Data);
	TestFalse(TEXT("A setting cannot subscribe to itself"), Data->OnListDataModified.IsBoundToObject(Data));
	return true;
}

#endif
