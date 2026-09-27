#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Components/TextBlock.h"
#include "Widgets/SOverlay.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Presentation/CardEffectMessageWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCardEffectMessageTest, "SeaHorse.Gameplay.UI.CardEffectMessage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHCardEffectMessageTest::RunTest(const FString& Parameters)
{
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* PC = World->SpawnActor<ASHPlayerController>();
	PC->SetAsLocalPlayerController();
	World->AddController(PC);
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	LocalPlayer->PlayerController = PC;
	PC->Player = LocalPlayer;
	auto* Viewport = NewObject<UGameViewportClient>(GEngine);
	const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	Viewport->SetViewportOverlayWidget(nullptr, Overlay);
	GEngine->GetWorldContextFromWorldChecked(World).GameViewport = Viewport;
	auto Advance = [World](int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame) { ++GFrameCounter; World->GetTimerManager().Tick(0.1f); }
	};
	const FText Notice = NSLOCTEXT("CardEffects", "TryNextTurn", "Spróbuj w następnej turze");
	PC->ClientShowCardEffectMessage(Notice);
	UCardEffectMessageWidget* First = PC->GetActiveCardEffectMessage();
	if (TestNotNull(TEXT("Message opens without a frontend primary layout or a configured widget"), First))
	{
		TestTrue(TEXT("Notice is in the player's viewport"), First->IsInViewport());
		TestEqual(TEXT("Widget exposes exact localized message"), First->Message.ToString(), Notice.ToString());
		const auto* Text = Cast<UTextBlock>(First->GetWidgetFromName(TEXT("MessageText")));
		if (TestNotNull(TEXT("Native default contains visible message text"), Text))
		{
			TestEqual(TEXT("Default renders the retry notice"), Text->GetText().ToString(), Notice.ToString());
		}
		TestEqual(TEXT("Notice and descendants cannot intercept game input"), First->GetVisibility(), ESlateVisibility::HitTestInvisible);
		Advance(25);
		TestEqual(TEXT("Notice stays visible before its four-second timeout"), PC->GetActiveCardEffectMessage(), First);
		PC->ClientShowCardEffectMessage(FText::FromString(TEXT("Second notice")));
		UCardEffectMessageWidget* Second = PC->GetActiveCardEffectMessage();
		TestNotNull(TEXT("A new notice replaces the old one"), Second);
		TestFalse(TEXT("Replacing a notice removes the old viewport widget"), First->IsInViewport());
		Advance(20);
		TestEqual(TEXT("Previous timer cannot close the replacement early"), PC->GetActiveCardEffectMessage(), Second);
		Advance(25);
		TestNull(TEXT("Notice reference clears after four seconds"), PC->GetActiveCardEffectMessage());
		if (Second) { TestFalse(TEXT("Timed-out notice is removed from viewport"), Second->IsInViewport()); }
	}

	auto* BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		UCardEffectMessageWidget::StaticClass(), GetTransientPackage(),
		MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), TEXT("TestCardEffectNotice")),
		BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
	if (!BP->WidgetTree) { BP->WidgetTree = NewObject<UWidgetTree>(BP); }
	BP->WidgetTree->RootWidget = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	FKismetEditorUtilities::CompileBlueprint(BP);
	PC->CardEffectMessageWidgetClass = BP->GeneratedClass.Get();
	PC->ClientShowCardEffectMessage(Notice);
	if (UCardEffectMessageWidget* Designer = PC->GetActiveCardEffectMessage(); TestNotNull(TEXT("Designer subclass is used"), Designer))
	{
		TestEqual(TEXT("Configured widget class is preserved"), Designer->GetClass(), BP->GeneratedClass.Get());
		TestTrue(TEXT("Native fallback does not replace the Designer root"), Designer->WidgetTree->RootWidget->IsA<UTextBlock>());
		const auto* Text = Cast<UTextBlock>(Designer->WidgetTree->RootWidget);
		TestEqual(TEXT("Optional bound Designer text receives the message"), Text->GetText().ToString(), Notice.ToString());
		PC->CloseCardEffectMessage();
		TestNull(TEXT("Explicit close clears controller reference"), PC->GetActiveCardEffectMessage());
		TestFalse(TEXT("Explicit close removes Designer widget"), Designer->IsInViewport());
	}
	PC->CloseCardEffectMessage();
	GEngine->GetWorldContextFromWorldChecked(World).GameViewport = nullptr;
	PC->Player = nullptr;
	LocalPlayer->PlayerController = nullptr;
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
