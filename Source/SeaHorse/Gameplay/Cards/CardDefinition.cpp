// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardFragment.h"

const UCardFragment* UCardDefinition::FindFragmentByClass(TSubclassOf<UCardDefinition> CardDefinition, TSubclassOf<UCardFragment> FragmentClass)
{
	if (CardDefinition && FragmentClass)
	{
		UCardDefinition* CardCDO = CardDefinition.GetDefaultObject();
		for (const TObjectPtr<UCardFragment>& Fragment : CardCDO->CardFragments)
		{
			if (IsValid(Fragment) && Fragment->IsA(FragmentClass))
			{
				return Fragment;
			}
		}
	}
	
	return nullptr;
}
