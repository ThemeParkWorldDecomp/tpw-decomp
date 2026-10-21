// CUI2DMap::UpdateAllDrawRelated (0x1022e2c4, 3688 bytes): draws the map grid (a filled cell and its borders per map cell), the coaster tracks,
// then the frames of the sprites that are showing.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "CellBase.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "Light.h"
#include "TPCoaster.h"
#include "TPMain.h"
#include "TbGraphicFile.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "ui_main.h"
#include "Main.h"
#include "TbRaConvert.h"
#include "TbTList.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "tb_resource_texture.h"

namespace {

// 0x10251a08 (the other unit's copy is not visible from here).
const char kZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

using SpriteList = TbTList<CUI2DMapSprite*, CUI2DMapSprite*>;

// One 36 byte CColour as nine words.
struct Colour9 {
    uint32_t w[9];
};

const uint8_t* Entry(int32_t offset) { return reinterpret_cast<const uint8_t*>(gUI2DMapColours) + offset; }

void CopyEntry(Colour9& out, int32_t offset) { std::memcpy(&out, Entry(offset), sizeof(out)); }

uint32_t Word(const void* base, size_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, 4);
    return v;
}

int32_t SWord(const void* base, size_t offset) { return static_cast<int32_t>(Word(base, offset)); }

uint16_t Half(const void* base, size_t offset) {
    uint16_t v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, 2);
    return v;
}

// The map cell (x, y): an eight byte entry at 0x60 of the map, NULL outside 128 x 128.
const uint8_t* Cell(const uint8_t* map, int32_t x, int32_t y) {
    if (x < 0 || x >= 0x80 || y < 0 || y >= 0x80) return nullptr;
    return map + 0x60 + (x + (y << 7)) * 8;
}

// The cell's thing, after the original's assert that row 0 is free.
CObject* ThingOf(const uint8_t* cell) {
    uint16_t id = Half(cell, 4);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kZeroMessage);
    return static_cast<CObject*>(gThingArray[id].mThing);
}

Colour9 Scaled(int32_t index) {
    bfl::draw::CColour c = UI2DMReturnScaleColour(index, *reinterpret_cast<const bfl::draw::CColour*>(Entry(0x1f8)),
                                                  *reinterpret_cast<const bfl::draw::CColour*>(Entry(0x21c)),
                                                  *reinterpret_cast<const bfl::draw::CColour*>(Entry(0x240)));
    Colour9 out;
    std::memcpy(&out, &c, sizeof(out));
    return out;
}

// The switch table at 0x102b2914 (types 0 to 12): the block each cell type runs; 0 is the default (nothing).
const int8_t kBlock[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

}  // namespace

