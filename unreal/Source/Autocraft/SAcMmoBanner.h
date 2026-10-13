// `SAcMmoBanner`: the MMORPG test mode's lines at the top middle of the view
// (AcGameFlow): where the local server is ("Starting the local server…"),
// why it failed (red, with the server log's path; it stays), and the short
// refusals of what is off in this mode ("Orders are off in the MMORPG
// test"), each for a few seconds. Dark glass in the HUD's chrome, hit-test
// invisible.
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
	TArray<FNote> Notes;
};
