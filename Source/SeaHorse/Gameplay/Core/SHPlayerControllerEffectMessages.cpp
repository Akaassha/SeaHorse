#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Presentation/CardEffectMessageWidget.h"
#include "TimerManager.h"

void ASHPlayerController::ClientShowCardEffectMessage_Implementation(const FText& Message)
{
	if (IsLocalController()) { ShowCardEffectMessage(Message); }
}

void ASHPlayerController::ShowCardEffectMessage_Implementation(const FText& Message)
{
	if (!IsLocalController() || !GetLocalPlayer() || !GetWorld() || !GetWorld()->GetGameViewport()) { return; }
	CloseCardEffectMessage();
	UClass* WidgetClass = CardEffectMessageWidgetClass.Get();
	if (!WidgetClass || WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
	{
		WidgetClass = UCardEffectMessageWidget::StaticClass();
	}
	ActiveCardEffectMessage = CreateWidget<UCardEffectMessageWidget>(this, WidgetClass);
	if (!ActiveCardEffectMessage) { return; }
	UCardEffectMessageWidget* Widget = ActiveCardEffectMessage;
	Widget->AddToViewport(200);
	Widget->SetMessage(Message);
	// A Designer event may have dismissed this notice synchronously.
	if (ActiveCardEffectMessage != Widget) { return; }
	Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	GetWorldTimerManager().SetTimer(CardEffectMessageTimer, this, &ASHPlayerController::CloseCardEffectMessage, 4.f, false);
}

void ASHPlayerController::CloseCardEffectMessage()
{
	GetWorldTimerManager().ClearTimer(CardEffectMessageTimer);
	UCardEffectMessageWidget* Previous = ActiveCardEffectMessage;
	ActiveCardEffectMessage = nullptr;
	if (IsValid(Previous)) { Previous->RemoveFromParent(); }
}
