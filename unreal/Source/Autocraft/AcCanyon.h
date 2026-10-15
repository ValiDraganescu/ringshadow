// The canyon game's words on the client (Z2, docs/canyon.md): each canyon
// zone of the world (read once from its region file: name, prize, tunnels)
// with its game as the sky carries it (`Sky::zones`): the wave panel's
// lines, the warnings' tunnels, the map's item and the Journal's line.
#pragma once

#include "CoreMinimal.h"

#include "NetProtocol.h"
#include "WorldAtlas.h"

#include <vector>

class FAcMmoClient;

/// A canyon zone as its region file has it.
struct FAcZonePlace
{
	/// The region's id and name, the zone's index in its `zones` and name.
	FString Region;
	FString RegionName;
	int32 Index = 0;
	FString Name;
	/// The zone's id ("the-fork"; A2's radio lines are keyed by it).
	FString Id;
	/// The prize's middle and the zone's reach round it (its outline's
	/// farthest corner), region cells.
	ac::Vec2 Middle{0, 0};
	double Reach = 0;
	/// Its entrances' names, by index ("North tunnel").
	TArray<FString> Entrances;
	/// Z3: its flooded floor (region cells; empty: none), and its roosts'
	/// places (region cells).
	TArray<ac::Vec2> Flood;
	TArray<ac::Vec2> Roosts;
	/// Z3: which entrances are roosts (by index, as `Entrances`).
	TArray<bool> Roost;
};

struct AUTOCRAFT_API AcCanyon
{
	/// Every canyon zone of the world (read once a world file).
	static const TArray<FAcZonePlace>& Places(const FAcMmoClient& Mmo);
	/// A2: a zone's id by its name ("The Fork" → "the-fork"; empty: none).
	static FString IdOf(const FAcMmoClient& Mmo, const FString& Name);
	/// The place of a zone of the sky (nil: not known).
	static const FAcZonePlace* Place(const FAcMmoClient& Mmo, const ac::net::Sky::Zone& Z);
	/// The zone in the pilot's region, else next to it (nil: none near).
	static const ac::net::Sky::Zone* Near(const FAcMmoClient& Mmo);
	/// "WAVE 3 / 10", "WAVE 1 · THE OPENING WAVE", "FALLEN", "HELD".
	static FString Title(const ac::net::Sky::Zone& Z);
	/// What it is doing now, against the world's clock `Now`: "7 bugs left
	/// · 120 / 300 ore taken", "Next wave in 0:24", "Wave 1 comes again in
	/// 2:31", "The Matriarch comes for the turrets", "Held · waves again in
	/// 31:20", "Waiting for a pilot".
	static FString Line(const ac::net::Sky::Zone& Z, double Now);
	/// "the North tunnel", "the North and East tunnels".
	static FString Tunnels(const ac::net::Sky::Zone& Z, const FAcZonePlace* Place);
	/// The zones on the world map.
	static std::vector<ac::net::atlas::ZoneMark> Marks(const FAcMmoClient& Mmo);
};