void CUI2DMap::UpdateAllDrawRelated() {
    const uint8_t* map = reinterpret_cast<const uint8_t*>(this);
    bfl::draw::CDraw draw;
    draw.SetCurrent(HostArea());   // the Mac's area at 0x2c (ui_main.h)
    int32_t flags = 8;
    const int32_t perX = static_cast<int32_t>(ppc_fctiwz(static_cast<double>(480.0f / static_cast<float>(SWord(map, 0x24)))));
    const int32_t perY = static_cast<int32_t>(ppc_fctiwz(static_cast<double>(425.0f / static_cast<float>(SWord(map, 0x28)))));
    CUI2DMapDrawOps* ctx = UI2DMapDrawOps(draw.mRenderContext);

    int32_t y = 0x2a;
    for (int32_t row = 0; row < SWord(map, 0x28); row++) {
        int32_t x = 0xf;
        for (int32_t col = 0; col < SWord(map, 0x24); col++) {
            const uint8_t* cell = Cell(map, col, row);
            Colour9 fill;
            std::memset(&fill, 0, sizeof(fill));
            bool filled = false;
            const int32_t type = SWord(cell, 0);
            if (type == 1 || type == 2 || type == 3 || type == 8) {
                if (SWord(map, 0x20) == 2) {
                    CObject* thing = ThingOf(cell);
                    fill = Scaled(thing->GetExcitementLevel(false) & 0xff);
                    filled = true;
                } else if (SWord(map, 0x20) == 3) {
                    CObject* thing = ThingOf(cell);
                    fill = Scaled(thing->GetCustomerSatisfaction());
                    filled = true;
                }
            }
            if (!filled && static_cast<uint32_t>(type) <= 12) {
                const uint32_t mask = Word(map, 0x18);
                bool scaled = false;
                switch (kBlock[type]) {
                case 1: scaled = (mask & 1) != 0; break;
                case 2: scaled = (mask & 2) != 0; break;
                case 3: scaled = (mask & 4) != 0; break;
                case 4: scaled = (mask & 8) != 0; break;
                case 5: scaled = SWord(map, 0x1c) == 1; break;
                case 6: scaled = SWord(map, 0x1c) == 3; break;
                case 7: scaled = SWord(map, 0x1c) != 0; break;
                case 8: scaled = (mask & 0x10) != 0; break;
                case 9: CopyEntry(fill, 0x168); filled = true; break;
                case 10: CopyEntry(fill, 0x120); filled = true; break;
                case 11: CopyEntry(fill, 0x144); filled = true; break;
                case 12: CopyEntry(fill, 0x18c); filled = true; break;
                default: break;
                }
                if (scaled) {
                    CObject* thing = ThingOf(cell);
                    fill = Scaled(static_cast<int32_t>(ppc_fctiwz(static_cast<double>(thing->mF40))) & 0xff);
                    filled = true;
                }
            }
            if (filled) {
                int32_t rect[4] = {x, y, x + perX, y + perY};
                if (ctx != nullptr) ctx->Slot28(&draw.mViewPort, &draw.mClipRect, flags, rect, &fill);
            }

            Colour9 border;
            std::memset(&border, 0, sizeof(border));
            if (type >= 1 && type <= 7) {
                CObject* thing = ThingOf(cell);
                if (thing->IsClosed()) {
                    CopyEntry(border, 0x48);
                } else {
                    CopyEntry(border, 0x6c);
                }
            } else {
                CopyEntry(border, 0x6c);
            }
            if (type >= 1 && type <= 8) {
                const uint8_t* n = Cell(map, col, row - 1);
                if (n == nullptr || type != SWord(n, 0) || Half(cell, 4) != Half(n, 4)) {
                    if (ctx != nullptr) ctx->Slot10(&draw.mViewPort, &draw.mClipRect, flags, x, y, perX, &border);
                }
                n = Cell(map, col, row + 1);
                if (n == nullptr || type != SWord(n, 0) || Half(cell, 4) != Half(n, 4)) {
                    if (ctx != nullptr) ctx->Slot10(&draw.mViewPort, &draw.mClipRect, flags, x, y + (perX - 1), perX, &border);
                }
                n = Cell(map, col - 1, row);
                if (n == nullptr || type != SWord(n, 0) || Half(cell, 4) != Half(n, 4)) {
                    if (ctx != nullptr) ctx->Slot14(&draw.mViewPort, &draw.mClipRect, flags, x, y, perX, &border);
                }
                n = Cell(map, col + 1, row);
                if (n == nullptr || type != SWord(n, 0) || Half(cell, 4) != Half(n, 4)) {
                    if (ctx != nullptr) ctx->Slot14(&draw.mViewPort, &draw.mClipRect, flags, x + (perX - 1), y, perX, &border);
                }
            }
            x += perX;
        }
        y += perY;
    }

    if ((Word(map, 0x18) & 0x10) != 0) DrawCoasterTracks();

    flags = 0xc;
    SpriteList* list = nullptr;
    std::memcpy(&list, map + 0x20080, sizeof(list));
    SpriteList::CNode* node = list->mHead;
    while (node != nullptr) {
        CUI2DMapSprite* sprite = node->mData;
        node = node->mNext;
        sprite->UpdateVisibilityAndColour(SWord(map, 0x18), SWord(map, 0x1c), SWord(map, 0x20));
        if (sprite->mKind != 2) continue;
        const int32_t mode = sprite->mMode;
        const bool show = (mode == 1 && (Word(map, 0x18) & static_cast<uint32_t>(sprite->mMatch)) != 0) ||
                          (mode == 2 && SWord(map, 0x1c) == sprite->mMatch) || (mode == 3 && SWord(map, 0x20) == sprite->mMatch);
        if (!show) continue;
        const uint8_t* thing = static_cast<const uint8_t*>(sprite->mThing);
        const uint16_t first = Half(thing, 0x206);
        const uint16_t second = Half(thing, 0x208);
        const uint32_t a = static_cast<uint32_t>(first) - 1;
        const uint32_t b = static_cast<uint32_t>(second) - 1;
        if ((a & 0x7f) == 0) continue;
        if ((a >> 7) == 0) continue;
        if (!(static_cast<int32_t>(b & 0x7f) < 0x7f)) continue;
        if (!(static_cast<int32_t>(b >> 7) < 0x7f)) continue;
        int32_t rect[4];
        rect[0] = perX * static_cast<int32_t>(a & 0x7f) + 0xf;
        rect[1] = perY * static_cast<int32_t>(a >> 7) + 0x2a;
        rect[2] = perX * (static_cast<int32_t>(b & 0x7f) + 1) + 0xf;
        rect[3] = perY * (static_cast<int32_t>(b >> 7) + 1) + 0x2a;
        void* current = nullptr;
        std::memcpy(&current, map + 0x20060, sizeof(current));
        Colour9 frame;
        std::memset(&frame, 0, sizeof(frame));
        CopyEntry(frame, sprite == current ? 0x1d4 : 0x1b0);
        if ((flags & 4) != 0) {
            if (ctx != nullptr) ctx->Slot24(&draw.mViewPort, &draw.mClipRect, flags, rect, &frame);
        } else {
            if (ctx != nullptr) ctx->Slot28(&draw.mViewPort, &draw.mClipRect, flags, rect, &frame);
        }
    }
}
