// host_tpi_save_repair.cpp: the TPI Worlds and Rides mod's repair of parks saved before a converter showed more of a
// world's ground (include/TpiSaveRepair.h, TPW_MOD_HOOK tpi_worlds) on the host, without game data: which saved map and
// track cells are reset on loading. The cells are made up here, with the modes and flags CMapCell::Reset and
// CTrackCell::Reset give them. Exits nonzero on the first failed check's count.
#include <cstdint>
#include <cstdio>
#include <string>

#include "TpiSaveRepair.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main() {
    using namespace tpw;
    // The hidden quad's mode, from the attribute byte (CMapCell::Reset's else branch).
    Check(TpiHiddenQuadMode(0) == 7, "plain attribute: 7");
    Check(TpiHiddenQuadMode(0x10) == 7, "a flag-only bit: 7");
    Check(TpiHiddenQuadMode(2) == 2, "bit 2: 2");
    Check(TpiHiddenQuadMode(0x80) == 0x1e, "bit 0x80: 0x1e");
    Check(TpiHiddenQuadMode(0x82) == 2, "bits 2 and 0x80: 2 first");

    // A bridge walkway cell saved with converter 12: mode 7, no parent, the level's quad now shown.
    Check(TpiStaleHiddenMapCell(7, 0, 0, 0x40, true), "walkway cell saved hidden, shown now: reset");
    Check(TpiStaleHiddenMapCell(7, 0x10, 0, 0x40 | 0x80, true), "with the attribute's flag 0x80: reset");
    Check(TpiStaleHiddenMapCell(7, 4, 0, 0x40 | 0x400, true), "with the attribute's flag 0x400: reset");
    Check(TpiStaleHiddenMapCell(2, 2, 0, 0x40, true), "bit 2 cell saved hidden (mode 2): reset");
    Check(TpiStaleHiddenMapCell(0x1e, 0x80, 0, 0x40, true), "bit 0x80 cell saved hidden (mode 0x1e): reset");
    // Inside the park's start land (0x40 clear): reset too, and stays the park's.
    Check(TpiStaleHiddenMapCell(7, 0, 0, 0, true), "inside the park (no 0x40): reset");
    Check(TpiStaleHiddenMapCell(2, 2, 0, 0, true), "bit 2 cell inside the park: reset");
    // The quad still hidden: the saved mode is the right one.
    Check(!TpiStaleHiddenMapCell(7, 0, 0, 0x40, false), "still hidden: kept");
    // A new park's cell (Reset with the quad shown): mode 0 or a path, never the hidden mode.
    Check(!TpiStaleHiddenMapCell(0, 0, 0, 0x40, true), "land outside the park: kept");
    Check(!TpiStaleHiddenMapCell(1, 8, 0, 0x20, true), "a start path: kept");
    // A mode that is not the hidden one of this attribute.
    Check(!TpiStaleHiddenMapCell(7, 2, 0, 0x40, true), "mode 7 with bit 2 (hidden mode 2): kept");
    Check(!TpiStaleHiddenMapCell(2, 0, 0, 0x40, true), "mode 2 with a plain attribute: kept");
    Check(!TpiStaleHiddenMapCell(0x1e, 0, 0, 0x40, true), "mode 0x1e with a plain attribute: kept");
    // Cells the park owns, builds on or edges: never touched.
    Check(!TpiStaleHiddenMapCell(7, 0, 12, 0x40, true), "belongs to something: kept");
    Check(!TpiStaleHiddenMapCell(7, 0, 0, 0x40 | 0x20, true), "a path edge: kept");

    // The flags a reset cell keeps: Reset's own, with the saved 0x40.
    Check(TpiRepairedFlags(0, 0x40) == 0, "inside the park: 0x40 cleared again");
    Check(TpiRepairedFlags(0x40, 0x40) == 0x40, "outside the park: 0x40 kept");
    Check(TpiRepairedFlags(0, 0x40 | 0x80 | 0x400) == (0x80 | 0x400), "the attribute's flags kept, inside the park");
    Check(TpiRepairedFlags(0x40 | 0x80, 0x40 | 0x400) == (0x40 | 0x400), "Reset's flags, not the saved ones, with 0x40");
    Check(TpiRepairedFlags(0x40, 0x20) == 0x20, "a start path edge: Reset's flags");
    Check(TpiRepairedFlags(0x40, 0x20 | 0x80) == (0x20 | 0x80), "a start path edge with 0x80: Reset's flags");

    // Track cells: 7 for a hidden quad, 0 for a shown one (CTrackCell::Reset).
    Check(TpiStaleHiddenTrackCell(7, 0, true), "track cell saved hidden, shown now: reset");
    Check(!TpiStaleHiddenTrackCell(7, 0, false), "track cell still hidden: kept");
    Check(!TpiStaleHiddenTrackCell(0, 0, true), "track cell of shown ground: kept");
    Check(!TpiStaleHiddenTrackCell(7, 5, true), "track cell that belongs to something: kept");

    // A second load: a reset cell (mode 0, outside the park) no longer matches.
    Check(!TpiStaleHiddenMapCell(0, 0, 0, 0x40, true), "after the reset: kept");
    Check(!TpiStaleHiddenTrackCell(0, 0, true), "track cell after the reset: kept");

    if (gFailures == 0) std::printf("tpi save repair: all checks passed\n");
    return gFailures;
}
