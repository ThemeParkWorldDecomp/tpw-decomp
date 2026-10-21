// Test library for the hire panel's functions (unit v5984; libtpw_unit_UpdateViewWindow_2.dylib, tests/test_UpdateViewWindow_2.py), main app:
//   src/game/ui/ui_FinancePanel.cpp, src/game/ui/ui_hirePanel.cpp   UI_FinancePanelPanelDateChange, FillHireList, UpdateViewWindow, HirePanelMsg,
//                                                          HireListLCFormat (LCFormat) and the global gHireListFormat
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The globals of other
// files the unit reads are defined here, zero at start like the Mac's bss. The game text object (gText) is raw storage under the symbol of
// gText, so that no constructor of its fonts is needed.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "InterfaceFont.h"
#include "LoadPanelMsg.h"
#include "MainWindowCallback.h"
#include "StaffDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ESP_sprites.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
CWorld* gGameWorld;
InterfaceWindow* gHirePanelWindow;
int32_t gHirePanelType;
int32_t gHireListScript;
int32_t gHireListSelA;
int32_t gHireListSelB;
int32_t gHireSortColumn;
int32_t gHireListWord10;
int32_t gHireListWord14;
int32_t gHireListDeposits;
uint32_t gHireLCFontIndex;
int16_t gUIRealScreenHeight;
CUIGrapher gFinanceGrapher;
InterfaceWindow* gFinanceGraphPanel;
uint32_t gStaffRoomSpawnBlock[1];
char16_t gUIMainConvertBuffer[0x4000];
extern "C" {
alignas(16) unsigned char gTextStorage[0x4000] __asm__("_gText");
}

namespace {

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Q(const char* key, int64_t dflt = 0) { return static_cast<int64_t>(ret(key, dflt)); }
int64_t QA(const char* key, int64_t arg, int64_t dflt = 0) { return static_cast<int64_t>(ret(key, arg, dflt)); }

template <class T>
T* Ptr(int64_t value) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(value));
}

// The text a callee writes or an event records: four characters.
int64_t Chars(const char16_t* text, int index) { return text == nullptr ? -1 : static_cast<int64_t>(text[index]); }

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() { ev("RectCtor", {0}); }
CoolRect::~CoolRect() { ev("RectDtor", {-1}); }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return Ptr<InterfaceWindow>(Q("GetWindow"));
}

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {lab(this)});
    rect->mLeft = static_cast<int16_t>(Q("RectL"));
    rect->mTop = static_cast<int16_t>(Q("RectT"));
    rect->mRight = static_cast<int16_t>(Q("RectR"));
    rect->mBottom = static_cast<int16_t>(Q("RectB"));
    return 0;
}

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return nullptr;
}

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(this), id, ms});
    return static_cast<int16_t>(Q("setAnim"));
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {lab(this), which});
    return 0;
}

float CalcZTrans(int32_t z) {
    ev("CalcZTrans", {z});
    uint32_t bits = static_cast<uint32_t>(Q("CalcZTrans"));
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

CStaffInfo* CStaffDB::GetStaff(uint32_t slot) const {
    ev("GetStaff", {lab(this), static_cast<int64_t>(slot)});
    return Ptr<CStaffInfo>(Q("GetStaff"));
}

int32_t CStaffDB::HireStaff(uint32_t slot) {
    ev("HireStaff", {lab(this), static_cast<int64_t>(slot)});
    return 0;
}

void CStaffInfo::GetName(char16_t* buffer, int32_t size) const {
    ev("GetName", {lab(this), size});
    buffer[0] = static_cast<char16_t>(0x41 + (mName & 15));
    buffer[1] = static_cast<char16_t>(0x61 + ((mName >> 4) & 15));
    buffer[2] = static_cast<char16_t>(0x30 + ((mName >> 8) & 15));
    buffer[3] = 0;
}

uint32_t CStaffInfo::GetSalary() const {
    ev("GetSalary", {lab(this)});
    return mTimeoutTime;
}

void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }

int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    ev("ESP_SpawnScript", {lab(block), static_cast<int64_t>(sprite), a, static_cast<int64_t>(acts), CanonBits(x), CanonBits(y), CanonBits(z)});
    return static_cast<int32_t>(Q("ESP"));
}

void SCR_SetFloatVariable(int32_t handle, uint32_t index, float value) {
    ev("SCR_SetFloatVariable", {handle, static_cast<int64_t>(index), CanonBits(value)});
}

void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {lab(this), Chars(item[0].mText, 0), Chars(item[0].mText, 1), Chars(item[0].mText, 2), item[1].mInt, data, position, update});
    return 0;
}

void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {lab(this), position});
    *data = static_cast<int32_t>(Q("GetData"));
    return 0;
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return Ptr<CBankAccount>(Q("Bank"));
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(Q("Balance"));
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    TbIntegerVTag* tag = va_arg(args, TbIntegerVTag*);
    va_end(args);
    ev("FormatString", {lab(this), lab(out), static_cast<int64_t>(size), static_cast<int64_t>(id), static_cast<int64_t>(tag->mMagic),
                        static_cast<int64_t>(tag->mId), tag->mValue});
    uint32_t value = static_cast<uint32_t>(tag->mValue);
    out[0] = static_cast<char16_t>(id);
    out[1] = static_cast<char16_t>(value & 0xffff);
    out[2] = static_cast<char16_t>(value >> 16);
    out[3] = 0x5a;
    out[4] = 0;
    return 0;
}

