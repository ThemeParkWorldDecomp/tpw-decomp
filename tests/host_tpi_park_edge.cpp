// host_tpi_park_edge.cpp: the TPI Worlds and Rides mod's soft edge of the park's land (include/TpiParkEdge.h, TPW_MOD_HOOK
// tpi_worlds in CHField::Execute) on the host, without game data: which corners of a height field cell are darkened, each
// from the cell under its grid point, on made-up ownership maps. Inside land the park does not own every corner is dark
// and inside its own land none, as the original's per cell rule gives; across an edge the cells beside the edge's grid
// line get dark corners only on the line's far side, so the ground fades over one cell. Exits nonzero on the failed
// checks' count.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "TpiParkEdge.h"

namespace {

// A stand-in for the game's sHFieldQuad: the helper reads only mF00.
struct Quad {
    uint16_t mF00 = 0;
    uint16_t mOther = 0;
};

int gFailures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

std::string At(uint32_t cell, uint32_t row) {
    return "cell " + std::to_string(cell) + "," + std::to_string(row);
}

// A width x height map, every cell given `flags`.
std::vector<Quad> Map(uint32_t width, uint32_t height, uint16_t flags) {
    std::vector<Quad> quads(width * height);
    for (Quad& quad : quads) quad.mF00 = flags;
    return quads;
}

}  // namespace

int main() {
    using namespace tpw;
    Check(kQuadUnowned == 0x400, "the unowned bit is RecalculateParkDarkening's 0x400");

    // All owned: nothing darkened. All unowned: every corner of every cell, the original's four, the last row and column
    // included.
    {
        const auto owned = Map(4, 3, 0x2);
        const auto unowned = Map(4, 3, 0x402);
        for (uint32_t row = 0; row < 3; ++row) {
            for (uint32_t cell = 0; cell < 4; ++cell) {
                Check(TpiParkEdgeCorners(owned.data(), 4, 3, cell, row) == 0, "owned " + At(cell, row));
                Check(TpiParkEdgeCorners(unowned.data(), 4, 3, cell, row) == 0xf, "unowned " + At(cell, row));
            }
        }
    }

    // Only the 0x400 bit counts, whatever else the cell's flags hold.
    {
        auto quads = Map(2, 2, 0x3ff);
        Check(TpiParkEdgeCorners(quads.data(), 2, 2, 0, 0) == 0, "flags 0x3ff: owned");
        quads = Map(2, 2, 0xfc00);
        Check(TpiParkEdgeCorners(quads.data(), 2, 2, 0, 0) == 0xf, "flags 0xfc00: unowned");
    }

    // One unowned cell (2,1) in owned land: its grid point (2,1) is the one dark point, so each of the four cells around
    // that point darkens only the corner on it (corner order (c,r), (c,r+1), (c+1,r+1), (c+1,r)).
    {
        auto quads = Map(5, 4, 0);
        quads[2 + 1 * 5].mF00 = 0x400;
        for (uint32_t row = 0; row < 4; ++row) {
            for (uint32_t cell = 0; cell < 5; ++cell) {
                uint32_t want = 0;
                if (cell == 2 && row == 1) want = 1u << 0;
                if (cell == 2 && row == 0) want = 1u << 1;
                if (cell == 1 && row == 0) want = 1u << 2;
                if (cell == 1 && row == 1) want = 1u << 3;
                Check(TpiParkEdgeCorners(quads.data(), 5, 4, cell, row) == want, "lone unowned, " + At(cell, row));
            }
        }
    }

    // An edge across the columns at grid line 3, either way round: the cells on column 2 are the fade (dark on the line's
    // unowned side only, corners 2 and 3 or 0 and 1), every other cell is as the original draws it.
    for (bool unownedEast : {true, false}) {
        auto quads = Map(6, 3, 0);
        for (uint32_t row = 0; row < 3; ++row) {
            for (uint32_t cell = 0; cell < 6; ++cell) {
                if ((cell >= 3) == unownedEast) quads[cell + row * 6].mF00 = 0x400;
            }
        }
        const std::string side = unownedEast ? "unowned east, " : "unowned west, ";
        for (uint32_t row = 0; row < 3; ++row) {
            for (uint32_t cell = 0; cell < 6; ++cell) {
                uint32_t want = ((quads[cell + row * 6].mF00 & 0x400) != 0) ? 0xfu : 0u;
                if (cell == 2) want = unownedEast ? 0xcu : 0x3u;
                Check(TpiParkEdgeCorners(quads.data(), 6, 3, cell, row) == want, side + At(cell, row));
            }
        }
    }

    // The same across the rows at grid line 2: the cells on row 1 fade.
    for (bool unownedNorth : {true, false}) {
        auto quads = Map(3, 5, 0);
        for (uint32_t row = 0; row < 5; ++row) {
            for (uint32_t cell = 0; cell < 3; ++cell) {
                if ((row >= 2) == unownedNorth) quads[cell + row * 3].mF00 = 0x400;
            }
        }
        const std::string side = unownedNorth ? "unowned north, " : "unowned south, ";
        for (uint32_t row = 0; row < 5; ++row) {
            for (uint32_t cell = 0; cell < 3; ++cell) {
                uint32_t want = ((quads[cell + row * 3].mF00 & 0x400) != 0) ? 0xfu : 0u;
                if (row == 1) want = unownedNorth ? 0x6u : 0x9u;
                Check(TpiParkEdgeCorners(quads.data(), 3, 5, cell, row) == want, side + At(cell, row));
            }
        }
    }

    // The last column's and row's far grid points are no cell's first: they take the cell before, so an unowned last
    // column or row stays dark to the map's edge and an owned one stays light.
    {
        auto quads = Map(3, 3, 0);
        for (uint32_t row = 0; row < 3; ++row) quads[2 + row * 3].mF00 = 0x400;
        for (uint32_t row = 0; row < 3; ++row) {
            Check(TpiParkEdgeCorners(quads.data(), 3, 3, 2, row) == 0xf, "unowned last column, " + At(2, row));
            Check(TpiParkEdgeCorners(quads.data(), 3, 3, 1, row) == 0xc, "beside it, " + At(1, row));
        }
        quads = Map(3, 3, 0x400);
        for (uint32_t cell = 0; cell < 3; ++cell) quads[cell + 2 * 3].mF00 = 0;
        for (uint32_t cell = 0; cell < 3; ++cell) {
            Check(TpiParkEdgeCorners(quads.data(), 3, 3, cell, 2) == 0, "owned last row, " + At(cell, 2));
            Check(TpiParkEdgeCorners(quads.data(), 3, 3, cell, 1) == 0x9, "below it, " + At(cell, 1));
        }
    }

    // No map: nothing darkened, nothing read.
    Check(TpiParkEdgeCorners<Quad>(nullptr, 4, 4, 0, 0) == 0, "no quads");
    {
        const auto quads = Map(1, 1, 0x400);
        Check(TpiParkEdgeCorners(quads.data(), 0, 1, 0, 0) == 0, "width 0");
        Check(TpiParkEdgeCorners(quads.data(), 1, 0, 0, 0) == 0, "height 0");
        Check(TpiParkEdgeCorners(quads.data(), 1, 1, 0, 0) == 0xf, "one cell, unowned");
    }

    if (gFailures == 0) std::printf("tpi_park_edge: all checks passed\n");
    return gFailures;
}
