// LLCC_DrawKartBlock 0x10089cfc (1308 bytes): marks the four cells of a 2x2 kart track block.
#include <cstdint>

#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "World.h"

int32_t LLCC_DrawKartBlock(int32_t x, int32_t y, int32_t /*unused*/, int32_t flagA, int32_t flagB, uint8_t direction) {
    if (!CMap::IsXYValid(x, y)) return 1;
    if (!CMap::IsXYValid(x, y + 1)) return 1;
    if (!CMap::IsXYValid(x + 1, y)) return 1;
    if (!CMap::IsXYValid(x + 1, y + 1)) return 1;

    CMapCell* base = reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8);
    auto at = [&](int32_t index) {
        uint16_t i = static_cast<uint16_t>(index);
        return base + (static_cast<int32_t>(i) - 1);
    };
    CMapCell* c0 = at(x + y * 128 + 1);
    CMapCell* c1 = at(x + y * 128 + 2);
    CMapCell* c2 = at(x + (y + 1) * 128 + 1);
    CMapCell* c3 = at(x + (y + 1) * 128 + 2);
    // order of the original's registers: r29 = c0, r28 = c2, r27 = c1, r30 = c3
    CMapCell* cells[4] = {c0, c2, c1, c3};

    int32_t track = LLCC_SystemGetRideTrack();
    if (track != 1 && track != 2) return 1;
    int32_t checked = track == 1 ? 0x20b : 0x210;
    int32_t plain = track == 1 ? 0x10b : 0x110;

    if (flagB != 0) {
        for (CMapCell* cell : cells) {
            if (cell) {
                uint32_t d = ADD_DIRECTION(direction, 0x10);
                if (cell->Modify(static_cast<CELL_MODES>(checked), 5, static_cast<int32_t>(d & 0xff)) == 0) return 0;
            }
        }
    } else if (flagA == 0) {
        for (CMapCell* cell : cells) {
            if (cell) {
                uint32_t d = ADD_DIRECTION(direction, 0x10);
                cell->Modify(static_cast<CELL_MODES>(plain), 5, static_cast<int32_t>(d & 0xff));
            }
        }
    }
    return 1;
}
