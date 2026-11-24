// TPW_MOD_HOOK tpi_worlds: paths on the Arabian tunnel's cleared top over its fixed route, on synthetic cells. No game data.
#include <array>
#include <cstdio>
#include "TpiTunnelTop.h"
#include "TpiFootbridge.h"
namespace {
std::array<int, 64> gModes{}; // an 8 by 8 park's cell modes
std::array<int, 64> gAttributes{}; // and the level's attribute bytes
int ModeAt(int x, int y) { return x >= 0 && y >= 0 && x < 8 && y < 8 ? gModes[x + y * 8] : 0; }
bool RouteAt(int x, int y) { return x >= 0 && y >= 0 && x < 8 && y < 8 && gAttributes[x + y * 8] == 0x80; }
uint16_t Id(int x, int y) { return uint16_t(1 + x + (y << 7)); }
struct Field { const float* mHeights; float mF08[4]; };
}
int main() {
    using namespace tpw;
    int checks = 0, failures = 0;
    auto check = [&](bool ok) { ++checks; if (!ok) { ++failures; std::printf("FAIL check %d\n", checks); } };
    TpiTunnelTop top;
    check(TpiParseTunnelTop("tpitop 1 1 6 6 2 2 4 4 20 0ff0 ffff", top)); // the top: x 2 to 5, rows 2 to 5
    // The route: rows 3 and 4 from mouth to mouth (x 1 to 6). Its eight cells under the top are covered.
    for (int y = 3; y <= 4; ++y) for (int x = 1; x <= 6; ++x) { gModes[x + y * 8] = 0x1e; gAttributes[x + y * 8] = 0x80; }
    check(TpiTunnelCoveredCells(top, RouteAt) == 0x0ff0);
    auto lip = top;
    lip.raised = 0x7fff; // the bottom row's last quad not raised: that cell is no part of the top
    check(TpiTunnelCoveredCells(lip, RouteAt) == 0x0ff0);
    lip.raised = 0xf7ff; // one route cell under the top not raised: it stays the tunnel's own
    check(TpiTunnelCoveredCells(lip, RouteAt) == 0x07f0);
    gAttributes[1 + 3 * 8] = 0x90; // the gate's pavement marks no route
    check(TpiTunnelCoveredCells(top, RouteAt) == 0x0ff0);
    gModTpiWorlds = true; TpiNoteTheme("arabian");
    std::array<float, 81> liveHeights{}, floorHeights{};
    liveHeights.fill(20); floorHeights.fill(0);
    const Field live{liveHeights.data(), {0, 0, 10, 10}}, floor{floorHeights.data(), {0, 0, 10, 10}};
    gTpiTunnelTop = top; gTpiTunnelTopField = &live; gTpiTunnelFloorSource = &floor; gTpiTunnelFloorMode = ModeAt;
    gTpiTunnelCovered = TpiTunnelCoveredCells(top, RouteAt);
    check(!TpiTunnelCovered(Id(3, 3))); // the top still stands: no route of its own
    gTpiTunnelTopOpen = true;
    // The cleared top: its route cells are its ground, with a path drawn north to south in column 3.
    for (int y = 3; y <= 4; ++y) for (int x = 2; x <= 5; ++x) gModes[x + y * 8] = 0;
    for (int y = 2; y <= 5; ++y) gModes[3 + y * 8] = 1;
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x)
        check(TpiTunnelCovered(Id(x, y)) == (x >= 2 && x <= 5 && y >= 3 && y <= 4));
    check(!TpiTunnelCovered(0) && !TpiTunnelCovered(Id(9, 9)));
    // Levels: a covered cell is the top's for a top walker and the tunnel's for a route walker; every other cell its own.
    for (int y = 3; y <= 4; ++y) {
        check(TpiTunnelTopLevel(Id(3, y), 1) == 1 && TpiTunnelTopLevel(Id(3, y), 0) == 0);
        check(TpiTunnelTopLevel(Id(1, y), 1) == 0 && TpiTunnelTopLevel(Id(6, y), 0) == 0); // the mouths' walkway
        check(TpiWalkerRoof(1, Id(3, y)) == 1 && TpiWalkerRoof(0, Id(3, y)) == 0);
        check(TpiWalkerRoof(1, Id(1, y)) == 0);
    }
    check(TpiTunnelTopLevel(Id(3, 2), 0) == 1 && TpiTunnelTopLevel(Id(3, 5), 0) == 1 && TpiTunnelTopLevel(Id(0, 0)) == 0);
    check(TpiWalkerRoof(0, Id(3, 2)) == 1 && TpiWalkerRoof(1, Id(0, 0)) == 0);
    // Steps: the layer comes from the cell a walker steps off, and is kept along the covered cells.
    check(TpiStepRoof(0, Id(3, 2), Id(3, 3)) == 1); // off the north end of the top's path: on the top
    check(TpiStepRoof(1, Id(3, 3), Id(3, 4)) == 1);
    check(TpiStepRoof(1, Id(3, 4), Id(3, 5)) == 1); // onto the south end
    check(TpiStepRoof(1, Id(1, 3), Id(2, 3)) == 0); // in at the west mouth: under the top
    check(TpiStepRoof(0, Id(2, 3), Id(3, 3)) == 0); // along the route: stays below
    check(TpiStepRoof(0, Id(5, 3), Id(6, 3)) == 0); // out at the east mouth
    // A walk north to south on the top and one west to east through the tunnel share (3, 3), each on its own layer.
    int roof = 0, tunnel = 0;
    for (int y = 2; y <= 6; ++y) { roof = TpiStepRoof(roof, Id(3, y - 1), Id(3, y)); check(roof == (y <= 5 ? 1 : 0)); }
    for (int x = 1; x <= 7; ++x) { tunnel = TpiStepRoof(tunnel, Id(x - 1, 3), Id(x, 3)); check(tunnel == 0); }
    // What each walker meets: under the top the route's footbridge, whatever the top holds; on the top the top's cells.
    const TpiFootbridgeCell below = TpiWalkerCell(1, 0x20, Id(3, 3), 0), above = TpiWalkerCell(1, 0x20, Id(3, 3), 1);
    check(TpiIsFootbridge(below) && below.level == 0);
    check(above.mode == 1 && above.flags == 0x20 && above.level == 1);
    check(TpiIsFootbridge(TpiWalkerCell(0, 0x80, Id(4, 3), 0))); // a gate flag of the top's cell is no pavement below
    const auto cell = [](int x, int y, int r) { return TpiWalkerCell(ModeAt(x, y), 0, Id(x, y), r); };
    for (int nav : {0, 1}) {
        // The route: mouth to mouth, under the top's path, never up onto the top's cells beside it.
        for (int x = 1; x <= 5; ++x) check(!TpiFootbridgeWall(cell(x, 3, 0), cell(x + 1, 3, 0), nav));
        check(!TpiFootbridgeWall(cell(3, 3, 0), cell(3, 4, 0), nav)); // the route's two lanes
        check(TpiFootbridgeWall(cell(3, 3, 0), cell(3, 2, 0), nav)); // up onto the top's path
        check(TpiFootbridgeWall(cell(3, 4, 0), cell(3, 5, 0), nav));
        // The top: along its path across the route, never down into the tunnel at a mouth.
        for (int y = 2; y <= 4; ++y) check(!TpiFootbridgeWall(cell(3, y, 1), cell(3, y + 1, 1), nav));
        check(TpiFootbridgeWall(cell(1, 3, 1), cell(2, 3, 1), nav)); // from the mouth onto the top's ground
        check(TpiFootbridgeWall(cell(2, 3, 1), cell(1, 3, 1), nav));
    }
    check(TpiFootbridgeStep(cell(1, 3, 0), cell(2, 3, 0)) && TpiFootbridgeStep(cell(2, 3, 0), cell(3, 3, 0)));
    check(!TpiFootbridgeStep(cell(3, 3, 0), cell(3, 2, 0)) && !TpiFootbridgeStep(cell(3, 2, 1), cell(3, 3, 0)));
    // Drawn heights: a route walker under the top is lowered to the floor; a top walker and every other cell are not.
    const auto top20 = [](float, float) { return 20.0f; };
    const auto flat = [](float, float) { return 0.0f; };
    check(TpiTunnelLift(0, 35, 35, top20, flat) == -20 && TpiTunnelLift(1, 35, 35, top20, flat) == 0);
    check(TpiTunnelLift(0, 35, 25, top20, flat) == 0); // the top's path beside the route
    check(TpiTunnelLift(0, 15, 35, top20, flat) == 0); // the mouth: the walking field gives the floor already
    check(TpiTunnelLift(0, 35, 35, flat, top20) == 0); // never lifted
    check(TpiTunnelLift(0, -5, 35, top20, flat) == 0 && TpiTunnelLift(0, 2000, 35, top20, flat) == 0);
    // Shadows: a sprite below the top over a route cell lies on the floor's heights, any other on the live field's.
    check(TpiShadowHeights(&live, 35, 0, 35) == floorHeights.data()); // a route walker under the path
    check(TpiShadowHeights(&live, 45, 0, 35) == floorHeights.data()); // under the top's ground
    check(TpiShadowHeights(&live, 15, 0, 35) == floorHeights.data()); // in the mouth
    check(TpiShadowHeights(&live, 35, 20, 35) == liveHeights.data()); // a walker on the top's path
    check(TpiShadowHeights(&live, 35, 0, 25) == liveHeights.data()); // beside the route
    check(TpiShadowHeights(&live, 75, 0, 75) == liveHeights.data()); // outside the tunnel
    check(TpiShadowHeights(&live, -5, 0, 35) == liveHeights.data() && TpiShadowHeights(&live, 35, 0, 2000) == liveHeights.data());
    const Field other{liveHeights.data(), {0, 0, 10, 10}}, empty{liveHeights.data(), {0, 0, 0, 0}};
    check(TpiShadowHeights(&other, 35, 0, 35) == liveHeights.data()); // another field (a ride's preview)
    gTpiTunnelTopField = &empty;
    check(TpiShadowHeights(&empty, 35, 0, 35) == liveHeights.data()); // no cell size: no division
    gTpiTunnelTopField = &live;
    // The nav mode bits: outside the original's modes 0 to 2 and apart.
    check(kTpiRoofNavBit > 2 && (kTpiRoofNavBit & 3) == 0 && kTpiTunnelNavBit > 2 && (kTpiTunnelNavBit & 3) == 0);
    check((kTpiRoofNavBit & kTpiTunnelNavBit) == 0);
    // Other parks and the mod switched off: one layer, the cells as they are, the live heights.
    const auto none = [&]() {
        check(!TpiTunnelCovered(Id(3, 3)) && TpiTunnelTopLevel(Id(3, 3), 0) == 0 && TpiWalkerRoof(0, Id(3, 3)) == 0);
        const TpiFootbridgeCell plain = TpiWalkerCell(1, 0, Id(3, 3), 0);
        check(plain.mode == 1 && plain.level == 0);
        check(TpiTunnelLift(0, 35, 35, top20, flat) == 0);
        check(TpiShadowHeights(&live, 45, 0, 35) == liveHeights.data());
    };
    for (const char* theme : {"jungle", "hallow", "space", "fantasy"}) { TpiNoteTheme(theme); none(); }
    TpiNoteTheme("arabian"); gModTpiWorlds = false;
    none();
    std::printf("tpi_tunnel_cross: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
