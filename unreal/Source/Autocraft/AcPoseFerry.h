// The walk-in Dropship's pose (C4, docs/haul-and-ferry.md section 4; the
// model is Tools/Editor/models/ferry.py, `ferry_blue`): the ramp turning on
// its hinge, the ramp post's button and its two lamps, the green take-off
// button, the console screens, the wipers on the windshield, the engines.
// Plain over the model's parts: `AAcShuttleShip` (AcShuttleShip.h) fills the
// look from the sim's shuttle each frame and draws the result.
#pragma once

#include "CoreMinimal.h"

struct FAcModelInfo;
struct FAcPose;

/// What the pose needs, all presentation.
struct FAcFerryLook
{
	/// The ship's clock (s): blinks, spins.
	double Time = 0;
	/// The ramp: 0 shut (standing in the opening) to 1 down on the ground.
	double RampOpen = 0;
	/// Which way it moves: 1 opening, -1 closing, 0 still.
	int32 RampWay = 0;
	/// Seconds since the ramp button and the take-off button were last
	/// pressed (large: long ago).
	double RampPressAge = 1e9;
	double TakeoffPressAge = 1e9;
	/// The take-off button is lit: the ship can take off (landed, the ramp shut).
	bool bCanTakeOff = false;
	/// Someone is aboard: the console's screens lit.
	bool bAboard = false;
	/// The wipers' angle on `ac::Wipers`'s arc (radians from straight up,
	/// positive to the right; `ParkAngle` parked).
	double WiperAngle = -UE_HALF_PI;
	/// The engines' thrust (0 idle on the pad, 1 full).
	double Thrust = 0;
};

namespace AcPoseFerry
{
	/// The model's catalog name.
	inline const TCHAR* Model = TEXT("ferry_blue");

	/// The ramp's hinge (the deck's back edge) and its open angle: it lies
	/// from the deck (0.5 up) down to the ground 2.0 away, 90° + asin(0.25).
	inline constexpr double RampOpenDeg = 104.47751218592992;
	/// A button's press: in for `PressIn`, back out by `PressOut` (s).
	inline constexpr double PressIn = 0.08;
	inline constexpr double PressOut = 0.25;
	/// How far each button goes in (cm).
	inline constexpr double RampButtonTravel = 1.5;
	inline constexpr double TakeoffButtonTravel = 2.0;
	/// The wipers' arc on the windshield (degrees from straight up along the
	/// glass, positive to the ship's right): parked lying left along the
	/// sill, out to the far stop. `ac::Wipers`'s arc (-90 to 90) maps onto it.
	inline constexpr double WiperParkDeg = -78.0;
	inline constexpr double WiperFarDeg = 62.0;

	/// How far in a button is `Age` seconds after its press (0 out, 1 in).
	AUTOCRAFT_API double Pressed(double Age);
	/// The wipers' angle on the glass (radians) for `ac::Wipers`'s `Angle`.
	AUTOCRAFT_API double WiperOnGlass(double Angle);

	/// Poses `M` (the `ferry_blue` model) from `In`. The pose is reset by
	/// the caller (`FAcPose::Reset`).
	AUTOCRAFT_API void Pose(const FAcModelInfo& M, FAcPose& P, const FAcFerryLook& In);
}
