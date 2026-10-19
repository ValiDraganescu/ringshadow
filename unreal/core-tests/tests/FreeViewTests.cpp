// The top-down view turned about its focus (`FreeView::yaw`, the right-drag
// orbit): the focus stays, the view's points and the ground still agree,
// and the view's own frame turns with it.
#include "test.h"

#include "FreeView.h"

#include <cmath>

namespace {

using namespace ac;

FreeView midMap() {
    FreeView v(GroundRect{-60, -40, 60, 40}, Vec2(3, -2), 40, Vec2(1280, 800));
    v.resize(Vec2(1280, 800), 234);
    v.center(Vec2(3, -2));
    return v;
}

} // namespace

TEST(FreeView_orbitKeepsTheFocus) {
    FreeView v = midMap();
    const Vec2 mid(640, 400);
    const Vec2 focus = v.ground(mid);
    EXPECT_TRUE(distance(focus, v.target) < 1e-9);
    const double eye = distance(Vec2(v.cameraPosition().x, v.cameraPosition().z), focus);
    v.orbit(0.7);
    EXPECT_EQ(v.yaw, 0.7);
    EXPECT_TRUE(distance(v.target, focus) < 1e-12);
    EXPECT_TRUE(distance(v.ground(mid), focus) < 1e-9);
    // The camera swung round the focus at the same distance and height.
    const Vec3 at = v.cameraPosition();
    EXPECT_TRUE(std::abs(distance(Vec2(at.x, at.z), focus) - eye) < 1e-9);
    // Turned right (it looks more toward +x), the eye goes round the
    // focus the other way: from over +z toward -x.
    const Vec2 off = Vec2(at.x, at.z) - focus;
    EXPECT_TRUE(off.y > 0 && off.x < 0);
    // Every view point still lands where the ray through it meets the ground.
    for (const Vec2 c : {Vec2(0, 0), Vec2(1280, 0), Vec2(200, 700), Vec2(900, 333)}) {
        const Vec2 g = v.ground(c);
        const Vec2 back = v.canvas(Vec3(g.x, 0, g.y));
        EXPECT_TRUE(distance(back, c) < 1e-6);
        const FreeView::Ray r = v.ray(c);
        const double t = -r.origin.y / r.direction.y;
        EXPECT_TRUE(distance(Vec2(r.origin.x + r.direction.x * t, r.origin.z + r.direction.z * t), g) < 1e-6);
    }
    // The view's right is the world's turned by the yaw.
    const Vec2 right = v.ground(Vec2(740, 400)) - focus;
    EXPECT_TRUE(std::abs(std::atan2(right.y, right.x) - 0.7) < 1e-6);
    EXPECT_TRUE(distance(v.toView(v.toWorld(Vec2(2, 5))), Vec2(2, 5)) < 1e-12);
    // A full turn back and the view is the unturned one again.
    v.orbit(-0.7);
    EXPECT_EQ(v.yaw, 0.0);
    EXPECT_TRUE(distance(v.ground(Vec2(0, 0)), midMap().ground(Vec2(0, 0))) < 1e-9);
}

TEST(FreeView_dragAndZoomWorkTurned) {
    FreeView v = midMap();
    v.orbit(-1.1);
    // A drag: what was under `from` ends up under `to`.
    const Vec2 from(500, 300), to(620, 420);
    const Vec2 g = v.ground(from);
    v.drag(from, to);
    EXPECT_TRUE(distance(v.ground(to), g) < 1e-9);
    // A zoom holds the ground under the pointer.
    const Vec2 c(300, 500);
    const Vec2 h = v.ground(c);
    v.zoom(1.3, c);
    EXPECT_TRUE(distance(v.ground(c), h) < 1e-9);
    // The zoom range does not depend on the turn; fit undoes it.
    FreeView w = midMap();
    EXPECT_EQ(v.minPointsPerCell(), w.minPointsPerCell());
    v.fit();
    EXPECT_EQ(v.yaw, 0.0);
}
