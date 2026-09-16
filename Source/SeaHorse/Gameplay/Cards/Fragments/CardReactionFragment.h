#pragma once
#include "CoreMinimal.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "CardReactionFragment.generated.h"

UENUM(BlueprintType)
enum class ECardReactionKind : uint8
{
	CaptureAfterActivation,
	CancelActivation
};

UCLASS(BlueprintType, EditInlineNew)
class SEAHORSE_API UCardReactionFragment : public UCardEffectFragment
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reaction")
	ECardReactionKind ReactionKind = ECardReactionKind::CaptureAfterActivation;
	/** Widget Blueprint derived from CardReactionPrompt, with its layout authored in UMG. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reaction")
	TSubclassOf<class UCardReactionPrompt> PromptWidgetClass;
};
