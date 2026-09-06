// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/DataObjects/MyListDataObjectStringResolution.h"
#include "Frontend/Widgets/Options/OptionsDataInteractionHelper.h"
#include "GameSettings/SHGameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"

void UMyListDataObjectStringResolution::InitResolutionValue()
{
	AvailableOptionsStringArray.Reset();
	AvailableOptionsTextArray.Reset();
	TArray<FIntPoint> AvailableResolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(AvailableResolutions);

	// Headless sessions and some display drivers report no fullscreen modes.
	if (AvailableResolutions.IsEmpty())
	{
		FIntPoint Current = USHGameUserSettings::Get()->GetScreenResolution();
		if (Current.X <= 0 || Current.Y <= 0) { Current = FIntPoint(1280, 720); }
		AvailableResolutions.Add(Current);
	}
	AvailableResolutions.Sort(
		[](const FIntPoint& A, const FIntPoint& B)->bool {
			return int64(A.X) * A.Y < int64(B.X) * B.Y;
		}
	);

	for (const FIntPoint& Resolution : AvailableResolutions)
	{
		AddDynamicOption(ResToValueString(Resolution), ResToDisplayText(Resolution));
	}

	MaximumAllowedResolution = ResToValueString(AvailableResolutions.Last());

	SetDefaultValueFromString(MaximumAllowedResolution);
}

void UMyListDataObjectStringResolution::OnDataObjectInitialized()
{
	Super::OnDataObjectInitialized();

	if (!TrySetDisplayTextFromStringValue(CurrentStringValue))
	{
		CurrentDisplayText = ResToDisplayText(USHGameUserSettings::Get()->GetScreenResolution());
	}
}

FString UMyListDataObjectStringResolution::ResToValueString(const FIntPoint& InResolution) const
{
	return FString::Printf(TEXT("(X=%i,Y=%i)"), InResolution.X, InResolution.Y);
}

FText UMyListDataObjectStringResolution::ResToDisplayText(const FIntPoint& InResolution) const
{
	const FString DisplayString = FString::Printf(TEXT("%i x %i"), InResolution.X, InResolution.Y);

	return FText::FromString(DisplayString);
}
