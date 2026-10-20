#include <cstdint>
#include "MapCell.h"
#include "World.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"
#include "platform/arena.h"

namespace {

int32_t Abs(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }

// The cell the listing computes: world + 0x2d8 + (u16(x + (y << 7) + 1) - 1) * 0x44.
CMapCell* LineCell(int32_t x, int32_t y) {
    uint16_t index = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
    return &gGameWorld->mMapCells[static_cast<int32_t>(index) - 1];
}

}  // namespace

// 0x1009c0d0 (768 bytes): walks from (x, y) to the end point along the longer axis (the x axis when |dx| > |dy|), Modify on each valid
// cell with the step on that axis; the last cell is done with gValidTypeChangeLock set. Answers 0 as soon as a Modify refuses.
void LLCC_DrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    LLCC_DrawLineBody(mode, x, y, endX, endY);
}

int32_t LLCC_DrawLineBody(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    gCalcNeighboursJoinEntry = 1;
    gValidTypeChangeLock = 0;
    int32_t dy = Abs(*endY - y);
    int32_t dx = Abs(*endX - x);
    if (dx > dy) {
        int32_t step = (x > *endX) ? -1 : 1;
        for (; x != *endX; x += step) {
            if (CMap::IsXYValid(x, y)) {
                if (LineCell(x, y)->Modify(mode, step, 0) == 0) {
                    gCalcNeighboursJoinEntry = 0;
                    return 0;
                }
                gCalcNeighboursJoinEntry = 0;
            }
        }
        gValidTypeChangeLock = 1;
        if (CMap::IsXYValid(x, y)) {
            if (LineCell(x, y)->Modify(mode, step, 0) == 0) {
                gCalcNeighboursJoinEntry = 0;
                gValidTypeChangeLock = 0;
                return 0;
            }
            gCalcNeighboursJoinEntry = 0;
            gValidTypeChangeLock = 0;
        }
    } else {
        int32_t step = (y > *endY) ? -1 : 1;
        for (; y != *endY; y += step) {
            if (CMap::IsXYValid(x, y)) {
                if (LineCell(x, y)->Modify(mode, 0, step) == 0) {
                    gCalcNeighboursJoinEntry = 0;
                    return 0;
                }
                gCalcNeighboursJoinEntry = 0;
            }
        }
        gValidTypeChangeLock = 1;
        if (CMap::IsXYValid(x, y)) {
            if (LineCell(x, y)->Modify(mode, 0, step) == 0) {
                gCalcNeighboursJoinEntry = 0;
                gValidTypeChangeLock = 0;
                return 0;
            }
            gCalcNeighboursJoinEntry = 0;
            gValidTypeChangeLock = 0;
        }
    }
    return 1;
}

// CLBGlobe::~CLBGlobe (0x100ac534) is in src/lobby_globe_CLBGlobe_dtor.cpp: the globe's vtable comes with it, and unit v5923 links it too.

// 0x100ae374 (172 bytes): the online globe's handling, then message 0x10005 with a == 0 and a picked city flag (the word at 0x70, an
// arena offset): the selection becomes the flag's city, the globe hides and the online islands show.
void CLBScreenComponentDownloadGlobe::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    CLBScreenComponentOnlineGlobe_WindowMessagesFn(this, message, a, b);
    if (message == 0x10005 && a == 0 && mF70 != 0) {
        CLBGlobeCityFlag* flag = reinterpret_cast<CLBGlobeCityFlag*>(tpw::gArenaBase + static_cast<uint32_t>(mF70));
        gOnlineParkSelection = reinterpret_cast<OnlineParkSelection*>(const_cast<CFSOnlineCity*>(flag->mCity));
        Hide();
        gLobbyScreens->mOnlineIslands->Show();
    }
}
