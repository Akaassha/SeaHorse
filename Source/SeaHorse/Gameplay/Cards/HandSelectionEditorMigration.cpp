#include "Gameplay/Cards/CardEffectsEditorLibrary.h"
#if WITH_EDITOR
#include "Gameplay/SHHand.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

bool UCardEffectsEditorLibrary::UpgradeHandSelectionBlueprint(bool bSave)
{
#if WITH_EDITOR
	auto* Hand = LoadObject<UBlueprint>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Hand.BP_Hand"));
	if (!Hand) { return false; }
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	Hand->Modify();
	TArray<UEdGraph*> Graphs;
	Hand->GetAllGraphs(Graphs);
	bool bFoundClick = false;
	for (UEdGraph* Graph : Graphs)
	{
		const auto Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			if (auto* Old = Cast<UK2Node_VariableGet>(Node);
				Old && Old->VariableReference.GetMemberName() == TEXT("SelectedCardIndex"))
			{
				// Both compatibility checks and the pair RPC now resolve the selected
				// card's current index, including after a replicated shuffle or draw.
				auto* Query = NewObject<UK2Node_CallFunction>(Graph);
				Graph->AddNode(Query, false, false); Query->CreateNewGuid();
				Query->SetFromFunction(ASHHand::StaticClass()->FindFunctionByName(TEXT("GetSelectedHandCardIndex")));
				Query->AllocateDefaultPins(); Query->NodePosX = Old->NodePosX; Query->NodePosY = Old->NodePosY;
				UEdGraphPin* Source = Old->FindPinChecked(TEXT("SelectedCardIndex"));
				const auto Links = Source->LinkedTo; Source->BreakAllPinLinks();
				for (UEdGraphPin* Link : Links)
				{
					if (!Schema->TryCreateConnection(Query->FindPinChecked(TEXT("ReturnValue")), Link)) { return false; }
				}
				Graph->RemoveNode(Old);
			}
			auto* Event = Cast<UK2Node_CustomEvent>(Node);
			if (!Event || Event->CustomFunctionName != TEXT("Clicked")) { continue; }
			bFoundClick = true;
			UEdGraphPin* Exec = Event->FindPinChecked(UEdGraphSchema_K2::PN_Then);
			if (Exec->LinkedTo.Num() == 1)
			{
				const auto* Existing = Cast<UK2Node_CallFunction>(Exec->LinkedTo[0]->GetOwningNode());
				if (Existing && Existing->FunctionReference.GetMemberName() == TEXT("TryDeselectLocalHandCard")) { continue; }
			}
			const auto OldLinks = Exec->LinkedTo;
			if (OldLinks.IsEmpty()) { return false; }
			auto* Deselect = NewObject<UK2Node_CallFunction>(Graph);
			Graph->AddNode(Deselect, false, false); Deselect->CreateNewGuid();
			Deselect->SetFromFunction(ASHHand::StaticClass()->FindFunctionByName(TEXT("TryDeselectLocalHandCard")));
			Deselect->AllocateDefaultPins(); Deselect->NodePosX = Event->NodePosX + 250; Deselect->NodePosY = Event->NodePosY - 220;
			auto* Branch = NewObject<UK2Node_IfThenElse>(Graph);
			Graph->AddNode(Branch, false, false); Branch->CreateNewGuid(); Branch->AllocateDefaultPins();
			Branch->NodePosX = Deselect->NodePosX + 300; Branch->NodePosY = Deselect->NodePosY;
			auto* ClearLegacy = NewObject<UK2Node_VariableSet>(Graph);
			Graph->AddNode(ClearLegacy, false, false); ClearLegacy->CreateNewGuid();
			ClearLegacy->VariableReference.SetSelfMember(TEXT("SelectedCardIndex")); ClearLegacy->AllocateDefaultPins();
			ClearLegacy->FindPinChecked(TEXT("SelectedCardIndex"), EGPD_Input)->DefaultValue = TEXT("-1");
			ClearLegacy->NodePosX = Branch->NodePosX + 240; ClearLegacy->NodePosY = Branch->NodePosY - 120;
			Exec->BreakAllPinLinks();
			if (!Schema->TryCreateConnection(Exec, Deselect->FindPinChecked(UEdGraphSchema_K2::PN_Execute)) ||
				!Schema->TryCreateConnection(Event->FindPinChecked(TEXT("Card")), Deselect->FindPinChecked(TEXT("Card"))) ||
				!Schema->TryCreateConnection(Deselect->FindPinChecked(UEdGraphSchema_K2::PN_Then), Branch->GetExecPin()) ||
				!Schema->TryCreateConnection(Deselect->FindPinChecked(TEXT("ReturnValue")), Branch->GetConditionPin()) ||
				!Schema->TryCreateConnection(Branch->GetThenPin(), ClearLegacy->FindPinChecked(UEdGraphSchema_K2::PN_Execute))) { return false; }
			for (UEdGraphPin* Link : OldLinks)
			{
				if (!Schema->TryCreateConnection(Branch->GetElsePin(), Link)) { return false; }
			}
		}
	}
	if (!bFoundClick) { return false; }
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Hand);
	FKismetEditorUtilities::CompileBlueprint(Hand);
	if (Hand->Status == BS_Error) { return false; }
	if (bSave)
	{
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Hand->GetOutermost(), Hand,
			*FPackageName::LongPackageNameToFilename(Hand->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
	}
	return true;
#else
	return false;
#endif
}
