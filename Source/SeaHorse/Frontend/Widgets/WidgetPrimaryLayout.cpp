// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/WidgetPrimaryLayout.h"

#include "Frontend/Debug/FrontendDebugHelper.h"

UCommonActivatableWidgetContainerBase* UWidgetPrimaryLayout::FindWidgetStackByTag(const FGameplayTag& InTag) const
{

	return RegisteredWidgetStackMap.FindRef(InTag);
}

void UWidgetPrimaryLayout::RegisterWidetStack(UPARAM(meta = (Categories = "Frontend.WidgetStack")) FGameplayTag InStackTag, UCommonActivatableWidgetContainerBase* InStack)
{
	if (!IsDesignTime() && InStack && InStackTag.IsValid())
	{
		if (!RegisteredWidgetStackMap.Contains(InStackTag))
		{
			RegisteredWidgetStackMap.Add(InStackTag, InStack);

			Debug::Print(TEXT("Widget Stack Registered under the tag ") + InStackTag.ToString());
		}
	}
}
