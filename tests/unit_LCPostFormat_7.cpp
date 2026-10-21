// Test library for the park find panel's list functions (unit v5930; libtpw_unit_LCPostFormat_7.dylib, tests/test_LCPostFormat_7.py), main app:
//   src/game/ui/ui_onlineParkFindPanel.cpp   OnlineParkFindLCFormat (LCFormat), OnlineParkFindLCPostFormat (LCPostFormat),
//                                                   FillOnlineParkFindList; it defines the guard byte and the row height.
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues. Texts are logged as a
// hash (the length first, then 31 h + character). The globals of other files the unit reads are defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"

using namespace calllog;

// ---- globals of other files, defined here
InterfaceWindow* gOnlineParkFindPanel;
CFSGlobalMasterSave* gMasterSave;
CFSOnlineSystem* gOnlineSystem;
int16_t gUIRealScreenHeight;
const char16_t gOnlineParkFindWide[] = u"wide";
// gGame is a CGame whose constructor is not part of this library: raw storage under its symbol.
alignas(16) char gGameStorage[256] __asm__("_gGame");

// ---- blocks the test sets (host pointers)
void* gTestFont;
void* gTestList;
void* gTestButton;
void* gTestThemeList;
void* gTestCityList;
void* gTestCity0;
void* gTestCity1;
void* gTestString;
void* gTestLastWindow;
void* gTestParks[16];

namespace {

int64_t HashU(const char16_t* p) {
    uint32_t n = 0;
    while (n < 80 && p[n] != 0) n++;
    uint32_t h = n;
    for (uint32_t i = 0; i < n; i++) h = (h * 31u + p[i]) & 0x7fffffffu;
    return h;
}

int64_t CellHash(const char16_t* p) {
    if (p == nullptr) return -1;
    if (p == gOnlineParkFindWide) return -2;
    return HashU(p);
}

int64_t HashC(const char* p) {
    uint32_t n = 0;
    while (n < 80 && p[n] != 0) n++;
    uint32_t h = n;
    for (uint32_t i = 0; i < n; i++) h = (h * 31u + static_cast<uint8_t>(p[i])) & 0x7fffffffu;
    return h;
}

std::deque<std::string> sThemeNames;
std::deque<std::u16string> sLocalised;

}  // namespace

// The weak FontHeight of the translation's file calls this; the strong fake below replaces that function, this only satisfies the linker.
int32_t InterfaceFont::GetHeight() { return 0; }
CFSGlobalMasterSave::CFSGlobalMasterSave() {}
uint32_t wcslen(const char16_t* text) {
    uint32_t n = 0;
    while (text[n]) n++;
    return n;
}
char16_t* wcscpy(char16_t* dst, const char16_t* src) {
    char16_t* d = dst;
    while ((*d++ = *src++) != 0) {}
    return dst;
}
char16_t* wcsncpy(char16_t* dst, const char16_t* src, uint32_t count) {
    uint32_t i = 0;
    for (; i < count && src[i]; i++) dst[i] = src[i];
    for (; i < count; i++) dst[i] = 0;
    return dst;
}
const char16_t* wcsstr(const char16_t* text, const char16_t* needle) {
    for (; *text; text++) {
        const char16_t *a = text, *b = needle;
        while (*b && *a == *b) { a++; b++; }
        if (*b == 0) return text;
    }
    return *needle ? nullptr : text;
}
char16_t* wcsncat(char16_t* dst, const char16_t* src, uint32_t count) {
    char16_t* d = dst;
    while (*d) d++;
    for (uint32_t i = 0; i < count && src[i]; i++) *d++ = src[i];
    *d = 0;
    return dst;
}
int32_t wcsncmp(const char16_t* a, const char16_t* b, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        if (a[i] == 0) break;
    }
    return 0;
}
// The static string's two virtuals sit in a vtable the dynamic string's file pulls in; never called here.
template <> void TbStaticStringTemplate<char>::ResizeTo(uint32_t) {}
template <> void TbStaticStringTemplate<char16_t>::ResizeTo(uint32_t) {}
#define STATIC_STR_STUBS(CH)                                                                                                     \
    template <> TbStaticStringTemplate<CH>& TbStaticStringTemplate<CH>::operator=(const CH*) { return *this; }                   \
    template <> TbStaticStringTemplate<CH>& TbStaticStringTemplate<CH>::operator=(const CH&) { return *this; }                  \
    template <> TbStaticStringTemplate<CH>& TbStaticStringTemplate<CH>::operator=(const TbStringBase<CH>&) { return *this; }     \
    template <> TbStaticStringTemplate<CH>& TbStaticStringTemplate<CH>::operator+=(const CH*) { return *this; }                  \
    template <> TbStaticStringTemplate<CH>& TbStaticStringTemplate<CH>::operator+=(const CH&) { return *this; }                 \
    template <> TbStaticStringTemplate<CH>& TbStaticStringTemplate<CH>::operator+=(const TbStringBase<CH>&) { return *this; }
