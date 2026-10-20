#include <cstddef>
#include <cstdint>
#include "CellBase.h"
#include "HoardingsBalance.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "TrackCell.h"
#include "World.h"
#include "ui_main.h"
#include "Debug.h"
#include "HLSprite.h"
#include "analyser.h"
#include "intrface.h"
#include "sprite_engine.h"
#include "tb_resource_display.h"

// 0x1009722c, 712 bytes: takes a bridge away from the cell and turns the two bridge cells beside it (a, b) to what they become without it.
// The two IsXYValid calls on the first block and the track end have their answers dropped (the Mac stores them in dead locals).
// A direction other than 1, 4, 0x10, 0x40 leaves the two flags (callee-saved r26, r25) as the caller had them: here 0.
void RemoveBridge(CTrackCell* cell, MapDelta a, MapDelta b) {
    CTrackCell* na = cell->GetNeighbouringCell(a, 2);
    CTrackCell* nb = cell->GetNeighbouringCell(b, 2);
    (void)CMap::IsXYValid(gLLCCFirstBlockX, gLLCCFirstBlockY);
    (void)CMap::IsXYValid(gLLCCE034, gLLCCE038);
    uint16_t flag26 = 0;
    uint16_t flag25 = 0;
    switch (cell->GetDirection()) {
        case 1:
        case 4:
            flag26 = 2;
            flag25 = 8;
            break;
        case 0x10:
        case 0x40:
            flag26 = 8;
            flag25 = 2;
            break;
        default:
            break;
    }
    cell->UnSetFlag(0xf);
    if (na != nullptr && na->IsBridge() != 0) {
        if (na->IsFlag(flag26) != 0) {
            na->UnSetFlag(0xf);
            na->SetFlag(1);
            na->SetHFieldTile();
        } else if (na->IsFlag(4) != 0) {
            na->UnSetFlag(0xf);
            na->SetFlag(flag25);
            na->SetHFieldTile();
        }
    }
    if (nb != nullptr && nb->IsBridge() != 0) {
        if (nb->IsFlag(flag25) != 0) {
            nb->UnSetFlag(0xf);
            nb->SetFlag(1);
            nb->SetHFieldTile();
        } else if (nb->IsFlag(4) != 0) {
            nb->UnSetFlag(0xf);
            nb->SetFlag(flag26);
            nb->SetHFieldTile();
        }
    }
    cell->SetHFieldTile();
}

// 0x10096bc8, 772 bytes: turns the mouse position into the sprite hot point (x / 1024 - 1, y / 767.5 - 1), and when a sprite is hot and a script owns
// it, walks the thing lists of the cell under the pointer and its eight neighbours for the first person-like thing (kinds 1, 4 to 8, 0x12) whose
// script handle (+0xc) is that script. The cell id 0 gives x 0x7f and a huge y (unsigned shift), as on the Mac.
CThing* LLCC_SystemGetPointedPeepThing() {
    const uint16_t cellId = gIntrface.mF24;
    int16_t mouseX;
    int16_t mouseY;
    UI_GetMouseXY(mouseX, mouseY);
    gSpriteHotX = static_cast<float>(mouseX) * 0.0009765625f - 1.0f;
    gSpriteHotY = static_cast<float>(mouseY) / 767.5f - 1.0f;
    SSpriteGameData* sprite = gHotSprite;
    if (sprite == nullptr) return nullptr;
    CTRACE(6, "Found sprite %lx\n", sprite);
    int32_t script = SCR_WhichScriptHasThisSprite(sprite);
    if (script == 0) return nullptr;
    CTRACE(6, " Found script %lx\n", script);
    const int32_t m = static_cast<int32_t>(cellId) - 1;
    const int32_t baseX = m & 0x7f;
    const int32_t baseY = static_cast<int32_t>(static_cast<uint32_t>(m) >> 7);
    for (int32_t i = 0; i < 9; ++i) {
        const int32_t x = baseX + gLLCCPointedOffsets[i].mX;
        const int32_t y = baseY + gLLCCPointedOffsets[i].mY;
        if (CMap::IsXYValid(x, y) == 0) continue;
        const uint16_t id = static_cast<uint16_t>(y * 128 + 1 + x);
        uint16_t who = gGameWorld->mMapCells[static_cast<int32_t>(id) - 1].mWho;
        while (who != gLLCCThingListEnd) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            uint8_t* thing = static_cast<uint8_t*>(gThingArray[who].mThing);
            const uint8_t kind = thing[2];
            if (kind == 1 || (kind >= 4 && kind <= 8) || kind == 0x12) {
                const int32_t handle = *reinterpret_cast<int32_t*>(thing + 0xc);
                CTRACE(6, "  Checking against scripts %lx\n", handle);
                if (handle == script) return reinterpret_cast<CThing*>(thing);
            }
            who = *reinterpret_cast<uint16_t*>(thing + 0xa);
        }
    }
    return nullptr;
}
