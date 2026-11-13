// host_tpi_obstacle_gate.cpp: the TPI Worlds and Rides mod's opening gate (include/TpiObstacleGate.h, TPW_MOD_HOOK
// tpi_worlds) on the host, without game data: a leaf's node name read back as the converter writes it
// (assets/tpi_terrain.h TpiGateLeafName), the names it turns down, the land a new park starts with (a hoarding's clicks
// and the land inside them), the rule that opens the gate (the park owns a cell of its footprint or a cell next to it
// that a new park does not start with), the swing from closed to open and the node matrix rows it gives. The names, the
// hoarding and the land are made up here. Exits nonzero on the first failed check's count.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <utility>

#include "TpiObstacleGate.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
    using namespace tpw;
    // A name as the converter writes it (the converter's own test makes this one).
    TpiGateLeaf leaf;
    Check(TpiParseGateLeaf("tpigate 1 0 2 1 0 0 0 1 0 0.9304 0 0.3665", leaf), "parse: the converter's name");
    Check(leaf.x == 1 && leaf.y == 0 && leaf.w == 2 && leaf.h == 1, "parse: the footprint");
    Check(Near(leaf.closed[0], 0) && Near(leaf.closed[1], 0) && Near(leaf.closed[2], 0) && Near(leaf.closed[3], 1),
          "parse: closed");
    Check(Near(leaf.open[0], 0) && Near(leaf.open[1], 0.9304f) && Near(leaf.open[2], 0) && Near(leaf.open[3], 0.3665f),
          "parse: open");
    TpiGateLeaf neg;
    Check(TpiParseGateLeaf("tpigate 42 49 2 2 0 0 0.5 1 -0.25 0.1235 0 -1 ", neg) && neg.x == 42 && neg.y == 49 &&
              Near(neg.open[0], -0.25f) && Near(neg.open[3], -1.0f),
          "parse: negative numbers and a trailing space");

    // Names that are not a leaf's.
    const char* bad[] = {
        nullptr,
        "",
        "gate02",
        "tpigate",
        "tpigate ",
        "tpigatex 1 0 2 1 0 0 0 1 0 1 0 0",
        "tpigate 1 0 2 1 0 0 0 1 0 1 0",          // a number missing
        "tpigate 1 0 2 1 0 0 0 1 0 1 0 0 7",      // one too many
        "tpigate 1 0 2 1 0 0 0 1 0 1 0 0x",       // junk after
        "tpigate 1 0 0 1 0 0 0 1 0 1 0 0",        // no width
        "tpigate 1 0 2 0 0 0 0 1 0 1 0 0",        // no height
        "tpigate -1 0 2 1 0 0 0 1 0 1 0 0",       // off the map
        "tpigate 127 0 2 1 0 0 0 1 0 1 0 0",      // runs off the map
        "tpigate 0 127 1 2 0 0 0 1 0 1 0 0",
        "tpigate 129 0 1 1 0 0 0 1 0 1 0 0",
    };
    for (const char* name : bad) {
        TpiGateLeaf none;
        Check(!TpiParseGateLeaf(name, none), std::string("reject: ") + (name != nullptr ? name : "(null)"));
    }
    TpiGateLeaf edge;
    Check(TpiParseGateLeaf("tpigate 126 126 2 2 0 0 0 1 0 1 0 0", edge), "parse: a footprint at the map's corner");

    // The rule alone, with no starting land: open while the park owns a cell of the footprint (x 41..44, y 48..51) or
    // a cell next to it (x 40..45, y 47..52), closed while it owns none of them.
    TpiGateLeaf gate;
    Check(TpiParseGateLeaf("tpigate 41 48 4 4 0 0 0 1 0 0.9304 0 0.3665", gate), "parse: the gate");
    std::set<std::pair<int, int>> land;
    const auto owned = [&land](int32_t x, int32_t y) { return land.count({x, y}) != 0; };
    const auto nostart = [](int32_t, int32_t) { return false; };
    Check(!TpiGateOpen(gate, owned, nostart), "open: nothing owned");
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            land = {{x, y}};
            const bool ring = x >= 40 && x <= 45 && y >= 47 && y <= 52;
            if (TpiGateOpen(gate, owned, nostart) != ring)
                Check(false, "open: only cell (" + std::to_string(x) + "," + std::to_string(y) + ") owned");
        }
    land = {{41, 48}};
    Check(TpiGateOpen(gate, owned, nostart), "open: one of its own cells, a corner post");
    land = {{42, 49}};
    Check(TpiGateOpen(gate, owned, nostart), "open: one of its own cells, the passage");
    land = {{42, 47}};
    Check(TpiGateOpen(gate, owned, nostart), "open: a cell beside it");
    land = {{45, 52}};
    Check(TpiGateOpen(gate, owned, nostart), "open: a cell at its corner");
    land = {{42, 46}, {39, 49}, {46, 50}, {43, 53}, {39, 46}, {46, 53}};
    Check(!TpiGateOpen(gate, owned, nostart), "open: land two cells away only");
    land = {{42, 46}, {42, 47}};
    Check(TpiGateOpen(gate, owned, nostart), "open: land reaching it from the park");
    land.erase({42, 47});
    Check(!TpiGateOpen(gate, owned, nostart), "open: closes again when the cell next to it is lost");
    // At the map's edges the cells next to it stop at the map: the cells there still count, none past it is asked for.
    TpiGateLeaf corner;
    Check(TpiParseGateLeaf("tpigate 0 0 2 2 0 0 0 1 0 1 0 0", corner), "parse: a gate at the map's first corner");
    bool inside = true;
    const auto onMap = [&inside, &land](int32_t x, int32_t y) {
        inside = inside && x >= 0 && y >= 0 && x < 128 && y < 128;
        return land.count({x, y}) != 0;
    };
    const auto startOnMap = [&inside](int32_t x, int32_t y) {
        inside = inside && x >= 0 && y >= 0 && x < 128 && y < 128;
        return false;
    };
    land = {{2, 2}};
    Check(TpiGateOpen(corner, onMap, startOnMap), "open: at the first corner, the cell at its corner");
    land = {{3, 0}};
    Check(!TpiGateOpen(corner, onMap, startOnMap), "open: at the first corner, two away");
    TpiGateLeaf last;
    Check(TpiParseGateLeaf("tpigate 126 126 2 2 0 0 0 1 0 1 0 0", last), "parse: a gate at the map's last corner");
    land = {{125, 125}};
    Check(TpiGateOpen(last, onMap, startOnMap), "open: at the last corner, the cell at its corner");
    land = {{127, 127}};
    Check(TpiGateOpen(last, onMap, startOnMap), "open: at the last corner, its own last cell");
    land.clear();
    Check(!TpiGateOpen(last, onMap, startOnMap) && !TpiGateOpen(corner, onMap, startOnMap),
          "open: at the corners, nothing owned");
    Check(inside, "open: never asks for a cell off the map");

    // A new park's land: a made-up hoarding clicked from (right 60, bottom 20), left 48, its corners one cell on from
    // the clicks, a step at (63, 31) and one at (65, 35), so the land is x 41..62 on rows 21..30, x 41..66 on rows
    // 31..34 and x 41..64 on rows 35..40, the two step cells (63, 30) and (65, 35), and the entrance's row 20 from
    // x 50 to 59 (row 21 is inside already).
    TpiStartFence fence;
    fence.left = 48;
    fence.right = 60;
    fence.bottom = 20;
    const int clicks[][2] = {{60, 20}, {62, 20}, {62, 30}, {66, 30}, {66, 34}, {64, 34}, {64, 40}, {40, 40}, {40, 20}, {52, 20}};
    for (const auto& click : clicks) TpiFenceClick(fence, click[0], click[1]);
    Check(fence.count == 10, "fence: ten clicks held");
    const auto expected = [](int x, int y) {
        return (x >= 41 && x <= 62 && y >= 21 && y <= 30) || (x >= 41 && x <= 66 && y >= 31 && y <= 34) ||
               (x >= 41 && x <= 64 && y >= 35 && y <= 40) || (x == 63 && y == 30) || (x == 65 && y == 35) ||
               (y == 20 && x >= 50 && x <= 59);
    };
    int startCount = 0;
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            startCount += TpiStartLand(fence, x, y);
            if (TpiStartLand(fence, x, y) != expected(x, y))
                Check(false, "start: cell (" + std::to_string(x) + "," + std::to_string(y) + ")");
        }
    Check(startCount == 22 * 10 + 26 * 4 + 24 * 6 + 2 + 10, "start: the number of cells, got " + std::to_string(startCount));
    Check(TpiInsideFence(fence, 41, 21) && !TpiInsideFence(fence, 40, 21) && !TpiInsideFence(fence, 41, 20),
          "inside: the first corner");
    Check(!TpiInsideFence(fence, 63, 30) && TpiStartLand(fence, 63, 30), "inside: a step's cell is the fill's only");
    Check(!TpiInsideFence(fence, 55, 20) && TpiStartLand(fence, 55, 20), "inside: the entrance's row is the fill's only");
    Check(!TpiStartLand(fence, 60, 20) && !TpiStartLand(fence, 49, 20), "start: the entrance's row stops short");
    Check(!TpiInsideFence(fence, -1, 25) && !TpiInsideFence(fence, 25, 128), "inside: none off the map");
    TpiStartFence many;
    for (int i = 0; i < 70; ++i) TpiFenceClick(many, i, i % 2 == 0 ? 0 : 5);
    Check(many.count == 64 && many.x[63] == 63, "fence: the game's 64 clicks at most");
    TpiStartFence empty;
    empty.left = 10;
    empty.right = 14;
    empty.bottom = 5;
    Check(!TpiInsideFence(empty, 12, 5) && TpiStartLand(empty, 12, 5) && TpiStartLand(empty, 13, 6) &&
              !TpiStartLand(empty, 14, 5) && !TpiStartLand(empty, 12, 7),
          "start: no clicks, the entrance's rows only");
    // A hoarding one cell wide (its land is x 11 on rows 6..9), with the entrance off the map: no cell has a
    // neighbour inside on both sides, so the land is only the cells inside.
    TpiStartFence thin;
    thin.left = thin.right = thin.bottom = 100;
    const int thinClicks[][2] = {{10, 5}, {11, 5}, {11, 9}, {10, 9}};
    for (const auto& click : thinClicks) TpiFenceClick(thin, click[0], click[1]);
    int thinCount = 0;
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            thinCount += TpiStartLand(thin, x, y);
            if (TpiStartLand(thin, x, y) != (x == 11 && y >= 6 && y <= 9))
                Check(false, "start: thin cell (" + std::to_string(x) + "," + std::to_string(y) + ")");
        }
    Check(thinCount == 4, "start: a thin hoarding's four cells, got " + std::to_string(thinCount));
    // A hoarding with a notch on its left (x 21..30 on rows 11..20 and x 17..20 on rows 16..20): its one step cell
    // (20, 15) has the land to its right and below.
    TpiStartFence notch;
    notch.left = notch.right = notch.bottom = 100;
    const int notchClicks[][2] = {{20, 10}, {30, 10}, {30, 20}, {16, 20}, {16, 15}, {20, 15}};
    for (const auto& click : notchClicks) TpiFenceClick(notch, click[0], click[1]);
    int notchCount = 0;
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            const bool want = (x >= 21 && x <= 30 && y >= 11 && y <= 20) || (x >= 17 && x <= 20 && y >= 16 && y <= 20) ||
                              (x == 20 && y == 15);
            notchCount += TpiStartLand(notch, x, y);
            if (TpiStartLand(notch, x, y) != want)
                Check(false, "start: notch cell (" + std::to_string(x) + "," + std::to_string(y) + ")");
        }
    Check(notchCount == 121, "start: the notched hoarding's cells, got " + std::to_string(notchCount));
    Check(!TpiInsideFence(notch, 20, 15) && TpiStartLand(notch, 20, 15), "start: the notch's step from the right");

    // The rule with that land: a gate whose footprint (x 50..53, y 39..42) the hoarding crosses, so a new park owns
    // its rows 39 and 40 and the row above (the cells next to it are x 49..54, y 38..43). Closed with the park's
    // starting land; open once the park owns any cell of the footprint or next to it outside that land, and only then.
    TpiGateLeaf crossed;
    Check(TpiParseGateLeaf("tpigate 50 39 4 4 0 0 0 1 0 0.9304 0 0.3665", crossed), "parse: the crossed gate");
    const auto start = [&fence](int32_t x, int32_t y) { return TpiStartLand(fence, x, y); };
    std::set<std::pair<int, int>> park;
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
            if (expected(x, y)) park.insert({x, y});
    land = park;
    Check(!TpiGateOpen(crossed, owned, start), "crossed: closed in a new park");
    Check(TpiGateOpen(crossed, owned, nostart), "crossed: the plain rule would open it in a new park");
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            if (park.count({x, y}) != 0)
                continue;
            land = park;
            land.insert({x, y});
            const bool counts = x >= 49 && x <= 54 && y >= 41 && y <= 43;
            if (TpiGateOpen(crossed, owned, start) != counts)
                Check(false, "crossed: cell (" + std::to_string(x) + "," + std::to_string(y) + ") bought");
        }
    land = park;
    land.insert({51, 41});
    Check(TpiGateOpen(crossed, owned, start), "crossed: open, one of its own cells bought");
    land = park;
    land.insert({54, 42});
    Check(TpiGateOpen(crossed, owned, start), "crossed: open, a cell beside it bought");
    land = park;
    land.insert({49, 43});
    Check(TpiGateOpen(crossed, owned, start), "crossed: open, a cell at its corner bought");
    land = park;
    for (int y = 41; y <= 44; ++y)
        for (int x = 55; x <= 58; ++x) land.insert({x, y});
    Check(!TpiGateOpen(crossed, owned, start), "crossed: closed, land bought two cells away");
    land = {{51, 41}};
    Check(TpiGateOpen(crossed, owned, start), "crossed: open, its own cell owned without the starting land");
    // A gate whose footprint and the cells next to it all lie in the starting land: owned, so open; none owned, closed.
    TpiGateLeaf within;
    Check(TpiParseGateLeaf("tpigate 45 25 2 2 0 0 0 1 0 1 0 0", within), "parse: a gate inside the starting land");
    land = park;
    Check(TpiGateOpen(within, owned, start), "within: open with the starting land");
    land = {{44, 24}};
    Check(TpiGateOpen(within, owned, start), "within: open with one cell of it");
    land.clear();
    Check(!TpiGateOpen(within, owned, start), "within: closed with none");

    // The swing: 40 ticks from closed to open, clamped at both ends, and back.
    float way = 0.0f;
    int ticks = 0;
    while (way < 1.0f && ticks < 100) {
        way = TpiGateStep(way, true);
        ++ticks;
    }
    Check(ticks >= 39 && ticks <= 41 && way == 1.0f, "step: about 40 ticks to open, got " + std::to_string(ticks));
    Check(TpiGateStep(1.0f, true) == 1.0f, "step: stays open");
    Check(TpiGateStep(0.0f, false) == 0.0f, "step: stays closed");
    Check(TpiGateStep(0.5f, false) < 0.5f, "step: closes when not owned");
    Check(Near(TpiGateEase(0.0f), 0.0f) && Near(TpiGateEase(1.0f), 1.0f) && Near(TpiGateEase(0.5f), 0.5f),
          "ease: ends and middle");
    Check(TpiGateEase(0.1f) < 0.1f && TpiGateEase(0.9f) > 0.9f, "ease: slow at both ends");

    // The turn: closed at 0, open at 1, a unit quaternion between, the shorter arc.
    float q[4];
    TpiGateTurn(gate, 0.0f, q);
    Check(Near(q[0], 0) && Near(q[1], 0) && Near(q[2], 0) && Near(q[3], 1), "turn: closed at 0");
    TpiGateTurn(gate, 1.0f, q);
    const float n = std::sqrt(0.9304f * 0.9304f + 0.3665f * 0.3665f);
    Check(Near(q[1], 0.9304f / n) && Near(q[3], 0.3665f / n), "turn: open at 1");
    for (int i = 1; i < 10; ++i) {
        TpiGateTurn(gate, i / 10.0f, q);
        const float length = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
        Check(Near(length, 1.0f), "turn: unit at " + std::to_string(i));
        Check(q[1] > 0.0f && q[1] < 0.9304f / n + 1e-4f, "turn: between the two at " + std::to_string(i));
    }
    // Half way (eased 0.5) is half the angle: the open turn is 2 acos(0.3665/n) about y.
    TpiGateTurn(gate, 0.5f, q);
    const float full = 2.0f * std::acos(0.3665f / n);
    Check(Near(2.0f * std::acos(q[3]), full / 2.0f), "turn: half the angle half way");
    // The same open orientation written negated takes the shorter arc: the same quaternions up to sign.
    TpiGateLeaf flipped = gate;
    for (float& v : flipped.open) v = -v;
    float a[4], b[4];
    TpiGateTurn(gate, 0.3f, a);
    TpiGateTurn(flipped, 0.3f, b);
    Check(Near(a[0], b[0]) && Near(a[1], b[1]) && Near(a[2], b[2]) && Near(a[3], b[3]), "turn: the shorter arc");

    // The rows: none for no turn; a turn of 90 degrees about y as the engine makes it (x goes to -z, z to x).
    float rows[3][3];
    const float none[4] = {0, 0, 0, 1};
    TpiGateRows(none, rows);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) Check(Near(rows[r][c], r == c ? 1.0f : 0.0f), "rows: none");
    const float h = std::sqrt(0.5f);
    const float quarter[4] = {0, h, 0, h};
    TpiGateRows(quarter, rows);
    Check(Near(rows[0][0], 0) && Near(rows[0][1], 0) && Near(rows[0][2], -1), "rows: quarter, x");
    Check(Near(rows[1][0], 0) && Near(rows[1][1], 1) && Near(rows[1][2], 0), "rows: quarter, y");
    Check(Near(rows[2][0], 1) && Near(rows[2][1], 0) && Near(rows[2][2], 0), "rows: quarter, z");
    // A turn about x and one about z, against the engine's formula written out.
    const float tilt[4] = {0.3f, -0.2f, 0.5f, 0.787401f};
    TpiGateRows(tilt, rows);
    const float x = tilt[0], y = tilt[1], z = tilt[2], w = tilt[3];
    Check(Near(rows[0][1], 2 * (x * y + w * z)) && Near(rows[1][2], 2 * (y * z + w * x)) &&
              Near(rows[2][0], 2 * (x * z + w * y)) && Near(rows[1][0], 2 * (x * y - w * z)) &&
              Near(rows[2][1], 2 * (y * z - w * x)) && Near(rows[0][2], 2 * (x * z - w * y)),
          "rows: the engine's formula");

    if (gFailures != 0) {
        std::printf("%d failed\n", gFailures);
        return gFailures;
    }
    std::printf("tpi obstacle gate: all passed\n");
    return 0;
}
