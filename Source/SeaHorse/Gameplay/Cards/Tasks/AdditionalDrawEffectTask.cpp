#include "SeaHorse/Gameplay/Cards/Tasks/AdditionalDrawEffectTask.h"

#include "SeaHorse/Gameplay/Core/SHGameMode.h"

void UAdditionalDrawEffectTask::StartEffect_Implementation()
{
	ASHGameMode* GameMode = GetTypedOuter<ASHGameMode>();
	checkf(IsValid(GameMode), TEXT("AdditionalDrawEffectTask has no valid GameMode"));

	UTurnComponent* TurnComponent = GameMode->GetTurnComponent();
	checkf(IsValid(TurnComponent), TEXT("GameMode has no TurnComponent"));

	TurnComponent->ScheduleAdditionalDraw(this, GetActivatingPlayer(), SourceRule);
}

UDrawAgainFromSamePlayerEffectTask::UDrawAgainFromSamePlayerEffectTask()
{
	SourceRule = EAdditionalDrawSourceRule::SamePlayer;
}

UDrawAgainFromDifferentPlayerEffectTask::UDrawAgainFromDifferentPlayerEffectTask()
{
	SourceRule = EAdditionalDrawSourceRule::DifferentPlayer;
}
