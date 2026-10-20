// CUI2DMap::MoveCentre, InitSprites and UpdateMapZoom (unit v5083; ui_2dmap.cpp, main app).
// The map's fields are read by Mac offset from `this` (the class has no members yet), see include/ui_main.h.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "analyser.h"
#include "Debug.h"
#include "TPMain.h"
#include "TbTList.h"
#include "World.h"
#include "ppc_math.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "ui_main.h"

namespace {

template <class T>
T& Field(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

// The 32-bit unsigned conversion the original builds with the 0x43300000 magic and no sign flip.
float FromU32(uint32_t value) { return static_cast<float>(value); }

}  // namespace

typedef TbTList<CUI2DMapSprite*, CUI2DMapSprite*> CUI2DMapSpriteList;

void CUI2DMap::MoveCentre(EUI2DMapMoveDirection direction, float amount) {
    float& x = Field<float>(this, 0x54);
    float& y = Field<float>(this, 0x58);
    switch (static_cast<int32_t>(direction)) {
    case 0: x = x - amount; break;
    case 1: x = x + amount; break;
    case 2: y = y + amount; break;
    case 3: y = y - amount; break;
    default: break;
    }
    // Each coordinate: below 0.0 it becomes 0.0, above (size - 1) it becomes that (NaN passes).
    float cx = x;
    if (cx < 0.0f) {
        cx = 0.0f;
    } else {
        float limit = static_cast<float>(Field<int32_t>(this, 0x24) - 1);
        if (cx > limit) cx = limit;
    }
    x = cx;
    float cy = y;
    if (cy < 0.0f) {
        cy = 0.0f;
    } else {
        float limit = static_cast<float>(Field<int32_t>(this, 0x28) - 1);
        if (cy > limit) cy = limit;
    }
    y = cy;
    UpdateMapZoom();
}

void CUI2DMap::InitSprites() {
    static const char kMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
    int32_t count = 0;

    uint16_t cur = gGameWorld->GetUsedThingHead().mIndex;
    while (cur != gUI2DMapThingListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kMessage);
        const uint8_t* thing = static_cast<const uint8_t*>(gThingArray[cur].mThing);
        switch (thing[2]) {
        case 1: count += 2; break;
        case 4: count += 1; break;
        case 5: count += 1; break;
        case 6: count += 2; break;
        case 7: count += 2; break;
        default: break;
        }
        cur = gGameWorld->GetNextUsedThing(reinterpret_cast<const CThing*>(thing)).mIndex;
    }

    uint8_t* world = reinterpret_cast<uint8_t*>(gGameWorld);
    for (int32_t y = 0; y < Field<int32_t>(this, 0x28); y++) {
        for (int32_t x = 0; x < Field<int32_t>(this, 0x24); x++) {
            uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
            // index 0 reads before the cells (the halfword minus one is a widened int)
            if (Field<int32_t>(world, 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x44 + 0x2c) != 0) count++;
        }
    }

    uint16_t id = Field<uint16_t>(world, 0x1da746);
    while (id != gUI2DMapObjectListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kMessage);
        uint8_t* thing = static_cast<uint8_t*>(gThingArray[id].mThing);
        if ((Field<uint16_t>(thing, 0x2e) & 0x10) != 0) count++;
        id = Field<uint16_t>(thing, 0xc);
    }

    gei_EZSprite.Init(count + 6, 0);
    gei_EZSprite.Enable();
    for (int32_t i = 0; i < 6; i++) {
        SSpriteEngineData* image = gei_EZSprite.LoadSprite(const_cast<char*>(gUI2DMapImages[i].mName));
        // the images are Ptr32 words at 0x20068 (see CreateMapSprites)
        Field<uint32_t>(this, 0x20068 + i * 4) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(image));
    }
    // The original also stores the address of a data object (0x102b2998) over the new list's first word, the vtable of its
    // class; the host list keeps the vtable of TbTList.
    CUI2DMapSpriteList* list = new CUI2DMapSpriteList();
    Field<CUI2DMapSpriteList*>(this, 0x20080) = list;
    CreateMapSprites();
}

void CUI2DMap::UpdateMapZoom() {
    const float mapW = static_cast<float>(Field<int32_t>(this, 0x24));
    const float mapH = static_cast<float>(Field<int32_t>(this, 0x28));
    const float screenW = FromU32(static_cast<uint32_t>(gScreenWidth));
    const float screenH = FromU32(static_cast<uint32_t>(gScreenHeight));
    const float zoom = Field<float>(this, 0x5c);
    const float viewW = screenW / zoom;
    const float viewH = screenH / zoom;
    const float centreX = ppc_fmadds(Field<float>(this, 0x54), 480.0f / mapW, 15.0f);
    const float centreY = ppc_fmadds(Field<float>(this, 0x58), 425.0f / mapH, 42.0f);
    const float halfW = 0.5f * viewW;
    const float halfH = 0.5f * viewH;
    const float width = screenW;
    const float height = screenH;

    float x0 = centreX - halfW;
    float y0 = centreY - halfH;
    float x1 = centreX + halfW;
    float y1 = centreY + halfH;
    if (x0 < 0.0f) x0 = 0.0f;
    if (y0 < 0.0f) y0 = 0.0f;
    float limitX = FromU32(static_cast<uint32_t>(Field<int32_t>(this, 0x48) - 1));
    if (x1 >= limitX) x1 = limitX;
    float limitY = FromU32(static_cast<uint32_t>(Field<int32_t>(this, 0x4c) - 1));
    if (y1 >= limitY) y1 = limitY;

    gei_MapZoom.Use(static_cast<float>(ppc_fctiwz(static_cast<double>(x0))), static_cast<float>(ppc_fctiwz(static_cast<double>(y0))),
                    static_cast<float>(ppc_fctiwz(static_cast<double>(x1))), static_cast<float>(ppc_fctiwz(static_cast<double>(y1))));

    const float zoom2 = Field<float>(this, 0x5c);
    float left = ppc_fnmsubs(zoom2, centreX - x0, 0.5f * width);
    float top = ppc_fnmsubs(zoom2, centreY - y0, 0.5f * height);
    float right = ppc_fmadds(zoom2, x1 - centreX, 0.5f * width);
    float bottom = ppc_fmadds(zoom2, y1 - centreY, 0.5f * height);
    left = (2.0f * left) / width - 1.0f;
    top = (2.0f * top) / height - 1.0f;
    right = (2.0f * right) / width - 1.0f;
    bottom = (2.0f * bottom) / height - 1.0f;

    auto clamp = [](float v) {
        if (v < -1.0f) return -1.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    };
    gei_MapZoom.Window(clamp(left), clamp(top), clamp(right), clamp(bottom));
    UpdateAllSpritePositions();
}
