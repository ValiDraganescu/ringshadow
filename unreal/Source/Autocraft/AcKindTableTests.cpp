// Automation tests for the kind table (AcKindTable.h): every kind has what the
// game layer draws and plays for it. Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Kinds
#include "AcKindTable.h"
#include "AcConsoleInfo.h"
#include "AcEffectsDeaths.h"
#include "AcEffectsShatter.h"
#include "AcModelCatalog.h"
#include "AcPose.h"

#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// The sound names in `Content/Audio/Sounds.json`.
	TSet<FName> SoundNames()
	{
		TSet<FName> Out;
		FString Text;
		TSharedPtr<FJsonObject> Root;
		const TSharedPtr<FJsonObject>* Sounds = nullptr;
		if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Audio/Sounds.json")))
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root
			&& Root->TryGetObjectField(TEXT("sounds"), Sounds))
		{
			for (const auto& Pair : (*Sounds)->Values) Out.Add(FName(*Pair.Key));
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcKindsCompleteTest, "Autocraft.Kinds.Complete", Flags)
bool FAcKindsCompleteTest::RunTest(const FString&)
{
	const FAcModelCatalog& Catalog = FAcModelCatalog::Get();
	if (!Catalog.IsLoaded())
	{
		AddError(TEXT("no model catalog"));
		return false;
	}
	const TSet<FName> Sounds = SoundNames();
	if (Sounds.Num() == 0)
	{
		AddError(TEXT("no sound table (Content/Audio/Sounds.json)"));
		return false;
	}
	auto Miss = [this](const FAcKindLook& L, const TCHAR* Field, const FString& What)
	{
		AddError(FString::Printf(TEXT("%s: %s (%s)"), *L.Base, Field, *What));
	};
	auto HasSound = [&](const FAcKindLook& L, const TCHAR* Field, const FName Name)
	{
		if (!Sounds.Contains(Name)) Miss(L, Field, FString::Printf(TEXT("no sound %s in Sounds.json"), *Name.ToString()));
	};

	for (const ac::UnitKind K : ac::allCases<ac::UnitKind>())
	{
		const FAcKindLook& L = AcKinds::Look(K);
		const bool bBrood = AcKinds::IsBrood(K);
		if (L.Kind != K) Miss(L, TEXT("Kind"), TEXT("the row is out of allCases order"));
		if (L.Base != UTF8_TO_TCHAR(std::string(ac::rawValue(K)).c_str())) Miss(L, TEXT("Base"), TEXT("not the core's raw name"));
		if (!AcPose::Find(K)) Miss(L, TEXT("pose"), TEXT("no FAcPoseRegistration"));
		if (!Catalog.Find(FName(*L.Model))) Miss(L, TEXT("Model"), L.Model + TEXT(" not in the catalog"));

		// The cockpit: its own in the catalog, or a declared stand-in or none.
		switch (L.Cockpit)
		{
		case EAcKindCockpit::Own:
			if (!Catalog.Find(FName(*L.CockpitModel)) && !L.StandIn) Miss(L, TEXT("Cockpit"), L.CockpitModel + TEXT(" not in the catalog and no StandIn"));
			break;
		case EAcKindCockpit::Borrowed:
			if (!L.StandIn) Miss(L, TEXT("StandIn"), TEXT("borrowed, from no kind"));
			break;
		case EAcKindCockpit::None:
			if (!bBrood) Miss(L, TEXT("Cockpit"), TEXT("a player kind that cannot be driven"));
			break;
		}
		if (L.StandIn && !Catalog.Find(FName(*AcKinds::Look(*L.StandIn).CockpitModel)))
			Miss(L, TEXT("StandIn"), TEXT("the stand-in has no cockpit model"));

		// The icon, for the player's kinds.
		if (!bBrood)
		{
			const FString Asset = TEXT("T_Icon_") + L.Icon;
			if (L.Icon.IsEmpty() || !LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/Icons/%s.%s"), *Asset, *Asset), nullptr, LOAD_NoWarn))
				Miss(L, TEXT("Icon"), FString::Printf(TEXT("no texture /Game/UI/Icons/%s"), *Asset));
		}

		// The death: a wreck, a body that falls, or the kind's own pose.
		switch (L.Death)
		{
		case EAcKindDeath::Shatter:
		{
			AcEffectsShatter::FSpec Spec;
			if (!AcEffectsShatter::SpecOf(K, Spec) || !Spec.Model || !Catalog.Find(FName(Spec.Model))) Miss(L, TEXT("Death"), TEXT("shatters with no wreck spec"));
			break;
		}
		case EAcKindDeath::BodyFall:
			if (!L.Fall || !AcDeaths::FallOf(K)) Miss(L, TEXT("Fall"), TEXT("a body fall with no fall"));
			break;
		case EAcKindDeath::Pose:
			if (!bBrood) Miss(L, TEXT("Death"), TEXT("a player kind dying in its pose"));
			break;
		}

		// The weapon: muzzles, or none on purpose.
		if (L.Muzzles.Num() == 0 && !L.bNoMuzzle) Miss(L, TEXT("Muzzles"), TEXT("none, and bNoMuzzle not set"));
		if (L.Weapon.Name.IsNone()) Miss(L, TEXT("Weapon"), TEXT("no shot sound"));
		else HasSound(L, TEXT("Weapon"), L.Weapon.Name);
		if (!L.AltWeapon.Name.IsNone()) HasSound(L, TEXT("AltWeapon"), L.AltWeapon.Name);

		// The sounds.
		HasSound(L, TEXT("DeathSound"), L.DeathSound);
		if (bBrood)
		{
			if (!L.Voice.IsNone()) HasSound(L, TEXT("Voice"), L.Voice);
			HasSound(L, TEXT("Strike"), L.Strike);
			HasSound(L, TEXT("Hurt"), L.Hurt);
			if (!L.Idle.IsNone()) HasSound(L, TEXT("Idle"), L.Idle);
		}
		else
		{
			HasSound(L, TEXT("Voice"), L.Voice);
		}
		if (!L.MoveSound.IsNone()) HasSound(L, TEXT("MoveSound"), L.MoveSound);

		// Heights and the view.
		// A player flier hovers in its rest transform; the Gloamwing's pose lifts it itself.
		if (!bBrood && ac::Rules::stats(K).air && !(L.Hover > 0)) Miss(L, TEXT("Hover"), TEXT("a flier at the ground"));
		if (!(L.ChestHeight > 0)) Miss(L, TEXT("ChestHeight"), TEXT("not over its feet"));
		if (!(L.MinimapRadius > 0)) Miss(L, TEXT("MinimapRadius"), TEXT("no mark"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcKindsNamesTest, "Autocraft.Kinds.Names", Flags)
bool FAcKindsNamesTest::RunTest(const FString&)
{
	int32 Player = 0;
	for (const ac::UnitKind K : ac::allCases<ac::UnitKind>())
	{
		const FAcKindLook& L = AcKinds::Look(K);
		TestTrue(*FString::Printf(TEXT("%s is named back"), *L.Base), AcKinds::Named(L.Base) == TOptional<ac::UnitKind>(K));
		TestTrue(*FString::Printf(TEXT("%s's model base"), *L.Base), AcKinds::OfBase(FName(*L.Base)) == TOptional<ac::UnitKind>(K));
		TestEqual(TEXT("model"), L.Model, L.Base + TEXT("_blue"));
		TestEqual(TEXT("cockpit"), L.CockpitModel, TEXT("cockpit_") + L.Base + TEXT("_blue"));
		TestEqual(TEXT("ModelBase reads the row"), FString(AcPose::ModelBase(K)), L.Base);
		Player += AcKinds::IsBrood(K) ? 0 : 1;
	}
	TestEqual(TEXT("twelve player kinds"), Player, 12);
	TestFalse(TEXT("an unknown name"), AcKinds::Named(TEXT("ranger_blue")).IsSet());
	TestEqual(TEXT("a Ranger's icon"), FAcConsoleInfo::IconName((int32)ac::UnitKind::ranger), FString(TEXT("ranger")));
	TestEqual(TEXT("a bug has none"), FAcConsoleInfo::IconName((int32)ac::UnitKind::shardling), FString());
	TestEqual(TEXT("nor an index past the table"), FAcConsoleInfo::IconName(99), FString());
	return true;
}

#endif
