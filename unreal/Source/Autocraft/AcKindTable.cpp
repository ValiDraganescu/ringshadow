#include "AcKindTable.h"

#include "AcAudioDirector.h"
#include "AcEffectsDeaths.h"
#include "AcModelCatalog.h"
#include "AcPoseAir.h"
#include "AcPoseNewKinds.h"

#include "Rules.h"
#include "Types.h"

#include <array>

namespace
{
	using K = ac::UnitKind;
	constexpr size_t KindCount = ac::EnumInfo<ac::UnitKind>::names.size();

	FString Raw(const ac::UnitKind Kind)
	{
		const std::string_view S = ac::rawValue(Kind);
		return FString(int32(S.size()), UTF8_TO_TCHAR(std::string(S).c_str()));
	}

	/// A row with the derived names and the defaults.
	FAcKindLook Derived(const ac::UnitKind Kind)
	{
		FAcKindLook L;
		L.Kind = Kind;
		L.Base = Raw(Kind);
		L.Model = L.Base + TEXT("_blue");
		L.CockpitModel = TEXT("cockpit_") + L.Base + TEXT("_blue");
		L.Icon = L.Base;
		L.DeathSound = FName(*(L.Base + TEXT("death")));
		L.Voice = FName(*(TEXT("v") + L.Base));
		L.Weapon = {"gun", 0.32f, 9};
		// A flyer's hull, over its root at its hover.
		if (ac::Rules::stats(Kind).air) L.BodyPart = TEXT("body");
		return L;
	}

	/// The kind's engine, `<base>move` unless named.
	void Engine(FAcKindLook& L, const float Gain, const FName Name = NAME_None)
	{
		L.MoveSound = Name.IsNone() ? FName(*(L.Base + TEXT("move"))) : Name;
		L.MoveGain = Gain;
	}

	void Wreck(FAcKindLook& L, const int32 Chunks, const double Size, const int32 Ring, TArray<FString> Strip = {})
	{
		L.Death = EAcKindDeath::Shatter;
		L.Shatter.Chunks = Chunks;
		L.Shatter.Size = Size;
		L.Shatter.Ring = Ring;
		L.Shatter.Strip = MoveTemp(Strip);
	}

	void Falls(FAcKindLook& L, const AcDeaths::EFall Fall)
	{
		L.Death = EAcKindDeath::BodyFall;
		L.Fall = Fall;
	}

	void Minimap(FAcKindLook& L, const double Radius, const EAcKindMinimap Shape = EAcKindMinimap::Disc, const uint8 Z = 1)
	{
		L.MinimapRadius = Radius;
		L.MinimapShape = Shape;
		L.MinimapZ = Z;
	}

