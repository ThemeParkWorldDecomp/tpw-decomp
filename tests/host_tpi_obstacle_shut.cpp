// host_tpi_obstacle_shut.cpp: the TPI Worlds and Rides mod's obstacles that stay until their land is bought
// (include/TpiObstacleShut.h, TPW_MOD_HOOK tpi_worlds) on the host, without game data: a node's name read back as the
// converter writes it (assets/tpi_terrain.h TpiShutName), with and without the cells it closed, the names it turns down,
// the rule that clears it (the gate's: the park owns a cell of its footprint or a cell next to it that a new park does
// not start with), the folded node and what its closed cells' quads and map cells get, and that a loaded park's paths
// and queues over its cleared cells are still drawn once the quads are shown. The names, the land and the tiles are made
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

    // What a closed cell gets when its quad changes. Shown again: the hidden quad's mode (7, attribute 0) and plain land
    // are reset; a path, a queue or a ride is kept. Hidden again: plain land is reset, the rest kept.
    Check(TpiShutResetsCell(1, 7, 0, 0, 0) && TpiShutResetsCell(1, 0, 0, 0, 0x40) && TpiShutResetsCell(-1, 0, 0, 0, 0),
          "cells: reset");
    Check(!TpiShutResetsCell(1, 1, 0, 0, 0) && !TpiShutResetsCell(1, 4, 0, 12, 0) && !TpiShutResetsCell(1, 0, 0, 0, 0x20) &&
              !TpiShutResetsCell(-1, 1, 0, 0, 0) && !TpiShutResetsCell(-1, 7, 0, 0, 0) && !TpiShutResetsCell(0, 0, 0, 0, 0),
          "cells: kept, not reset");

    // A loaded park with the obstacle's land bought: the load draws each cell's own tile (CMap::UpdateMapInEngine), then
    // the first tick shows the closed quads, drawing the copy's tile (0x37) over each, then resets or redraws the cell.
    // Whatever is built there must still be drawn after it, plain land is the copy's. The tiles here are made up: 0x37
    // the copy's, anything else the cell's own.
    struct Cell {
        int32_t mode;
        uint8_t attribute;
        uint16_t parent, flags;
        uint32_t own;   // the tile its mode draws (CMapCell::SetHFieldTile; plain land and 5 draw the copy's)
    };
    const Cell cells[] = {
        {1, 0, 0, 0x00, 0x10093},   // a path laid over the cleared rock after its land was bought
        {1, 0, 0, 0x20, 0x10081},   // a path edge
        {4, 0, 0, 0x00, 0x20002},   // a queue
        {3, 0, 0, 0x00, 0x10082},   // a path's corner
        {0x12, 0, 9, 0x00, 0x30004},   // a ride's cell, belonging to it
        {0, 0, 0, 0x40, 0x37},      // plain land
        {7, 0, 0, 0x00, 0x37},      // the hidden quad's mode a park saved before the land was bought: reset as land
    };
    for (const Cell& c : cells) {
        const std::string name = "load: mode " + std::to_string(c.mode) + " flags " + std::to_string(c.flags);
        uint32_t drawn = c.mode == 7 ? 0u : c.own;   // UpdateMapInEngine (a mode 7 cell's tile hides its quad)
        const int32_t change = TpiShutQuadChange(true, true);
        Check(change == 1, name + ": the quad is shown");
        drawn = 0x37;   // SetHeightfieldTexture's copy tile
        const bool reset = TpiShutResetsCell(change, c.mode, c.attribute, c.parent, c.flags);
        if (reset) drawn = 0x37;   // CMapCell::Reset: plain land, its tile drawn
        if (TpiShutRedrawsCell(change, reset, c.mode)) drawn = c.own;
        Check(drawn == c.own, name + ": drawn as its own after the quad is shown");
    }
    // Nothing is drawn again when no quad changed or one is hidden again, nor over a reset cell, nor for plain land or a
    // hidden quad's mode, whose tile would hide the quad just shown.
    Check(!TpiShutRedrawsCell(0, false, 1) && !TpiShutRedrawsCell(-1, false, 1) && !TpiShutRedrawsCell(1, true, 1),
          "redraw: only a shown quad's kept cell");
    for (int32_t mode : {0, 2, 5, 7, 0x1e}) Check(!TpiShutRedrawsCell(1, false, mode), "redraw: not mode " + std::to_string(mode));

    if (gFailures != 0) {
        std::printf("%d failed\n", gFailures);
        return gFailures;
    }
    std::printf("tpi obstacle shut: all passed\n");
    return 0;
}
