// VisitorStatusPanel::UpdateStatusWindow (0x10142ae4, 1056 bytes) and UpdateViewWindow (0x10142170, 140 bytes).
// The panel and the visitor are reached by Mac offsets (the panel as UIStatusPanelFields, the visitor's fields by raw offset).
#include <cstdint>
#include <cstring>
#include <new>
#include "ActionRecorder.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "LbFile.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "Serialise.h"
#include "TPSound.h"
#include "TbITable.h"
#include "Text.h"
#include "bfl_FileSystem.h"
#include "platform/arena.h"
#include "ui_main.h"
#include "InterfaceWindow.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "TbVTag.h"
#include "ThingReceivers.h"
#include "World.h"

namespace visitor_callee {
void WindowSetValue(InterfaceWindow* window, int32_t value);   // the virtual SetValue (slot 0x24 of the table at +0x118)
void WindowSetText(InterfaceWindow* window, const char16_t* text);   // the virtual at slot 0x2c
void RectCtor(void* rect);   // 0x101c7448 CoolRect::CoolRect()
void RectDtor(void* rect);   // 0x101c74e0 CoolRect::~CoolRect() (flag -1)
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, const char16_t* text) {
    window->SetText(const_cast<char16_t*>(text));
}
__attribute__((weak)) void RectCtor(void* rect) { new (rect) CoolRect(); }
__attribute__((weak)) void RectDtor(void* rect) { static_cast<CoolRect*>(rect)->~CoolRect(); }
}  // namespace visitor_callee

namespace {

UIStatusPanelFields* Fields(VisitorStatusPanel* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

void CallSlot(VisitorStatusPanel* panel, uint32_t slot) {
    Ptr32<void (*)(void*)> entry;
    entry.mOffset = Fields(panel)->mVtable.get()[slot / 4];
    (*entry.get())(panel);
}

uint32_t Word(const uint8_t* thing, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, thing + offset, 4);
    return value;
}

// fctiwz's low word (saturating; NaN gives 0x80000000).
int32_t Fctiwz(float value) {
    if (!(value > -2147483648.0f)) return INT32_MIN;
    if (value >= 2147483648.0f) return INT32_MAX;
    return static_cast<int32_t>(value);
}

TbITable* Table(int index) { return static_cast<TbITable*>(gText.mResourcePointers[index]); }

}  // namespace

// 0x10142ae4 (1056 bytes)
void VisitorStatusPanel::UpdateStatusWindow() {
    UIStatusPanelFields* f = Fields(this);
    uint32_t* words = reinterpret_cast<uint32_t*>(this);
    const uint8_t* thing = reinterpret_cast<const uint8_t*>(gStaffRoomPanelThing);
    if (thing == nullptr) {
        CallSlot(this, 0x34);
        return;
    }
    if (words[0x24 / 4] == 0) {
        words[0x24 / 4] = Word(thing, 0x1d8);
        if (words[0x24 / 4] != 0) {
            ShowStatusTag tag(0x18, static_cast<int32_t>(words[0x24 / 4]));
            Table(6)->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c8, &tag);
            UIMain_SetTitleText(f->mWindow.get(), 0x3736, gUIMainConvertBuffer);
        }
    }
    InterfaceWindow* panel = f->mF10.get();
    if (panel != nullptr) {
        InterfaceWindow* window = panel->GetWindow(0x3724);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1a0)));

        window = f->mF10.get()->GetWindow(0x372d);
        if (window != nullptr) {
            uint32_t turns = reinterpret_cast<const CPerson*>(thing)->GetTimeInPark();
            TbTimeDiff span = gGameWorld->mParkTime.GetFunnyTimeForGameTurns(turns << 2);
            // The runtime's 64-bit divide by 36,000,000,000 (ticks per hour); the window gets the low word.
            visitor_callee::WindowSetValue(window, static_cast<int32_t>(span.mTicks / 36000000000LL));
        }

        window = f->mF10.get()->GetWindow(0x372e);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1c4)));
        window = f->mF10.get()->GetWindow(0x372f);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1c8)));
        window = f->mF10.get()->GetWindow(0x3730);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1cc)));
        window = f->mF10.get()->GetWindow(0x3731);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1d0)));

        window = f->mF10.get()->GetWindow(0x3732);
        if (window != nullptr) {
            float happiness;
            std::memcpy(&happiness, thing + 0x19c, 4);
            int32_t scaled = static_cast<int32_t>((static_cast<uint32_t>(Fctiwz(happiness)) & 0xff) << 10);
            visitor_callee::WindowSetValue(window, scaled / 100);
        }

        // No test for NULL on this window.
        window = f->mF10.get()->GetWindow(0x3733);
        uint32_t state = Word(thing, 0x220);
        if (state == 0x10) {
            uint16_t index;
            std::memcpy(&index, thing + 0x1dc, 2);
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            char16_t name[0x40];
            reinterpret_cast<CObject*>(gThingArray[index].mThing)->GetUIName(name, 0x40);
            TbUniStringVTag tag(0, name);
            Table(9)->FormatString(gUIMainConvertBuffer, 0x3fff, 0x10, &tag);
        } else {
            Table(9)->FormatString(gUIMainConvertBuffer, 0x3fff, state);
        }
        visitor_callee::WindowSetText(window, gUIMainConvertBuffer);
    }

    uint32_t frame = Word(thing, 0x30);
    InterfaceWindow* child = f->mWindow.get()->GetWindow(0x3734)->GetWindow(0x3735);
    child->SetFrame(frame != 0 ? static_cast<int32_t>(frame - 1) : 999);
    char16_t* text = UIMain_ConvertMulti(Table(8)->GetConstString(frame));
    visitor_callee::WindowSetText(f->mWindow.get()->GetWindow(0x3734), text);
}

// 0x10142170 (140 bytes)
void VisitorStatusPanel::UpdateViewWindow() {
    if (gStaffRoomPanelThing == nullptr) return;
    alignas(8) uint8_t storage[0x20];
    visitor_callee::RectCtor(storage);
    CoolRect* rect = reinterpret_cast<CoolRect*>(storage);
    Fields(this)->mF14.get()->GetClientRect(rect);
    InterfaceWindow* window = Fields(this)->mF14.get();
    UIMain_ShowSpritePreview(*rect, reinterpret_cast<CPeep*>(gStaffRoomPanelThing), window->mZ + 2, 12.0f);
    visitor_callee::RectDtor(storage);
}
