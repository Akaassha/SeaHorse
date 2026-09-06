#include "GameSettings/SHGameUserSettings.h"
#include "Engine/Engine.h"
#include "Frontend/Settings/FrontendDeveloperSettings.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Misc/App.h"
#include "Sound/SoundClass.h"

USHGameUserSettings* USHGameUserSettings::Get()
{
	return CastChecked<USHGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

void USHGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	CurrentCulture.Empty();
	DisplayGamma = 2.2f;
	OverallVolume = MusicVolume = SFXVolume = 1.f;
	bAllowBackgroundAudio = false;
}

void USHGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();
	if (IsRunningDedicatedServer()) { return; }
	if (GEngine) { GEngine->DisplayGamma = DisplayGamma; }
	FApp::SetVolumeMultiplier(OverallVolume);
	FApp::SetUnfocusedVolumeMultiplier(bAllowBackgroundAudio ? 1.f : 0.f);
	const auto* Settings = GetDefault<UFrontendDeveloperSettings>();
	if (USoundClass* Music = Settings->MusicSoundClass.LoadSynchronous()) { Music->Properties.Volume = MusicVolume; }
	if (USoundClass* SFX = Settings->SFXSoundClass.LoadSynchronous()) { SFX->Properties.Volume = SFXVolume; }
	if (!CurrentCulture.IsEmpty()) { FInternationalization::Get().SetCurrentCulture(CurrentCulture); }
}

FString USHGameUserSettings::GetCurrentCulture() const
{
	return FInternationalization::Get().GetCurrentCulture()->GetName();
}

void USHGameUserSettings::SetCurrentCulture(const FString& Value)
{
	if (FInternationalization::Get().SetCurrentCulture(Value)) { CurrentCulture = Value; }
}

void USHGameUserSettings::SetCurrentDisplayGamma(float Value)
{
	DisplayGamma = FMath::Clamp(Value, 1.7f, 2.7f);
	if (GEngine) { GEngine->DisplayGamma = DisplayGamma; }
}

void USHGameUserSettings::SetOverallVolume(float Value) { OverallVolume = FMath::Clamp(Value, 0.f, 2.f); }
void USHGameUserSettings::SetMusicVolume(float Value) { MusicVolume = FMath::Clamp(Value, 0.f, 2.f); }
void USHGameUserSettings::SetSFXVolume(float Value) { SFXVolume = FMath::Clamp(Value, 0.f, 2.f); }
void USHGameUserSettings::SetAllowBackgroundAudio(bool bValue) { bAllowBackgroundAudio = bValue; }
