// Fill out your copyright notice in the Description page of Project Settings.


#include "SeaHorse/Gameplay/Core/SHGameState.h"
#include "Kismet/GameplayStatics.h"
#include "SeaHorse/Gameplay/Core/SHPlayerController.h"
#include "SeaHorse/Gameplay/Core/SHPlayerState.h"
#include "Net/UnrealNetwork.h"

void ASHGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASHGameState, InitialDealtCardCount);
    DOREPLIFETIME(ASHGameState, bMatchReady);
}

void ASHGameState::AddPlayerState(APlayerState* PlayerState)
{
    Super::AddPlayerState(PlayerState);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[SH_INIT][%.3f][GS] AddPlayerState | Player=%s | PlayerArray=%d"),
        GetWorld()->GetTimeSeconds(),
        *GetNameSafe(PlayerState),
        PlayerArray.Num()
    );

    if (!bMatchReady)
    {
        return;
    }

    ASHPlayerController* PC =
        Cast<ASHPlayerController>(
            UGameplayStatics::GetPlayerController(this, 0)
        );

    if (IsValid(PC) && PC->IsLocalController())
    {
        PC->TrySetupTableView();
    }
}

void ASHGameState::SetMatchReady(bool bReady)
{
    checkf(HasAuthority(), TEXT("SetMatchReady can only be called on server"));

    bMatchReady = bReady;

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][GS] SetMatchReady=%d | InitialCards=%d"),
        GetWorld()->GetTimeSeconds(),
        bMatchReady,
        InitialDealtCardCount);

    HandleMatchReady();
}

void ASHGameState::OnRep_MatchReady()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][GS] OnRep_MatchReady=%d | InitialCards=%d"),
        GetWorld()->GetTimeSeconds(),
        bMatchReady,
        InitialDealtCardCount);

    HandleMatchReady();
}

void ASHGameState::HandleMatchReady()
{

    if (!bMatchReady)
    {
        return;
    }

    ASHPlayerController* PC =Cast<ASHPlayerController>(UGameplayStatics::GetPlayerController(this, 0));

    if (!IsValid(PC) || !PC->IsLocalController())
    {
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[SH_INIT][%.3f][GS] HandleMatchReady | LocalPC=%s"),
        GetWorld()->GetTimeSeconds(),
        *GetNameSafe(PC));

    PC->TrySetupTableView();
}
