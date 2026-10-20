// Test library for the save panel's row maker and OK handler (unit v5702; libtpw_unit_LCFormat_5.dylib, tests/test_LCFormat_5.py), main app:
//   src/LoadPanelMsg_LCFormat_5.cpp   SavePanelLCFormat (LCFormat 0x101a3154), SaveCallback_OK 0x101a2d74, gSavePanelListFormat
//
// The callees outside the unit are recording fakes: GetFontTableEntry, the font's height virtual (InterfaceFont::GetHeight stands in for the
// __ptr_glue call), the InterfaceWindow constructor and CreateObject, CFSLoadSave::Save, RemoveSavePanel and the string constructor and
// destructor. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LoadPanelMsg.h"
#include "FS_LoadSave.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "UI_Base.h"

using namespace calllog;

int16_t gUIRealScreenHeight;
char16_t gSavePanelName[34];
CFSLoadSave gLoadSave;

// The constructor fake stores the vptr of InterfaceWindow, whose ~125 virtuals live in files this library does not compile and which nothing
// here calls: a zeroed block stands in for the vtable (the Itanium name of "vtable for InterfaceWindow").
extern "C" {
alignas(16) char gFakeInterfaceWindowVtable[4096] __asm__("__ZTV15InterfaceWindow");
}

// ---- the fakes
// CoolRect is a member of InterfaceWindow: the constructor fake below makes it, so its vtable (and CoolRegion's) must link.
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    static char block[64];
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(block);
}

int32_t InterfaceFont::GetHeight() {
    ev("ptr_glue", {lab(this)});
    return static_cast<int32_t>(ret("ptr_glue"));
}

InterfaceWindow::InterfaceWindow(InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                 int16_t bottom) {
    newlab(this);
    ev("InterfaceWindow", {lab(this), lab(parent), attributes, id, left, top, right, bottom});
}

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return nullptr;
}

void CFSLoadSave::Save(const TbDynamicStringTemplate<char16_t>& name) { ev("Save", {lab(this), lab(&name)}); }

void RemoveSavePanel() { ev("RemoveSavePanel", {}); }

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    newlab(this);
    ev("StringCtor", {lab(this)}, {narrow(text).c_str()});
}
// The other virtuals exist only to fill the vtable the destructor fake emits; nothing calls them.
using Str16 = TbDynamicStringTemplate<char16_t>;
template <> void Str16::AllocBuffer(uint32_t) {}
template <> void Str16::Format(const char16_t*, ...) {}
template <> void Str16::ResizeTo(uint32_t) {}
template <> Str16& Str16::operator=(const TbStringBase<char16_t>&) { return *this; }
template <> Str16& Str16::operator=(const Str16&) { return *this; }
template <> Str16& Str16::operator=(const char16_t*) { return *this; }
template <> Str16& Str16::operator=(const char16_t&) { return *this; }
template <> Str16& Str16::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <> Str16& Str16::operator+=(const char16_t&) { return *this; }
template <> Str16& Str16::operator+=(const char16_t*) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {
    ev("StringDtor", {lab(this), -1});
}

extern "C" {

uint64_t tpw_lcf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"format", &gSavePanelListFormat},
        {"screen", &gUIRealScreenHeight},
        {"name", gSavePanelName},
        {"loadsave", &gLoadSave},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lcf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lcf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gSavePanelName));
    out[n++] = static_cast<int64_t>(sizeof(gSavePanelListFormat));
    out[n++] = static_cast<int64_t>(sizeof(gUIRealScreenHeight));
    return n;
}

// The label of the window the row maker answers.
int64_t tpw_lcf_format() { return lab(SavePanelLCFormat()); }
int32_t tpw_lcf_save() { return SaveCallback_OK(nullptr); }

}  // extern "C"
