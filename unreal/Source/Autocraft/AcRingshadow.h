// `AAcRingshadow`: the ringshadow sweep on screen in the MMORPG mode
// (docs/weather.md, "Ringshadow sweep"; docs/mmorpg-m7.md E1). The band is
// `ac::Ringshadow::band` at the world's clock (`AAcDaylight::WorldSky`) over
// the region the pilot is in, the same function the server pays the
// drilling bonus from, so the two agree with nothing on the wire.
//
// Each frame:
// - MPC_AcWorld's `SweepDirX`, `SweepDirY`, `SweepLead`, `SweepTrail`,
//   `SweepSoft`: the band, for M_Opal (the veins flare under it) and for
//   M_Ringshadow (Tools/Editor/make_ringshadow_material.py), a post-process
//   pass on this actor's unbound component: the ground in the band goes to
//   the sky's cold light with soft edges, god rays fan from the sun as the
//   ring's edge crosses it, the air turns cold while the camera is in it;
// - MPC_AcWorld's `RingOn`, `RingOff`, `RingHalf`: the ring's arc on the
//   sky dome (M_AcSky, behind the clouds), translucent and banded, its edge
//   crossing the sun as the band's edge crosses the camera;
// - the sun dims while the camera is in the band (`AAcDaylight::RingshadowSun`);
// - 60 s before a sweep the KSTR radio's toast, "RINGSHADOW IN 60" (the
//   progression HUD's toast path; a voice line can come later).
//
// Usage: `AAcRingshadow::SpawnFor(World)` once per game world (AAcGameMode).
// Out of the mode it does nothing. Dev: -AcMmoRingshadow=S starts a sweep S
// seconds after the local server starts (its `--ringshadow`).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Ringshadow.h"

#include "AcRingshadow.generated.h"

class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UPostProcessComponent;

UCLASS()
class AUTOCRAFT_API AAcRingshadow : public AActor
{
	GENERATED_BODY()

public:
	AAcRingshadow();

	static AAcRingshadow* SpawnFor(UWorld* World);
	static AAcRingshadow* Find(const UWorld* World);

	virtual void Tick(float DeltaSeconds) override;

	/// How deep the camera is in the band now (0-1).
	double CameraCover() const { return CamCover; }
	/// The band over the pilot's region now (null: no sweep): the Brood's
	/// glow brightens under it (the renderer hands it to the poses).
	const ac::Ringshadow::Band* SweepBand() const { return SweepNow ? &*SweepNow : nullptr; }

private:
	void Off();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> WorldParameters;
	bool bLoaded = false;
	bool bOn = false;
	double CamCover = 0;
	TOptional<ac::Ringshadow::Band> SweepNow;
	/// The sweep (its start, world seconds) already announced, begun, ended.
	double Announced = -1, Begun = -1, Ended = -1;
};
