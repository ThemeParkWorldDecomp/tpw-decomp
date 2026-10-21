// Test library for the load, save and hire panels (unit v5183; libtpw_unit_LoadPanelMsg.dylib, tests/test_LoadPanelMsg.py), main app:
//   src/game/ui/ui_hirePanel.cpp, src/game/ui/ui_loadSavePanel.cpp   LoadPanelMsg, UI_HirePanelStateChange, SavePanelMsg (and the five globals they own)
//
// The callees outside the unit are recording fakes; the objects they answer are static blocks here. The globals of other files the unit
// reads (gGame, gText, gLoadSave, gGameWorld) are defined here: gGame and gText as raw bytes under the program's symbol names, because
// their constructors are not part of this library.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_LoadSave.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "StaffDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
alignas(16) unsigned char gGameBytes[sizeof(CGame)] asm("_gGame");
alignas(16) unsigned char gTextBytes[sizeof(CText)] asm("_gText");
CFSLoadSave gLoadSave;
CWorld* gGameWorld = nullptr;

namespace {

CGame* Game() { return reinterpret_cast<CGame*>(gGameBytes); }
CText* Text() { return reinterpret_cast<CText*>(gTextBytes); }

int64_t HashW(const char16_t* text) {
    if (text == nullptr) return 0xFFFFFFFFll;
    uint32_t h = 0;
    for (; *text != 0; ++text) h = h * 31 + static_cast<uint32_t>(*text);
    return h;
}

// The window fake: a table laid out like InterfaceWindow's (the Itanium table: the destructor takes two slots), SetText is slot 10.
struct FakeWin {
    virtual void v0() {}
    virtual void v1() {}
    virtual ~FakeWin() {}
    virtual int32_t v3() { return 0; }
    virtual void v4() {}
    virtual int16_t v5(int16_t) { return 0; }
    virtual int32_t v6(InterfaceWindow&) { return 0; }
    virtual int32_t v7(int32_t) { return 0; }
    virtual int32_t v8() { return 0; }
    virtual int32_t SetText(char16_t* text) {
        ev("SetText", {lab(this), 1, HashW(text)});
        return 1;
    }
};
FakeWin gFakeWin;

alignas(16) uint32_t gStaffBlock[8];
alignas(16) unsigned char gDetails[6][sizeof(TFSLoadSaveSaveFileDetails)];
int32_t gDetailCount = 0;

int32_t DetailIndex(const void* p) {
    const unsigned char* c = static_cast<const unsigned char*>(p);
    if (c < &gDetails[0][0] || c >= &gDetails[0][0] + sizeof(gDetails)) return -1;
    return static_cast<int32_t>((c - &gDetails[0][0]) / sizeof(gDetails[0]));
}

}  // namespace

// ---- the fakes (same event names and arguments as the module's PowerPC stubs)
void RemoveLoadPanel() { ev("RemoveLoadPanel", {}); }
void RemoveSavePanel() { ev("RemoveSavePanel", {}); }
void UIMain_DecideKbCapture() { ev("DecideKbCapture", {}); }
void UIMain_EnableAllKeyTables() { ev("EnableAllKeyTables", {}); }
void CGame::Unpause() { ev("Unpause", {lab(this)}); }
int32_t SaveCallback_OK(void*) { return 0; }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("GetWindow") != 0 ? reinterpret_cast<InterfaceWindow*>(&gFakeWin) : nullptr;
}

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(ret("GetData"));
    return 0;
}
int32_t InterfaceListControl2::FindData(int32_t data) {
    ev("FindData", {lab(this), data});
    return static_cast<int32_t>(ret("FindData"));
}
int32_t InterfaceListControl2::RemoveItem(int32_t position) {
    ev("RemoveItem", {lab(this), position});
    return 0;
}
int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {lab(this), HashW(item[0].mText), item[1].mInt, data, position, update});
    return 0;
}
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

