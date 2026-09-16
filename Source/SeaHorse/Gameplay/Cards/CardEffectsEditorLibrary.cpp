#include "Gameplay/Cards/CardEffectsEditorLibrary.h"
#if WITH_EDITOR
#include "Gameplay/Cards/CardDefinition.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#endif

bool UCardEffectsEditorLibrary::UpgradeHandPairingBlueprint()
{
#if WITH_EDITOR
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/SeaHorse/Cards/BP_Hand.BP_Hand"));
	if (!BP) { return false; }
	for (UEdGraph* Graph : BP->FunctionGraphs)
	{
		if (Graph->GetFName() != TEXT("CheckPairCompatibility")) { continue; }
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CallFunction* Old = Cast<UK2Node_CallFunction>(Node);
			if (!Old) { continue; }
			if (Old->FunctionReference.GetMemberName() == TEXT("ArePairDefinitionsCompatible")) { return true; }
			if (Old->FunctionReference.GetMemberName() != TEXT("EqualEqual_ClassClass")) { continue; }
			BP->Modify(); Graph->Modify();
			UK2Node_CallFunction* Replacement = NewObject<UK2Node_CallFunction>(Graph);
			Graph->AddNode(Replacement, false, false);
			Replacement->CreateNewGuid();
			Replacement->SetFromFunction(UCardDefinition::StaticClass()->FindFunctionByName(TEXT("ArePairDefinitionsCompatible")));
			Replacement->AllocateDefaultPins();
			Replacement->NodePosX = Old->NodePosX; Replacement->NodePosY = Old->NodePosY;
			for (FName Name : {FName(TEXT("A")), FName(TEXT("B")), FName(TEXT("ReturnValue"))})
			{
				UEdGraphPin* From = Old->FindPinChecked(Name);
				UEdGraphPin* To = Replacement->FindPinChecked(Name);
				const TArray<UEdGraphPin*> Links = From->LinkedTo;
				From->BreakAllPinLinks();
				for (UEdGraphPin* Link : Links) { To->MakeLinkTo(Link); }
			}
			Graph->RemoveNode(Old);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
			FKismetEditorUtilities::CompileBlueprint(BP);
			if (BP->Status == BS_Error) { return false; }
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			return UPackage::SavePackage(BP->GetOutermost(), BP,
				*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
		}
	}
#endif
	return false;
}
