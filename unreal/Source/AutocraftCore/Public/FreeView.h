// Port of Sources/GameCore/FreeView.swift.
#pragma once

#include "AutocraftCoreApi.h"

#include "Projection.h"
#include "SimdMath.h"

#include <optional>
#include <vector>

namespace ac {

/// Where a window looks at the ground, and how close: the free camera of
/// window mode. The camera keeps the wallpaper's pitch and field of view, so
/// the picture is the same; only its focus point (`target`), its scale
/// (`pointsPerCell`, view points per cell at the focus) and its turn about
/// the focus (`yaw`, the right-drag orbit) change.
struct AUTOCRAFTCORE_API FreeView {
    GroundRect bounds;
    /// The ground point at the centre of the view.
    Vec2 target;
    double pointsPerCell = 0;
    /// The view's size, points.
    Vec2 size;
    /// Points along the bottom of the view that the HUD hides: the map's
    /// near edge stops above them.
    double cover = 0;
    /// The camera's turn about the focus, radians: 0 looks toward -z (the
    /// Swift view); a positive turn looks more to the right (toward +x), as
    /// the pilot's look turns, the eye going round the focus to the left.
    /// Everything below but `projection` follows it.
    double yaw = 0;

    /// The most a cell can fill, and how the default looks (the wallpaper's 52).
    static constexpr double maxPointsPerCell = 130.0;
    static constexpr double defaultPointsPerCell = 40.0;

    explicit FreeView(GroundRect bounds_, std::optional<Vec2> target_ = std::nullopt,
                      double pointsPerCell_ = defaultPointsPerCell, Vec2 size_ = Vec2(1280, 800));

    /// The camera for the current view size, centred on the world origin,
    /// in the view's own frame: `target` shifts everything by a ground
    /// offset and `yaw` turns it about the focus.
    CanvasProjection projection() const;

    /// Turn the camera about its focus by `by` radians: the ground under
    /// the view's centre stays.
    void orbit(double by);

    /// A ground offset in the view's frame (x right, y down the view) to
    /// the world's, and back.
    Vec2 toWorld(Vec2 v) const;
    Vec2 toView(Vec2 v) const;

    /// Camera position in the world.
    Vec3 cameraPosition() const;

    /// Distance from the camera to its focus.
    double distance() const { return projection().distance(); }

    /// Least zoom: the whole map in the open view (above `cover`, perspective
    /// included, so the near edge is what limits it), but never past a floor.
    double minPointsPerCell() const;

    /// View point (y down) of a world position.
    Vec2 canvas(Vec3 w) const;

    /// `(origin: SIMD3<Double>, direction: SIMD3<Double>)`.
    struct Ray { Vec3 origin, direction; };
    /// The ray through a view point.
    Ray ray(Vec2 c) const;

    /// Where the ray through a view point meets the ground plane (height 0).
    Vec2 ground(Vec2 c) const;

    /// Drag the ground: what was under `from` ends up under `to`.
    void drag(Vec2 from, Vec2 to);

    /// Zoom by `factor` (above 1 closer) with the ground under `c` held still.
    void zoom(double factor, Vec2 c);

    /// Show the whole map (unturned).
    void fit();

    /// Centre on a ground point.
    void center(Vec2 p);

    /// A new view size (the window was resized) or HUD `cover`; keeps the
    /// scale, unless the map would now be smaller than the view.
    void resize(Vec2 s, std::optional<double> c = std::nullopt);

    /// Keep the view on the map: the map's side edges stop at
    /// the view's sides (at the middle of the open view, so a little ground
    /// past the edge shows in the far corners), its far edge at the view's
    /// top and its near edge just above `cover`. Along an axis where the map
    /// is smaller than the view, the map is centred in it. Turned
    /// (`yaw` not 0), the focus only stays on the map.
    void clamp();

    /// Ground the view covers: its four corners on the ground plane.
    std::vector<Vec2> footprint() const;

    /// Cells the view shows top to bottom at the focus, for pan speed.
    double visibleCells() const { return size.y / pointsPerCell; }

    bool operator==(const FreeView&) const = default;
};

} // namespace ac
