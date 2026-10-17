// `FAcGaitTable`: a motion cycle measured from a video, as a table over one
// cycle (Content/Models/Gaits/<name>.json, written by the mocap tools of
// the ringshadow-model-pipeline skill): named channels of `bins` samples
// each, radians or cells (a walk: hip swing, knee bend and the sole's pitch;
// a drill stroke: the hull's lean, the arm's push, the drill's pitch), and
// `period_s`, the cycle's length when the motion runs on the clock rather
// than on a stride. A pose function samples it by phase in place of its
// procedural sines; a missing file leaves the sines in charge.
#pragma once

#include "CoreMinimal.h"

struct AUTOCRAFT_API FAcGaitTable
{
	/// Every channel of the file's `table`, by name.
	TMap<FName, TArray<double>> Channels;
	/// The walk channels, when the table has them (empty otherwise).
	TArray<double> Hip, Knee, Foot;
	/// `period_s`: seconds per cycle for a clocked motion (0: none given).
	double PeriodS = 0.0;

	/// The table `Content/Models/Gaits/<Name>.json`, read once; null when
	/// there is none (or it does not parse).
	static const FAcGaitTable* Find(FName Name);

	/// A channel at `Phase01` (0..1 over the stride, wrapping), linear
	/// between bins.
	static double Sample(const TArray<double>& Channel, double Phase01);
	double HipAt(const double P) const { return Sample(Hip, P); }
	double KneeAt(const double P) const { return Sample(Knee, P); }
	double FootAt(const double P) const { return Sample(Foot, P); }
	/// A named channel at `Phase01`; 0 when the table has no such channel.
	double At(FName Channel, double Phase01) const;
	bool Has(const FName Channel) const { return Channels.Contains(Channel); }

	static FString PathFor(FName Name);
	bool Load(const FString& Path);
};
