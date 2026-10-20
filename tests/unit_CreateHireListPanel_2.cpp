// Test library for the hire panel's builder (unit v51019; libtpw_unit_CreateHireListPanel_2.dylib, tests/test_CreateHireListPanel_2.py), main app:
//   src/HirePanelLCHandler_CreateHireListPanel_2.cpp   CreateHireListPanel and the globals it defines
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's tables: the windows,
// strings, fonts and other objects the test made are looked up by label (the case's table), the numbers come from the queues.
// The globals of other files the unit reads or writes are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "LoadPanelMsg.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "analyser.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

using calllog::ev;
using calllog::newlab;
using calllog::ret;

namespace {
struct Entry {
    uint32_t label;
    void* pointer;
};
Entry gObjects[160];
int gObjectCount = 0;

// The case's objects by address first (the library's fixed table is small), then the library's own labels.
int64_t lab(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 0; i < gObjectCount; ++i)
        if (gObjects[i].pointer == p) return gObjects[i].label;
    return calllog::lab(p);
}
}  // namespace

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gGameWindow;
CWorld* gGameWorld;
int32_t gHirePanelType;
InterfaceWindow* gHirePanelWindow;
char16_t gUIMainConvertBuffer[0x4000];
// gText without its constructor and destructor (the unit reads one word of it).
alignas(16) unsigned char gTextRaw[sizeof(CText)] __asm__("_gText");

namespace {

// The case's objects by label: windows 0x10000 + id, strings 0x20000 + id, fonts 0x30000 + index, and the named ones.
void* Obj(uint32_t label) {
    for (int i = 0; i < gObjectCount; ++i)
        if (gObjects[i].label == label) return gObjects[i].pointer;
    return nullptr;
}

int64_t FnId(const void* f) {
    if (f == reinterpret_cast<const void*>(&HireListLCFormat)) return 1;
    if (f == reinterpret_cast<const void*>(&HirePanelLCPostFormat)) return 2;
    if (f == reinterpret_cast<const void*>(&HirePanelLCHandler)) return 3;
    if (f == reinterpret_cast<const void*>(&HirePanelMsg)) return 4;
    return f == nullptr ? 0 : -1;
}

}  // namespace

// The hire panel's functions the unit takes the address of (src/HirePanelLCHandler.cpp is not compiled here).
void HirePanelLCPostFormat(InterfaceWindow*) {}
int32_t HirePanelLCHandler(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return false;
}
void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }
void FillHireList(StaffType type, int32_t flag) { ev("FillHireList", {static_cast<int32_t>(type), flag}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return static_cast<const char16_t*>(Obj(0x20000u + static_cast<uint32_t>(id)));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("SetTitleText", {lab(window), id, lab(text)}); }
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {lab(window)}); }
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("IndentClient", {lab(window), a, b}); }
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("AdvisorMessage", {static_cast<int32_t>(message)}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return static_cast<InterfaceFont*>(Obj(0x30000u + index));
}
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
    ev("MAKECOLUMNBUTTON", {lab(window), lab(text), a, b, c, d, e});
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(Obj(0x10000u + static_cast<uint32_t>(id)));
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {lab(this), description == gHirePanelScript ? 1 : 0, FnId(reinterpret_cast<const void*>(callback))});
    return static_cast<InterfaceWindow*>(Obj(0x40002u));
}
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), FnId(reinterpret_cast<const void*>(callback))});
}
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(this), id});
    return -1;
}
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {lab(this), FnId(reinterpret_cast<const void*>(makeRow)), FnId(reinterpret_cast<const void*>(attach))});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) {
    ev("SetColumnType", {lab(this), column, static_cast<int32_t>(type)});
}
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {lab(this), lab(object)}); }
void InterfaceListControl2::SetSort(int32_t column, bool descending) { ev("SetSort", {lab(this), column, descending ? 1 : 0}); }
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {name - gHireListStrings});
    return static_cast<InterfaceDrawObject*>(Obj(0x40006u));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return static_cast<CBankAccount*>(Obj(0x40004u));
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return static_cast<CParkAnalyser*>(Obj(0x40005u));
}
template <>
bool CHistory<int, 144>::GetValue(int& out, int back) const {
    const char* base = static_cast<const char*>(Obj(0x40005u));
    const int64_t index = (reinterpret_cast<const char*>(this) - base - static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100))) /
                          static_cast<int64_t>(sizeof(CHistory<int, 144>));
    ev("GetValue", {index, lab(&out), back});
    out = static_cast<int32_t>(ret("GetValue", index, 0));
    return true;
}
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    TbIntegerVTag* tag = static_cast<TbIntegerVTag*>(va_arg(args, TbVTag*));
    va_end(args);
    ev("FormatString", {lab(this), lab(out), static_cast<int64_t>(size), static_cast<int64_t>(id), static_cast<int64_t>(tag->mMagic),
                        static_cast<int64_t>(tag->mId), tag->mValue});
    return 0;
}

