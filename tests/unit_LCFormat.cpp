// Test library for the online park list panel's row functions, message function and fill (unit v5210; libtpw_unit_LCFormat.dylib,
// tests/test_LCFormat.py), main app:
//   src/PublishPanelMsg_LCFormat.cpp   OnlineParkListLCFormat, OnlineParkListLCPostFormat, OnlineParkListPanelMsg, FillParkList
//
// Every callee outside the unit is a fake that records what the real one reads (the same event names and arguments as the module's
// PowerPC stubs) and answers from the test's queues. The globals of other files are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "UI_Base.h"
#include "calllog.h"
#include "lobby.h"
#include "online.h"
#include "platform/arena.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
CLBLobby* gLobby;
CFSGlobalMasterSave* gMasterSave;
CFSOnlineSystem* gOnlineSystem;
InterfaceWindow* gOnlineParkListPanel;
void (*gOnlineParkListClose)();
int16_t gUIRealScreenHeight;
// CGame has no constructor of its own in this library: the storage is the symbol.
alignas(16) char gGameStorage[sizeof(CGame)] __asm__("_gGame");
// The weak FilterMsl of the unit's file names this; the strong FilterMsl below replaces it, so this is never called.
bool SwearFilter::Filter(std::u16string&, bool) { return false; }

namespace {

int64_t I(int64_t v) { return v; }

// The persistent objects (zeroed, 32-bit addressable, the same blocks in every case), by index.
void* Pool(int index, size_t size) {
    static void* blocks[32];
    if (blocks[index] == nullptr) blocks[index] = tpw::ArenaAllocZeroed(static_cast<uint32_t>(size));
    return blocks[index];
}
InterfaceWindow* Win(int i) { return static_cast<InterfaceWindow*>(Pool(i, sizeof(InterfaceWindow))); }   // 0..7
void* ParkBlock(int i) { return Pool(8 + i, sizeof(CFSOnlinePark)); }                                       // 8..11
InterfaceFont* FontBlock() { return static_cast<InterfaceFont*>(Pool(12, sizeof(InterfaceFont))); }
void* MasterBlock() { return Pool(13, sizeof(CFSGlobalMasterSave)); }
void* OnlineBlock() { return Pool(14, sizeof(CFSOnlineSystem)); }
void* ThemeListBlock() { return Pool(15, 64); }
void* LobbyBlock() { return Pool(16, sizeof(CLBLobby)); }

char gThemeNames[4][16] = {"fantasy", "hallow", "jungle", "space"};
const char16_t gLocalised[4][8] = {u"Fant", u"Hall", u"Jung", u"Spac"};
LCStringRep gRep;

uint32_t Hash16(const char16_t* text) {
    if (text == nullptr) return 0x1234;
    uint32_t h = 2166136261u;
    for (; *text != 0; ++text) h = (h ^ static_cast<uint32_t>(*text)) * 16777619u;
    return h;
}

void FakeClose() { ev("Close", {}); }

// The Mac keeps the park in a 32-bit word; the host's word holds only the low half of the pointer. The persistent parks give the
// full pointer back (labels are by address).
const void* Widen(const void* p) {
    for (int i = 0; i < 4; ++i)
        if ((reinterpret_cast<uintptr_t>(ParkBlock(i)) & 0xFFFFFFFFu) == reinterpret_cast<uintptr_t>(p)) return ParkBlock(i);
    return p;
}

}  // namespace

// ---- the fakes
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {I(index)});
    return FontBlock();
}
int32_t InterfaceFont::GetHeight() {
    ev("GetHeight", {I(lab(this))});
    return static_cast<int32_t>(ret("GetHeight"));
}

