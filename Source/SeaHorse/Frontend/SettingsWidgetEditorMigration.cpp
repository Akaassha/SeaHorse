#include "Frontend/FrontendFunctionLibrary.h"

#if WITH_EDITOR
#include "Frontend/Widgets/WidgetSettingsScreen.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Variable.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

bool UFrontendFunctionLibrary::UpgradeSettingsLanguageWidget(bool bSave)
{
#if WITH_EDITOR
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr,
		TEXT("/Game/SeaHorse/Frontend/Settings/WBP_WAB_Settings.WBP_WAB_Settings"));
	if (!BP || !BP->WidgetTree || !BP->ParentClass ||
		!BP->ParentClass->IsChildOf(UWidgetActivatableBase::StaticClass())) { return false; }
	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(BP);
	if (!Graph || !BP->WidgetTree->FindWidget(TEXT("CommonTextBlock_0"))) { return false; }
	const TArray<TPair<FName, FName>> Bindings = {
		{TEXT("WBP_LobbyButton_138"), TEXT("SelectPreviousLanguage")},
		{TEXT("WBP_LobbyButton_266"), TEXT("SelectNextLanguage")},
		{TEXT("WBP_LobbyButton"), TEXT("ApplyLocalSettings")}
	};
	for (const auto& Binding : Bindings)
	{
		UWidget* Button = BP->WidgetTree->FindWidget(Binding.Key);
		if (!Button || !FindFProperty<FMulticastDelegateProperty>(Button->GetClass(), TEXT("OnClicked"))) { return false; }
	}
	BP->Modify();
	Graph->Modify();
	BP->ParentClass = UWidgetSettingsScreen::StaticClass();
	// Replace only the unfinished language collection graph. Keep the existing Back event.
	const auto OldNodes = Graph->Nodes;
	for (UEdGraphNode* Node : OldNodes)
	{
		bool bRemove = false;
		if (const auto* Event = Cast<UK2Node_Event>(Node))
		{
			bRemove = Event->EventReference.GetMemberName() == TEXT("Construct");
		}
		if (const auto* Call = Cast<UK2Node_CallFunction>(Node))
		{
			const FName Name = Call->FunctionReference.GetMemberName();
			bRemove |= Name == TEXT("GetLocalizedCultures") || Name == TEXT("GetCultureDisplayName") ||
				Name == TEXT("Map_Add") || Name == TEXT("Get");
		}
		if (const auto* Variable = Cast<UK2Node_Variable>(Node))
		{
			const FName Name = Variable->VariableReference.GetMemberName();
			bRemove |= Name == TEXT("SHUserSettings") || Name == TEXT("LocalizedCultures");
		}
		if (const auto* Macro = Cast<UK2Node_MacroInstance>(Node))
		{
			bRemove |= Macro->GetMacroGraph() && Macro->GetMacroGraph()->GetFName() == TEXT("ForEachLoop");
		}
		if (bRemove) { Node->BreakAllNodeLinks(); Graph->RemoveNode(Node); }
	}
	for (const auto& Binding : Bindings) { BP->WidgetTree->FindWidget(Binding.Key)->bIsVariable = true; }
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) { return false; }
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	int32 Row = 0;
	for (const auto& Binding : Bindings)
	{
		UK2Node_ComponentBoundEvent* Event = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			auto* Candidate = Cast<UK2Node_ComponentBoundEvent>(Node);
			if (Candidate && Candidate->GetComponentPropertyName() == Binding.Key &&
				Candidate->DelegatePropertyName == TEXT("OnClicked")) { Event = Candidate; break; }
		}
		if (!Event)
		{
			const FObjectProperty* Property = FindFProperty<FObjectProperty>(BP->SkeletonGeneratedClass, Binding.Key);
			if (!Property) { return false; }
			const auto* Delegate = FindFProperty<FMulticastDelegateProperty>(Property->PropertyClass, TEXT("OnClicked"));
			if (!Delegate) { return false; }
			Event = NewObject<UK2Node_ComponentBoundEvent>(Graph);
			Graph->AddNode(Event, false, false); Event->CreateNewGuid();
			Event->InitializeComponentBoundEventParams(Property, Delegate);
			Event->AllocateDefaultPins(); Event->NodePosX = 0; Event->NodePosY = 400 + Row * 200;
		}
		++Row;
		UEdGraphPin* Exec = Event->FindPinChecked(UEdGraphSchema_K2::PN_Then);
		if (!Exec->LinkedTo.IsEmpty())
		{
			const auto* Existing = Cast<UK2Node_CallFunction>(Exec->LinkedTo[0]->GetOwningNode());
			if (Existing && Existing->FunctionReference.GetMemberName() == Binding.Value) { continue; }
			return false; // Never overwrite an unexpected designer click action.
		}
		UK2Node_CallFunction* Call = NewObject<UK2Node_CallFunction>(Graph);
		Graph->AddNode(Call, false, false); Call->CreateNewGuid();
		Call->SetFromFunction(UWidgetSettingsScreen::StaticClass()->FindFunctionByName(Binding.Value));
		Call->AllocateDefaultPins(); Call->NodePosX = 500; Call->NodePosY = Event->NodePosY;
		if (!Schema->TryCreateConnection(Exec, Call->FindPinChecked(UEdGraphSchema_K2::PN_Execute))) { return false; }
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) { return false; }
	if (!bSave) { return true; }
	FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(BP->GetOutermost(), BP,
		*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
#else
	return false;
#endif
}
