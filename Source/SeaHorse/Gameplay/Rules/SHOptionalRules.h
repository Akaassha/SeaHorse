#pragma once

#include "CoreMinimal.h"
#include "SHOptionalRules.generated.h"

/** Host-selected rules, copied from the lobby into the authoritative match. */
USTRUCT(BlueprintType)
struct SEAHORSE_API FSHOptionalRules
{
	GENERATED_BODY()

	/** Automatically removes unpaired Ratfolk when no allowed Paulus remains in deck, hands or activation zones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Szczuroludzie")
	bool bAllowOrphanedRatfolkRemoval = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Szczuroludzie")
	bool bRemoveOtherPaulusAfterRatfolkPair = true;

	bool operator==(const FSHOptionalRules& Other) const
	{
		return bAllowOrphanedRatfolkRemoval == Other.bAllowOrphanedRatfolkRemoval &&
			bRemoveOtherPaulusAfterRatfolkPair == Other.bRemoveOtherPaulusAfterRatfolkPair;
	}

	FString ToTravelOptions() const;
	static FSHOptionalRules FromTravelOptions(const FString& Options);
};
