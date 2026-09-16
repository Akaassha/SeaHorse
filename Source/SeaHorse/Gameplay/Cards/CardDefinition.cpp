#include "SeaHorse/Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "SeaHorse/Gameplay/Cards/Fragments/CardFragment.h"

bool UCardDefinition::ArePairDefinitionsCompatible(TSubclassOf<UObject> A, TSubclassOf<UObject> B)
{
	if (!A || !B || !A->IsChildOf(StaticClass()) || !B->IsChildOf(StaticClass())) { return false; }
	const auto* RulesA = Cast<UImmediateVictoryPairFragment>(FindFragmentByClass(A.Get(), UImmediateVictoryPairFragment::StaticClass()));
	const auto* RulesB = Cast<UImmediateVictoryPairFragment>(FindFragmentByClass(B.Get(), UImmediateVictoryPairFragment::StaticClass()));
	if (RulesA || RulesB)
	{
		return (!RulesA || RulesA->AllowedPartners.Contains(FSoftClassPath(B.Get()))) &&
			(!RulesB || RulesB->AllowedPartners.Contains(FSoftClassPath(A.Get())));
	}
	return A == B;
}

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
