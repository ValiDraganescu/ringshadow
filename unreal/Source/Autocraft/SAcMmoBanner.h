// `SAcMmoBanner`: the MMORPG test mode's lines at the top middle of the view
// (AcGameFlow): where the local server is ("Starting the local server…"),
// why it failed (red, with the server log's path; it stays), and the short
// refusals of what is off in this mode ("Orders are off in the MMORPG
// test"), each for a few seconds; under them, small, the round trip. On top,
// small, where the pilot is: the region's name (in amber, "Flying to …" while
// in flight). Dark glass in the HUD's chrome, hit-test invisible.
//
// Placed clear of the pilot's compass (docs/mmorpg-shots.md #4): the place
// and the connection's line in a small box at the view's top left; the
// status and the refusals centred under the compass and its marks. On
// arriving in a region (a landing, an edge crossing, the start) its title
// goes to the one banner slot (U1, `UAcLevelingSubsystem::ShowBanner`): the
// name, under it the style; `ShowTitle` paints it here only without one
// (0.5 s in, 2.5 s, 1 s out). The round trip shows only when it is high
// (`SetStats` with `bWarn`: amber).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class AUTOCRAFT_API SAcMmoBanner : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcMmoBanner) {}
	SLATE_END_ARGS()

	/// Seconds a refusal shows.
	static constexpr double NoteSeconds = 2.5;

	void Construct(const FArguments& InArgs);
	/// The standing line (empty: none); `bError` draws it red.
	void SetStatus(const FString& Text, bool bError);
	/// A refusal, shown from `Now` (real seconds) for `NoteSeconds`.
	void AddNote(const FString& Text, double Now);
	/// Drops the notes that are done.
	void Update(double Now);
	/// The connection's small line under the rest (the round trip, the lag
	/// set); empty: none.
	void SetStats(const FString& Text, bool bWarn = false);
	/// The region's title over the view from `Now` (real seconds): `Name`
	/// large, `Style` under it.
	void ShowTitle(const FString& Name, const FString& Style, double Now);
	/// The title is up at `Now`.
	bool TitleShown(double Now) const { return !TitleName.IsEmpty() && Now - TitleSince < TitleSeconds; }
	/// A title's whole run.
	static constexpr double TitleSeconds = 4.0;
	/// Where the pilot is (empty: none); `bFlying` draws it amber.
	void SetPlace(const FString& Text, bool bFlying);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(0, 0); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	struct FNote
	{
		FString Text;
		double Until = 0;
	};
	FString Status;
	bool bStatusError = false;
	FString Stats;
	bool bStatsWarn = false;
	FString Place;
	FString TitleName;
	FString TitleStyle;
	double TitleSince = -1e9;
	TSharedPtr<FActiveTimerHandle> TitleTimer;
	bool bPlaceFlying = false;
	TArray<FNote> Notes;

	EActiveTimerReturnType AnimateTitle(double CurrentTime, float DeltaTime);
	void PaintTitle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, double Now) const;
};
