// Test library for the save and load panels' openers and list fills (unit v5969; libtpw_unit_CreateLoadPanel_2.dylib,
// tests/test_CreateLoadPanel_2.py), main app:
//   src/game/ui/ui_loadSavePanel.cpp   CreateLoadPanel, CreateSavePanel, FillLoadList,
//   FillSaveList, LoadSaveLCPostFormat (the Mac's LCPostFormat 0x101a3258) and the globals gLoadSaveScript, gLoadSaveHilight,
//   gLoadSaveFormat.
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the unit reads (gGame, gText, gLoadSave, the convert buffer, the panels' words) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FS_LoadSave.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "TbVTag.h"
#include "Text.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
// gGame is raw zeroed storage under its symbol: CGame's member constructors live in files this library does not compile.
extern CGame gGame;
alignas(16) unsigned char gGameStorage[sizeof(CGame)] __asm__("_gGame");
CText gText;
CFSLoadSave gLoadSave;
char16_t gUIMainConvertBuffer[0x4000];
int32_t gLoadPanelOpen;
int32_t gSavePanelOpen;
char16_t gSavePanelName[34];

InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}   // gText's member; its real constructor is in a file this library does not compile

// The row makers and message functions are only compared by address.
InterfaceWindow* SavePanelLCFormat() { return nullptr; }
int32_t LoadPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t SavePanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
// The vtable of the integer tag lives with its Format (an interim in the launcher).
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

namespace {

constexpr int kObjects = 11;
void* gObjects[kObjects];                               // 32-bit addressable fake objects, labels 1..11
alignas(16) unsigned char gEntryStorage[4][128];        // fake TFSLoadSaveSaveFileDetails (only the fields the unit reads are set)
char16_t gEntryNames[4][34];
int32_t gEntryCount;

#define I(x) static_cast<int64_t>(x)

void* Obj(int i) {
    if (gObjects[i] == nullptr) gObjects[i] = tpw::ArenaAllocZeroed(0x40);
    return gObjects[i];
}

int64_t Hash(const char16_t* text) {
    if (text == nullptr) return -1;
    uint32_t h = 0;
    for (; *text != 0; ++text) h = h * 31 + *text;
    return h;
}

TFSLoadSaveSaveFileDetails* Entry(int i) { return reinterpret_cast<TFSLoadSaveSaveFileDetails*>(gEntryStorage[i]); }

int EntryIndex(const TFSLoadSaveSaveFileDetails* d) {
    for (int i = 0; i < 4; ++i)
        if (d == Entry(i)) return i;
    return -1;
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {I(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {I(newlab(mem))}); }
}  // namespace lender_callee
namespace loadsave_callee {
void CursorPosCtor(void* mem) { ev("CursorPosCtor", {I(newlab(mem))}); }
void FileTimeToLocal(const void* in, void* out) {
    const uint32_t* a = static_cast<const uint32_t*>(in);
    uint32_t* b = static_cast<uint32_t*>(out);
    ev("FileTimeToLocal", {I(a[0]), I(a[1])});
    b[0] = a[0] ^ 0x1234u;
    b[1] = a[1] + 7u;
}
}  // namespace loadsave_callee
namespace onlineparkfind_callee {
void WStrCtor(void*) { ev("WStrCtor", {}); }
void WStrDtor(void*) { ev("WStrDtor", {-1}); }   // the Mac passes the delete flag -1
}  // namespace onlineparkfind_callee
namespace hirelist_callee {
void WindowSetText(InterfaceWindow* window, char16_t* text) { ev("WindowSetText", {I(lab(window)), I(lab(text))}); }
}  // namespace hirelist_callee

void CGame::Pause(ePauseFlag flag, bool allSounds) { ev("Pause", {I(lab(this)), I(flag), I(allSounds ? 1 : 0)}); }
bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return false;
}
InterfaceWindow* Dialog(int16_t* script, int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t), int32_t modal) {
    int64_t which = handler == LoadPanelMsg ? 1 : handler == SavePanelMsg ? 2 : 0;
    ev("Dialog", {I(script == gLoadSaveScript ? 1 : 0), which, I(modal)});
    return ret("Dialog") != 0 ? static_cast<InterfaceWindow*>(Obj(0)) : nullptr;
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {I(lab(this)), I(on)}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {I(id)});
    return static_cast<const char16_t*>(Obj(id == 0xca ? 6 : id == 0xc9 ? 7 : 8));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("SetTitleText", {I(lab(window)), I(id), I(lab(text))});
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {I(lab(this)), I(id)});
    if (id == -1) return static_cast<InterfaceWindow*>(Obj(1));
    if (id == 0x23bce2) return static_cast<InterfaceWindow*>(Obj(2));
    if (id == 0x23bce0) return ret("list") != 0 ? static_cast<InterfaceWindow*>(Obj(3)) : nullptr;
    return nullptr;
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {I(lab(this)), I(on)});
    return 0;
}
void InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*makeRow)(), void (*attach)(InterfaceWindow*)) {
    ev("SetColumnFormatsAll", {I(lab(this)), I(makeRow == SavePanelLCFormat ? 1 : 0), I(attach == LoadSaveLCPostFormat ? 1 : 0)});
}
void InterfaceListControl2::SetColumnType(int16_t column, LCDataType type) { ev("SetColumnType", {I(lab(this)), I(column), I(type)}); }
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {I(name == gLoadSaveHilight ? 1 : 0)});
    return static_cast<InterfaceDrawObject*>(Obj(4));
}
void InterfaceListControl2::SetSelectionObject(InterfaceDrawObject* object) { ev("SetSelectionObject", {I(lab(this)), I(lab(object))}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {I(lab(this)), I(lab(object))}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {I(index)});
    return static_cast<InterfaceFont*>(Obj(5));
}
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {I(lab(this)), I(lab(font))}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {I(lab(this)), I(red), I(green), I(blue), I(alpha)});
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {I(lab(this)), I(horizontal), I(vertical)}); }
InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {I(lab(this)), I(unused)});
    return static_cast<InterfaceWindow*>(Obj(9));
}
int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t capacity) {
    ev("SetStaticTextArea", {I(lab(this)), I(text == gSavePanelName ? 1 : 0), I(capacity)});
    return 0;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* object) { ev("SetCursorDrawObject", {I(lab(this)), I(lab(object))}); }
