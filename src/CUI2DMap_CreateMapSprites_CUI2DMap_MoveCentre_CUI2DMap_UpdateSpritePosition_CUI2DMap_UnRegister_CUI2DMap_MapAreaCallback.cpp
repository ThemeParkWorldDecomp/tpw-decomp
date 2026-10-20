// CUI2DMap::MapAreaCallback 0x1022d0ac (2652 bytes), main app: the message function of the map area window.
//   0x10001 and 0x10003 (the mouse moved): the mouse position is stored (x in the high halfword of `a`, y in the low one), the visible
//   sprite nearest to it (within 5 units on both axes) becomes the hovered one (map + 0x20060), the map is redrawn when the hovered
//   thing changes to or from a staff/mechanic kind, the grid cell under the mouse gives the hovered thing id (0x20064) and the
//   name of the hovered thing is set as the text of the child window 0x991 of the parent. Then the default handler answers.
//   0x1e: with the setting byte 0x38 set and the drag flag (0x20084) set the centre moves by the mouse offset from the screen middle.
//   0x10004 and 0x10005 with a == 1 clear and set the drag flag. 0x10006 with a == 0 (a click): the centre moves to the click, is
//   clamped, the camera goes there and the status panel of the hovered thing opens; the answer is 0 without the default handler.
// The Mac pointers kept in the map object (the window at 4, the hovered sprite at 0x20060, the sprite list at 0x20080) are 32-bit words
// that hold native (arena) addresses on the host. The sprite's thing and the thing array hold native pointers.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Debug.h"
#include "InterfaceWindow.h"
#include "TPCamera.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTList.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ppc_math.h"
#include "ui_main.h"

uint16_t gMapNoThingA;   // 0x102b290c (name invented): compared with the hovered thing id after a mouse move
uint16_t gMapNoThingB;   // 0x102b290e (name invented): compared with the hovered thing id on a click

namespace mapcb_callee {
// The virtual at slot 0x2c of the table at window + 0x118 (called through __ptr_glue): faked by the test library.
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, const char16_t* text) { window->SetText(const_cast<char16_t*>(text)); }
}  // namespace mapcb_callee

namespace {

const char kThingZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10251a08
const char16_t kEmptyText[] = u"";                                                                        // 0x10251aec

template <class T> T& Field(CUI2DMap* map, size_t offset) { return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(map) + offset); }

// Host-only places (the Mac keeps 32-bit words at 4, 0x20060 and the flag at 0x20084, which a native pointer cannot share with the
// list pointer at 0x20080): the hovered sprite, the drag flag and the map panel window.
constexpr size_t kHoverOffset = 0x200a0;
constexpr size_t kDragOffset = 0x200a8;
constexpr size_t kPanelOffset = 0x200b0;
CUI2DMapSprite*& HoverOf(CUI2DMap* map) { return Field<CUI2DMapSprite*>(map, kHoverOffset); }
CUI2DMapSprite* SpriteOf(CUI2DMapSprite* p) { return p; }
CUI2DMapSprite* WordOf(CUI2DMapSprite* p) { return p; }

uint8_t KindOf(const void* thing) { return static_cast<const uint8_t*>(thing)[2]; }
// The kind tests of the listing (4, 5, 6, 8, 7 one after the other): a staff or mechanic.
bool IsStaffKind(uint8_t kind) { return kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7; }

// The thing under a sprite or, without one, the thing of the hovered id (index 0 must stay empty).
void* ThingOfId(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kThingZeroMessage);
    return gThingArray[id].mThing;
}

}  // namespace

void CUI2DMap::MapAreaCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    MapAreaCallbackResult(window, message, a, b);
}

