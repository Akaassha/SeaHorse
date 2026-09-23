#include "Gameplay/Presentation/CardSelectionPrompt.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"

ASHPlayerController* UCardSelectionPrompt::GetSelectionController() const
{
	ASHPlayerController* PC = GetOwningPlayer<ASHPlayerController>();
	// A retained reference to a closed widget cannot answer a subsequent request.
	return IsValid(PC) && PC->SelectionPromptWidget == this ? PC : nullptr;
}

int32 UCardSelectionPrompt::GetMinimumCards() const
{
	const auto* PC = GetSelectionController();
	return PC ? PC->LocalSelectionMin : 0;
}

int32 UCardSelectionPrompt::GetMaximumCards() const
{
	const auto* PC = GetSelectionController();
	return PC ? PC->LocalSelectionMax : 0;
}

TArray<ASHCard*> UCardSelectionPrompt::GetSelectedCards() const
{
	TArray<ASHCard*> Result;
	if (const auto* PC = GetSelectionController())
	{
		for (ASHCard* Card : PC->LocallySelectedEffectCards) { Result.Add(Card); }
	}
	return Result;
}

TArray<ASHCard*> UCardSelectionPrompt::GetCandidateCards() const
{
	TArray<ASHCard*> Result;
	if (const auto* PC = GetSelectionController())
	{
		for (ASHCard* Card : PC->LocalHandCardSelectionCandidates) { Result.Add(Card); }
	}
	return Result;
}

int32 UCardSelectionPrompt::GetSelectedCardCount() const
{
	const auto* PC = GetSelectionController();
	return PC ? PC->LocallySelectedEffectCards.Num() : 0;
}

bool UCardSelectionPrompt::CanConfirmSelection() const
{
	const auto* PC = GetSelectionController();
	return PC && PC->LocallySelectedEffectCards.Num() >= PC->LocalSelectionMin &&
		PC->LocallySelectedEffectCards.Num() <= PC->LocalSelectionMax;
}

void UCardSelectionPrompt::ConfirmSelection()
{
	if (auto* PC = GetSelectionController()) { PC->ConfirmEffectCardSelection(); }
}

void UCardSelectionPrompt::ToggleCardSelection(ASHCard* Card)
{
	if (auto* PC = GetSelectionController()) { PC->ToggleEffectCardSelection(Card); }
}

void UCardSelectionPrompt::ClearSelection()
{
	if (auto* PC = GetSelectionController())
	{
		PC->LocallySelectedEffectCards.Reset();
		PC->RefreshSelectionPrompt();
	}
}