// The integer tag's vtable lives with its key function (not compiled here); the fake is never called.
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }
// The constructors the weak default callees of the src file name (the real ones are other files').
// (a symbol alias: a defined constructor would need the class's whole vtable).
void CashCtorSymbol(void*) __asm__("__ZN23InterfaceCashTextObjectC1EPv");
void CashCtorSymbol(void*) {}
void TitleCtorSymbol(void*) __asm__("__ZN24InterfaceTitleTextObjectC1EPv");
void TitleCtorSymbol(void*) {}
void SpriteCtorSymbol(void*) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
void SpriteCtorSymbol(void*) {}
void GradientCtorSymbol(void*) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
void GradientCtorSymbol(void*) {}

namespace hirelist_callee {
int32_t Labs(int32_t value) {
    ev("labs", {value});
    return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
}
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void TitleCtor(void* mem) { ev("TitleCtor", {newlab(mem)}); }
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem)}); }
void GradientCtor(void* mem, const char* mesh) {
    ev("GradientCtor", {newlab(mem)});
    ev("GradientMesh", {mesh - gHireListStrings});
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {lab(window), value}); }
void WindowSetText(InterfaceWindow* window, char16_t* text) { ev("WindowSetText", {lab(window), lab(text)}); }
}  // namespace hirelist_callee

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_hl_addr(const char* name) {
    struct NamedAddress {
        const char* name;
        const void* address;
    };
    const NamedAddress entries[] = {
        {"gGameWindow", &gGameWindow},
        {"gGameWorld", &gGameWorld},
        {"gHirePanelType", &gHirePanelType},
        {"gHirePanelWindow", &gHirePanelWindow},
        {"gTextTable", &static_cast<CText*>(static_cast<void*>(gTextRaw))->mResourcePointers[6]},
        {"gUIMainConvertBuffer", gUIMainConvertBuffer},
        {"gHirePanelScript", gHirePanelScript},
        {"gHireColumnColour", &gHireColumnColour},
        {"gHireSortColumn", &gHireSortColumn},
        {"gHireListWord10", &gHireListWord10},
        {"gHireListWord14", &gHireListWord14},
        {"gHireListWithdrawals", &gHireListWithdrawals},
        {"gHireListDeposits", &gHireListDeposits},
        {"gHireListStrings", gHireListStrings},
    };
    for (const NamedAddress& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_hl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The case's objects: the fakes look them up by label.
void tpw_hl_clearobj() { gObjectCount = 0; }
void tpw_hl_setobj(uint32_t label, uint64_t pointer) {
    gObjects[gObjectCount].label = label;
    gObjects[gObjectCount].pointer = reinterpret_cast<void*>(static_cast<uintptr_t>(pointer));
    ++gObjectCount;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_hl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100) + 13 * sizeof(CHistory<int, 144>) <= 0x40000 ? 1 : 0);
    return n;
}

// ---- the function
void tpw_hl_create(int32_t type) {
    CreateHireListPanel(static_cast<StaffType>(type));
}

}  // extern "C"
