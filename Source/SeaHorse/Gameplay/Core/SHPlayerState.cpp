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
}

void ASHPlayerState::SetHand(ASHHand* NewHand)
{
	Hand = NewHand;
}

ASHHand* ASHPlayerState::GetHand()
{
	return Hand;
}

void ASHPlayerState::SetSeatIndex(int32 NewSeatIndex)
{
	checkf(HasAuthority(), TEXT("SeatIndex can only be assigned on the server"));

	SeatIndex = NewSeatIndex;
}

int32 ASHPlayerState::GetSeatIndex()
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
