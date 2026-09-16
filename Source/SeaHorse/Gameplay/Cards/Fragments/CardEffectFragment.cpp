// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Cards/Fragments/CardEffectFragment.h"
#include "Gameplay/Cards/SHCard.h"
#include "Gameplay/Cards/CardDefinition.h"
#include "Gameplay/Core/SHPlayerState.h"
#include "Gameplay/SHHand.h"

TArray<ASHCard*> UStoredPairFilterEffectFragment::GetEligibleCards(const ASHPlayerState* Activator, const ASHPlayerState* Owner) const
{
	TArray<ASHCard*> Cards;
	if (!IsValid(Owner) || Owner == Activator || Owner->IsProtectedFromCardEffects() || !IsValid(Owner->GetHand())) { return Cards; }
	for (const FActivatedPair& Pair : Owner->GetHand()->GetLogicalActivationPairs())
	{
		if (!IsValid(Pair.CardA) || !IsValid(Pair.CardB) || Pair.bActivated || Pair.State != EActivationPairState::Ready) { continue; }
		// Opponents' definitions are public through the revealed field on clients.
		UClass* Definition = Pair.CardA->GetKnownCardDefinition().Get();
		if (AllowedCardDefinitions.Contains(FSoftClassPath(Definition)))
		{
			Cards.Add(Pair.CardA); Cards.Add(Pair.CardB);
		}
	}
	return Cards;
}

