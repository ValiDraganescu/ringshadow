// `FAcPilotChase`: the third-person camera of a driven unit (GAME-LAYER.md
// §2.12, chunk E3), the port of `PilotCamera.chase` and `reach(of:)`
// (Sources/Autocraft/PilotCamera.swift).
//
// As shooters do it: the camera on a boom that swings round a pivot over the
// unit's right shoulder, turned by the mouse alone (`ChaseYaw` across the
// ground, `Pitch` up at the sky or down at its feet). Since 2026-10-18 the
// shoulder is nearly centred (`ac.Chase.Shoulder`, 0.12 cells at reach 1;
// 0.85 the old 0.4 + 0.45·reach) and W walks along the camera's forward
// (`AcPilotAim::ChaseWalk`). The pivot and the
// shoulder offset grow with how far the model reaches (`Reach`), the boom is
// `(2.6 + 3.3·reach) × FAcPilotCamera::Boom(kind).length` long. Two of E1's
// `Blocked` rays shorten it (pivot → camera, then camera → the unit's body),
// and the camera stays 0.3 cells over the ground. The sight stays in the
// middle of the view; the unit faces what that falls on
// (`AcPilotAim::Facing`), and its shots start at `Eye()` (the unit's own eye
// on its model), not at the camera.
//
// `AAcPilotPawn` owns one: V toggles `FAcPilotCamera::bThirdPerson` (kept
// for the next ride), and `Follow` puts the camera here instead of in the
// cockpit. No `USpringArmComponent`: its probe would need collision on the
// instanced units.
#pragma once

#include "CoreMinimal.h"

#include "Types.h"

struct FAcModelInfo;
class UWorld;

class AUTOCRAFT_API FAcPilotChase
{
public:
	/// The farthest a model's parts reach from its centre across the ground,
	/// in cells, its root's scale counted (`PilotCamera.reach(of:)`), from
	/// the pieces of the parts shown, posed as `Local` and `Visible` (empty:
	/// the rest pose); measured once per model, the first time.
	static float Reach(const FAcModelInfo& Model, TConstArrayView<bool> Visible = {}, TConstArrayView<FTransform> Local = {});
	/// `Reach` of the unit's model posed at ease (as Swift first meets it).
	static float AtEase(UWorld* World, const ac::Unit& U, const FAcModelInfo& Model);

	/// The wheel's zoom on the boom: the factor wanted (`Target`) and the
	/// one shown (`Shown`), eased toward it.
	struct AUTOCRAFT_API FZoom
	{
		static constexpr float Min = 0.45f;
		static constexpr float Max = 2.6f;
		/// Each notch scales the boom by this much (wheel down: out).
		static constexpr float Step = 1.12f;
		/// About this long from one notch's start to its end, real seconds.
		static constexpr double EaseSeconds = 0.15;
		/// The far end's tilt down past the mouse pitch, radians (25°).
		static constexpr double MaxTilt = 25.0 * UE_DOUBLE_PI / 180.0;
		/// While a building is held out the boom is at least this long (and
		/// tilted as far): the unit, nearly centred, would hide the ghost
		/// ahead of it.
		static constexpr float Placing = 1.8f;

		float Target = 1.f;
		/// The shown zoom eases to at least this (`Placing` while placing, else 0);
		/// `Target`, the player's, stays as it is.
		float Floor = 0.f;
		double Shown = 1.0;
		/// `Shown`'s real seconds (< 0: none yet, `Tick` snaps).
		double ShownAt = -1;

		/// `Notches` of the wheel: + in (the boom shorter), - out.
		void Notch(float Notches);
		/// Straight to `Zoom` (clamped): the setting at a new ride, a picture.
		void Set(float Zoom, bool bSnap);
		/// The factor this frame, eased toward `Target` (at least `Floor`).
		double Tick(double RealNow);
		/// How far the camera tilts down past the mouse pitch at `Zoom`
		/// (radians, >= 0): none up to 1, `MaxTilt` at `Max`, smoothly.
		static double Tilt(double Zoom);
	};
	FZoom Zoom;

	/// The right-drag look-around (docs/shooter-feel.md "Look around"):
	/// while the right button is held the mouse swings the camera round the
	/// unit by this much over the aim's yaw and pitch, and nothing else: the
	/// unit's heading, its aim and the crosshair's ray, and the walk's frame
	/// stay the aim's. Let go, it eases back behind the unit.
	struct AUTOCRAFT_API FOrbit
	{
		/// From let go to back behind the unit, real seconds.
		static constexpr double EaseSeconds = 0.25;
		/// The mouse's pitch counts this much of the look's...
		static constexpr double PitchShare = 0.5;
		/// ...and swings no farther than this up or down, radians (20°).
		static constexpr double MaxPitch = 20.0 * UE_DOUBLE_PI / 180.0;

		/// Off the aim's view now, radians (yaw as `heading`, pitch up).
		double Yaw = 0, Pitch = 0;
		bool bHeld = false;

		/// The right button held (or not) at `RealNow`; let go starts the
		/// ease back from where it is.
		void Hold(bool bDown, double RealNow);
		/// The mouse's turn while held, radians as the look's (`DPitch` up).
		void Turn(double DYaw, double DPitch);
		/// Eased back toward none after the release.
		void Tick(double RealNow);
		/// Swung off the aim (held, or on the way back).
		bool Off() const { return bHeld || Yaw != 0 || Pitch != 0; }
		void Reset() { *this = FOrbit(); }

	private:
		double ReleasedAt = -1, FromYaw = 0, FromPitch = 0;
	};
	FOrbit Orbit;

	/// A new ride or a switch of view: nothing eased yet (the zoom shown
	/// snaps to the wanted one, the look-around is let go at once).
	void Reset();

	/// The camera behind the unit (`chase`). `Root`: the model root's world
	/// (UE cm); `Model`: its model (null: a reach of 1); `Pitch`, `Yaw`: the
	/// view (radians; yaw as `heading`); `RealNow` real seconds. Out: the
	/// camera's location and rotation (UE: looking along +X).
	void Follow(UWorld* World, const FTransform& Root, const ac::Unit& U, const FAcModelInfo* Model, double Pitch, double Yaw,
		double RealNow, FVector& OutLocation, FQuat& OutRotation);

	/// The unit's eye in the world (cm) as of the last `Follow`: where its
	/// line of fire starts (`chaseEye`). Unset before the first.
	const TOptional<FVector>& Eye() const { return ChaseEye; }

private:
	TOptional<TPair<double, double>> PivotZ;
	TOptional<FVector> ChaseEye;
};
