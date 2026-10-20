#include <cstdint>
#include <cstring>
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "World.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "RideDB.h"
#include "UIInterface.h"
#include "ThingReceivers.h"
#include "BMP_InsideArea.h"
#include "ObjectParts.h"
#include "analyser.h"
#include "Debug.h"
#include "ppc_math.h"
#include "platform/arena.h"
#include "TrackCell.h"
#include "CellBase.h"
#include "HLSprite.h"
#include "ESP_sprites.h"
#include "intrface.h"
#include "rsse.h"

// Globals of the two static initialisers that no header names.
int32_t gLLCCDB74;      // 0x102ddb74
int32_t gLLCCDB78;      // 0x102ddb78
int32_t gIntrfaceDC1D8; // 0x102dc1d8
int32_t gIntrfaceDC1DC; // 0x102dc1dc

namespace {
// gLLCCE094 is a 32-bit pointer on the Mac (the selected ride record's word 0x18): an offset from the arena base here, like every Ptr32.
inline const uint8_t* RideCellAddress(int32_t col, int32_t row, uint32_t extra) {
    uint32_t offset = static_cast<uint32_t>(gLLCCE094) + static_cast<uint32_t>(row) * 0xa0u + static_cast<uint32_t>(col) * 8u + extra;
    return tpw::gArenaBase + offset;
}
}  // namespace

// 0x1008bccc
int32_t LLCR_GetRideCell(int32_t col, int32_t row) {
    return *reinterpret_cast<const int32_t*>(RideCellAddress(col, row, 8));
}

// 0x1008bd14
uint8_t LLCR_GetRideCellDirection(int32_t col, int32_t row) {
    return *RideCellAddress(col, row, 0xc);
}

// 0x1008bd68
void LLCR_SetTopLeft(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    gLLCCE092 = cell.mIndex;
}

// 0x1008bdcc
void LLCR_GetWidthHeight(int32_t* width, int32_t* height) {
    const uint8_t* record = tpw::gArenaBase + static_cast<uint32_t>(gLLCCE094);
    *width = *reinterpret_cast<const int32_t*>(record);
    record = tpw::gArenaBase + static_cast<uint32_t>(gLLCCE094);
    *height = *reinterpret_cast<const int32_t*>(record + 4);
}

// 0x1008c7b0 (the same machine code as LLCC_SetTrackAnimReverse, walking from the start of the track)
void LLCC_SetTrackAnim(CObject* object, bool flag) {
    const uint8_t* obj = reinterpret_cast<const uint8_t*>(object);
    uint32_t packed = static_cast<uint16_t>(obj[4] + (obj[6] << 7) + 1);
    int32_t x = static_cast<int32_t>((packed - 1) & 0x7f);
    int32_t y = static_cast<int32_t>((packed - 1) >> 7);
    int32_t rotation = *reinterpret_cast<const int32_t*>(obj + 0x10);
    uint8_t direction = LLCR_OffsetStartOfTrack(&x, &y, rotation);
    int32_t cx = x;
    int32_t cy = y;
    bool keepGoing = true;
    do {
        int32_t index = static_cast<int32_t>(static_cast<uint16_t>(static_cast<uint32_t>(cx) + (static_cast<uint32_t>(cy) << 7) + 1)) - 1;
        char* address = reinterpret_cast<char*>(gGameWorld) + 0x1102d8 + index * 0x28;
        CTrackCell* cell = reinterpret_cast<CTrackCell*>(address);
        cell->GetMapCellEquiv();
        if (cell->IsTrack() == 0) {
            direction = 0;
        } else {
            if (cell->IsCorner() != 0) {
                uint8_t opposite = OPPOSITE(direction);
                if ((opposite & cell->GetNeighbours()) == 0) opposite = direction;
                direction = static_cast<uint8_t>(opposite ^ cell->GetNeighbours());
            }
            uint32_t mesh = cell->mMeshInstance;
            if (mesh != 0) {
                SetCurRideAnim(gRideAnimSets[mesh], flag ? 5 : 0xc, 0, 7, 1.0f, 0);
            }
        }
        if (cell->IsCorner() == 0 && (cell->GetNeighbours() & direction) == 0) keepGoing = false;
        CTrackCell* next = cell->GetNeighbouringCell(direction, 2);
        if (next != nullptr && next->IsTrackStartBlock() != 0 && next->mParentID == cell->mParentID) {
            uint8_t opposite = OPPOSITE(direction);
            if ((next->GetNeighbours() & opposite) != 0) keepGoing = false;
        }
        switch (direction) {
            case 0x10: cy += 2; break;
            case 4: cx += 2; break;
            case 1: cy -= 2; break;
            case 0x40: cx -= 2; break;
            default: break;
        }
    } while (keepGoing);
}

// 0x10088ff4
void __sinit_LLCellMouse_cpp() {
    gLLCCDB78 = 0;
    gLLCCDB74 = 0x10000;
    gLLCCDB68 = 0;
    gLLCCDB5C = 0;
    gLLCCDB5A = 0;
    gLLCCDB58 = 0;
}

// 0x10088970
int32_t LLCM_GetIllegalPlacement() { return gLLCMIllegalPlacement; }

// 0x100888fc: the Mac's word; the host keeps the whole pointer in gLLCMCurrentObject while the two agree.
CObject* LLCM_GetCurrentObject() {
    if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C)) return gLLCMCurrentObject;
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
}

// 0x1008288c
void LLCCM_DestroyHoardingMesh(uint32_t mesh) { DestroyRideInstance(mesh); }
// 0x10082508
void LLCCM_DestroyWTrackMesh(uint32_t mesh) { DestroyRideInstance(mesh); }
// 0x10082120
void LLCCM_DestroyKTrackMesh(uint32_t mesh) { DestroyRideInstance(mesh); }
// 0x10081d80
void LLCCM_DestroyQMesh(uint32_t mesh) { DestroyRideInstance(mesh); }

// 0x1007de28
void __sinit_intrface_cpp() {
    gIntrfaceDC1DC = 0;
    gIntrfaceDC1D8 = 0x10000;
    gIntrface.mF24 = 1;
    gIntrface.mShowFPS = 0;
}

// 0x1007c990
int32_t ESP_GetNumberOfCustomAnims(ESP_Sprites set, int32_t index) {
    return gESPSprites[gESPGroups[set].mFirst + index]->mCustomCount;
}

// 0x1007c8b0
int32_t ESP_GetNumberOfSprites(ESP_Sprites set) { return gESPGroups[set].mCount; }

// 0x1007b114
void DrawHLSprite(tagSCRIPT* script) { script->mHasSprite = 1; }

// 0x1007aca4
void ESP_DisableSpriteDraw() { gESPDrawEnabled = 0; }

// 0x1007ac68
void ESP_EnableSpriteDraw() { gESPDrawEnabled = 1; }

// 0x10079168 (the original leaves r3 as it was; the answer is not meaningful)
int32_t FLY_SetMotionScale(int32_t fly, int32_t scale) {
    if (fly <= 0 || fly >= 100) return 0;
    uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
    if (base == nullptr) return 0;
    std::memcpy(base + 0x24, &scale, 4);
    return 0;
}

// 0x10079114
int32_t FLY_SetCarSize(int32_t fly, int32_t size) {
    if (fly <= 0 || fly >= 100) return 0;
    uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
    if (base == nullptr) return 0;
    std::memcpy(base + 0x20, &size, 4);
    return 0;
}