STATIC_STR_STUBS(char)
STATIC_STR_STUBS(char16_t)
#define DYN_STR_STUBS(CH)                                                                                                        \
    template <> void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}                                                        \
    template <> void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                                           \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; }                \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }                \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>&) { return *this; }
DYN_STR_STUBS(char16_t)
template <> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
char16_t* wcscat(char16_t* dst, const char16_t* src) {
    char16_t* d = dst;
    while (*d) d++;
    while ((*d++ = *src++) != 0) {}
    return dst;
}

// ---- the fakes
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return static_cast<InterfaceFont*>(gTestFont);
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return static_cast<const char16_t*>(gTestString);
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (id == 0xe05b) return static_cast<InterfaceWindow*>(gTestList);
    if (id == 0xe059) return static_cast<InterfaceWindow*>(gTestButton);
    return nullptr;
}

InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(ret("GetChild")));
}

InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(ret("GetNext")));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this), lab(text)}); }

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return nullptr;
}

void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    // The row's data is a park pointer cut to 32 bits (the host's addresses are wider): matched by its low word, as the park's label.
    int64_t dataLabel = data == 0 ? 0 : -1;
    for (int i = 0; i < 16; i++)
        if (gTestParks[i] != nullptr && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gTestParks[i])) == static_cast<uint32_t>(data))
            dataLabel = 40 + i;
    ev("AddItem", {lab(this), dataLabel, position, update,
                   CellHash(item[0].mText), CellHash(item[1].mText), CellHash(item[2].mText), item[3].mInt, item[4].mInt, item[5].mInt});
    return 0;
}

int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(this), state, redraw});
    return 0;
}

bool CFSGlobalMasterSave::IsSwearFilterOn() const {
    ev("IsSwearFilterOn", {lab(this)});
    return (ret("Swear") & 0xff) != 0;
}

bool SwearFilter::Filter(std::u16string& text, bool replace) {
    ev("Filter", {lab(this), HashU(text.c_str()), replace ? 1 : 0});
    if (ret("FilterMod") != 0 && !text.empty()) text[0] = u'*';
    return true;
}

char* CFSOnlineSystemThemeList::ReturnOnlineThemeName(int32_t theme) {
    ev("ReturnOnlineThemeName", {lab(this), theme});
    sThemeNames.push_back("Th" + std::to_string(theme & 7));
    return const_cast<char*>(sThemeNames.back().c_str());
}

void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    ev("GetLocalisedThemeName", {lab(this), HashC(theme.mData)});
    std::u16string text = u"L";
    for (const char* p = theme.mData; *p != 0; p++) text.push_back(static_cast<char16_t>(static_cast<uint8_t>(*p)));
    sLocalised.push_back(text);
    out.mData = const_cast<char16_t*>(sLocalised.back().c_str());
}

namespace park_info_callee {
CFSOnlineCity* ReturnCityInfo(CFSOnlineSystemCityList* list, int32_t city) {
    ev("ReturnCityInfo", {lab(list), city});
    return static_cast<CFSOnlineCity*>((city & 1) != 0 ? gTestCity1 : gTestCity0);
}
}  // namespace park_info_callee

namespace findpanel_callee {
CFSOnlineSystem* NewOnlineSystem() {
    ev("new", {0x404});
    CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystem)));
    ev("NewOnlineSystem", {newlab(system)});
    system->mThemeList = static_cast<CFSOnlineSystemThemeList*>(gTestThemeList);
    system->mCityList = static_cast<CFSOnlineSystemCityList*>(gTestCityList);
    return system;
}
}  // namespace findpanel_callee

namespace parklist_callee {
void RankCtor(void* mem) { ev("RankCtor", {newlab(mem)}); }
}  // namespace parklist_callee

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void NumberCtor(void* mem) { ev("NumberCtor", {newlab(mem)}); }
void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                int16_t bottom) {
    ev("WindowCtor", {newlab(mem), lab(parent), attributes, id, left, top, right, bottom});
    gTestLastWindow = mem;
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {lab(window), value}); }
}  // namespace lender_callee

