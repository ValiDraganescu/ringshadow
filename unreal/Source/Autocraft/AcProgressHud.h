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
#include "AcPilotText.h"
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
		/// Where the site's model is, a share of the view (unset: off
		/// screen or not drawn; the ring sits round the reticle).
		TOptional<FVector2f> At;
		bool operator==(const FOpening&) const = default;
	};
	void SetOpening(const TOptional<FOpening>& In);
	/// The Prospector fell (P6): the view dimmed, "BACK IN 7 s", where it
	/// comes back, the cargo Second wind kept. Unset: on its feet.
	struct FRespawn
	{
		/// Whole seconds left.
		int32 Left = 0;
		/// "AT YOUR CITADEL", "AT THE LANDING PAD, COLD FORK".
		FString Where;
		/// "Second wind kept 40 ore" (empty: nothing kept).
		FString Kept;
		bool operator==(const FRespawn&) const = default;
	};
	void SetRespawn(const TOptional<FRespawn>& In);

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
	TOptional<FRespawn> Respawn;
	FAcCabLayout Lay;
	TArray<FNote> Notes;
	TSharedPtr<FActiveTimerHandle> Timer;

	EActiveTimerReturnType Animate(double Now, float Dt);
	void PaintSlots(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f Size) const;
	void PaintOpening(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f Size) const;
	void PaintRespawn(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f Size) const;
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
	/// Every blueprint site of region `Region` (known or not), for the
	/// models on the ground (AAcSites): its id "<region id>/<site name>",
	/// kind and point in region cells from the north-west corner.
	struct FSitePlace
	{
		FString Id;
		ac::SiteKind Kind = ac::SiteKind::crate;
		ac::Vec2 Cell;
	};
	static TArray<FSitePlace> SitesIn(const FAcMmoClient& Mmo, const FString& Region);
	/// For the skill screen's BLUEPRINTS: where each found blueprint came
	/// from ("Landing crate, Prospect Landing"), and the sites known, not
	/// opened, of each blueprint not found.
	static void BlueprintSites(const FAcMmoClient& Mmo, std::map<ac::StructureKind, FString>& Sources,
		std::map<ac::StructureKind, int32>& Marked);
	/// The KSTR contract panel's rows (docs/contracts.md): title, the
	/// radio's line, "2 / 6 LOADS · COLD FORK", the pay, and for a done or
	/// abandoned slot the countdown to dawn against the world's clock.
	static FAcContractsInfo Contracts(const FAcMmoClient& Mmo, bool bArmed);
	/// The open reach and drill contracts on the world map: their region, and
	/// the site or pad a reach names.
	static TArray<FAcWorldMapInfo::FMark> ContractMarks(const FAcMmoClient& Mmo);
	/// Those of them in the pilot's region with a place (a reach), for the
	/// compass: ground position and distance from `From`.
	static TArray<FAcPilotGoal> ContractGoals(const FAcMmoClient& Mmo, ac::Vec2 From);
	/// The first ten minutes' open step (nil: not on it), its hint for the
	/// panel and the toast, and its tip for the line under the card.
	static const ac::net::Contract* FirstStep(const FAcMmoClient& Mmo);
	static FString FirstHint(const ac::net::Contract& C);
	static FString FirstTip(const ac::net::Contract& C);
	/// "23:41", "1:02:09": seconds as a countdown.
	static FString Countdown(double Seconds);
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

	/// X: arm an abandon of a KSTR contract (again: keep them); while armed
	/// 1, 2, 3 drop that slot's open contract. Armed for 6 s.
	void ToggleAbandon();
	bool AbandonArmed() const;
	/// Drops slot `Index`'s (0…2) contract if it is open; disarms. False: none
	/// to drop there.
	bool Abandon(int32 Index);
	/// T: the contract panel whole, or back to one line a contract.
	void ToggleContracts();
	bool ContractsExpanded() const;

	/// Pictures (-AcMmoShotsG2): a piece of news as if the server sent it
	/// (the toast's words from the real path), and the respawn screen for
	/// `R` while no real fall is under way (unset: off).
	void PreviewNews(const ac::net::ProgressMessage::News& N);
	void PreviewRespawn(const std::optional<ac::net::ProgressMessage::Respawn>& R);

private:
	void OnFrame(const FAcFrame& Frame);
	bool Mount();
	/// The server's news: the banner, the sound, the toasts.
	void News(FAcMmoClient& Mmo);
	/// One piece of news: its toast; a level reached goes into `Reached`.
	void NewsItem(FAcMmoClient& Mmo, const ac::net::ProgressMessage::News& N, double Now, int64& Reached);

	TSharedPtr<SAcProgressHud> Hud;
	FDelegateHandle FrameHandle;
	/// The level last shown (-1: not told yet: the first progress sets it, no banner).
	int64 Shown = -1;
	/// When X armed an abandon (-1: not armed).
	double ArmedAt = -1;
	/// T opened the whole contract panel.
	bool bContractsOpen = false;
	/// The contracts as last seen (ids, states, progress) and when that
	/// changed: the panel opens for `ContractsShown` s after.
	FString ContractsSeen;
	double ContractsChangedAt = -1;
	/// The first ten minutes' step last told ("KSTR: <title>" when another opens).
	uint32 FirstSeen = 0;
	/// Every contract's title as last seen, by id: a done one's toast when
	/// the list no longer has it (the first ten minutes' last step).
	TMap<uint32, FString> ContractTitles;
	std::optional<ac::net::ProgressMessage::Respawn> RespawnPreview;
};