int32_t CUI2DMap::MapAreaCallbackResult(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CUI2DMap* map = this;
    if (message == 0x10003 || message == 0x10001) {
        Field<int32_t>(map, 0x10) = static_cast<int32_t>(static_cast<uint32_t>(a) >> 16);
        Field<int32_t>(map, 0x14) = static_cast<int32_t>(static_cast<uint32_t>(a) & 0xffff);
        const float screenW = static_cast<float>(static_cast<uint32_t>(gScreenWidth));
        const float screenH = static_cast<float>(static_cast<uint32_t>(gScreenHeight));
        CUI2DMapSprite* const oldHover = HoverOf(map);
        const float mouseX = (static_cast<float>(Field<int32_t>(map, 0x10)) * screenW) / 2048.0f;
        const float mouseY = (static_cast<float>(0x600 - Field<int32_t>(map, 0x14)) * screenH) / 1536.0f;
        HoverOf(map) = nullptr;
        auto* list = Field<TbTList<CUI2DMapSprite*, CUI2DMapSprite*>*>(map, 0x20080);
        auto* node = list->mHead;
        float best = 100000000.0f;
        while (node != nullptr) {
            CUI2DMapSprite* sprite = node->mData;
            node = node->mNext;
            if (sprite->mVisible == 0) continue;
            const float spriteX = *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(sprite) + 0xc);
            const float spriteY = *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(sprite) + 0x10);
            const float dx = std::fabs(mouseX - spriteX);
            const float dy = std::fabs(mouseY - spriteY);
            if (!(dx < 5.0f)) continue;
            if (!(dy < 5.0f)) continue;
            const float distance = static_cast<float>(std::sqrt(static_cast<double>(ppc_fmadds(dx, dx, dy * dy))));
            if (distance < best) {
                HoverOf(map) = sprite;
                best = distance;
            }
        }
        CUI2DMapSprite* const newHover = HoverOf(map);
        if (oldHover != newHover) {
            bool redraw = false;
            const void* oldThing = oldHover != nullptr ? oldHover->mThing : nullptr;
            if (oldThing != nullptr && IsStaffKind(KindOf(oldThing))) {
                redraw = true;
            } else {
                const void* newThing = newHover != nullptr ? newHover->mThing : nullptr;
                if (newThing != nullptr && IsStaffKind(KindOf(newThing))) redraw = true;
            }
            if (redraw) UpdateMap();
        }
        const float mapW = static_cast<float>(Field<int32_t>(map, 0x24));
        const float mapH = static_cast<float>(Field<int32_t>(map, 0x28));
        const float offsetX = ppc_fnmsubs(0.5f, screenW, mouseX);
        const float offsetY = ppc_fnmsubs(0.5f, screenH, mouseY);
        const float zoom = Field<float>(map, 0x5c);
        const float gridX = Field<float>(map, 0x54) + offsetX / (zoom * (480.0f / mapW));
        const float gridY = Field<float>(map, 0x58) + offsetY / (zoom * (425.0f / mapH));
        const int32_t cellX = ppc_fctiwz(static_cast<double>(gridX));
        const int32_t cellY = ppc_fctiwz(static_cast<double>(gridY));
        const uint8_t* cell = static_cast<const uint8_t*>(GetMapGridCell(cellX, cellY));
        if (cell == nullptr) {
            Field<uint16_t>(map, 0x20064) = 0;
        } else {
            Field<uint16_t>(map, 0x20064) = *reinterpret_cast<const uint16_t*>(cell + 4);
        }
        {
            TbDynamicStringTemplate<char16_t> text(kEmptyText);
            const CUI2DMapSprite* hovered = HoverOf(map);
            void* thing = hovered != nullptr ? hovered->mThing : nullptr;
            if (thing == nullptr) {
                const uint16_t id = Field<uint16_t>(map, 0x20064);
                if (id != gMapNoThingA) {
                    void* object = ThingOfId(id);
                    static_cast<CObject*>(object)->GetUIName(gUIMainConvertBuffer, 0x3fff);
                    text = static_cast<const char16_t*>(gUIMainConvertBuffer);
                }
            } else {
                const uint8_t kind = KindOf(thing);
                if (IsStaffKind(kind)) {
                    text = static_cast<const CStaff*>(thing)->GetName();
                } else if (kind == 1) {
                    auto* table = static_cast<TbITable*>(gText.mResourcePointers[6]);   // Mac 0x564
                    const int32_t value = *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(thing) + 0x1d8);
                    if (value == 0) {
                        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c9);
                    } else {
                        TbIntegerVTag tag(0x18, value);
                        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c8, &tag);
                    }
                    text = static_cast<const char16_t*>(gUIMainConvertBuffer);
                }
            }
            const char16_t* shown = text.mData;
            InterfaceWindow* parent = window->GetParent();
            InterfaceWindow* label = parent->GetWindow(0x991);
            mapcb_callee::WindowSetText(label, shown);
        }
    } else if (message == 0x1e) {
        if (reinterpret_cast<const uint8_t*>(&gPlayerSettings)[0x38] != 0 && Field<int32_t>(map, kDragOffset) != 0) {
            const float stepX = (5.0f * static_cast<float>(Field<int32_t>(map, 0x10) - 0x400)) / 2048.0f;
            MoveCentre(static_cast<EUI2DMapMoveDirection>(1), stepX);
            const float stepY = (5.0f * static_cast<float>(Field<int32_t>(map, 0x14) - 0x300)) / 1536.0f;
            MoveCentre(static_cast<EUI2DMapMoveDirection>(3), stepY);
        }
    } else if (message == 0x10006) {
        if (a == 0) {
            const float screenW = static_cast<float>(static_cast<uint32_t>(gScreenWidth));
            const float screenH = static_cast<float>(static_cast<uint32_t>(gScreenHeight));
            const float mouseX = (static_cast<float>(Field<int32_t>(map, 0x10)) * screenW) / 2048.0f;
            const float mouseY = (static_cast<float>(0x600 - Field<int32_t>(map, 0x14)) * screenH) / 1536.0f;
            const float mapW = static_cast<float>(Field<int32_t>(map, 0x24));
            const float mapH = static_cast<float>(Field<int32_t>(map, 0x28));
            const float offsetX = ppc_fnmsubs(0.5f, screenW, mouseX);
            const float offsetY = ppc_fnmsubs(0.5f, screenH, mouseY);
            Field<float>(map, 0x54) = Field<float>(map, 0x54) + offsetX / (Field<float>(map, 0x5c) * (480.0f / mapW));
            Field<float>(map, 0x58) = Field<float>(map, 0x58) + offsetY / (Field<float>(map, 0x5c) * (425.0f / mapH));
            void* target = nullptr;
            const CUI2DMapSprite* hoverSprite = HoverOf(map);
            if (hoverSprite != nullptr && hoverSprite->mThing != nullptr) {
                target = hoverSprite->mThing;
            } else if (Field<uint16_t>(map, 0x20064) != gMapNoThingB) {
                target = ThingOfId(Field<uint16_t>(map, 0x20064));
            }
            // Each coordinate: below 0.0 it becomes 0.0, above (size - 1) it becomes that (NaN passes).
            float cx = Field<float>(map, 0x54);
            if (cx < 0.0f) {
                cx = 0.0f;
            } else {
                const float limit = static_cast<float>(Field<int32_t>(map, 0x24) - 1);
                if (cx > limit) cx = limit;
            }
            Field<float>(map, 0x54) = cx;
            float cy = Field<float>(map, 0x58);
            if (cy < 0.0f) {
                cy = 0.0f;
            } else {
                const float limit = static_cast<float>(Field<int32_t>(map, 0x28) - 1);
                if (cy > limit) cy = limit;
            }
            Field<float>(map, 0x58) = cy;
            const int32_t ix = ppc_fctiwz(static_cast<double>(Field<float>(map, 0x54)));
            const int32_t iy = ppc_fctiwz(static_cast<double>(Field<float>(map, 0x58)));
            float poiX = static_cast<float>(ix * 10);
            float poiY = static_cast<float>(iy * 10);
            SetCameraPOI(&poiX, &poiY, nullptr, nullptr, nullptr);
            UI_SendMessage(Field<InterfaceWindow*>(map, kPanelOffset), 4, 0, 0);
            if (target != nullptr) UIMain_GotoStatusPanelForThing(target);
            return 0;
        }
    } else if (message == 0x10005) {
        if (a == 1) Field<int32_t>(map, kDragOffset) = 1;
    } else if (message == 0x10004) {
        if (a == 1) Field<int32_t>(map, kDragOffset) = 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
