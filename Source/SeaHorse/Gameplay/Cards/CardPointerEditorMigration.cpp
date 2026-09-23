#include "Gameplay/Cards/CardEffectsEditorLibrary.h"
#if WITH_EDITOR
#include "Gameplay/SHHand.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
UEdGraph* FindFunctionGraph(UBlueprint* BP, FName Name)
{
	for (UEdGraph* Graph : BP->FunctionGraphs) { if (Graph->GetFName() == Name) { return Graph; } }
	return nullptr;
}

bool ReplaceQueryBody(UEdGraph* Graph, UFunction* Function, const TArray<TPair<FName, FName>>& Outputs)
{
	UK2Node_FunctionEntry* Entry = nullptr;
	UK2Node_FunctionResult* Result = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (auto* Found = Cast<UK2Node_FunctionEntry>(Node)) { Entry = Found; }
		if (!Result) { Result = Cast<UK2Node_FunctionResult>(Node); }
	}
	if (!Entry || !Result || !Function) { return false; }
	for (const auto& Output : Outputs) { if (!Result->FindPin(Output.Value)) { return false; } }
	Graph->Modify();
	const auto OldNodes = Graph->Nodes;
	for (UEdGraphNode* Node : OldNodes)
	{
		Node->BreakAllNodeLinks();
		if (Node != Entry && Node != Result) { Graph->RemoveNode(Node); }
	}
	auto* Query = NewObject<UK2Node_CallFunction>(Graph);
	Graph->AddNode(Query, false, false); Query->CreateNewGuid();
	Query->SetFromFunction(Function); Query->AllocateDefaultPins();
	Query->NodePosX = Entry->NodePosX + 250; Query->NodePosY = Entry->NodePosY + 150;
	Result->NodePosX = Query->NodePosX + 400; Result->NodePosY = Entry->NodePosY;
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Schema->TryCreateConnection(Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then), Result->FindPinChecked(UEdGraphSchema_K2::PN_Execute))) { return false; }
	for (const auto& Output : Outputs)
	{
		if (!Schema->TryCreateConnection(Query->FindPinChecked(Output.Key), Result->FindPinChecked(Output.Value))) { return false; }
	}
	return true;
}
}
#endif

bool UCardEffectsEditorLibrary::UpgradeCardPointerBlueprints(bool bSave)
{
#if WITH_EDITOR
	auto* Hand = LoadObject<UBlueprint>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Hand.BP_Hand"));
	auto* PC = LoadObject<UBlueprint>(nullptr, TEXT("/Game/SeaHorse/Core/BP_SHPlayerController.BP_SHPlayerController"));
	if (!Hand || !PC) { return false; }
	UEdGraph* Hover = FindFunctionGraph(Hand, TEXT("FindNearestCardToTheMouseCursor"));
	UEdGraph* Click = FindFunctionGraph(Hand, TEXT("ClickedHandCard"));
	UEdGraph* Cursor = FindFunctionGraph(PC, TEXT("IsCardUnderCursor?"));
	if (!Hover || !Click || !Cursor) { return false; }
	Hand->Modify(); PC->Modify();
	if (!ReplaceQueryBody(Hover, ASHHand::StaticClass()->FindFunctionByName(TEXT("GetHoveredHandCard")), {{TEXT("ReturnValue"), TEXT("Card")}}) ||
		!ReplaceQueryBody(Cursor, ASHPlayerController::StaticClass()->FindFunctionByName(TEXT("GetCardInteractionUnderCursor")),
			{{TEXT("Card"), TEXT("HitActor")}, {TEXT("Location"), TEXT("Location")}, {TEXT("ReturnValue"), TEXT("ReturnValue")}})) { return false; }
	const auto ClickNodes = Click->Nodes;
	bool bReplaced = false;
	for (UEdGraphNode* Node : ClickNodes)
	{
		if (auto* Existing = Cast<UK2Node_CallFunction>(Node); Existing && Existing->FunctionReference.GetMemberName() == TEXT("GetPointerPressedHandCardIndex")) { bReplaced = true; }
		auto* Old = Cast<UK2Node_VariableGet>(Node);
		if (!Old || Old->VariableReference.GetMemberName() != TEXT("FocusedCardIndex")) { continue; }
		auto* Query = NewObject<UK2Node_CallFunction>(Click);
		Click->AddNode(Query, false, false); Query->CreateNewGuid();
		Query->SetFromFunction(ASHHand::StaticClass()->FindFunctionByName(TEXT("GetPointerPressedHandCardIndex")));
		Query->AllocateDefaultPins(); Query->NodePosX = Old->NodePosX; Query->NodePosY = Old->NodePosY;
		UEdGraphPin* Source = Old->FindPinChecked(TEXT("FocusedCardIndex"));
		const auto Links = Source->LinkedTo; Source->BreakAllPinLinks();
		for (UEdGraphPin* Link : Links)
		{
			if (!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Query->FindPinChecked(TEXT("ReturnValue")), Link)) { return false; }
		}
		Click->RemoveNode(Old); bReplaced = true;
	}
	if (!bReplaced) { return false; }
	// Keep the existing world-distance drag threshold, measured on a fixed plane.
	// Animated card meshes must not turn a stationary click into a drag.
	for (UEdGraph* Graph : PC->UbergraphPages)
	{
		const auto Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			UEdGraphPin* Target = nullptr;
			if (auto* Set = Cast<UK2Node_VariableSet>(Node); Set && Set->VariableReference.GetMemberName() == TEXT("PressedMousePos"))
			{
				Target = Set->FindPin(TEXT("PressedMousePos"), EGPD_Input);
			}
			if (auto* Distance = Cast<UK2Node_CallFunction>(Node); Distance && Distance->FunctionReference.GetMemberName() == TEXT("Vector_Distance"))
			{
				for (UEdGraphPin* Link : Distance->FindPinChecked(TEXT("V1"))->LinkedTo)
				{
					if (auto* Variable = Cast<UK2Node_VariableGet>(Link->GetOwningNode()); Variable && Variable->VariableReference.GetMemberName() == TEXT("PressedMousePos"))
					{
						Target = Distance->FindPinChecked(TEXT("V2"));
					}
				}
			}
			if (!Target) { continue; }
			if (Target->LinkedTo.Num() == 1)
			{
				auto* Existing = Cast<UK2Node_CallFunction>(Target->LinkedTo[0]->GetOwningNode());
				if (Existing && Existing->FunctionReference.GetMemberName() == TEXT("GetCardDragCursorLocation")) { continue; }
			}
			auto* Query = NewObject<UK2Node_CallFunction>(Graph);
			Graph->AddNode(Query, false, false); Query->CreateNewGuid();
			Query->SetFromFunction(ASHPlayerController::StaticClass()->FindFunctionByName(TEXT("GetCardDragCursorLocation")));
			Query->AllocateDefaultPins(); Query->NodePosX = Node->NodePosX - 300; Query->NodePosY = Node->NodePosY + 150;
			Target->BreakAllPinLinks();
			if (!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Query->FindPinChecked(TEXT("ReturnValue")), Target)) { return false; }
		}
	}
	for (UBlueprint* BP : {PC, Hand})
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
		FKismetEditorUtilities::CompileBlueprint(BP);
		if (BP->Status == BS_Error) { return false; }
	}
	if (bSave)
	{
		for (UBlueprint* BP : {PC, Hand})
		{
			FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone;
			if (!UPackage::SavePackage(BP->GetOutermost(), BP,
				*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args)) { return false; }
		}
	}
	return true;
#else
	return false;
#endif
}
