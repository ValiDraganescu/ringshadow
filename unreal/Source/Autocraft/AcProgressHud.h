// The Prospector's progression on the HUD in the MMORPG mode
// (docs/prospector-progression.md, "The level-up" and "The HUD"):
//   - `SAcProgressHud`: the slot row (Q, C, G over the dashboard's middle:
//     the active's icon, a cooldown ring and its seconds; a closed slot its
//     level) and the toasts for a find ("BLUEPRINT FOUND: FOUNDRY");
//   - `UAcProgressHudSubsystem` feeds it each frame from the client's
//     progress (`FAcMmoClient::Progress`) and turns the server's news into
//     the level-up banner ("PROSPECTOR  LEVEL 12", "+1 SKILL POINT · K",
//     "TIER 2 OPEN"; `UAcLevelingSubsystem::ShowBanner`), the `levelup`
//     sound and the toasts. Out of the mode it stays hidden.
// The points badge by the XP bar is the console card's (`FAcLevelInfo::Badge`,
// `AcProgressHud::Level`).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Widgets/SLeafWidget.h"

#include "AcCab.h"
#include "AcConsoleInfo.h"
#include "Progression.h"
#include "SAcWorldMap.h"

#include <array>
#include <map>
#include <optional>

#include "AcProgressHud.generated.h"

class FAcMmoClient;
struct FAcFrame;

/// One of Q, C, G as the HUD shows it.
struct FAcSlotView
{
	std::optional<ac::Skill> Skill;
	/// Opens at this level (closed: the pilot is below it).
	int32 OpenLevel = 1;
	bool bOpen = false;
	/// Seconds until it is ready again, of `Cooldown`.
	double Left = 0;
	double Cooldown = 0;

	bool operator==(const FAcSlotView&) const = default;
};

class AUTOCRAFT_API SAcProgressHud : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcProgressHud) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/// The slots (none: the row hides) over the dashboard of `Layout`.
	void SetSlots(const TOptional<std::array<FAcSlotView, 3>>& Slots, const FAcCabLayout& Layout);
	/// A find: `Title` large in `Accent`, `Sub` under it; 4.5 s, newest on top.
	void Toast(const FString& Title, const FString& Sub, const FLinearColor& Accent, double Now);
	int32 Toasts() const { return Notes.Num(); }
	/// A site being opened (the act key held at a wreck, a cache, a vault):
	/// a ring round the reticle, `Share` of the way; or why holding does
	/// nothing there. Unset: none.
	struct FOpening
	{
		FString Title;
		double Share = 0;
		FString Refusal;
		bool operator==(const FOpening&) const = default;
	};
	void SetOpening(const TOptional<FOpening>& In);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	struct FNote
	{
		FString Title;
		FString Sub;
		FLinearColor Accent;
		double At = 0;
	};
	TOptional<std::array<FAcSlotView, 3>> Row;
	TOptional<FOpening> Opening;
	FAcCabLayout Lay;
	TArray<FNote> Notes;
	TSharedPtr<FActiveTimerHandle> Timer;

	EActiveTimerReturnType Animate(double Now, float Dt);
	void PaintSlots(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f Size) const;
	void PaintOpening(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f Size) const;
	void PaintToasts(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f Size, double Now) const;
};

struct AUTOCRAFT_API AcProgressHud
{
	/// The console card's level in the mode: the Prospector's level and XP
	/// (this level's XP to the next), and the badge "3 POINTS · K".
	static TOptional<FAcLevelInfo> Level(const FAcMmoClient& Mmo);
	/// The banner's lines for reaching `Level` from `Before`: "+1 SKILL POINT
	/// · K", and the gates crossed ("TIER 2 OPEN · SLOT Q OPEN").
	static FString BannerSub(int64 Before, int64 Level);
	static FString BannerGate(int64 Before, int64 Level);
	/// Why the build menu greys `Kind` in the mode (empty: it doesn't):
	/// "blueprint not found · level 15", "level 15". No progress yet: the
	/// world's first two (`net::kWorldBuildKinds`) only.
	static FString BlueprintRefusal(const FAcMmoClient* Mmo, ac::StructureKind Kind);
	/// That refusal in a word or two: "NO BLUEPRINT", "LV 15", "LIMIT".
	static FString BlueprintTag(const FAcMmoClient* Mmo, ac::StructureKind Kind);
	/// The pilot's progression as the server sent it (none: not told yet).
	static std::optional<ac::ProspectorProgress> Progress(const FAcMmoClient& Mmo);
	/// The sites the pilot knows of, for the world map: looked up by
	/// "<region id>/<site name>" in the world's region files (read once).
	static TArray<FAcWorldMapInfo::FSite> KnownSites(const FAcMmoClient& Mmo);
	/// For the skill screen's BLUEPRINTS: where each found blueprint came
	/// from ("Landing crate, Prospect Landing"), and the sites known, not
	/// opened, of each blueprint not found.
	static void BlueprintSites(const FAcMmoClient& Mmo, std::map<ac::StructureKind, FString>& Sources,
		std::map<ac::StructureKind, int32>& Marked);
};

UCLASS()
class AUTOCRAFT_API UAcProgressHudSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/// Over the console (20) and the pilot overlay (35), under the level banner (41).
	static constexpr int32 HudLayer = 38;

	static UAcProgressHudSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	TSharedPtr<SAcProgressHud> Widget() const { return Hud; }

private:
	void OnFrame(const FAcFrame& Frame);
	bool Mount();
	/// The server's news: the banner, the sound, the toasts.
	void News(FAcMmoClient& Mmo);

	TSharedPtr<SAcProgressHud> Hud;
	FDelegateHandle FrameHandle;
	/// The level last shown (-1: not told yet: the first progress sets it, no banner).
	int64 Shown = -1;
};