void InterfaceEditBox::SetUnAcceptedChars(char16_t* text) { ev("SetUnAcceptedChars", {I(lab(this)), I(text - gLoadSaveFormat)}); }
void UIMain_DisableAllKeyTables() { ev("DisableAllKeyTables", {}); }

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    const char16_t* second = item[1].mText;
    ev("AddItem", {I(lab(this)), Hash(item[0].mText), I(second == gUIMainConvertBuffer ? 1 : 0), Hash(second), I(data), I(position),
                   I(update)});
    return 0;
}

TFSLoadSaveSaveFileDetails* CFSLoadSave::GetFirstSaveFileDetails() {
    ev("GetFirst", {I(lab(this))});
    return gEntryCount > 0 ? Entry(0) : nullptr;
}
TFSLoadSaveSaveFileDetails* CFSLoadSave::GetNextSaveFileDetails(const TFSLoadSaveSaveFileDetails* previous) {
    int i = EntryIndex(previous);
    ev("GetNext", {I(lab(this)), I(i)});
    return i + 1 < gEntryCount ? Entry(i + 1) : nullptr;
}

TbTimeStamp& TbTimeStamp::operator=(NS_MacDoze::tagFILETIME& ft) {
    ev("StampSet", {I(ft.dwLowDateTime), I(ft.dwHighDateTime)});
    mTicks = static_cast<int64_t>((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
    return *this;
}
TbError TbTimeStamp::GetTime(int32_t* year, int32_t* month, int32_t* day, int32_t* dayOfWeek, int32_t* hour, int32_t* minute,
                             int32_t* second, int32_t* millisecond) const {
    uint64_t t = static_cast<uint64_t>(mTicks);
    ev("GetTime", {I(t >> 32), I(t & 0xffffffffu), I(dayOfWeek == nullptr ? 1 : 0), I(second == nullptr ? 1 : 0),
                   I(millisecond == nullptr ? 1 : 0), I(year != nullptr), I(month != nullptr), I(day != nullptr), I(hour != nullptr),
                   I(minute != nullptr)});
    if (year) *year = static_cast<int32_t>(ret("year"));
    if (month) *month = static_cast<int32_t>(ret("month"));
    if (day) *day = static_cast<int32_t>(ret("day"));
    if (hour) *hour = static_cast<int32_t>(ret("hour"));
    if (minute) *minute = static_cast<int32_t>(ret("minute"));
    return TbError(0);
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    const TbIntegerVTag* tags[3];
    for (auto& tag : tags) tag = va_arg(args, const TbIntegerVTag*);
    va_end(args);
    ev("FormatString", {I(lab(this)), I(size), I(id), I(tags[0]->mMagic), I(tags[0]->mId), I(tags[0]->mValue), I(tags[1]->mMagic),
                        I(tags[1]->mId), I(tags[1]->mValue), I(tags[2]->mMagic), I(tags[2]->mId), I(tags[2]->mValue)});
    out[0] = static_cast<char16_t>(0x61 + (ret("fmtc") & 15));
    out[1] = 0;
    return 0;
}

int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list args;
    va_start(args, format);
    const char16_t* text = va_arg(args, const char16_t*);
    int32_t hour = va_arg(args, int32_t);
    int32_t minute = va_arg(args, int32_t);
    va_end(args);
    ev("swprintf", {I(buffer == gUIMainConvertBuffer ? 1 : 0), I(size), Hash(format), Hash(text), I(hour), I(minute)});
    buffer[0] = static_cast<char16_t>(0x41 + (hour & 15));
    buffer[1] = static_cast<char16_t>(0x30 + (minute & 7));
    buffer[2] = 0;
    return 2;
}

extern "C" {

uint64_t tpw_cl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"game", &gGame},
        {"game_f3c", &gGame.mF3C},   // the Mac's gGame+0x3c (the host's CGame is laid out with 8-byte pointers)
        {"loadsave", &gLoadSave},
        {"buf", gUIMainConvertBuffer},
        {"panel_open", &gLoadPanelOpen},
        {"save_open", &gSavePanelOpen},
        {"savename", gSavePanelName},
        {"fmt", gLoadSaveFormat},
        {"script", gLoadSaveScript},
        {"hilight", gLoadSaveHilight},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_cl_obj(int32_t i) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Obj(i))); }

