// Unit v5959: the sprite list handling and the map picture of the 2D map (ui_2dmap.cpp, main app):
//   UnRegister 0x10230cd4, DeleteSpriteListEntries 0x10230bd4, UpdateAllSpritePositions 0x10230974, CloseSprites 0x102307ec,
//   Zoom 0x1022fa1c, LoadMap 0x1022dbdc, UpdateMap 0x1022db4c.
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

typedef TbTList<CUI2DMapSprite*, CUI2DMapSprite*> SpriteList;

template <class T>
T& Field(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

}  // namespace

// 0x102b2910: the path separator appended to the theme path (initial data, a colon).
char gUI2DMapSeparator = ':';

// The original's destructor is the virtual one of slot 8 (untranslated); the host class has no vtable, so a delete of a sprite
// runs this one. Weak: the test library defines a recording one.
__attribute__((weak)) CUI2DMapSprite::~CUI2DMapSprite() {}

// 0x10230cd4 (84 bytes): takes the sprite out of the list when it is there.
void CUI2DMap::UnRegister(CUI2DMapSprite* sprite) {
    SpriteList* list = Field<SpriteList*>(this, 0x20080);
    POSITION position = list->Find(sprite);
    if (position != nullptr) list->RemoveAt(position);
}

// 0x10230bd4 (100 bytes): deletes every sprite of the list (a NULL entry is only dropped).
void CUI2DMap::DeleteSpriteListEntries() {
    while (Field<SpriteList*>(this, 0x20080)->GetCount() != 0) {
        CUI2DMapSprite* sprite = Field<SpriteList*>(this, 0x20080)->RemoveHead();
        if (sprite != nullptr) delete sprite;
    }
}

// 0x10230974 (96 bytes)
void CUI2DMap::UpdateAllSpritePositions() {
    SpriteList* list = Field<SpriteList*>(this, 0x20080);
    POSITION position = list->GetHeadPosition();
    while (position != nullptr) {
        CUI2DMapSprite* sprite = list->GetNext(position);
        UpdateSpritePosition(sprite);
    }
}

// 0x102307ec (188 bytes)
void CUI2DMap::CloseSprites() {
    DeleteSpriteListEntries();
    SpriteList*& list = Field<SpriteList*>(this, 0x20080);
    if (list != nullptr) delete list;   // the virtual deleting destructor, flag 1
    list = nullptr;
    for (int32_t i = 0; i < 6; i++)
        gei_EZSprite.DeleteSprite(reinterpret_cast<void*>(static_cast<uintptr_t>(Field<uint32_t>(this, 0x20068 + i * 4))));
    gei_EZSprite.Disable();
    gei_EZSprite.Close();
}

// 0x1022fa1c (128 bytes): the zoom float at 0x5c, 0 in (adds), 1 out (subtracts), any other kind leaves it; then kept in 1.0 .. 3.0
// (a NaN becomes 1.0).
void CUI2DMap::Zoom(EUI2DMapZoom zoom, float amount) {
    float& value = Field<float>(this, 0x5c);
    int32_t kind = static_cast<int32_t>(zoom);
    if (kind == 1) {
        value = value - amount;
    } else if (kind < 1 && kind >= 0) {
        value = value + amount;
    }
    float z = value;
    // fcmpo then bge / ble branch on "not less" / "not greater": a NaN passes both and stays
    float result;
    if (!(z < 1.0f)) {
        result = 3.0f;
        if (!(z > 3.0f)) result = z;
    } else {
        result = 1.0f;
    }
    value = result;
    UpdateMapZoom();
}

// 0x1022dbdc (460 bytes): loads <theme path>:<2dmap.tga> into a local area; with reload 0 first sets the map's own area (at 0x2c) up
// from the texture format and allocates it; the picture is converted into the map's area.
void CUI2DMap::LoadMap(int32_t reload) {
    TbDynamicStringTemplate<char> path(get_theme_path());
    path += gUI2DMapSeparator;
    path += *reinterpret_cast<const TbStringBase<char>*>(gUI2DMapTgaName);
    TbTGALoader loader;
    TbRenderArea area;
    loader.ReadHeader(path.mData, nullptr, &area);
    area.AllocMemory();
    loader.Load(area, path.mData, nullptr, nullptr);
    TbRenderArea* mapArea = reinterpret_cast<TbRenderArea*>(reinterpret_cast<uint8_t*>(this) + 0x2c);
    if (reload == 0) {
        Field<int32_t>(this, 0x48) = area.mWidth;
        Field<int32_t>(this, 0x4c) = area.mPictureHeight;
        // the texture format's pixel format (DDPIXELFORMAT at 0x30 of the settings), eight words
        const uint32_t* format = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(&gei_TextureSettings) + 0x30);
        uint32_t w[8];
        for (int32_t i = 0; i < 8; i++) w[i] = format[i];
        Field<uint32_t>(this, 0x2c) = w[4];
        Field<uint32_t>(this, 0x30) = w[5];
        Field<uint32_t>(this, 0x34) = w[6];
        Field<uint32_t>(this, 0x38) = w[7];
        Field<uint32_t>(this, 0x3c) = w[3];
        Field<uint32_t>(this, 0x40) = w[1] & 0x20;
        mapArea->AllocMemory();
    }
    TbRAConv converter;
    converter.Move(*mapArea, area, nullptr);
    area.FreeMemory();
}

// 0x1022db4c (100 bytes)
void CUI2DMap::UpdateMap() {
    LoadMap(1);
    UpdateAllDrawRelated();
    gei_MapZoom.Update(0, 0, Field<int32_t>(this, 0x48) - 1, Field<int32_t>(this, 0x4c) - 1);
}
