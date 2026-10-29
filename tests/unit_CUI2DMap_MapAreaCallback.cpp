// Test library for the map area window's message function (unit v5985; libtpw_unit_CUI2DMap_MapAreaCallback.dylib,
// tests/test_CUI2DMap_MapAreaCallback.py), main app:
//   src/CUI2DMap_..._CUI2DMap_MapAreaCallback.cpp   CUI2DMap::MapAreaCallback and the two "no thing" halfwords
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues: UpdateMap, MoveCentre,
// GetMapGridCell, the string class (constructor, assignment, destructor), CStaff::GetName, CObject::GetUIName, TbITable::FormatString (and
// the integer tag it is given), GetParent, GetWindow, the virtual set-text, SetCameraPOI, UI_SendMessage, the status panel, the default
// window handler and GAME_ASSERTMSG. The globals of other files (the screen size, the text resources, the player settings, the thing array
// and the conversion buffer) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
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
#include "ui_main.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gScreenWidth;
int32_t gScreenHeight;
char16_t gUIMainConvertBuffer[0x4000];
CThingArrayEntry gThingArray[kThingArraySize];
// The text resources and the player settings have constructors the library does not link: raw storage under the same symbols.
alignas(16) uint8_t gTextStorage[sizeof(CText)] __asm__("_gText");
alignas(16) uint8_t gPlayerSettingsStorage[sizeof(CPlayerSettings)] __asm__("_gPlayerSettings");

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Addr(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// ---- the fakes, the same event names and arguments as the module's PowerPC stubs
void CUI2DMap::UpdateMap() { ev("UpdateMap", {lab(this)}); }

void CUI2DMap::MoveCentre(EUI2DMapMoveDirection direction, float amount) {
    ev("MoveCentre", {lab(this), static_cast<int64_t>(static_cast<int32_t>(direction)), CanonBits(amount)});
}

// The answer 0 is NULL, n is the entry n of the grid (8 bytes each, at Mac offset 0x60).
void* CUI2DMap::GetMapGridCell(int32_t x, int32_t y) {
    ev("GetMapGridCell", {lab(this), x, y});
    int64_t n = ret("GetMapGridCell");
    if (n == 0) return nullptr;
    return reinterpret_cast<uint8_t*>(this) + 0x60 + 8 * n;
}

// The string class: the data pointer is the text it was given.
template <> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("StrCtor", {}, {narrow(text).c_str()});
    mLength = 0;
    mData = const_cast<char16_t*>(text);
    mCapacity = 0;
    mGrowBy = 0;
}
template <> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() { ev("StrDtor", {-1}); }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t* text) {
    ev("StrAssign", {}, {narrow(text).c_str()});
    mData = const_cast<char16_t*>(text);
    return *this;
}
template <> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template <> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate<char16_t>&) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template <> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

// The name of a staff member is at thing + 0x100 (the test writes it there).
const char16_t* CStaff::GetName() const {
    ev("GetName", {lab(this)});
    return reinterpret_cast<const char16_t*>(reinterpret_cast<const uint8_t*>(this) + 0x100);
}

// The text written is 'N' and the two digits of the object's label.
void CObject::GetUIName(char16_t* out, uint32_t size) {
    int64_t l = lab(this);
    ev("GetUIName", {l, size});
    out[0] = u'N';
    out[1] = static_cast<char16_t>(u'0' + (l / 10) % 10);
    out[2] = static_cast<char16_t>(u'0' + l % 10);
    out[3] = 0;
}

// The text written is 'F', the id's last digit and the tag's value (low three bits) or 'x'.
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {lab(this), size, id});
    char16_t last = u'x';
    if (id == 0x1c8) {
        va_list args;
        va_start(args, id);
        const TbIntegerVTag* tag = va_arg(args, const TbIntegerVTag*);
        va_end(args);
        ev("FormatTag", {tag->mMagic, tag->mId, tag->mValue});
        last = static_cast<char16_t>(u'0' + (tag->mValue & 7));
    }
    out[0] = u'F';
    out[1] = static_cast<char16_t>(u'0' + id % 10);
    out[2] = last;
    out[3] = 0;
    return 0;
}

int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

// The windows of a case sit in one block: the window, 0x200 on the parent, 0x400 on the child.
InterfaceWindow* InterfaceWindow::GetParent() {
    ev("GetParent", {lab(this)});
    return reinterpret_cast<InterfaceWindow*>(reinterpret_cast<uint8_t*>(this) + 0x200);
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(reinterpret_cast<uint8_t*>(this) + 0x200);
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

namespace mapcb_callee {
void WindowSetText(InterfaceWindow* window, const char16_t* text) { ev("PtrGlue", {lab(window)}, {narrow(text).c_str()}); }
}  // namespace mapcb_callee

void SetCameraPOI(float* x, float* y, float* zoom, float* yaw, float* value) {
    ev("SetCameraPOI", {CanonBits(*x), CanonBits(*y), Addr(zoom), Addr(yaw), Addr(value)});
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("UI_SendMessage"));
}

void UIMain_GotoStatusPanelForThing(void* thing) { ev("UIMain_GotoStatusPanelForThing", {lab(thing)}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_map_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"w", &gScreenWidth},
        {"h", &gScreenHeight},
        {"set", gPlayerSettingsStorage},
        {"text", gTextStorage},
        {"buf", gUIMainConvertBuffer},
        {"none1", &gMapNoThingA},
        {"none2", &gMapNoThingB},
        {"arr", gThingArray},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_map_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A block's offset in the arena: what a Ptr32 word holds for it (the map panel window at map + 4).
uint32_t tpw_map_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_map_layout(int64_t* out) {
    typedef TbTList<CUI2DMapSprite*, CUI2DMapSprite*> List;
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(List, mHead));
    out[n++] = static_cast<int64_t>(offsetof(List::CNode, mNext));
    out[n++] = static_cast<int64_t>(offsetof(List::CNode, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mVisible));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mThing));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(offsetof(CText, mResourcePointers) + 6 * sizeof(void*));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mF0C));
    return n;
}

// ---- the function
int32_t tpw_map_call(uint64_t map, uint64_t window, int32_t message, int32_t a, int32_t b) {
    return reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->MapAreaCallbackResult(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
