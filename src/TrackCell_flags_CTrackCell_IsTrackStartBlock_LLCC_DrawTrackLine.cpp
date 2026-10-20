#include <cstdint>
#include <cstdlib>
#include <new>

#include "CellBase.h"
#include "TrackCell.h"
#include "TbDynamicStringTemplate.h"
#include "ui_publishParkPanel.h"
#include "World.h"

// LLCellSystem.h declares this void, but the listing answers r3 (1, or 0 when a cell refused); the same mangled name, so it is
// declared here with its real return type.
int32_t LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY);

namespace {

// 0x1009dfe4 and CMap::IsXYValid are the callees; the cell is the track cell of the map cell (x + (y << 7) + 1), 16 bits.
CTrackCell* CellAt(int32_t x, int32_t y) {
    const uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    return &gGameWorld->mTrackCells[static_cast<int32_t>(id) - 1];
}

// Rounds the distance down to an even number (the listing's srawi/addze/slwi), returns the step towards the end.
int32_t Snap(int32_t from, int32_t* end) {
    int32_t d = from - *end;
    if (d > 0 && d != (d / 2) * 2) d = d + 1;
    *end = from - (d / 2) * 2;
    if (from > *end) return -2;
    if (from < *end) return 2;
    return 0;
}

}  // namespace

// 0x100a30f8 (912 bytes): draws a track line of mode `mode` from (x, y) along the longer axis, two cells per step; the end is
// moved to an even distance. gTrackNbrOn485C is 1 during the call; gLLCC4858 is 1 for the last cell.
int32_t LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    gTrackNbrOn485C = 1;
    gLLCC4858 = 0;
    const int32_t dy = std::abs(*endY - y);
    const int32_t dx = std::abs(*endX - x);
    if (dx > dy) {
        const int32_t step = Snap(x, endX);
        while (x != *endX) {
            if (CMap::IsXYValid(x, y) != 0) {
                if (CellAt(x, y)->ModifyBody(mode, step, 0) == 0) {
                    gTrackNbrOn485C = 0;
                    return 0;
                }
                gTrackNbrOn485C = 0;
            }
            x += step;
        }
        gLLCC4858 = 1;
        if (CMap::IsXYValid(x, y) != 0) {
            if (CellAt(x, y)->ModifyBody(mode, step, 0) == 0) {
                gTrackNbrOn485C = 0;
                gLLCC4858 = 0;
                return 0;
            }
            gTrackNbrOn485C = 0;
            gLLCC4858 = 0;
        }
    } else {
        const int32_t step = Snap(y, endY);
        while (y != *endY) {
            if (CMap::IsXYValid(x, y) != 0) {
                if (CellAt(x, y)->ModifyBody(mode, 0, step) == 0) {
                    gTrackNbrOn485C = 0;
                    return 0;
                }
                gTrackNbrOn485C = 0;
            }
            y += step;
        }
        gLLCC4858 = 1;
        if (CMap::IsXYValid(x, y) != 0) {
            if (CellAt(x, y)->ModifyBody(mode, 0, step) == 0) {
                gTrackNbrOn485C = 0;
                gLLCC4858 = 0;
                return 0;
            }
            gTrackNbrOn485C = 0;
            gLLCC4858 = 0;
        }
    }
    return 1;
}

// ---- 0x100a6be0 (148 bytes): the static initialiser of lobby.cpp. The objects and registration nodes have no known names (invented;
// the Mac addresses are in the comments).
void UIMain_DestroyWString(void* object);   // 0x103bcbb8
void UIMain_DestroyString(void* object);    // 0x103bcd38
extern "C" void __register_global_object(void* object, void* destructor, void* node);

alignas(8) uint8_t gLobbyWString48AC[64];   // 0x102e48ac
alignas(8) uint8_t gLobbyWString488C[64];   // 0x102e488c
uint8_t gLobbyNode48C0[16];                 // 0x102e48c0
uint8_t gLobbyNode48A0[16];                 // 0x102e48a0
uint8_t gLobbyNode487C[16];                 // 0x102e487c

// The strings: 0x1023e586 (wide), 0x1023e586 + 0xe (wide), 0x1023e585 (narrow, empty).
static const char16_t kLobbyW1[] = u"upload";
static const char16_t kLobbyW2[] = u"gms.dat";

void __sinit_lobby_cpp() {
    using W = TbDynamicStringTemplate<char16_t>;
    void* wide1 = new (static_cast<void*>(gLobbyWString48AC)) W(kLobbyW1);
    __register_global_object(wide1, reinterpret_cast<void*>(&UIMain_DestroyWString), gLobbyNode48C0);
    void* wide2 = new (static_cast<void*>(gLobbyWString488C)) W(kLobbyW2);
    __register_global_object(wide2, reinterpret_cast<void*>(&UIMain_DestroyWString), gLobbyNode48A0);
    void* narrow = new (static_cast<void*>(&gOnlineUploadFile)) TbDynamicStringTemplate<char>("");
    __register_global_object(narrow, reinterpret_cast<void*>(&UIMain_DestroyString), gLobbyNode487C);
}
