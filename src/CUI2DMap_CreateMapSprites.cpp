// CUI2DMap::CreateMapSprites 0x1022fd78 (2628 bytes, unit v5058): makes the map's sprites: for every used thing of a few kinds one or two
// sprites at the thing's map position, one sprite for every map cell with a nonzero word at +0x2c, and one for every ride of the object list
// with flag 0x10. The map's fields are read by Mac offset (see ui_main.h); the things by Mac offset too (they are loaded images).
#include <cstddef>
#include <cstdint>

#include "analyser.h"
#include "Debug.h"
#include "MainBalance.h"
#include "World.h"
#include "ppc_math.h"
#include "ui_main.h"

namespace {

const char kAssertText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

uint8_t* Bytes(const void* p) { return const_cast<uint8_t*>(static_cast<const uint8_t*>(p)); }
uint8_t Byte(const void* p, size_t off) { return Bytes(p)[off]; }
uint16_t Half(const void* p, size_t off) { return *reinterpret_cast<const uint16_t*>(Bytes(p) + off); }
int32_t Word(const void* p, size_t off) { return *reinterpret_cast<const int32_t*>(Bytes(p) + off); }

// The image of the Mac word at off (0x20068 .. 0x2007c): the native pointer InitSprites keeps for it (CUI2DMap::HostImage).
void* Image(const CUI2DMap* map, size_t off) {
    return const_cast<CUI2DMap*>(map)->HostImage(static_cast<int32_t>((off - 0x20068) / 4));
}

int32_t Balance(size_t off) { return Word(&gMainBalance, off); }

// The two thing id loops read the table's thing with the assertion on row 0 first.
void* ThingOfId(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kAssertText);
    return gThingArray[id].mThing;
}

}  // namespace

void CUI2DMap::CreateMapSprites() {
    CUI2DMap* map = this;

    // One sprite for a used thing: the position is fmadds(byte5, 1/256, byte4), fmadds(byte7, 1/256, byte6).
    auto place = [&](const void* thing, size_t imageOff, int32_t id, int32_t a, int32_t b, float scale) {
        CUI2DMapSprite* sprite = new CUI2DMapSprite(Image(map, imageOff), static_cast<EUI2DMapSpriteID>(id), TUI2DMapIndividualShowStatus{a, b}, scale);
        sprite->mX = ppc_fmadds(static_cast<float>(Byte(thing, 5)), 0.00390625f, static_cast<float>(Byte(thing, 4)));
        sprite->mY = ppc_fmadds(static_cast<float>(Byte(thing, 7)), 0.00390625f, static_cast<float>(Byte(thing, 6)));
        sprite->mThing = const_cast<void*>(thing);
        UpdateSpritePosition(sprite);
    };

    uint16_t cur = gGameWorld->GetUsedThingHead().mIndex;
    while (cur != gThingIdNone) {
        const void* thing = ThingOfId(cur);
        switch (Byte(thing, 2)) {
        case 1:
            place(thing, 0x2006c, 1, 1, 0x20, 1.0f);
            place(thing, 0x20068, 0, 3, 1, 1.0f);
            break;
        case 4:
            place(thing, 0x20070, 2, 2, 2, 1.0f);
            break;
        case 5:
            place(thing, 0x20070, 2, 2, 1, 1.0f);
            break;
        case 6:
            place(thing, 0x20070, 2, 2, 4, 1.0f);
            place(thing, 0x20078, 4, 2, 4, 0.5f * static_cast<float>(Balance(0x5d0)));
            break;
        case 7:
            place(thing, 0x20070, 2, 2, 3, 1.0f);
            place(thing, 0x2007c, 5, 2, 3, 0.5f * static_cast<float>(Balance(0x618)));
            break;
        default:
            break;
        }
        cur = gGameWorld->GetNextUsedThing(static_cast<const CThing*>(thing)).mIndex;
    }

    const uint8_t* cells = Bytes(gGameWorld) + 0x2d8;
    for (int32_t y = 0; y < Word(map, 0x28); y++) {
        for (int32_t x = 0; x < Word(map, 0x24); x++) {
            uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
            // the original's index minus one is a widened halfword, so index 0 reads before the cells
            int32_t type = Word(cells + (static_cast<int32_t>(index) - 1) * 0x44, 0x2c);
            if (type == 0) continue;
            float scale = 1.0f;
            if (type == 8) {
                scale = static_cast<float>(Balance(0x648));
            } else if (type == 7) {
                scale = static_cast<float>(Balance(0x600));
            }
            CUI2DMapSprite* sprite = new CUI2DMapSprite(Image(map, 0x20074), static_cast<EUI2DMapSpriteID>(3), TUI2DMapIndividualShowStatus{2, 1}, 0.5f * scale);
            sprite->mX = 0.5f + static_cast<float>(x);
            sprite->mY = 0.5f + static_cast<float>(y);
            sprite->mThing = nullptr;
            UpdateSpritePosition(sprite);
        }
    }

    uint16_t id = Half(gGameWorld, 0x1da746);
    while (id != gObjectIdNone) {
        const void* thing = ThingOfId(id);
        if ((Half(thing, 0x2e) & 0x10) != 0) {
            CUI2DMapSprite* sprite = new CUI2DMapSprite(Image(map, 0x2007c), static_cast<EUI2DMapSpriteID>(5), TUI2DMapIndividualShowStatus{2, 3},
                                                        0.5f * static_cast<float>(Balance(0x630)));
            int32_t v = static_cast<int32_t>(static_cast<uint16_t>(Byte(thing, 4) + Byte(thing, 6) * 0x80 + 1)) - 1;
            sprite->mX = 0.5f + static_cast<float>(v & 0x7f);
            sprite->mY = 0.5f + static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(v) >> 7));
            sprite->mThing = nullptr;
            UpdateSpritePosition(sprite);
        }
        id = Half(thing, 0xc);
    }
}
