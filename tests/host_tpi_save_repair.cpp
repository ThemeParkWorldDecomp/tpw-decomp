// host_tpi_save_repair.cpp: the TPI Worlds and Rides mod's repair of parks saved before a converter showed more of a
// world's ground (include/TpiSaveRepair.h, TPW_MOD_HOOK tpi_worlds) on the host, without game data: which saved map and
// track cells are reset on loading. The cells are made up here, with the modes and flags CMapCell::Reset and
// CTrackCell::Reset give them. Then the repair of the shops of parks saved before converter 15: which saved RipOffOK
// values rise and which placed shops become choosable, with made-up records. Exits nonzero on the failed checks' count.
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

    // Shops (converter 15). A shop type's control saved with converter 14's RipOffOK: 0 where the shops folder named none,
    // 100 for most, 250 for the photo booth; its record now says more.
    Check(TpiRepairedRipOffOK(0, 100, 1) == 100, "a shop saved with RipOffOK 0: the record's 100");
    Check(TpiRepairedRipOffOK(100, 434, 1) == 434, "a shop saved with 100: the record's 434");
    Check(TpiRepairedRipOffOK(250, 345, 1) == 345, "the photo booth saved with 250: the record's 345");
    // Already right, or the record says less: kept.
    Check(TpiRepairedRipOffOK(434, 434, 1) == 434, "a shop saved after converter 15: kept");
    Check(TpiRepairedRipOffOK(300, 100, 1) == 300, "a record below the saved value: kept, nothing falls");
    // Not a shop (rides 0, the other kinds): kept, whatever the record says.
    Check(TpiRepairedRipOffOK(0, 100, 0) == 0, "a ride: kept");
    Check(TpiRepairedRipOffOK(0, 100, 2) == 0, "another kind: kept");
    Check(TpiRepairedRipOffOK(10, 50, 3) == 10, "a third kind: kept");

    // A photo booth built before converter 15: no flag 4, its record now says Info.IsChoosable 1.
    Check(TpiShopToMakeChoosable(0x40, 1, 1), "booth built without flag 4: made choosable");
    Check(TpiShopToMakeChoosable(0, 1, 1), "booth with no flags at all: made choosable");
    // Already choosable, a record that still says 0, or not a shop: kept.
    Check(!TpiShopToMakeChoosable(0x44, 1, 1), "a shop built choosable: kept");
    Check(!TpiShopToMakeChoosable(0x40, 0, 1), "a shop whose record says 0: kept");
    Check(!TpiShopToMakeChoosable(0x08, 1, 0), "a ride: kept");
    Check(!TpiShopToMakeChoosable(0, 0, 2), "an unchoosable add-on: kept");
    // Its state: 3 (built unchoosable) becomes the normal 0; broken down, wanting an upgrade or condemned stays.
    Check(TpiRepairedShopState(3) == 0, "state 3: normal");
    Check(TpiRepairedShopState(0) == 0, "normal: kept");
    Check(TpiRepairedShopState(1) == 1, "broken down: kept");
    Check(TpiRepairedShopState(2) == 2, "wants an upgrade: kept");
    Check(TpiRepairedShopState(4) == 4, "condemned: kept");
    // A second load: the flag is set and the state normal, so nothing changes again.
    Check(!TpiShopToMakeChoosable(0x40 | 4, 1, 1), "after the repair: kept");
    Check(TpiRepairedRipOffOK(100, 100, 1) == 100, "RipOffOK after the repair: kept");

    // Walkways (converter 16): base.map now gives a walkway cell attribute 0x80 alone over a quad the level hides, a
    // footbridge like Lost Kingdom's (mode 0x1e). A park saved before holds land, a path or a hidden quad's mode there.
    Check(TpiWalkwayToFootbridge(0, 0x80, 0, true), "walkway saved as land (converters 14, 15): footbridge");
    Check(TpiWalkwayToFootbridge(1, 0x80, 0, true), "a path on the walkway, the player's or the start path: footbridge");
    Check(TpiWalkwayToFootbridge(7, 0x80, 0, true), "walkway saved hidden over land (converter 12): footbridge");
    Check(TpiWalkwayToFootbridge(2, 0x80, 0, true), "walkway saved hidden over the river (attribute 3): footbridge");
    Check(TpiWalkwayToFootbridge(5, 0x80, 0, true), "walkway saved as shown land of attribute 1: footbridge");
    // Anything built there, a cell the level does not make a footbridge, or a quad the level shows: kept.
    Check(!TpiWalkwayToFootbridge(1, 0x80, 12, true), "a walkway cell that belongs to something: kept");
    Check(!TpiWalkwayToFootbridge(0, 0x01, 0, true), "a cell that is no walkway: kept");
    Check(!TpiWalkwayToFootbridge(0, 0x90, 0, true), "gate pavement (0x80 with 0x10): kept");
    Check(!TpiWalkwayToFootbridge(0, 0x80, 0, false), "a quad the level shows: kept");
    // A second load: the cell is a footbridge (mode 0x1e), so nothing changes again.
    Check(!TpiWalkwayToFootbridge(0x1e, 0x80, 0, true), "after the repair: kept");

    if (gFailures == 0) std::printf("tpi save repair: all checks passed\n");
    return gFailures;
}
