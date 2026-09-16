#include "Gameplay/Presentation/CardReactionPrompt.h"
#include "Gameplay/Core/SHPlayerController.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UCardReactionPrompt::InitializeOffer(int32 InOfferId, ASHCard* InReactionCard, ASHCard* InTargetCard)
{
	OfferId = InOfferId;
	ReactionCard = InReactionCard;
	TargetCard = InTargetCard;
}

void UCardReactionPrompt::AcceptReaction()
{
	if (ASHPlayerController* PC = GetOwningPlayer<ASHPlayerController>()) { PC->ServerRespondToCardReaction(OfferId, true); }
}

void UCardReactionPrompt::DeclineReaction()
{
	if (ASHPlayerController* PC = GetOwningPlayer<ASHPlayerController>()) { PC->ServerRespondToCardReaction(OfferId, false); }
}

void UCardReactionPrompt::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (AcceptButton) { AcceptButton->OnClicked.AddUniqueDynamic(this, &UCardReactionPrompt::AcceptReaction); }
	if (DeclineButton) { DeclineButton->OnClicked.AddUniqueDynamic(this, &UCardReactionPrompt::DeclineReaction); }
}

void UCardReactionPrompt::NativeConstruct()
{
	Super::NativeConstruct();
	if (!QuestionText) { return; }
	auto CardName = [](ASHCard* Card)
	{
		const UCardDefinition* Definition = IsValid(Card) ? Card->GetKnownCardDefinition().GetDefaultObject() : nullptr;
		return Definition ? Definition->CardName.ToString() : FString(TEXT("para"));
	};
	const FText Question = FText::FromString(FString::Printf(TEXT("Czy aktywować %s?\nReakcja na: %s"), *CardName(ReactionCard), *CardName(TargetCard)));
	QuestionText->SetText(Question);
}
