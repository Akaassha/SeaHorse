// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/Widgets/Options/DataObjects/ListDataObjectValue.h"

void UListDataObjectValue::SetDataDynamicGetter(const TSharedPtr<FOptionsDataInteractionHelper>& InDynamicGetter)
{
	DataDynamicGetter = InDynamicGetter;
}

void UListDataObjectValue::SetDataDynamicSetter(TSharedPtr<FOptionsDataInteractionHelper> InDynamicSetter)
{
	DataDynamicSetter = InDynamicSetter;
}