InterfaceWindow* InterfaceWindow::GetChild(int32_t a) {
    ev("GetChild", {I(lab(this)), I(a)});
    return Win(3);
}
InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {I(lab(this))});
    for (int i = 3; i < 6; ++i)
        if (this == Win(i)) return Win(i + 1);
    return nullptr;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {I(lab(this)), I(lab(object))}); }
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {I(lab(this)), I(lab(object))}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {I(lab(this)), I(lab(font))}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {I(lab(this)), I(lab(text))}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) {
    ev("SetFontJust", {I(lab(this)), I(horizontal), I(vertical)});
}
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {I(lab(this)), I(on)}); }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {I(lab(this)), I(id)});
    return ret("GetWindow") != 0 ? Win(1) : nullptr;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {I(lab(this)), I(message), I(a), I(b)});
    return static_cast<int32_t>(ret("CallDefault"));
}
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {I(lab(this)), I(kind), I(attributes), I(id), I(left), I(top), I(right), I(bottom)});
    return nullptr;
}

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* data) {
    ev("GetData", {I(lab(this)), I(position)});
    *data = static_cast<int32_t>(reinterpret_cast<uintptr_t>(ParkBlock(0)));
    return 0;
}
void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {I(lab(this))}); }
void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {I(lab(this))}); }
int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    const void* park = Widen(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(data))));
    ev("AddItem", {I(lab(this)), I(Hash16(item[0].mText)), I(Hash16(item[1].mText)), I(item[2].mInt), I(item[3].mInt), I(item[4].mInt),
                   I(lab(park)), I(position), I(update)});
    return 0;
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {I(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {I(newlab(mem)), 0}); }
void NumberCtor(void* mem) { ev("NumberCtor", {I(newlab(mem)), 0}); }
void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                int16_t bottom) {
    ev("WindowCtor", {I(newlab(mem)), I(lab(parent)), I(attributes), I(id), I(left), I(top), I(right), I(bottom)});
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {I(lab(window)), I(value)}); }
}  // namespace lender_callee

namespace parklist_callee {
void RankCtor(void* mem) { ev("RankCtor", {I(newlab(mem)), 0}); }
bool FilterMsl(SwearFilter* filter, void* slot, bool replace) {
    ev("Filter", {I(lab(filter)), I(replace ? 1 : 0)});
    LCStringRep* rep = *static_cast<LCStringRep**>(slot);
    if (ret("FilterChanges") != 0 && rep->mData[0] != 0) rep->mData += 1;
    return false;
}
}  // namespace parklist_callee

namespace online_callee {
void WStringCtor(void* self) {
    ev("WStringCtor", {});
    gRep.mLength = 0;
    gRep.mCapacity = 3;
    gRep.mRefCount = 1;
    gRep.mData = nullptr;
    *static_cast<LCStringRep**>(self) = &gRep;
}
void WStringAssign(void* self, const char16_t* text, uint32_t n) {
    ev("WStringAssign", {I(Hash16(text)), I(n)});
    LCStringRep* rep = *static_cast<LCStringRep**>(self);
    rep->mLength = static_cast<int32_t>(n);
    rep->mData = const_cast<char16_t*>(text);
}
void WStringDtor(void*, int16_t flag) { ev("WStringDtor", {I(flag)}); }
}  // namespace online_callee

CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("MasterSaveCtor", {I(newlab(this))}); }
bool CFSGlobalMasterSave::IsSwearFilterOn() const {
    ev("IsSwearFilterOn", {I(lab(this))});
    return (ret("IsSwearFilterOn") & 0xff) != 0;
}
CFSOnlineSystem::CFSOnlineSystem() {
    ev("OnlineSystemCtor", {I(newlab(this))});
    mThemeList = nullptr;
}
char* CFSOnlineSystemThemeList::ReturnOnlineThemeName(int32_t theme) {
    ev("ReturnOnlineThemeName", {I(lab(this)), I(theme)});
    return gThemeNames[theme & 3];
}
void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    ev("GetLocalisedThemeName", {I(lab(this)), I(lab(&theme)), I(lab(&out))});
    out.mData = const_cast<char16_t*>(gLocalised[ret("Loc") & 3]);
}

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("CharCtor", {I(newlab(this))}, {text});
}
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    ev("CharDtor", {I(lab(this)), -1});
}
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {
    ev("WideCtor", {I(newlab(this))});
}
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {
    ev("WideDtor", {I(lab(this)), -1});
}

// Virtual slots the fakes' vtables name (never called here).
template <>
void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template <>
void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
#define TPW_VSTUBS(CH)                                                                                                  \
    template <> void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}                                             \
    template <> void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                                 \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH*) { return *this; }        \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) { return *this; }        \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; }       \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }
TPW_VSTUBS(char16_t)
TPW_VSTUBS(char)

