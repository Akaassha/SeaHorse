#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CardEffectsEditorLibrary.generated.h"

UCLASS()
class SEAHORSE_API UCardEffectsEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** One-time migration of BP_Hand's pairing predicate to the shared native rule. */
	UFUNCTION(BlueprintCallable, Category = "Card Effects|Editor")
	static bool UpgradeHandPairingBlueprint();
	/** Adds an editable UMG layout only to an empty reaction Widget Blueprint. Existing layouts are preserved. */
	UFUNCTION(BlueprintCallable, Category = "Card Effects|Editor")
	static bool UpgradeReactionPromptBlueprint(const FString& AssetPath);
};
