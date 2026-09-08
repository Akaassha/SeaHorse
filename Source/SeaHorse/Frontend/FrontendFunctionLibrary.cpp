// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/FrontendFunctionLibrary.h"
#include "Frontend/Settings/FrontendDeveloperSettings.h"
#include "Internationalization/BreakIterator.h"

FText UFrontendFunctionLibrary::ShortenText(const FText& Text, int32 MaxCharacters, bool bAddEllipsis)
{
    if (MaxCharacters <= 0) { return FText::GetEmpty(); }

    const FString String = Text.ToString();
    const TSharedRef<IBreakIterator> Iterator = FBreakIterator::CreateCharacterBoundaryIterator();
    Iterator->SetString(String);
    Iterator->ResetToBeginning();
    int32 End = 0;
    for (int32 Index = 0; Index < MaxCharacters; ++Index)
    {
        const int32 Next = Iterator->MoveToNext();
        if (Next == INDEX_NONE) { return Text; }
        End = Next;
    }
    if (End >= String.Len()) { return Text; }

    return FText::FromString(String.Left(End) + (bAddEllipsis ? TEXT("\u2026") : TEXT("")));
}

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
