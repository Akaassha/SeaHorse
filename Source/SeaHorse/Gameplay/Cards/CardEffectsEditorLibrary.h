#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CardEffectsEditorLibrary.generated.h"

UCLASS()
class SEAHORSE_API UCardEffectsEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Migrate hand hover and click selection to the shared pointer resolver. Validates both Blueprints before saving. */
	UFUNCTION(BlueprintCallable, Category = "Card Effects|Editor")
	static bool UpgradeCardPointerBlueprints(bool bSave = false);
	/** One-time migration of BP_Hand's pairing predicate to the shared native rule. */
	UFUNCTION(BlueprintCallable, Category = "Card Effects|Editor")
	static bool UpgradeHandPairingBlueprint();
	/** Adds an editable UMG layout only to an empty reaction Widget Blueprint. Existing layouts are preserved. */
	UFUNCTION(BlueprintCallable, Category = "Card Effects|Editor")
	static bool UpgradeReactionPromptBlueprint(const FString& AssetPath);
};
