// `AAcBrood`: the Brood's raids told to the pilot in the MMORPG mode
// (docs/creatures.md, "Raids on buildings"). The server's director tells a
// raid 20 s before it reaches its building (`net::Sky::raids`); for a raid in
// the pilot's region or next to it this shows the banner (the one slot at
// the top, U1), "KSTR: BROOD SWARM" over "heading for the Hab Dome  ·  140 m NE", and plays
// the swarm (`broodswarm`) in the pilot's ear, once a raid.
//
// The Matriarch's roars (N3, `net::Sky::roars`): heard across her region and
// faintly next door, in sight or not (`matriarchroar`, quieter with
// distance), and the ground shakes for a camera within `RoarShakeCells`.
//
// Her slam and spit as effects (`UAcEffects::MatriarchSlam`, `MatriarchSpit`)
// on each strike the wire counts.
//
// The bugs themselves are drawn by the renderer (AcPoseBrood.cpp) and heard
// through the audio director (its Brood rules); nothing here.
//
// Usage: `AAcBrood::SpawnFor(World)` once per game world (AAcGameMode). Out
// of the mode it does nothing.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AcBrood.generated.h"

UCLASS()
class AUTOCRAFT_API AAcBrood : public AActor
{
	GENERATED_BODY()

public:
	AAcBrood();

	static AAcBrood* SpawnFor(UWorld* World);
	static AAcBrood* Find(const UWorld* World);

	virtual void Tick(float DeltaSeconds) override;

	/// The toast's two lines for a raid on `Building` (its name, "Hab Dome")
	/// `Metres` off at compass `Bearing` from the pilot, or in `Region` when
	/// that is another region.
	static FString Title();
	static FString Line(const FString& Building, double Metres, double Bearing, const FString& Region);

	/// The roar's gain at `Cells` from the pilot (0: another region next
	/// door), and how hard it shakes the view there (0 past its reach).
	static float RoarGain(double Cells, bool bNextDoor);
	static double RoarShake(double Cells);

private:
	/// The raids already told; the roars already heard.
	TSet<uint32> Told;
	TSet<uint32> Roared;
	/// The Matriarch's strikes last seen (N3): each new one is drawn, her
	/// slam's crash after its windup, her spit's glob and puddle.
	TMap<int64, int64> QueenStrikes;
};