namespace onlineparkfind_callee {
int16_t FontHeight(InterfaceFont* font) {
    ev("FontHeight", {lab(font)});
    return static_cast<int16_t>(ret("FontHeight"));
}
CFSGlobalMasterSave* NewMasterSave() {
    ev("new", {0x4c});
    void* master = tpw::ArenaAllocZeroed(0x4c);
    ev("NewMasterSave", {newlab(master)});
    return static_cast<CFSGlobalMasterSave*>(master);
}
void WStrCtor(void* mem) {
    ev("WStrCtor", {});
    std::memset(mem, 0, sizeof(TbDynamicStringTemplate<char16_t>));
}
void WStrDtor(void*) { ev("WStrDtor", {}); }
void CStrCtor(void* mem, const char* text) {
    ev("CStrCtor", {HashC(text)});
    std::memset(mem, 0, sizeof(TbDynamicStringTemplate<char>));
    static_cast<TbDynamicStringTemplate<char>*>(mem)->mData = const_cast<char*>(text);
}
void CStrDtor(void*) { ev("CStrDtor", {}); }
}  // namespace onlineparkfind_callee

extern "C" {

uint64_t tpw_opf_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gOnlineParkFindPanel}, {"game", gGameStorage},        {"master", &gMasterSave},     {"os", &gOnlineSystem},
        {"screen", &gUIRealScreenHeight}, {"rowinit", &gOnlineParkFindRowInit}, {"rowheight", &gOnlineParkFindRowHeight},
        {"font", &gTestFont},             {"list", &gTestList},          {"button", &gTestButton},     {"themelist", &gTestThemeList},
        {"citylist", &gTestCityList},     {"city0", &gTestCity0},        {"city1", &gTestCity1},       {"string", &gTestString},
        {"lastwin", &gTestLastWindow},    {"parks", gTestParks},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_opf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets and sizes the module needs (the Mac's are in the module).
int32_t tpw_opf_layout(int64_t* out) {
    alignas(16) static char parkBuf[sizeof(CFSOnlinePark)];
    alignas(16) static char listBuf[sizeof(TbPtrList<CFSOnlinePark>)];
    alignas(16) static char cityBuf[sizeof(CFSOnlineCity)];
    alignas(16) static char systemBuf[sizeof(CFSOnlineSystem)];
    auto* park = reinterpret_cast<CFSOnlinePark*>(parkBuf);
    auto* list = reinterpret_cast<TbPtrList<CFSOnlinePark>*>(listBuf);
    auto* city = reinterpret_cast<CFSOnlineCity*>(cityBuf);
    auto* system = reinterpret_cast<CFSOnlineSystem*>(systemBuf);
    auto* node = reinterpret_cast<TbPtrList<CFSOnlinePark>::CNode*>(listBuf);
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&park->mText0C.mData) - parkBuf;   // 0
    out[n++] = reinterpret_cast<char*>(&park->mF4C) - parkBuf;            // 1
    out[n++] = reinterpret_cast<char*>(&park->mF48) - parkBuf;            // 2
    out[n++] = reinterpret_cast<char*>(&park->mFB8) - parkBuf;            // 3
    out[n++] = reinterpret_cast<char*>(&park->mF50) - parkBuf;            // 4
    out[n++] = reinterpret_cast<char*>(&park->mF54) - parkBuf;            // 5
    out[n++] = sizeof(CFSOnlinePark);                                     // 6
    out[n++] = reinterpret_cast<char*>(&list->mHead) - listBuf;           // 7
    out[n++] = reinterpret_cast<char*>(&list->mCount) - listBuf;          // 8
    out[n++] = sizeof(TbPtrList<CFSOnlinePark>);                          // 9
    out[n++] = reinterpret_cast<char*>(&node->mNext) - listBuf;           // 10
    out[n++] = sizeof(TbPtrList<CFSOnlinePark>::CNode);                   // 11
    out[n++] = reinterpret_cast<char*>(&city->mName) - cityBuf;           // 12
    out[n++] = sizeof(CFSOnlineCity);                                     // 13
    out[n++] = reinterpret_cast<char*>(&system->mThemeList) - systemBuf;  // 14
    out[n++] = reinterpret_cast<char*>(&system->mCityList) - systemBuf;   // 15
    out[n++] = sizeof(CFSOnlineSystem);                                   // 16
    out[n++] = reinterpret_cast<char*>(&node->mData) - listBuf;           // 17
    return n;
}

uint64_t tpw_opf_format() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(OnlineParkFindLCFormat())); }

void tpw_opf_post(uint64_t window) { OnlineParkFindLCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }

void tpw_opf_fill(uint64_t parks) { FillOnlineParkFindList(reinterpret_cast<TbPtrList<CFSOnlinePark>*>(static_cast<uintptr_t>(parks))); }

}  // extern "C"
