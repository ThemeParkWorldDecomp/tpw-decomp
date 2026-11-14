// host_tpi_obstacle_shut.cpp: the TPI Worlds and Rides mod's obstacles that stay until their land is bought
// (include/TpiObstacleShut.h, TPW_MOD_HOOK tpi_worlds) on the host, without game data: a node's name read back as the
// converter writes it (assets/tpi_terrain.h TpiShutName), with and without the cells it closed, the names it turns down,
// the rule that clears it (the gate's: the park owns a cell of its footprint or a cell next to it that a new park does
// not start with), the folded node and what its closed cells' quads and map cells get. The names and the land are made
// up here. Exits nonzero on the first failed check's count.
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <utility>

#include "TpiObstacleShut.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main() {
    using namespace tpw;
    // A tunnel's name as the converter writes it (the converter's own test makes these): a footprint, no cells.
    TpiShut tunnel;
    Check(TpiParseShut("tpishut 1 0 2 2", tunnel), "parse: a tunnel's name");
    Check(tunnel.x == 1 && tunnel.y == 0 && tunnel.w == 2 && tunnel.h == 2 && !tunnel.masked, "parse: its footprint");
    Check(!TpiShutHides(tunnel, 1, 0) && !TpiShutHides(tunnel, 2, 1), "parse: a tunnel closes no cell");
    // A rock's: its cells, four to a digit, the first the lowest bit, row by row from the first cell.
    TpiShut rock;
    Check(TpiParseShut("tpishut 28 64 2 3 d2", rock) && rock.masked, "parse: a rock's name");
    const bool hid[6] = {true, false, true, true, false, true};
    for (int32_t k = 0; k < 6; ++k)
        Check(TpiShutHides(rock, 28 + k % 2, 64 + k / 2) == hid[k], "parse: cell " + std::to_string(k));
    Check(!TpiShutHides(rock, 27, 64) && !TpiShutHides(rock, 28, 67) && !TpiShutHides(rock, 30, 64),
          "parse: no cell outside the footprint");
    TpiShut spaced;
    Check(TpiParseShut("tpishut 28 64 2 3 d2 ", spaced) && spaced.masked, "parse: a trailing space");
    // Names it turns down: another prefix, a footprint past the map or empty, a number missing, a mask too short, too
    // long, not hex or upper case, and anything after it.
    TpiShut bad;
    for (const char* name : {"tpigate 1 0 2 2", "tpishut", "tpishut 1 0 2", "tpishut 127 0 2 2", "tpishut 1 0 0 2",
                             "tpishut 1 0 2 x", "tpishut 28 64 2 3 d", "tpishut 28 64 2 3 d20", "tpishut 28 64 2 3 g2",
                             "tpishut 28 64 2 3 D2", "tpishut 28 64 2 3 d2 x", "tpishut -1 0 2 2"})
        Check(!TpiParseShut(name, bad), std::string("parse: turns down \"") + name + "\"");
    Check(!TpiParseShut(nullptr, bad), "parse: no name");
    TpiShut big;
    Check(!TpiParseShut("tpishut 0 0 20 20 0", big), "parse: too many cells for a mask");

    // The rule: a new park starts with x >= 28. The footprint (28, 64) 2 x 3, grown by a cell, reaches x = 27, which a
    // new park lacks: the obstacle stands until the park owns a cell there, then it is gone.
    std::set<std::pair<int32_t, int32_t>> bought;
    const auto start = [](int32_t x, int32_t) { return x >= 28; };
    const auto owned = [&](int32_t x, int32_t y) { return start(x, y) || bought.count({x, y}) != 0; };
    Check(!TpiShutCleared(rock, owned, start), "rule: a new park");
    bought.insert({40, 65});
    Check(!TpiShutCleared(rock, owned, start), "rule: land elsewhere");
    bought.insert({27, 63});
    Check(TpiShutCleared(rock, owned, start), "rule: the corner beside it");
    bought.clear();
    bought.insert({27, 66});
    Check(TpiShutCleared(rock, owned, start), "rule: a cell beside it");

    // The node: its own rows while it stands, folded to nothing at its place once it is gone.
    const float own[3][3] = {{0, 0, -1}, {0, 1, 0}, {1, 0, 0}};
    float rows[3][3];
    TpiShutRows(own, false, rows);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) Check(rows[r][c] == own[r][c], "rows: standing");
    TpiShutRows(own, true, rows);
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) Check(rows[r][c] == own[r][c] * kTpiShutFold, "rows: gone");
    Check(kTpiShutFold > 0.0f && kTpiShutFold < 1.0e-3f, "rows: folded small, never to zero");

    // The closed cells' quads in the level's copy: shown once it is gone, hidden again while it stands, else as they are.
    Check(TpiShutQuadChange(true, true) == 1 && TpiShutQuadChange(false, false) == -1, "quads: change");
    Check(TpiShutQuadChange(true, false) == 0 && TpiShutQuadChange(false, true) == 0, "quads: as they are");
    // Map cells closed again: plain land that belongs to nothing and is no path edge; a path, a building, a footbridge
    // or a hidden cell is kept.
    Check(TpiShutCellToClose(0, 0, 0x40), "cells: plain land");
    Check(!TpiShutCellToClose(1, 0, 0) && !TpiShutCellToClose(0, 12, 0) && !TpiShutCellToClose(0, 0, 0x20) &&
              !TpiShutCellToClose(7, 0, 0) && !TpiShutCellToClose(0x1e, 0, 0),
          "cells: kept");

    if (gFailures != 0) {
        std::printf("%d failed\n", gFailures);
        return gFailures;
    }
    std::printf("tpi obstacle shut: all passed\n");
    return 0;
}
