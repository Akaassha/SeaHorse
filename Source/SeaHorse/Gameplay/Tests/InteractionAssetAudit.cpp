#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelSlot.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"

// Explicit read-only diagnostic, kept out of the gameplay regression suite.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHInteractionAssetAudit,
	"SeaHorse.Diagnostics.InteractionAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHInteractionAssetAudit::RunTest(const FString& Parameters)
{
	FString Report;
	for (const TCHAR* Path : {
		TEXT("/Game/SeaHorse/Board/BP_PlayerRepresentation"),
		TEXT("/Game/SeaHorse/Board/WBP_PlayerRepresentation"),
		TEXT("/Game/SeaHorse/Widgets/WBP_HUD"),
		TEXT("/Game/SeaHorse/Cards/BP_Card")})
	{
		UBlueprint* BP = LoadObject<UBlueprint>(nullptr, Path);
		if (!TestNotNull(Path, BP)) { continue; }
		Report += FString::Printf(TEXT("\nASSET %s\n"), Path);
		if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(BP))
		{
			for (UWidget* Widget : WidgetBP->GetAllSourceWidgets())
			{
				Report += FString::Printf(TEXT("WIDGET %s (%s) Visibility=%s\n"),
					*Widget->GetName(), *Widget->GetClass()->GetName(),
					*UEnum::GetValueAsString(Widget->GetVisibility()));
				if (Widget->Slot)
				{
					for (TFieldIterator<FProperty> It(Widget->Slot->GetClass()); It; ++It)
					{
						FString Value;
						It->ExportText_InContainer(0, Value, Widget->Slot, nullptr, Widget->Slot, PPF_None);
						Report += FString::Printf(TEXT("  %s=%s\n"), *It->GetName(), *Value);
					}
				}
			}
		}
		TArray<UEdGraph*> Graphs;
		BP->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			TSet<UObject*> Nodes;
			for (UEdGraphNode* Node : Graph->Nodes) { if (Node) { Nodes.Add(Node); } }
			FString Text;
			FEdGraphUtilities::ExportNodesToText(Nodes, Text);
			Report += TEXT("\nGRAPH ") + Graph->GetName() + TEXT("\n") + Text;
		}
	}
	return FFileHelper::SaveStringToFile(Report,
		*(FPaths::ProjectSavedDir() / TEXT("ContextReview/InteractionAssetAudit.txt")));
}

#endif
