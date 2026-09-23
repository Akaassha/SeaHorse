#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "InputKeyEventArgs.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/UnrealType.h"
#include "Widgets/SOverlay.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/SHHand.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Presentation/CardInfoWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCardInspectionTest, "SeaHorse.Gameplay.Input.CardInspection",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSHCardInspectionTest::RunTest(const FString& Parameters)
{
 TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 auto* PC = World->SpawnActor<ASHPlayerController>();
 PC->SetAsLocalPlayerController(); World->AddController(PC);
 auto* PS = World->SpawnActor<ASHPlayerState>(); PC->PlayerState = PS; PS->SetOwner(PC);
 auto* Hand = World->SpawnActor<ASHHand>(); Hand->SetOwner(PC); PS->SetHand(Hand);
 auto* OtherHand = World->SpawnActor<ASHHand>();
 auto* Card = World->SpawnActor<ASHCard>();
 Card->SetOwner(Hand); Card->SetCardZone(ECardZone::Hand); Card->SetCardDefinition(UCardDefinition::StaticClass());
 TestTrue(TEXT("Own private hand is inspectable"), PC->CanInspectCard(Card));
 Card->SetOwner(OtherHand);
 TestFalse(TEXT("Host knowledge and stale owner-only definitions do not disclose opponents' cards"), PC->CanInspectCard(Card));
 OtherHand->SetIsNPC(true);
 TestFalse(TEXT("Hidden BN cards are not inspectable"), PC->CanInspectCard(Card));
 Card->SetCardZone(ECardZone::Activation);
 TestFalse(TEXT("An unrevealed activation card cannot use the host's private definition"), PC->CanInspectCard(Card));
 Card->Reveal(); Card->SetCardDefinition(nullptr);
 TestTrue(TEXT("Public activation uses the replicated revealed definition"), PC->CanInspectCard(Card));
 Card->SetCardZone(ECardZone::Victory);
 TestTrue(TEXT("Public victory cards are inspectable"), PC->CanInspectCard(Card));
 Card->SetCardZone(ECardZone::Deck);
 TestFalse(TEXT("Deck is excluded even with historical disclosure"), PC->CanInspectCard(Card));
 Card->SetCardZone(ECardZone::None);
 TestFalse(TEXT("Removed cards are excluded"), PC->CanInspectCard(Card));
 Card->SetCardZone(ECardZone::Activation);

 ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
 LocalPlayer->PlayerController = PC; PC->Player = LocalPlayer;
 auto* Viewport = NewObject<UGameViewportClient>(GEngine);
 const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
 Viewport->SetViewportOverlayWidget(nullptr, Overlay);
 GEngine->GetWorldContextFromWorldChecked(World).GameViewport = Viewport;
 TestFalse(TEXT("No configured widget means no fallback overlay"), PC->ShowCardInfo(Card));
 auto* BP = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
  UCardInfoWidget::StaticClass(), GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), TEXT("TestCardInfo")),
  BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
 if (!BP->WidgetTree) { BP->WidgetTree = NewObject<UWidgetTree>(BP); }
 BP->WidgetTree->RootWidget = BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CustomClose"));
 FKismetEditorUtilities::CompileBlueprint(BP);
 PC->CardInfoWidgetClass = BP->GeneratedClass.Get();
 TestTrue(TEXT("Designer subclass opens"), PC->ShowCardInfo(Card));
 UCardInfoWidget* Widget = PC->ActiveCardInfoWidget;
 if (TestNotNull(TEXT("Info widget exists"), Widget))
 {
  TestEqual(TEXT("Widget exposes selected actor"), Widget->GetInspectedCard(), Card);
  TestNull(TEXT("Not-yet-rendered face is unavailable"), Widget->GetCardFaceRenderTarget());
  auto* Face = NewObject<UTextureRenderTarget2D>(Card);
  Face->SizeX = 512; Face->SizeY = 768;
  FindFProperty<FObjectPropertyBase>(ASHCard::StaticClass(), TEXT("CardFaceRenderTarget"))->SetObjectPropertyValue_InContainer(Card, Face);
  TestEqual(TEXT("Inspection returns the existing face render target"), Widget->GetCardFaceRenderTarget(), Face);
  auto* Preview = NewObject<UImage>(Widget);
  Preview->SetBrush(Widget->GetCardFaceBrush());
  TestEqual(TEXT("Image brush uses the full rendered card"), Preview->GetBrush().GetResourceObject(), static_cast<UObject*>(Face));
  TestEqual(TEXT("Brush preserves face dimensions"), FVector2D(Preview->GetBrush().ImageSize), FVector2D(512, 768));
  TestEqual(TEXT("Widget exposes definition CDO"), Widget->GetCardDefinition(), GetDefault<UCardDefinition>());
  auto* Button = Cast<UButton>(Widget->GetWidgetFromName(TEXT("CustomClose")));
  if (TestNotNull(TEXT("Designer layout preserved"), Button))
  {
   Button->OnClicked.AddDynamic(Widget, &UCardInfoWidget::CloseCardInfo); Button->OnClicked.Broadcast();
   TestNull(TEXT("Custom close button removes panel"), PC->GetInspectedCard());
  }
  PC->ShowCardInfo(Card); Widget->CloseCardInfo();
  TestNull(TEXT("Old widget cannot read a new panel's texture"), Widget->GetCardFaceRenderTarget());
  TestNotNull(TEXT("Old widget cannot close a later panel"), PC->GetInspectedCard());
  FInputKeyEventArgs WorldClick;
  WorldClick.Key = EKeys::LeftMouseButton;
  WorldClick.Event = IE_Pressed;
  const bool WithPreview = PC->InputKey(WorldClick);
  TestNull(TEXT("World click closes preview"), PC->GetInspectedCard());
  TestEqual(TEXT("Preview dismissal preserves the normal input result"), WithPreview, PC->InputKey(WorldClick));
  PC->ShowCardInfo(Card);
  Card->SetCardZone(ECardZone::None);
  TestNull(TEXT("Access is revoked immediately"), PC->ActiveCardInfoWidget->GetCardDefinition());
  TestNull(TEXT("Texture access is also revoked immediately"), PC->ActiveCardInfoWidget->GetCardFaceRenderTarget());
  TestNull(TEXT("Denied brush contains no texture"), PC->ActiveCardInfoWidget->GetCardFaceBrush().GetResourceObject());
  PC->ValidateCardInfoAccess();
  TestNull(TEXT("Invalidated panel is removed"), PC->ActiveCardInfoWidget.Get());
 }
 PC->CloseCardInfo();
 GEngine->GetWorldContextFromWorldChecked(World).GameViewport = nullptr;
 PC->Player = nullptr; LocalPlayer->PlayerController = nullptr;
 World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
 return true;
}
#endif
