#include "Gameplay/Core/SHGameMode.h"
#include "Gameplay/Cards/Tasks/CompareHandsEffectTask.h"
#include "Gameplay/Cards/Tasks/RevealHandEffectTask.h"

void ASHGameMode::AcknowledgeHandReveal(ASHPlayerState* Player, FGuid SessionId)
{
	if (!HasAuthority()) { return; }
	for (UCardEffectTask* Task : ActiveEffectTasks)
	{
		if (URevealHandEffectTask* Reveal = Cast<URevealHandEffectTask>(Task))
		{
			if (Reveal->IsSessionFor(Player, SessionId)) { Reveal->AcknowledgePresentation(Player, SessionId); return; }
		}
		if (UCompareHandsEffectTask* Comparison = Cast<UCompareHandsEffectTask>(Task))
		{
			if (Comparison->IsSessionFor(Player, SessionId)) { Comparison->AcknowledgePresentation(Player, SessionId); return; }
		}
	}
}

void ASHGameMode::TransferComparedHandCard(ASHPlayerState* Player, FGuid SessionId,
	ASHCard* Card, int32 InsertIndex)
{
	if (!HasAuthority()) { return; }
	const auto Tasks = ActiveEffectTasks;
	for (UCardEffectTask* Task : Tasks)
	{
		if (UCompareHandsEffectTask* Comparison = Cast<UCompareHandsEffectTask>(Task))
		{
			if (Comparison->IsSessionFor(Player, SessionId))
			{
				Comparison->TransferCard(Player, SessionId, Card, InsertIndex);
				return;
			}
		}
	}
}

void ASHGameMode::FinishHandReveal(ASHPlayerState* Player, FGuid SessionId)
{
	if (!HasAuthority()) { return; }
	const auto Tasks = ActiveEffectTasks;
	for (UCardEffectTask* Task : Tasks)
	{
		if (URevealHandEffectTask* Reveal = Cast<URevealHandEffectTask>(Task))
		{
			if (Reveal->IsSessionFor(Player, SessionId)) { Reveal->FinishViewing(Player, SessionId); return; }
		}
	}
}

void ASHGameMode::ReorderRevealedHand(ASHPlayerState* Player, FGuid SessionId, ASHCard* Card, int32 InsertIndex)
{
	if (!HasAuthority()) { return; }
	const auto Tasks = ActiveEffectTasks;
	for (UCardEffectTask* Task : Tasks)
	{
		if (URevealHandEffectTask* Reveal = Cast<URevealHandEffectTask>(Task))
		{
			if (Reveal->IsSessionFor(Player, SessionId)) { Reveal->Reorder(Player, SessionId, Card, InsertIndex); return; }
		}
	}
}

void ASHGameMode::CloseHandRevealsForDisconnect(ASHPlayerState* Player)
{
	if (!HasAuthority()) { return; }
	const auto Tasks = ActiveEffectTasks;
	for (UCardEffectTask* Task : Tasks)
	{
		if (URevealHandEffectTask* Reveal = Cast<URevealHandEffectTask>(Task))
		{
			Reveal->HandleParticipantDisconnected(Player);
		}
		if (UCompareHandsEffectTask* Comparison = Cast<UCompareHandsEffectTask>(Task))
		{
			Comparison->HandleParticipantDisconnected(Player);
		}
	}
}

bool ASHGameMode::IsPlayerInHandReveal(const ASHPlayerState* Player) const
{
	for (UCardEffectTask* Task : ActiveEffectTasks)
	{
		const URevealHandEffectTask* Reveal = Cast<URevealHandEffectTask>(Task);
		if (IsValid(Reveal) && Reveal->IsSessionFor(const_cast<ASHPlayerState*>(Player), Reveal->GetSessionId())) { return true; }
		const UCompareHandsEffectTask* Comparison = Cast<UCompareHandsEffectTask>(Task);
		if (IsValid(Comparison) && Comparison->IsSessionFor(
			const_cast<ASHPlayerState*>(Player), Comparison->GetSessionId())) { return true; }
	}
	return false;
}

bool ASHGameMode::HasActiveHandReveal() const
{
	for (UCardEffectTask* Task : ActiveEffectTasks)
	{
		const URevealHandEffectTask* Reveal = Cast<URevealHandEffectTask>(Task);
		if (IsValid(Reveal) && Reveal->IsSessionFor(Reveal->GetActivatingPlayer(), Reveal->GetSessionId())) { return true; }
		const UCompareHandsEffectTask* Comparison = Cast<UCompareHandsEffectTask>(Task);
		if (IsValid(Comparison) && Comparison->IsSessionFor(
			Comparison->GetActivatingPlayer(), Comparison->GetSessionId())) { return true; }
	}
	return false;
}
