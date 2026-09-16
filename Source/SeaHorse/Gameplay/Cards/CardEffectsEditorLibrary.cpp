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
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Gameplay/Presentation/CardReactionPrompt.h"
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

bool UCardEffectsEditorLibrary::UpgradeReactionPromptBlueprint(const FString& AssetPath)
{
#if WITH_EDITOR
	UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
	if (!BP || !BP->ParentClass || !BP->ParentClass->IsChildOf(UCardReactionPrompt::StaticClass()) || !BP->WidgetTree) { return false; }
	UWidgetTree* Tree = BP->WidgetTree;
	const UPanelWidget* ExistingPanel = Cast<UPanelWidget>(Tree->RootWidget);
	// Never replace an authored layout. This migration only fills the old empty prompts.
	if (Tree->RootWidget && (!ExistingPanel || ExistingPanel->GetChildrenCount() > 0)) { return true; }
	BP->Modify();
	Tree->Modify();
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Tree->RootWidget);
	if (!Canvas) { Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PromptCanvas")); }
	Tree->RootWidget = Canvas;
	UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PromptPanel"));
	Panel->SetPadding(FMargin(30.f));
	Panel->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.04f, 0.98f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetPosition(FVector2D::ZeroVector);
	PanelSlot->SetAutoSize(true);
	UVerticalBox* Body = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PromptBody"));
	Panel->SetContent(Body);
	UTextBlock* Question = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuestionText"));
	Question->SetText(FText::FromString(TEXT("Czy aktywować tę parę?")));
	Question->SetWrapTextAt(540.f);
	Body->AddChildToVerticalBox(Question)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
	UHorizontalBox* Buttons = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DecisionButtons"));
	Body->AddChildToVerticalBox(Buttons);
	for (bool bAccept : {true, false})
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), bAccept ? TEXT("AcceptButton") : TEXT("DeclineButton"));
		UHorizontalBoxSlot* Slot = Buttons->AddChildToHorizontalBox(Button);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(5.f));
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), bAccept ? TEXT("AcceptLabel") : TEXT("DeclineLabel"));
		Label->SetText(FText::FromString(bAccept ? TEXT("Tak") : TEXT("Nie")));
		Button->SetContent(Label);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) { return false; }
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(BP->GetOutermost(), BP,
		*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
#else
	return false;
#endif
}
