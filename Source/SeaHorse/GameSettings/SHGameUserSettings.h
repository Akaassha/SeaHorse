#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "SHGameUserSettings.generated.h"

// Local presentation preferences only; match rules remain server authoritative.
UCLASS(Config=GameUserSettings, BlueprintType)
class SEAHORSE_API USHGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category = "Settings", meta = (DisplayName = "Get SeaHorse Game User Settings"))
	static USHGameUserSettings* Get();
	virtual void ApplyNonResolutionSettings() override;
	virtual void SetToDefaults() override;
	UFUNCTION(BlueprintPure) FString GetCurrentCulture() const;
	/** Only languages with compiled game localization resources, in stable code order. */
	UFUNCTION(BlueprintPure, Category = "Settings|Language")
	static TArray<FString> GetAvailableGameCultures();
	/** Match a regional culture (e.g. pl-PL) to an available game language without changing it. */
	UFUNCTION(BlueprintPure, Category = "Settings|Language")
	FString GetSelectedGameCulture() const;
	UFUNCTION(BlueprintCallable) void SetCurrentCulture(const FString& Value);
	UFUNCTION(BlueprintPure) float GetCurrentDisplayGamma() const { return DisplayGamma; }
	UFUNCTION(BlueprintCallable) void SetCurrentDisplayGamma(float Value);
	UFUNCTION(BlueprintPure) float GetOverallVolume() const { return OverallVolume; }
	UFUNCTION(BlueprintCallable) void SetOverallVolume(float Value);
	UFUNCTION(BlueprintPure) float GetMusicVolume() const { return MusicVolume; }
	UFUNCTION(BlueprintCallable) void SetMusicVolume(float Value);
	UFUNCTION(BlueprintPure) float GetSFXVolume() const { return SFXVolume; }
	UFUNCTION(BlueprintCallable) void SetSFXVolume(float Value);
	UFUNCTION(BlueprintPure) bool GetAllowBackgroundAudio() const { return bAllowBackgroundAudio; }
	UFUNCTION(BlueprintCallable) void SetAllowBackgroundAudio(bool bValue);
private:
	UPROPERTY(Config) FString CurrentCulture;
	UPROPERTY(Config) float DisplayGamma = 2.2f;
	UPROPERTY(Config) float OverallVolume = 1.f;
	UPROPERTY(Config) float MusicVolume = 1.f;
	UPROPERTY(Config) float SFXVolume = 1.f;
	UPROPERTY(Config) bool bAllowBackgroundAudio = false;
};
