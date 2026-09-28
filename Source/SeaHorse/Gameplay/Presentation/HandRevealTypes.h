#pragma once

#include "CoreMinimal.h"
#include "HandRevealTypes.generated.h"

class ASHCard;
class UCardDefinition;

/** Private presentation snapshot. Send only through the two participants' owner channels. */
USTRUCT(BlueprintType)
struct FSHRevealedHandCard
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hand Reveal")
	TObjectPtr<ASHCard> SourceCard;
	UPROPERTY(BlueprintReadOnly, Category = "Hand Reveal")
	TSubclassOf<UCardDefinition> CardDefinition;
	UPROPERTY(BlueprintReadOnly, Category = "Hand Reveal")
	TSubclassOf<ASHCard> CardActorClass;
};
