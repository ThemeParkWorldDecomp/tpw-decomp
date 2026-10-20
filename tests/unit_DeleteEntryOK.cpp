// Test library for the postcard address delete button, two list row makers, ResearchPanelCategoryComplete and the timed messages' Add
// (unit v51043; libtpw_unit_DeleteEntryOK.dylib, tests/test_DeleteEntryOK.py), main app:
//   src/ui_Postcard_panel_CreateAddressBook_DeleteEntryOK.cpp   DeleteEntryOK, PostcardLCFormat, PostcardLCPostFormat,
//   ResearchPanelCategoryComplete, ResearchListLCFormat, InterfaceTimedMessages::Add
//
// The callees outside the unit are recording fakes (operator new, the constructors, Remove, FillAddressList, the font table and its
// virtual, the window calls, the research lab, NameWindow, PercentComplete, IveGotTicks). The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "email.h"
#include "ui_DrawObject.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CFSUserInfo* gUserInfo;
CWorld* gGameWorld;
InterfaceWindow* gResearchPanel;
int16_t gUIRealScreenHeight;
uint32_t gParkListFontIndex;

int16_t PostcardFontVirtual(InterfaceFont* font);   // the unit's weak seam, replaced below

namespace {
char gFontDummy[0x40];
const uintptr_t kLabObject = 0x1000;   // what GetResearchLab answers
uint32_t P32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// ---- the fakes
namespace postcard_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size == 0 ? 4 : size);
}
}  // namespace postcard_callee

int32_t InterfaceFont::GetHeight() { return 0; }   // only the weak seam's default calls it

// CFSUserInfo has string members the unit does not link: its constructor is defined under its mangled name without the class.
extern "C" void* UserInfoCtorFake(void* self) __asm__("__ZN11CFSUserInfoC1Ev");
extern "C" void* UserInfoCtorFake(void* self) {
    ev("CFSUserInfo", {newlab(self)});
    return self;
}

int32_t AddressBook::Remove(int32_t position) {
    // The offset of the book in the user info, as the Mac's (0xc4): the host's object is bigger (native pointers in its strings).
    const int64_t offset = reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(gUserInfo);
    ev("Remove", {offset - static_cast<int64_t>(offsetof(CFSUserInfo, mAddressBook)) + 0xc4, position});
    return 0;
}

void FillAddressList(int32_t n) { ev("FillAddressList", {n}); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(gFontDummy);
}

int16_t PostcardFontVirtual(InterfaceFont* font) {
    ev("FontVirt", {lab(font)});
    return static_cast<int16_t>(ret("FontVirt"));
}

// (The arm64 ABI returns `this` from a constructor, and the callers use it: the fakes below answer their first argument.)
extern "C" void* WindowCtorFake(void* self, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                int16_t bottom) __asm__("__ZN15InterfaceWindowC1EPS_iissss");
extern "C" void* WindowCtorFake(void* self, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                int16_t bottom) {
    ev("WindowCtor", {newlab(self), lab(parent), attributes, id, left, top, right, bottom});
    return self;
}

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return nullptr;
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}

// The sprite's bases and members are not part of the unit, so its constructor is defined under its mangled name without the class.
extern "C" void* SpriteCtorFake(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void* SpriteCtorFake(void* self, void* data) {
    ev("SpriteCtor", {newlab(self), static_cast<int64_t>(P32(data))});
    return self;
}

CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {static_cast<int64_t>(P32(this))});
    return reinterpret_cast<CResearchLab*>(kLabObject);
}

bool CResearchLab::IsCategoryActive(ResearchCat category) const {
    ev("IsCategoryActive", {static_cast<int64_t>(P32(this)), static_cast<int32_t>(category)});
    return ret("IsCategoryActive") & 0xff;
}

void CResearchLab::NextProject(ResearchCat category) { ev("NextProject", {static_cast<int64_t>(P32(this)), static_cast<int32_t>(category)}); }

void NameWindow(int32_t group) { ev("NameWindow", {group}); }
void PercentComplete(int32_t group) { ev("PercentComplete", {group}); }

uint32_t IveGotTicks() {
    ev("IveGotTicks", {});
    return static_cast<uint32_t>(ret("IveGotTicks"));
}

extern "C" {

uint64_t tpw_dwe_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gUserInfo", &gUserInfo},
        {"gGameWorld", &gGameWorld},
        {"gResearchPanel", &gResearchPanel},
        {"gUIRealScreenHeight", &gUIRealScreenHeight},
        {"gParkListFontIndex", &gParkListFontIndex},
        {"gResearchRowHeight", &gResearchRowHeight},
        {"font", gFontDummy},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dwe_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int64_t tpw_dwe_lab(uint64_t p) { return static_cast<int64_t>(lab(reinterpret_cast<const void*>(static_cast<uintptr_t>(p)))); }

int32_t tpw_dwe_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(InterfaceTimedMessages));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTimedMessages, mCount));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTimedMessages, mEntries));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceTimedMessage));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTimedMessage, mWindow));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTimedMessage, mMessage));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTimedMessage, mA));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTimedMessage, mB));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceTimedMessage, mTime));
    out[n++] = static_cast<int64_t>(offsetof(CFSUserInfo, mAddressBook));
    return n;
}

uint32_t tpw_dwe_delete(uint32_t position) {
    return static_cast<uint32_t>(DeleteEntryOK(reinterpret_cast<void*>(static_cast<uintptr_t>(position))));
}
uint64_t tpw_dwe_lcformat1() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(PostcardLCFormat())); }
void tpw_dwe_lcpost(uint32_t window) { PostcardLCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }
void tpw_dwe_research(int32_t category) { ResearchPanelCategoryComplete(category); }
uint64_t tpw_dwe_lcformat2() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ResearchListLCFormat())); }
int32_t tpw_dwe_add(uint64_t object, uint32_t window, int32_t message, int32_t a, int32_t b, uint32_t time) {
    InterfaceTimedMessages* messages = reinterpret_cast<InterfaceTimedMessages*>(static_cast<uintptr_t>(object));
    return messages->Add(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b, time);
}

}  // extern "C"