CStaffInfo* CStaffDB::GetStaff(uint32_t slot) const {
    ev("GetStaff", {lab(this), static_cast<int64_t>(slot)});
    return ret("GetStaff") != 0 ? reinterpret_cast<CStaffInfo*>(gStaffBlock) : nullptr;
}
void CStaffInfo::GetName(char16_t* buffer, int32_t size) const {
    ev("GetName", {lab(this), size});
    buffer[0] = static_cast<char16_t>(0x41 + (ret("GetName") & 15));
    buffer[1] = 0x42;
    buffer[2] = 0;
}
uint32_t CStaffInfo::GetSalary() const {
    ev("GetSalary", {lab(this)});
    return static_cast<uint32_t>(ret("GetSalary"));
}

TFSLoadSaveSaveFileDetails* CFSLoadSave::GetFirstSaveFileDetails() {
    ev("GetFirst", {lab(this)});
    return gDetailCount > 0 ? reinterpret_cast<TFSLoadSaveSaveFileDetails*>(gDetails[0]) : nullptr;
}
TFSLoadSaveSaveFileDetails* CFSLoadSave::GetNextSaveFileDetails(const TFSLoadSaveSaveFileDetails* previous) {
    int32_t index = DetailIndex(previous);
    ev("GetNext", {lab(this), index});
    return index >= 0 && index + 1 < gDetailCount ? reinterpret_cast<TFSLoadSaveSaveFileDetails*>(gDetails[index + 1]) : nullptr;
}
int32_t CFSLoadSave::Load(const TbDynamicStringTemplate<char16_t>& name) {
    ev("Load", {lab(this), DetailIndex(&name), HashW(name.mData)});
    return 0;
}
void CFSLoadSave::Save(const TbDynamicStringTemplate<char16_t>& name) { ev("Save", {lab(this), HashW(name.mData)}); }

int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("PostMessage", {lab(window), message, a, b});
    return 0;
}
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                              dlgbutton button3, dlgcallback callback3, bool flag) {
    ev("DialogBox", {HashW(params.mText), params.mRed, params.mGreen, params.mBlue, params.mValue8, static_cast<int32_t>(button1),
                     callback1 != nullptr, static_cast<int32_t>(button2), callback2 == &SaveCallback_OK, static_cast<int32_t>(button3),
                     callback3 != nullptr, flag});
    return nullptr;
}

int32_t wcscmp(const char16_t* a, const char16_t* b) {
    ev("wcscmp", {HashW(a), HashW(b)});
    for (; *a != 0 && *a == *b; ++a, ++b) {
    }
    return *a < *b ? -1 : (*a > *b ? 1 : 0);
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    const TbUniStringVTag* tag = va_arg(args, const TbUniStringVTag*);
    va_end(args);
    ev("FormatString", {lab(this), static_cast<int64_t>(size), static_cast<int64_t>(id), tag->mMagic, tag->mId, HashW(tag->mText)});
    out[0] = static_cast<char16_t>(0x41 + (ret("fmt") & 15));
    out[1] = 0x42;
    out[2] = 0;
    return 0;
}
int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("StrCtor", {HashW(text)});
    mData = const_cast<char16_t*>(text);
}
// The string class's own virtuals (vtable members): never reached, the constructor and destructor above are fakes.
template <>
void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template <>
void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <>
void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {
    ev("StrDtor", {-1});
}

extern "C" {

uint64_t tpw_lp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"game3c", &Game()->mF3C},
        {"loadflag", &gLoadPanelOpen},
        {"saveflag", &gSavePanelOpen},
        {"hirewin", &gHirePanelWindow},
        {"hiretype", &gHirePanelType},
        {"name", gSavePanelName},
        {"tbl", &Text()->mResourcePointers[6]},
        {"staff", gStaffBlock},
        {"win", &gFakeWin},
        {"loadsave", &gLoadSave},
        {"game", gGameBytes},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CStaffInfo, mType));
    return n;
}

void tpw_lp_ndet(int32_t count) { gDetailCount = count; }
void tpw_lp_setdetail(int32_t index, uint64_t text) {
    reinterpret_cast<TFSLoadSaveSaveFileDetails*>(gDetails[index])->mName.mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text));
}

uint32_t tpw_lp_load(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(LoadPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}
uint32_t tpw_lp_save(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return static_cast<uint32_t>(SavePanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b));
}
void tpw_lp_hire(int32_t slot) { UI_HirePanelStateChange(slot); }

}  // extern "C"