// Entry i: its stamp words (Mac 0x14 and 0x18) and the address of its 34-character name.
uint64_t tpw_cl_entry(int32_t i, uint32_t w0, uint32_t w1) {
    TFSLoadSaveSaveFileDetails* d = Entry(i);
    std::memset(gEntryStorage[i], 0, sizeof gEntryStorage[i]);
    std::memset(gEntryNames[i], 0, sizeof gEntryNames[i]);
    d->mName.mData = gEntryNames[i];
    d->mStampHigh = w0;
    d->mStampLow = w1;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gEntryNames[i]));
}
void tpw_cl_count(int32_t n) { gEntryCount = n; }

void tpw_cl_set_table(uint64_t table) { gText.mResourcePointers[6] = reinterpret_cast<void*>(static_cast<uintptr_t>(table)); }

int32_t tpw_cl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CGame, mF3C));
    return n;
}

void tpw_cl_load(int32_t mode) { CreateLoadPanel(mode); }
void tpw_cl_save(int32_t mode) { CreateSavePanel(mode); }
void tpw_cl_fillload(uint64_t list) { FillLoadList(reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))); }
void tpw_cl_fillsave(uint64_t list) { FillSaveList(reinterpret_cast<InterfaceListControl2*>(static_cast<uintptr_t>(list))); }
void tpw_cl_post(uint64_t window) { LoadSaveLCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }

}  // extern "C"
