#include "Online/SHSteamAvatarSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"

#if SH_WITH_STEAM_AVATARS
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

bool USHSteamAvatarSubsystem::IsSteamAvailable() const
{
#if SH_WITH_STEAM_AVATARS
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) { return false; }
    const IOnlineSubsystem* Service = Online::GetSubsystem(GetWorld());
    if (!Service || Service->GetSubsystemName() != FName(TEXT("STEAM"))) { return false; }
    const IOnlineIdentityPtr Identity = Service->GetIdentityInterface();
    // Do not touch the delay-loaded Steam DLL unless the OSS has initialized a logged-in client.
    return Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
#else
    return false;
#endif
}

void USHSteamAvatarSubsystem::RequestAvatar(APlayerState* Player, FCompletion Completion)
{
    if (bShuttingDown || !IsValid(Player) || Player->GetWorld() != GetWorld())
    {
        Completion(nullptr, TEXT("PlayerState is unavailable or belongs to another world."));
        return;
    }
    if (!IsSteamAvailable())
    {
        Completion(nullptr, TEXT("A logged-in Steam client is required to load avatars."));
        return;
    }
    bool bFinished = false;
    FString Error;
    UTexture2D* Texture = ResolveAvatar(Player, bFinished, Error);
    if (bFinished) { Completion(Texture, Error); return; }
    FRequest& Request = Requests.AddDefaulted_GetRef();
    Request.Player = Player;
    Request.Deadline = FPlatformTime::Seconds() + 20.0;
    Request.Completion = MoveTemp(Completion);
    if (!TickHandle.IsValid())
    {
        TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &ThisClass::TickRequests), 0.1f);
    }
}

UTexture2D* USHSteamAvatarSubsystem::ResolveAvatar(APlayerState* Player, bool& bFinished, FString& Error)
{
    bFinished = true;
#if SH_WITH_STEAM_AVATARS
    const FUniqueNetIdRepl& Id = Player->GetUniqueId();
    if (!Id.IsValid()) { bFinished = false; return nullptr; } // Initial replication can arrive after the row.
    if (Id->GetType() != FName(TEXT("STEAM")))
    {
        Error = TEXT("Player does not have a Steam identity.");
        return nullptr;
    }
    const FString Key = Id->ToString();
    if (const auto* Cached = Cache.Find(Key)) { return Cached->Get(); }
    if (Cache.Num() >= 256) { Cache.Reset(); } // Bound memory across many lobbies.
    ISteamFriends* Friends = SteamFriends();
    ISteamUtils* Utils = SteamUtils();
    if (!Friends || !Utils) { Error = TEXT("Steam avatar interfaces are unavailable."); return nullptr; }
    const CSteamID SteamId(FCString::Strtoui64(*Key, nullptr, 10));
    if (!SteamId.IsValid()) { Error = TEXT("Invalid Steam identity."); return nullptr; }
    if (!RequestedUsers.Contains(Key))
    {
        if (RequestedUsers.Num() >= 512) { RequestedUsers.Reset(); }
        RequestedUsers.Add(Key);
        Friends->RequestUserInformation(SteamId, false);
    }
    // Steam's OSS pumps callbacks. Poll the cached handle while the SDK downloads the image.
    const int Handle = Friends->GetLargeFriendAvatar(SteamId);
    if (Handle <= 0)
    {
        // Zero can also occur before persona information arrives; retain the fallback and wait.
        bFinished = false;
        return nullptr;
    }
    uint32 Width = 0, Height = 0;
    if (!Utils->GetImageSize(Handle, &Width, &Height) || Width == 0 || Height == 0 || Width > 512 || Height > 512)
    {
        Error = TEXT("Steam returned an invalid avatar size.");
        return nullptr;
    }
    TArray<uint8> Pixels;
    Pixels.SetNumUninitialized(Width * Height * 4);
    if (!Utils->GetImageRGBA(Handle, Pixels.GetData(), Pixels.Num()))
    {
        Error = TEXT("Steam avatar pixels could not be read.");
        return nullptr;
    }
    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_R8G8B8A8);
    if (!Texture) { Error = TEXT("Avatar texture could not be created."); return nullptr; }
    Texture->SRGB = true;
    Texture->NeverStream = true;
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num());
    Mip.BulkData.Unlock();
    Texture->UpdateResource();
    Cache.Add(Key, Texture);
    return Texture;
#else
    Error = TEXT("Steam avatars are unsupported on this platform.");
    return nullptr;
#endif
}

bool USHSteamAvatarSubsystem::TickRequests(float DeltaTime)
{
    // Complete outside Requests iteration: Blueprint callbacks may immediately request another avatar.
    TArray<FRequest> Pending = MoveTemp(Requests);
    Requests.Reset();
    for (FRequest& Request : Pending)
    {
        if (bShuttingDown) { Request.Completion(nullptr, TEXT("Avatar service is shutting down.")); continue; }
        bool bFinished = true;
        FString Error;
        UTexture2D* Texture = nullptr;
        if (!Request.Player.IsValid() || Request.Player->GetWorld() != GetWorld()) { Error = TEXT("Player left the world."); }
        else if (!IsSteamAvailable()) { Error = TEXT("Steam is unavailable."); }
        else { Texture = ResolveAvatar(Request.Player.Get(), bFinished, Error); }
        if (!bFinished && FPlatformTime::Seconds() >= Request.Deadline)
        {
            bFinished = true;
            Error = TEXT("No Steam avatar became available within 20 seconds.");
        }
        if (bFinished) { Request.Completion(Texture, Error); }
        else { Requests.Add(MoveTemp(Request)); }
    }
    if (Requests.IsEmpty()) { TickHandle.Reset(); return false; }
    return true;
}

void USHSteamAvatarSubsystem::Deinitialize()
{
    bShuttingDown = true;
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    TickHandle.Reset();
    TArray<FRequest> Pending = MoveTemp(Requests);
    Requests.Reset();
    for (FRequest& Request : Pending) { Request.Completion(nullptr, TEXT("Avatar service is shutting down.")); }
    Cache.Reset();
    RequestedUsers.Reset();
    Super::Deinitialize();
}