	void Row(FAcKindLook& L)
	{
		switch (L.Kind)
		{
		case K::prospector:
			L.Weapon = {"weld", 0.3f, 6};
			L.bNoMuzzle = true;
			L.Eye = FVector(0.0, 1.2, 0);
			L.bCab = true;
			L.Rig = EAcKindCockpitRig::Fork;
			Wreck(L, 7, 0.7, 16);
			break;
		case K::ranger:
			L.Muzzles = {TEXT("flash")};
			L.UpgradedMuzzles = {TEXT("minigun_flash")};
			// Behind the Ranger's visor.
			L.Eye = FVector(0.14, 0.92, 0);
			Minimap(L, 1.4);
			Falls(L, AcDeaths::EFall::Ranger);
			break;
		case K::comet:
			L.Weapon = {"pistol", 0.3f, 8};
			L.Muzzles = {TEXT("flashes_0"), TEXT("flashes_1")};
			// Behind the mask's eyes: as tall as the Comets it runs with.
			L.Eye = FVector(0.12, 0.93, 0);
			L.Rig = EAcKindCockpitRig::Held;
			L.HeldNode = TEXT("guns_%d_node");
			L.HeldMuzzle = TEXT("flashes_%d");
			L.HeldAxis = FVector(0, -1, 0);
			Falls(L, AcDeaths::EFall::Comet);
			L.Burst = EAcKindBurst::CometJets;
			break;
		case K::firefly:
			L.Weapon = {"flame", 0.4f, 5};
			L.Muzzles = {TEXT("nozzle")};
			Engine(L, AcSound::FireflyEngineGain);
			// In the driver's seat inside the canopy, the stinger over it; the
			// driver stays in the seat while the tail aims.
			L.Eye = FVector(0.12, 0.66, 0);
			L.Pivot = L.Eye;
			L.StartPitch = -0.18;
			L.bCab = true;
			L.Rig = EAcKindCockpitRig::UnitModel;
			Minimap(L, 2.0);
			// `strip: [h.flame, h.flare, h.pilot]`.
			Wreck(L, 10, 1, 12, {TEXT("flame"), TEXT("flare"), TEXT("pilot")});
			L.Burst = EAcKindBurst::FireflyFire;
			break;
		case K::juggernaut:
			L.Weapon = {"breacher", 0.4f, 6};
			L.Muzzles = {TEXT("muzzles_0"), TEXT("muzzles_1")};
			L.ChestHeight = 0.85;
			// Behind the helmet's visor.
			L.Eye = FVector(0.12, 1.3, 0);
			L.Rig = EAcKindCockpitRig::Held;
			L.HeldNode = TEXT("launchers_%d_node");
			L.HeldMuzzle = TEXT("muzzles_%d");
			L.HeldAxis = FVector(1, 0, 0);
			Minimap(L, 2.0);
			Falls(L, AcDeaths::EFall::Juggernaut);
			L.Burst = EAcKindBurst::MuzzleSmoke;
			break;
		case K::dropship:
			L.Muzzles = {TEXT("beamOrigin")};
			Engine(L, AcSound::DropshipEngineGain);
			L.Hover = AcPoseAir::DropshipHover;
			// In the nose, behind the canopy, at the hover.
			L.Eye = FVector(0.7, AcPoseAir::DropshipHover, 0);
			L.StartPitch = -0.55;
			Minimap(L, 2.6, EAcKindMinimap::Diamond, 2);
			// `strip: [m.beam]` + its ground shadow.
			Wreck(L, 12, 1.2, 6, {TEXT("beam"), TEXT("shadow")});
			L.Shatter.bFlyer = true;
			L.Burst = EAcKindBurst::Thrusters;
			L.BurstSpeed[0] = 2.4, L.BurstSpeed[1] = 1.3;
			L.BurstRate[0] = 80, L.BurstRate[1] = 32;
			break;
		case K::longbow:
			L.Weapon = {"cannon", AcSound::TankCannonGain, 5, true};
			L.AltWeapon = {"anchorshot", 0.55f, 4, true};
			// The Rhino Longbow's two prongs share one muzzle (`[muzzle, muzzle]`).
			L.Muzzles = {TEXT("tankMuzzles_0"), TEXT("tankMuzzles_0"), TEXT("anchorMuzzle")};
			// Up in the turret hatch, over the rail's breech.
			L.Eye = FVector(-0.1, 1.3, 0);
			L.Pivot = FVector(-0.22, 0.78, 0);
			L.StartPitch = -0.12;
			L.bCab = true;
			L.Rig = EAcKindCockpitRig::UnitModel;
			Minimap(L, 2.5);
			// `strip: t.flashes + [t.smoke]`.
			Wreck(L, 14, 1.35, 10, {TEXT("flashes_"), TEXT("smoke")});
			break;
		case K::kestrel:
			L.Weapon = {"rockets", AcSound::RocketGain, 6, true};
			// Its rail gun at a flier.
			L.AltWeapon = {"anchorshot", 0.45f, 3, true};
			L.Muzzles = {TEXT("muzzles_0"), TEXT("muzzles_1")};
			Engine(L, AcSound::KestrelEngineGain);
			L.Hover = AcPoseAir::KestrelHover;
			// In the front seat, under the canopy, at the hover.
			L.Eye = FVector(0.5, AcPoseAir::KestrelHover + 0.2, 0);
			L.StartPitch = -0.55;
			Minimap(L, 2.2, EAcKindMinimap::Diamond, 2);
			Wreck(L, 10, 1.0, 8, {TEXT("flashes_"), TEXT("shadow")});
			L.Shatter.bFlyer = true;
			L.Burst = EAcKindBurst::Thrusters;
			L.BurstSpeed[0] = 2.2, L.BurstSpeed[1] = 1.1;
			L.BurstRate[0] = 70, L.BurstRate[1] = 26;
			break;
		case K::hailstorm:
			L.Weapon = {"flak", AcSound::FlakGain, 6, true};
			L.Muzzles = {TEXT("muzzles_0"), TEXT("muzzles_1"), TEXT("muzzles_2"), TEXT("muzzles_3")};
			Engine(L, AcSound::HailstormEngineGain);
			// Up on the flak mount, behind the cradle; the mount's centre on the bed.
			L.Eye = FVector(-0.35, 1.15, 0);
			L.Pivot = FVector(-0.3, 0.52, 0);
			L.StartPitch = 0.1;
			L.BoomLift = 0.1f;
			L.BoomLength = 1.5f;
			L.bCab = true;
			L.Rig = EAcKindCockpitRig::UnitModel;
			Minimap(L, 2.0);
			Wreck(L, 12, 1.2, 10, {TEXT("flashes_")});
			break;
		case K::peregrine:
			L.Weapon = {"seekers", AcSound::RocketGain, 6, true};
			// The four rails (a volley leaves a pair of them).
			L.Muzzles = {TEXT("missiles_0"), TEXT("missiles_1"), TEXT("missiles_2"), TEXT("missiles_3")};
			L.bMuzzleReach = true;
			// The Dart-like shriek (docs/new-units.md, Peregrine sound).
			Engine(L, AcSound::KestrelEngineGain, "peregrinepass");
			L.Hover = AcPoseNew::PeregrineHover;
			// In the front of the Peregrine's bubble canopy (it spans x -14..42 cm), the eye
			// 21 cm over the fuselage axis: the needle nose runs ahead down the middle.
			L.Eye = FVector(0.25, AcPoseNew::PeregrineHover + 0.21, 0);
			L.StartPitch = -0.55;
			L.StandIn = K::kestrel;
			L.Rig = EAcKindCockpitRig::UnitModel;
			Minimap(L, 1.8, EAcKindMinimap::Diamond, 2);
			// A light dart: shot down whole, then it bursts.
			Wreck(L, 9, 0.9, 8);
			L.Shatter.bFlyer = true;
			break;
		case K::atlas:
			L.Weapon = {"atlasshot", AcSound::AtlasShotGain, 3, true};
			L.Muzzles = {TEXT("cannons_0"), TEXT("cannons_1")};
			L.bMuzzleReach = true;
			Engine(L, AcSound::AtlasEngineGain);
			// In the cockpit slit, in the front of the torso; the torso turns
			// about its own axis over the legs; it looks nearly level: its
			// barrels run along the upper edge of the slit.
			L.Eye = FVector(0.55, 2.58, 0);
			L.Pivot = FVector(0.0, 1.82, 0);
			L.StartPitch = -0.08;
			L.StandIn = K::juggernaut;
			L.Rig = EAcKindCockpitRig::UnitModel;
			Minimap(L, 3.2);
			// Kneels, topples, then bursts: the biggest wreck of the units.
			Wreck(L, 18, 2.0, 4);
			L.Shatter.bTopple = true;
			break;
		case K::scorpion:
			L.Weapon = {"scorpionsting", AcSound::ScorpionStingGain, 3};
			L.Muzzles = {TEXT("launcher")};
			L.bMuzzleReach = true;
			Engine(L, AcSound::ScorpionEngineGain);
			// Low over the shoulders, behind its head: the pincers at the bottom
			// edges, the tail's launcher hung over the top of the view.
			L.Eye = FVector(0.52, 0.64, 0);
			L.StandIn = K::firefly;
			L.Rig = EAcKindCockpitRig::UnitModel;
			Minimap(L, 1.6);
			// `strip: [mound]`: the heap of soil it buried in is not a wreck.
			Wreck(L, 8, 0.7, 12, {TEXT("mound")});
			L.bOutlineBuried = true;
			break;
		case K::mule:
			// C4 (docs/haul-and-ferry.md): the hauler, driven standing at the
			// levers on its perch. No gun: the table's shot is the weld's, never
			// played. Its driver's voice is the Prospector's (vmule, muledeath).
			L.Weapon = {"weld", 0.3f, 6};
			L.bNoMuzzle = true;
			Engine(L, AcSound::MuleEngineGain);
			L.ChestHeight = 0.9;
			// The driver's eyes on the perch (models/mule.py DRIVER_X 0.74, the
			// deck 0.35, a Prospector's eyes 1.2 over its feet), a little back
			// and looking down, so the levers, the rail and the nose are at the
			// bottom of the view.
			L.Eye = FVector(0.45, 1.55, 0);
			L.StartPitch = -0.45;
			// Third person: the boom over the hopper, shorter than the reach
			// of its 3 cells would make it (the Mule drawn as big as a Firefly).
			L.BoomLift = 0.3f;
			L.BoomLength = 0.7f;
			L.Rig = EAcKindCockpitRig::UnitModel;
			Minimap(L, 2.2);
			// The load and the readouts are not a wreck.
			Wreck(L, 12, 1.1, 14, {TEXT("heap"), TEXT("roomDigits_"), TEXT("roomBar_"), TEXT("oreDigits_"), TEXT("mhDigits_")});
			L.Burst = EAcKindBurst::MuleDust;
			break;
		// The Brood (docs/creatures.md): no icon, no voice, no cockpit; they
		// bite (no muzzle) and die in their own poses.
		case K::shardling:
		case K::gloamwing:
		case K::lodeback:
		case K::matriarch:
			L.Icon.Reset();
			L.Voice = NAME_None;
			L.Cockpit = EAcKindCockpit::None;
			L.bNoMuzzle = true;
			L.Death = EAcKindDeath::Pose;
			L.Strike = FName(*(L.Base + TEXT("bite")));
			L.Hurt = FName(*(L.Base + TEXT("hurt")));
			L.Idle = FName(*(L.Base + TEXT("idle")));
			// The Gloamwing's wings and the Matriarch's breath loop.
			if (L.Kind == K::gloamwing) L.Strike = "gloamwingspit", L.Idle = NAME_None, L.BodyPart = TEXT("thorax");
			if (L.Kind == K::matriarch) L.Strike = "matriarchslam", L.Idle = NAME_None;
			break;
		}
	}

