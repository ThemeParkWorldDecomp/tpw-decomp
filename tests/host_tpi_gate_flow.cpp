// host_tpi_gate_flow.cpp: the TPI Worlds and Rides mod's walks outside the park gate (include/TpiGateFlow.h,
// TPW_MOD_HOOK tpi_worlds) on the host, without game data: the four cells a visitor leaving waits for the bus on, with
// Theme Park World's rows and the mod's, the crossing's bus side in the mod's worlds, and when the mod's walks are used.
// The cells are made up here (a cell id is x + (y << 7) + 1). Exits nonzero on the first failed check's count.
#include <cstdint>
#include <cstdio>
#include <string>

#include "TpiGateFlow.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

static uint16_t Cell(uint32_t x, uint32_t y) { return static_cast<uint16_t>(x + (y << 7) + 1u); }

int main() {
    using namespace tpw;
    // Theme Park World's four: the stop, east of it, and the same two rows up, as the Mac's -256 on the cell id.
    const uint16_t stop = Cell(42, 5);
    Check(TpiBusStopCell(stop, 0, kTpwBusStopPairRows) == stop, "tpw: the stop");
    Check(TpiBusStopCell(stop, 1, kTpwBusStopPairRows) == Cell(43, 5), "tpw: east of the stop");
    Check(TpiBusStopCell(stop, 2, kTpwBusStopPairRows) == Cell(42, 3), "tpw: two rows up");
    Check(TpiBusStopCell(stop, 3, kTpwBusStopPairRows) == Cell(43, 3), "tpw: two rows up, east");
    for (int32_t i = 0; i < 4; ++i) {
        const int32_t dx = i & 1;
        const int32_t dy = (i & 2) != 0 ? -256 : 0;
        Check(TpiBusStopCell(stop, i, kTpwBusStopPairRows) == static_cast<uint16_t>(dx + dy + stop), "tpw: the Mac's sum, cell " + std::to_string(i));
    }

    // The mod's four: the second pair is the row below the stop.
    const uint16_t tpiStop = Cell(50, 2);
    Check(TpiBusStopCell(tpiStop, 0, kTpiBusStopPairRows) == tpiStop, "tpi: the stop");
    Check(TpiBusStopCell(tpiStop, 1, kTpiBusStopPairRows) == Cell(51, 2), "tpi: east of the stop");
    Check(TpiBusStopCell(tpiStop, 2, kTpiBusStopPairRows) == Cell(50, 3), "tpi: the row below");
    Check(TpiBusStopCell(tpiStop, 3, kTpiBusStopPairRows) == Cell(51, 3), "tpi: the row below, east");

    // The sum is cut to 16 bits, as the Mac's halfword store: a stop on row 0 with rows up wraps.
    const uint16_t edge = Cell(10, 0);
    Check(TpiBusStopCell(edge, 2, kTpwBusStopPairRows) == static_cast<uint16_t>(edge - 256), "wrap: two rows up from row 0");
    Check(TpiBusStopCell(edge, 3, kTpwBusStopPairRows) == static_cast<uint16_t>(edge - 255), "wrap: two rows up, east, from row 0");
    Check(TpiBusStopCell(0xffff, 3, kTpiBusStopPairRows) == static_cast<uint16_t>(0xffff + 1 + 128), "wrap: past the top of 16 bits");

    // The crossing's bus side: the crossing's column on the row below bus stop B.
    const uint16_t stopB = Cell(41, 2);
    Check(TpiCrossingBusSide(Cell(47, 5), stopB) == Cell(47, 3), "crossing: column 47 on row 3");
    Check(TpiCrossingBusSide(Cell(48, 5), stopB) == Cell(48, 3), "crossing: column 48 on row 3");
    Check(TpiCrossingBusSide(Cell(0, 9), Cell(127, 0)) == Cell(0, 1), "crossing: column 0, stop at the row's end");
    Check(TpiCrossingBusSide(Cell(127, 20), Cell(5, 6)) == Cell(127, 7), "crossing: column 127");
    Check(TpiCrossingBusSide(Cell(47, 5), Cell(60, 4)) == Cell(47, 5), "crossing: already on the row below the stop");

    // When the mod's walks are used: only with the hook on and the theme noted as one of the mod's worlds.
    Check(!gTpiThemeIsMod && !TpiWorldInPlay(), "before any theme: Mac's walks");
    for (bool flag : {false, true}) {
        gModTpiWorlds = flag;
        const std::string state = flag ? "on" : "off";
        for (const char* theme : {"arabian", "water", "science", "WATER"}) {
            TpiNoteTheme(theme);
            Check(gTpiThemeIsMod, state + ": noted as a mod world: " + theme);
            Check(TpiWorldInPlay() == flag, state + ": the mod's walks only with the hook on: " + theme);
        }
        for (const char* theme : {"jungle", "hallow", "fantasy", "space", "", "polar"}) {
            TpiNoteTheme(theme);
            Check(!gTpiThemeIsMod && !TpiWorldInPlay(), state + ": Mac's walks in the original or unknown " + theme);
        }
        TpiNoteTheme("water");
        TpiNoteTheme(nullptr);
        Check(!gTpiThemeIsMod && !TpiWorldInPlay(), state + ": no theme after a mod world: Mac's walks");
    }
    // The hook switched off after a mod world was noted: the Mac's walks.
    gModTpiWorlds = true;
    TpiNoteTheme("science");
    gModTpiWorlds = false;
    Check(!TpiWorldInPlay(), "hook off after a mod world: Mac's walks");
    TpiNoteTheme(nullptr);

    if (gFailures != 0) return gFailures;
    std::printf("host_tpi_gate_flow: all checks passed\n");
    return 0;
}
