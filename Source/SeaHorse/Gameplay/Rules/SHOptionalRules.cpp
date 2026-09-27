#include "Gameplay/Rules/SHOptionalRules.h"
#include "Kismet/GameplayStatics.h"

FString FSHOptionalRules::ToTravelOptions() const
{
	return FString::Printf(TEXT("?SHRemoveOrphanedRatfolk=%d?SHRemoveOtherPaulus=%d"),
		bAllowOrphanedRatfolkRemoval ? 1 : 0, bRemoveOtherPaulusAfterRatfolkPair ? 1 : 0);
}

FSHOptionalRules FSHOptionalRules::FromTravelOptions(const FString& Options)
{
	FSHOptionalRules Rules;
	// Only recognize exact boolean values. Missing or malformed options keep the default.
	const FString Orphaned = UGameplayStatics::ParseOption(Options, TEXT("SHRemoveOrphanedRatfolk"));
	const FString OtherPaulus = UGameplayStatics::ParseOption(Options, TEXT("SHRemoveOtherPaulus"));
	if (Orphaned == TEXT("0") || Orphaned == TEXT("1")) { Rules.bAllowOrphanedRatfolkRemoval = Orphaned == TEXT("1"); }
	if (OtherPaulus == TEXT("0") || OtherPaulus == TEXT("1")) { Rules.bRemoveOtherPaulusAfterRatfolkPair = OtherPaulus == TEXT("1"); }
	return Rules;
}
