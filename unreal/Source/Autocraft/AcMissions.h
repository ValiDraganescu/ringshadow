// The Citadel missions' words on the client (K2, docs/missions.md): a
// mission's kind and tier as labels, its reward and progress as lines, the
// region it points at by name. The Citadel's MISSIONS tab and the Journal
// (L) show the same words; the server writes the title, the briefing and
// the instructions.
#pragma once

#include "CoreMinimal.h"

#include "NetProtocol.h"

class FAcMmoClient;

struct AUTOCRAFT_API AcMissions
{
	/// "HAUL", "SURVEY", "SALVAGE", "HOLD", "HUNT".
	static FString KindName(ac::net::CitadelMission::Kind Kind);
	/// "EASY", "HARD", "PERILOUS".
	static FString TierName(ac::net::CitadelMission::Tier Tier);
	/// A tier's colour: easy green, hard amber, perilous red (the Citadel's
	/// board, the Journal, the markers).
	static FLinearColor TierColor(ac::net::CitadelMission::Tier Tier);
	/// "1,500 XP · 450 ore · 40 MH · Hab Dome blueprint ×1".
	static FString Reward(const ac::net::CitadelMission& M);
	/// "120 / 300 ore", "8 / 15 s in the ring", "2 / 6 bugs", "the site
	/// unopened"; done: "DONE".
	static FString Progress(const ac::net::CitadelMission& M);
	/// The share done, 0…1.
	static float Share(const ac::net::CitadelMission& M);
	/// The region's name from the World message ("Rust Hollow"; the id when
	/// not known; empty: none).
	static FString RegionName(const FAcMmoClient& Mmo, const std::string& Id);
	/// The way along ground offset `D` in words ("north-east"; north is -z).
	static FString Heading(ac::Vec2 D);
	/// The pilot's taken missions with a place on the map (its markers).
	static TArray<const ac::net::CitadelMission*> Marked(const FAcMmoClient& Mmo);
	/// Those in the pilot's region with a point (not a hunt), for the
	/// compass: their place and distance from `From`.
	static TArray<struct FAcPilotGoal> Goals(const FAcMmoClient& Mmo, ac::Vec2 From);
};