void CreateOnlineParkInfoPanel(const CFSOnlinePark* park, InterfaceWindow* parent, void (*onCancel)(),
                               void (*onComplete)(int32_t, const void*), void* context) {
    ev("InfoPanel", {I(lab(Widen(park))), I(lab(parent)), onCancel != nullptr ? 1 : 0,
                     onComplete == GeneralParkDownloadCompleteCallback ? 1 : (onComplete != nullptr ? 2 : 0), context != nullptr ? 1 : 0});
}
void GeneralParkDownloadCompleteCallback(int32_t, const void*) {}
void RemoveOnlineParkListPanel() { ev("RemoveOnlineParkListPanel", {}); }

extern "C" {

// The host address of a persistent object or global by the module's name.
uint64_t tpw_pl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"w0", Win(0)}, {"w1", Win(1)}, {"w2", Win(2)}, {"w3", Win(3)}, {"w4", Win(4)}, {"w5", Win(5)}, {"w6", Win(6)}, {"w7", Win(7)},
        {"park0", ParkBlock(0)}, {"park1", ParkBlock(1)}, {"park2", ParkBlock(2)}, {"park3", ParkBlock(3)},
        {"font", FontBlock()}, {"master", MasterBlock()}, {"onlinesys", OnlineBlock()}, {"themelist", ThemeListBlock()},
        {"game", &gGameStorage}, {"lobby", LobbyBlock()}, {"closefn", reinterpret_cast<const void*>(&FakeClose)},
        {"g_panel", &gOnlineParkListPanel}, {"g_close", &gOnlineParkListClose}, {"g_lobby", &gLobby}, {"g_master", &gMasterSave},
        {"g_online", &gOnlineSystem}, {"g_rowh", &gOnlineParkListRowHeight}, {"g_rowinit", &gOnlineParkListRowInit},
        {"g_screenh", &gUIRealScreenHeight},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The label the call log gives a pointer.
int64_t tpw_pl_lab(uint64_t p) { return lab(reinterpret_cast<const void*>(static_cast<uintptr_t>(p))); }

static int64_t Off(const void* base, const void* member) {
    return static_cast<int64_t>(reinterpret_cast<const char*>(member) - reinterpret_cast<const char*>(base));
}

// Host offsets of the members the module writes by Mac offset (the Mac's in Python's MAC table), then sizes.
int32_t tpw_pl_layout(int64_t* out) {
    int32_t n = 0;
    CLBLobby* lobby = static_cast<CLBLobby*>(LobbyBlock());
    out[n++] = Off(lobby, &lobby->mWindow);                                  // 0
    CFSOnlinePark* park = static_cast<CFSOnlinePark*>(ParkBlock(0));
    out[n++] = Off(park, &park->mText0C.mData);                              // 1
    out[n++] = Off(park, &park->mF4C);                                       // 2
    out[n++] = Off(park, &park->mF50);                                       // 3
    out[n++] = Off(park, &park->mF54);                                       // 4
    out[n++] = Off(park, &park->mFB8);                                       // 5
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlinePark));                  // 6
    TbPtrList<CFSOnlinePark>* list = reinterpret_cast<TbPtrList<CFSOnlinePark>*>(tpw::ArenaAllocZeroed(sizeof(TbPtrList<CFSOnlinePark>)));
    out[n++] = Off(list, &list->mHead);                                      // 7
    out[n++] = Off(list, &list->mCount);                                     // 8
    out[n++] = static_cast<int64_t>(sizeof(TbPtrList<CFSOnlinePark>));       // 9
    TbTList<CFSOnlinePark*, CFSOnlinePark*>::CNode* node =
        reinterpret_cast<TbTList<CFSOnlinePark*, CFSOnlinePark*>::CNode*>(tpw::ArenaAllocZeroed(64));
    out[n++] = Off(node, &node->mNext);                                      // 10
    out[n++] = Off(node, &node->mPrev);                                      // 11
    out[n++] = static_cast<int64_t>(sizeof(*node));                          // 12
    CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(OnlineBlock());
    out[n++] = Off(system, &system->mThemeList);                             // 13
    return n;
}

// ---- the functions, one wrapper each with C types only
int64_t tpw_pl_fmt() { return lab(OnlineParkListLCFormat()); }
void tpw_pl_post(uint64_t window) { OnlineParkListLCPostFormat(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window))); }
int32_t tpw_pl_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return OnlineParkListPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}
void tpw_pl_fill(uint64_t list) { FillParkList(reinterpret_cast<TbPtrList<CFSOnlinePark>*>(static_cast<uintptr_t>(list))); }

}  // extern "C"
