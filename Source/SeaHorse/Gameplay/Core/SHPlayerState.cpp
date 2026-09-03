// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

void ASHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHPlayerState, Hand);
	DOREPLIFETIME(ASHPlayerState, SeatIndex);
	DOREPLIFETIME(ASHPlayerState, VictoryPoints);
}

void ASHPlayerState::SetVictoryPoints(int32 NewVictoryPoints)
{
	checkf(HasAuthority(), TEXT("Victory points can only be changed on the server"));

	if (VictoryPoints == NewVictoryPoints)
	{
		return;
	}

	VictoryPoints = NewVictoryPoints;
	OnRep_VictoryPoints();
	ForceNetUpdate();
}

void ASHPlayerState::OnRep_VictoryPoints()
{
	OnVictoryPointsChanged.Broadcast(VictoryPoints);
}

void ASHPlayerState::SetHand(ASHHand* NewHand)
{
	Hand = NewHand;
	ForceNetUpdate();
}

void ASHPlayerState::OnRep_Hand()
{
	if (ASHPlayerController* PC = Cast<ASHPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
		IsValid(PC) && PC->IsLocalController())
	{
		PC->TrySetupTableView();
	}
}

ASHHand* ASHPlayerState::GetHand() const
{
    return Hand;

}

void ASHPlayerState::SetSeatIndex(int32 NewSeatIndex)
{
	checkf(HasAuthority(), TEXT("SeatIndex can only be assigned on the server"));

	SeatIndex = NewSeatIndex;
	ForceNetUpdate();
}

int32 ASHPlayerState::GetSeatIndex() const
{
	return SeatIndex;
}

void ASHPlayerState::OnRep_SeatIndex()
{
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[SH_INIT][%.3f][PS] OnRep_SeatIndex | PS=%s Seat=%d"),
        GetWorld()->GetTimeSeconds(),
        *GetNameSafe(this),
        SeatIndex
    );

    ASHPlayerController* PC =
        Cast<ASHPlayerController>(
            UGameplayStatics::GetPlayerController(this, 0)
        );

    if (IsValid(PC))
    {
        PC->TrySetupTableView();
    }
}
