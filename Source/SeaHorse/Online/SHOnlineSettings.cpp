#include "Online/SHOnlineSettings.h"
#include "Misc/PackageName.h"

TSoftObjectPtr<UWorld> USHOnlineSettings::GetMatchMap(ESHMatchMap Map) const
{
	switch (Map)
	{
	case ESHMatchMap::Small: return MatchMap;
	case ESHMatchMap::Medium: return MediumMatchMap;
	default: return nullptr;
	}
}

int32 USHOnlineSettings::GetMapSeatCount(ESHMatchMap Map)
{
	switch (Map)
	{
	case ESHMatchMap::Small: return 4;
	case ESHMatchMap::Medium: return 6;
	default: return 0;
	}
}

bool USHOnlineSettings::BuildMatchURL(ESHMatchMap Map, int32 PlayerCount, FString& URL, FString& Error) const
{
	URL.Reset();
	Error.Reset();
	if (PlayerCount < 2 || PlayerCount > GetMapSeatCount(Map))
	{
		Error = TEXT("The selected map cannot accommodate this player count.");
		return false;
	}
	const TSoftObjectPtr<UWorld> World = GetMatchMap(Map);
	if (World.IsNull() || !FPackageName::DoesPackageExist(World.GetLongPackageName()))
	{
		Error = TEXT("The selected match map is missing. Check SeaHorse Multiplayer project settings and packaged maps.");
		return false;
	}
	URL = World.GetLongPackageName() + FString::Printf(TEXT("?SHExpectedPlayers=%d"), PlayerCount);
	return true;
}