	const std::array<FAcKindLook, KindCount>& Table()
	{
		static const std::array<FAcKindLook, KindCount> T = []
		{
			std::array<FAcKindLook, KindCount> Out;
			for (const ac::UnitKind Kind : ac::allCases<ac::UnitKind>())
			{
				FAcKindLook& L = Out[size_t(Kind)];
				L = Derived(Kind);
				Row(L);
			}
			return Out;
		}();
		return T;
	}
}

namespace AcKinds
{
	const FAcKindLook& Look(const ac::UnitKind Kind) { return Table()[size_t(Kind)]; }

	bool IsBrood(const ac::UnitKind Kind)
	{
		// The core's kind table says (docs/kind-registry.md): the enum's
		// order means nothing here.
		return ac::Rules::stats(Kind).brood;
	}

	TOptional<ac::UnitKind> Named(const FString& Name)
	{
		const std::optional<ac::UnitKind> K = ac::parse<ac::UnitKind>(TCHAR_TO_UTF8(*Name));
		return K ? TOptional<ac::UnitKind>(*K) : TOptional<ac::UnitKind>();
	}

	TOptional<ac::UnitKind> OfBase(const FName Base)
	{
		static const TMap<FName, ac::UnitKind> Bases = []
		{
			TMap<FName, ac::UnitKind> M;
			for (const ac::UnitKind Kind : ac::allCases<ac::UnitKind>()) M.Add(FName(*Look(Kind).Base), Kind);
			return M;
		}();
		const ac::UnitKind* Found = Bases.Find(Base);
		return Found ? TOptional<ac::UnitKind>(*Found) : TOptional<ac::UnitKind>();
	}

	bool HasCockpit(const ac::UnitKind Kind)
	{
		const FAcKindLook& L = Look(Kind);
		if (L.Cockpit == EAcKindCockpit::Borrowed) return false;
		if (L.Cockpit == EAcKindCockpit::None || !L.StandIn) return true;
		// Asked every frame of a drive: once the catalog is read, the answer
		// does not change.
		static std::array<int8, KindCount> Known = []
		{
			std::array<int8, KindCount> A;
			A.fill(-1);
			return A;
		}();
		int8& Has = Known[size_t(Kind)];
		const FAcModelCatalog& Catalog = FAcModelCatalog::Get();
		if (Has < 0 && Catalog.IsLoaded()) Has = Catalog.Find(FName(*L.CockpitModel)) ? 1 : 0;
		return Has > 0;
	}

	ac::UnitKind CockpitKind(const ac::UnitKind Kind)
	{
		const FAcKindLook& L = Look(Kind);
		return L.StandIn && !HasCockpit(Kind) ? *L.StandIn : Kind;
	}
}
