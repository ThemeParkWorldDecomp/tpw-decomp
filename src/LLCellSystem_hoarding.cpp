// LLCellSystem.cpp range of the main app (SimTheme Park 2.04b3), unit w133: LLCC_DrawInitialHoarding (0x10095420, 600 bytes).
#include <cstddef>
#include <cstdint>

#include "HoardingsBalance.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "TPDialog.h"
#include "World.h"
#include "analyser.h"

int32_t gHoardingLeft = -1;     // 0x1029e044
int32_t gHoardingTop = -1;      // 0x1029e048
int32_t gHoardingRight = -1;    // 0x1029e03c
int32_t gHoardingBottom = -1;   // 0x1029e040
uint8_t gLLCCE0CC = 0;           // 0x102de0cc

// The park's first hoarding. The extents come from the park entrances' cell ids (1-based: row = (id - 1) >> 7 (a logical shift), column = (id - 1) & 0x7f):
// left = column - 4 and top = row of entrance A (0), right = column + 3 and bottom = row of entrance B (1), all four in the globals. The cell
// system's mode is set to 0x19 while the hoardings are drawn (from the balance file, or the default ones when dialog option 0x400
// is set), the inside of the park is filled between locking and unlocking, and the hoarding database is built from the extents. Then, with
// mode 0, the two rows of cells at bottom and bottom + 1 from column left + 2 up to right - 1 lose cell flag 0x40 and get their height
// field tile set (the cell index is 16-bit: ushort(x + row * 128 + 1) - 1). The flag byte gLLCC4818 is 1 meanwhile, and the park's
// darkening is recalculated at the end.
void LLCC_DrawInitialHoarding() {
    gLLCC4818 = 1;
    const uint16_t first = gGameWorld->GetMap()->GetParkEntrancePos(0).mIndex;
    const uint16_t second = gGameWorld->GetMap()->GetParkEntrancePos(1).mIndex;
    gHoardingLeft = ((static_cast<int32_t>(first) - 1) & 0x7f) - 4;
    gHoardingTop = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(first) - 1) >> 7);
    gHoardingRight = ((static_cast<int32_t>(second) - 1) & 0x7f) + 3;
    gHoardingBottom = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(second) - 1) >> 7);
    gLLCCE0CC = 4;
    LLCC_SystemSetMode(0x19, true);
    LLCM_SetCurrentObject(true);
    if ((gTPDialogOptions & 0x400) == 0) {
        LLCC_DrawHoardingsFromBalanceFile(&gHoardingsBalance434C);
    } else {
        LLCC_DrawDefaultHoardings();
    }
    LLCC_LockHoarding(true);
    LLCC_FillInsideOfPark();
    LLCC_LockHoarding(false);
    LLCC_BuildHoardingDB(gHoardingRight, gHoardingBottom, gHoardingLeft, gHoardingTop, 4, true);
    LLCC_SystemSetMode(0, true);
    for (int32_t x = gHoardingLeft + 2; x < gHoardingRight; ++x) {
        char* map = reinterpret_cast<char*>(gGameWorld->GetMap());
        const int32_t indexA = static_cast<int32_t>(static_cast<uint16_t>(x + static_cast<int32_t>(static_cast<uint32_t>(gHoardingBottom) << 7) + 1)) - 1;
        CMapCell* cell = reinterpret_cast<CMapCell*>(map + static_cast<ptrdiff_t>(indexA) * 0x44);
        cell->UnSetFlag(0x40);
        cell->SetHFieldTile();
        map = reinterpret_cast<char*>(gGameWorld->GetMap());
        const int32_t indexB = static_cast<int32_t>(static_cast<uint16_t>(x + (gHoardingBottom + 1) * 0x80 + 1)) - 1;
        cell = reinterpret_cast<CMapCell*>(map + static_cast<ptrdiff_t>(indexB) * 0x44);
        cell->UnSetFlag(0x40);
        cell->SetHFieldTile();
    }
    gLLCC4818 = 0;
    RecalculateParkDarkening();
}
