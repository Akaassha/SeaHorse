// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/FrontendFunctionLibrary.h"
#include "Frontend/Settings/FrontendDeveloperSettings.h"

TSoftClassPtr<UWidgetActivatableBase> UFrontendFunctionLibrary::GetFrontendSoftWidgetClassByTag(UPARAM(meta = (Categories = "Frontend.Widget")) FGameplayTag InWidgetTag)
{
    const UFrontendDeveloperSettings* FrontendDeveloperSettings = GetDefault<UFrontendDeveloperSettings>();


    return FrontendDeveloperSettings->FrontendWidgetMap.FindRef(InWidgetTag);
}

TSoftObjectPtr<UTexture2D> UFrontendFunctionLibrary::GetOptionsSoftImageByTag(FGameplayTag InImageTag)
{
    const UFrontendDeveloperSettings* FrontendDeveloperSettings = GetDefault<UFrontendDeveloperSettings>();


    return FrontendDeveloperSettings->OptionsScreenSoftImageMap.FindRef(InImageTag);
}
