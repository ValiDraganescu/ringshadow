// `SAcScoreboard`: every player's army and the games won, on a plate left of
// the resource bar (`HUD.buildScoreboard`, Sources/Autocraft/HUD.swift:128,
// :407). Hidden while driving.
//
// Two sides (1v1, or two teams such as 4v4) read as Swift's
// [■ 24   1 : 0   18 ■]: the local player's side on the left, each member
// a chip in its colour and its army, the games won in the middle. Three or
// more teams (2v2v2v2, or a free-for-all) line up left to right, the local
// player's first: each side's members, then its games won, then a rule.
// A player with nothing left (no buildings, no units) is dimmed.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

namespace ac { struct GameState; }

class AUTOCRAFT_API SAcScoreboard : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcScoreboard) {}
	SLATE_END_ARGS()

	static constexpr float Height = 42.0f;
	/// Swift's width for two players; wider when more need it.
	static constexpr float MinWidth = 220.0f;

	void Construct(const FArguments& InArgs);
	void Update(const ac::GameState& State, int64 LocalPlayer);
	/// Count every unit, not only the army (the MMORPG test: the pilot's
	/// Prospectors are its units): "■ 1 UNITS", no games won.
	void SetCountAllUnits(bool bAll)
	{
		if (bAll == bAllUnits) return;
		bAllUnits = bAll;
		Key.Reset();
	}
	/// The MMORPG test: the local player's units away from the region (a
	/// Prospector in flight between pads), counted with the rest.
	void SetAway(int64 N)
	{
		if (N == Away) return;
		Away = N;
		Key.Reset();
	}
	/// The units word as drawn ("UNIT" for one, else "UNITS").
	const FString& UnitsWord() const { return Units; }
	/// The games won are shown (not in the MMORPG test).
	bool ShowsGamesWon() const
	{
		for (const FSide& S : Sides)
		{
			if (S.ScoreX >= 0) return true;
		}
		return !Middle.IsEmpty();
	}
	/// The count shown for a player (-1: not shown).
	int64 ShownCount(int64 Player) const;

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Width, Height); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	struct FMember
	{
		int64 Player = 0;
		int64 Army = 0;
		bool bOut = false;
		/// Chip centre and label anchor (x, points).
		float ChipX = 0;
		float LabelX = 0;
		bool bRightAligned = false;
	};
	struct FSide
	{
		int64 Team = 0;
		int64 Score = 0;
		TArray<FMember> Members;
		/// Where its score is drawn (>= 0), and the rule after it (> 0).
		float ScoreX = -1;
		float RuleX = 0;
	};
	void Layout();

	TArray<FSide> Sides;
	/// Two sides: the "a : b" in the middle.
	FString Middle;
	float Width = MinWidth;
	/// The MMORPG test: where "UNITS" is drawn (< 0: not).
	float UnitsX = -1;
	/// What the layout was made from, to skip it when nothing changed.
	FString Key;
	bool bAllUnits = false;
	int64 Away = 0;
	FString Units = TEXT("UNITS");
};