int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }   // the key function of the tag's vtable; never called here

namespace hirelist_callee {
int32_t Labs(int32_t value) {
    ev("Labs", {value});
    return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
void WindowSetText(InterfaceWindow* window, char16_t* text) {
    ev("SetText", {lab(window), Chars(text, 0), Chars(text, 1), Chars(text, 2), Chars(text, 3)});
}
}  // namespace hirelist_callee

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return Ptr<void>(Q("New"));
}
void WindowCtor(void*, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("WindowCtor", {parent != nullptr ? lab(parent) : 0, attributes, id, left, top, right, bottom});
}
}  // namespace lender_callee

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return Ptr<InterfaceFont>(Q("Font"));
}

int32_t InterfaceFont::GetHeight() {
    ev("FontHeight", {lab(this)});
    return static_cast<int32_t>(Q("FontHeight"));
}

void RemoveHireListPanel() { ev("RemoveHireListPanel", {0}); }
void KissMyLines(CUIGrapher&) { ev("KissMyLines", {0}); }     // only its address is used (DrawMeGraph is a fake)

void CreateRideListPanel(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selected) {
    ev("CreateRideListPanel", {static_cast<int64_t>(static_cast<int32_t>(type)),static_cast<int64_t>(selected.mIndex)});
}

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("Advisor", {static_cast<int64_t>(message)}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisor", {static_cast<int64_t>(message)}); }

int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("Default", {lab(window), message, a, b});
    return static_cast<int32_t>(Q("Default"));
}

int32_t GetMaxYValue() {
    ev("GetMaxY", {0});
    return static_cast<int32_t>(Q("MaxY"));
}

int32_t GetMinYValue() {
    ev("GetMinY", {0});
    return static_cast<int32_t>(Q("MinY"));
}

void CUIGrapher::SetYRange(int32_t a, int32_t b, int32_t c) { ev("SetYRange", {lab(this), a, b, c}); }

void DrawMeGraph(InterfaceWindow* window, CUIGrapher& grapher, void (*callback)(CUIGrapher&)) {
    ev("DrawMeGraph", {lab(window), lab(&grapher), lab(reinterpret_cast<void*>(callback))});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_hmsg_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"hirewin", &gHirePanelWindow},
        {"hiretype", &gHirePanelType},
        {"script", &gHireListScript},
        {"selA", &gHireListSelA},
        {"selB", &gHireListSelB},
        {"sort", &gHireSortColumn},
        {"w10", &gHireListWord10},
        {"w14", &gHireListWord14},
        {"dep", &gHireListDeposits},
        {"fmt", &gHireListFormat},
        {"fontidx", &gHireLCFontIndex},
        {"screenh", &gUIRealScreenHeight},
        {"table", &reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[6]},
        {"graphpanel", &gFinanceGraphPanel},
        {"grapher", &gFinanceGrapher},
        {"kiss", reinterpret_cast<const void*>(&KissMyLines)},
        {"spawn", gStaffRoomSpawnBlock},
        {"cbuf", gUIMainConvertBuffer},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
// The blocks come from a pool in the library's image (addresses below 4 GB, so the call log's labels can name them); the pool starts over
// when used up (the test writes every block it uses).
uint64_t tpw_hmsg_alloc(uint32_t size) {
    alignas(16) static unsigned char pool[0x20000];
    static uint32_t used = 0;
    size = (size + 15) & ~15u;
    if (used + size > sizeof(pool)) used = 0;
    unsigned char* block = pool + used;
    used += size;
    std::memset(block, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}

int32_t tpw_hmsg_zoff();

// The offsets the module relies on (HOST_LAYOUT): the staff record's fields, which are the Mac's.
int32_t tpw_hmsg_layout(int64_t* out) {
    alignas(16) static unsigned char block[0x400];
    CStaffInfo* staff = reinterpret_cast<CStaffInfo*>(block);
    int32_t n = 0;
    out[n++] = reinterpret_cast<unsigned char*>(&staff->mName) - block;
    out[n++] = reinterpret_cast<unsigned char*>(&staff->mPayGrade) - block;
    out[n++] = reinterpret_cast<unsigned char*>(&staff->mSubType) - block;
    out[n++] = reinterpret_cast<unsigned char*>(&staff->mTimeoutTime) - block;
    out[n++] = tpw_hmsg_zoff();
    return n;
}

// The host offset of a window's mZ (the Mac's is 0xdc).
int32_t tpw_hmsg_zoff() {
    alignas(16) static unsigned char block[0x400];
    InterfaceWindow* window = reinterpret_cast<InterfaceWindow*>(block);
    return static_cast<int32_t>(reinterpret_cast<unsigned char*>(&window->mZ) - block);
}

// ---- the functions, one wrapper each with C types only
void tpw_hmsg_fin(int32_t month) { UI_FinancePanelPanelDateChange(month); }
void tpw_hmsg_fhl(int32_t type, int32_t flag) { FillHireList(static_cast<StaffType>(type), flag); }
void tpw_hmsg_uvw(int32_t slot) { UpdateViewWindow(slot); }
int32_t tpw_hmsg_hpm(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return HirePanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}
uint64_t tpw_hmsg_lcf() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(HireListLCFormat())); }

}  // extern "C"
