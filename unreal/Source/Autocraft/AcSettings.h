// `UAcSettings` (chunk F1, GAME-LAYER.md §2.14): the game's settings, kept
// in `GameUserSettings.ini` (`[/Script/Autocraft.AcSettings]`), the Unreal
// side of Swift's UserDefaults (`windowMap`, `showPerf`, the view) and of the
// new-game choices. It is the engine's game user settings class
// (`DefaultEngine.ini`: `GameUserSettingsClassName`), so the window size,
// fullscreen and frame-rate cap are the base class's.
//
// What reads what:
//   LastMap, LastTeams      F1: the map the game opens on (Swift `windowMap`)
//                           and the new-game dialog's first choice.
//   bNewGameAI, bNewGameFog F1: the dialog's toggles for a new game.
//   bShowPerf               F1 applies it to `ac.PerfPanel` at start and
//                           saves the cvar back at quit (Swift `showPerf`).
//   bShowPerfMmo            the same for `ac.PerfPanelMmo`: the MMORPG
//                           mode's panel, off until a developer turns it on.
//   bThirdPerson            for E3 (Swift's remembered 1st/3rd view).
//   MasterVolume, bMusic, MusicVolume  for C7/C8 (Swift `--volume`, `--music`).
//   VoiceVolume             U2: the spoken words (KSTR's radio lines, the
//                           narrations), under the master volume.
//   SfxVolume, AmbientVolume  the menu's Settings page: the effects (shots,
//                           tools, the units' report-ins) and the world
//                           around (wind, hum).
//   LookSpeed               the menu's Settings page: the driven unit's mouse turn.
//   bMmoLag                 U2: the lag simulator on (two_clients.py's
//                           preset on `ac.MmoLag`), for testing.
//   FullscreenMode, ResolutionSizeX/Y  the base class's: the Settings page's
//                           SCREEN and RESOLUTION, applied on OK.
//
// A first run (no file): full screen at the display's own resolution (the
// base class's WindowedFullscreen at 0 x 0), the volumes 80 music, 100
// voice and effects (ac::PlayerSettings), the lag off.
//
// Agents' headless runs (`-unattended`) neither read nor write `LastMap`
// (so one test cannot change the map of the next), unless `-AcRemember`.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "PlayerSettings.h"

#include "AcSettings.generated.h"

UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class AUTOCRAFT_API UAcSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/// The settings (the engine's game user settings object when it is this
	/// class, else one of our own, loaded once).
	static UAcSettings& Get();
	/// Write them to `GameUserSettings.ini`.
	static void Store();
	/// False in agents' headless runs (see the header comment).
	static bool Remembers();

	/// U2: the resolutions the Settings page offers (ac::ScreenChoices::list
	/// over the display's modes), smallest first: full screen up to the
	/// display's own (always offered), a window those smaller than it.
	static TArray<FIntPoint> Resolutions(bool bFullScreen = true);
	static FIntPoint Desktop();
	/// Applies SCREEN and RESOLUTION (OK on the Settings page). A hidden run
	/// (-unattended) never takes the display: it resizes its window and only
	/// logs a full screen.
	static void ApplyScreen();
	/// The lag simulator's switch onto `ac.MmoLag` (a -AcMmoLag on the
	/// command line wins).
	static void ApplyLag();

	/// The last map played, as `-AcMap` takes it (`badlands-large-8`); empty: none yet.
	UPROPERTY(config)
	FString LastMap;
	/// Its teams (`4v4`, `2v2v2v2`, `ffa`).
	UPROPERTY(config)
	FString LastTeams;
	UPROPERTY(config)
	bool bNewGameAI = true;
	UPROPERTY(config)
	bool bNewGameFog = true;
	UPROPERTY(config)
	bool bShowPerf = true;
	UPROPERTY(config)
	bool bShowPerfMmo = false;
	UPROPERTY(config)
	bool bThirdPerson = false;
	UPROPERTY(config)
	float MasterVolume = float(ac::PlayerSettings::defaultMaster);
	UPROPERTY(config)
	bool bMusic = true;
	UPROPERTY(config)
	float MusicVolume = float(ac::PlayerSettings::defaultMusic);
	UPROPERTY(config)
	float VoiceVolume = float(ac::PlayerSettings::defaultVoice);
	UPROPERTY(config)
	float SfxVolume = float(ac::PlayerSettings::defaultEffects);
	UPROPERTY(config)
	float AmbientVolume = float(ac::PlayerSettings::defaultEnvironment);
	/// Times the base mouse turn while driving (0.25-3).
	UPROPERTY(config)
	float LookSpeed = 1.0f;
	UPROPERTY(config)
	bool bMmoLag = false;
};
