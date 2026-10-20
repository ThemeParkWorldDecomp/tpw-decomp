// Unit v51058: twenty small functions of the main app: the world lobby controller's messages, the static initialisers of
// ui_worldLobbyController.cpp, ui_publishParkPanel.cpp, TPGlobals.cpp and OnlinePerson.cpp, the 2D map callback, two lobby screen
// callbacks that call a virtual slot, a few std::__cdeque / std::less helpers.
#include <cstdint>
#include <cstring>
#include <new>
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "MslWStringTree.h"
#include "TPGlobals.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "lobby.h"
#include "online.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// ---- storage of the static initialisers (the Mac addresses are in the comments; the names are invented)
alignas(16) uint8_t gWorldLobbyControllerText[0x30];   // 0x10396ff0
uint8_t gWorldLobbyControllerTextNode[12];             // 0x10397004
alignas(16) uint8_t gPublishParkPanelText[0x30];       // 0x1039639c
uint8_t gPublishParkPanelTextNode[12];                 // 0x103963b0
uint8_t gTPGlobalsDtorVector[8];                       // stand-in for the TVector at 0x10268a08 (~TPGlobals)
uint8_t gTPGlobalsNode[12];                            // 0x103bc158
uint32_t gOnlinePersonWord20;                          // 0x10396d20
uint32_t gOnlinePersonWord24;                          // 0x10396d24

// ---- weak stand-ins of the callees this unit declares itself (the test library has its own)
namespace wstring_less_callee {
__attribute__((weak)) uint32_t Size(const MslWideString*) { return 0; }
__attribute__((weak)) const char16_t* Begin(const MslWideString*) { return nullptr; }
}  // namespace wstring_less_callee

// ---- the world lobby controller: the satellite parent window is the controller itself
// 0x1022b718
void ShowWorldLobbyController() { UI_SendMessage(gUISatelliteParent, 6, 1, 0); }
// 0x1022b780
void HideWorldLobbyController() { UI_SendMessage(gUISatelliteParent, 6, 0, 0); }
// 0x1022b6ac
void RemoveWorldLobbyController() { UI_SendMessage(gUISatelliteParent, 4, 0, 0); }

// 0x1022bacc
void __sinit_ui_worldLobbyController_cpp() {
    static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= sizeof gWorldLobbyControllerText, "storage");
    auto* text = ::new (gWorldLobbyControllerText) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(text, reinterpret_cast<void*>(&UIMain_DestroyWString), gWorldLobbyControllerTextNode);
}

// 0x1022bf74 (the weak stand-in in the CUI2DMap file yields to this one)
int32_t UI_2DMap_CallbackFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    return gUI2DMap.MapCallback(window, message, a, b);
}

// 0x1022a9a8: the download globe's virtual slot 0x4c
void FindParkOnlineParkInfoPanelCancelCallback() { parkinfo_callee::DownloadGlobeSlot4c(gLobbyScreens->mDownloadGlobe); }

// ---- std::__cdeque helpers (the Mac words: count +0, start +8, block table +0xc; a cursor is block table, index, slot)
namespace {
void CursorAddWords(uint32_t* w, int32_t n);

void CursorAddPositive(uint32_t* w, int32_t n) {
    const uint32_t* table = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | w[0]));
    uint32_t t = table[2] + (w[1] + static_cast<uint32_t>(n));
    uint32_t count = table[0];
    if (t < count) w[2] = w[2] + (static_cast<uint32_t>(n) << 2);
    else w[2] = table[3] + ((t - count) << 2);
    w[1] = w[1] + static_cast<uint32_t>(n);
}

void CursorAddWords(uint32_t* w, int32_t n) {
    if (n >= 0) {
        CursorAddPositive(w, n);
        return;
    }
    int32_t negated = static_cast<int32_t>(0u - static_cast<uint32_t>(n));
    if (negated < 0) {   // INT_MIN: the original calls itself with the same arguments, for ever
        CursorAddWords(w, n);
        return;
    }
    const uint32_t* table = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | w[0]));
    uint32_t t = table[2] + w[1];
    uint32_t back = static_cast<uint32_t>(negated);
    if (back > t) w[2] = table[3] + ((table[0] - back) << 2);
    else w[2] = w[2] - (back << 2);
    w[1] = w[1] + static_cast<uint32_t>(n);
}
}  // namespace

// 0x101f5508 const_iterator::operator+=, declared twice (online.h and std_deque_errorthingy.h)
namespace argdeque_callee {
void CursorAdd(MslArgumentCursor* cursor, int32_t count) { CursorAddWords(reinterpret_cast<uint32_t*>(cursor), count); }
}
namespace errdeque_callee {
void CursorAdd(MslErrorDequeCursor* cursor, int32_t count) { CursorAddWords(reinterpret_cast<uint32_t*>(cursor), count); }
}

// 0x10229498 iterator::operator+= of the deque of vector<Argument>*: a negative count is operator-= with the count negated
MslErrorDequeCursor* MslArgumentVectorCursorAdd(MslErrorDequeCursor* cursor, int32_t count) {
    uint32_t* w = reinterpret_cast<uint32_t*>(cursor);
    if (count < 0) {
        errdeque_callee::CursorSub(cursor, static_cast<int32_t>(0u - static_cast<uint32_t>(count)));
        return cursor;
    }
    CursorAddPositive(w, count);
    return cursor;
}

// 0x101f4c84 iterator::operator--
namespace errdeque_callee {
void CdequeDecrement(MslErrorDequeCursor* cursor) {
    cursor->mWord4 = cursor->mWord4 - 1;
    const uint32_t* table = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | cursor->mCdeque));
    uint32_t slot = cursor->mSlot;
    if (slot == table[3]) cursor->mSlot = table[3] + (table[0] << 2);
    cursor->mSlot = cursor->mSlot - 4;
}
}  // namespace errdeque_callee

// 0x101f2004 operator[]: the slot's address
namespace {
void* CdequeIndexWords(void* cdeque, uint32_t index) {
    const uint32_t* d = static_cast<const uint32_t*>(cdeque);
    uint32_t i = index + d[2];
    if (!(i < d[0])) i -= d[0];
    return reinterpret_cast<void*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | static_cast<uint32_t>(d[3] + (i << 2))));
}
}  // namespace
namespace argdeque_callee {
void* CdequeIndex(void* cdeque, uint32_t index) { return CdequeIndexWords(cdeque, index); }
}
namespace errdeque_callee {
void* CdequeIndex(void* cdeque, uint32_t index) { return CdequeIndexWords(cdeque, index); }
}

// 0x102255a0 operator!= of two deque iterators: the element words differ
namespace argdeque_callee {
bool IterNotEqual(const MslArgumentDequeIter* a, const MslArgumentDequeIter* b) { return a->mElement != b->mElement; }
}

// ---- strings
namespace wstring_less_callee {
// 0x10225e8c operator<(wstring, wstring): compare of all of a with all of b
bool Lt(const MslWideString* a, const MslWideString* b) {
    uint32_t bSize = Size(b);
    const char16_t* bBegin = Begin(b);
    uint32_t aSize = Size(a);
    return IME::MslWideString_compare(reinterpret_cast<const IME::MslWideString*>(a), 0, aSize, bBegin, bSize) < 0;
}
}  // namespace wstring_less_callee

// 0x1021f000 map<nocase_string, BuddyInfo>::value_compare::operator()(pair, key) const: the pair's first member is at 0
bool BuddyMapValueCompare_PairKey(const void* functor, const void* pair, const void* key) {
    (void)functor;
    return NocaseWStringLess(pair, key);
}
// 0x1021eed0 the same with (key, pair)
bool BuddyMapValueCompare_KeyPair(const void* functor, const void* key, const void* pair) {
    (void)functor;
    return NocaseWStringLess(key, pair);
}

// ---- TPGlobals.cpp and OnlinePerson.cpp
// 0x1023331c
void __sinit_TPGlobals_cpp() {
    TPGlobals* globals = ::new (static_cast<void*>(&gTPGlobals)) TPGlobals();
    __register_global_object(globals, gTPGlobalsDtorVector, gTPGlobalsNode);
}
// 0x10213548
void __sinit_OnlinePerson_cpp() {
    gOnlinePersonWord24 = 0;
    gOnlinePersonWord20 = 0x10000;
}

// ---- the error dialog's handler: closing it (0x14) forgets it, then the standard dialog handler
// 0x101f1c54
int32_t ErrorMessageCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gUIErrorDialog = nullptr;
    return DialogBoxCallback(window, message, a, b);
}

// ---- the publish park panel
// 0x101ecfe8
void __sinit_ui_publishParkPanel_cpp() {
    static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= sizeof gPublishParkPanelText, "storage");
    auto* text = ::new (gPublishParkPanelText) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(text, reinterpret_cast<void*>(&UIMain_DestroyWString), gPublishParkPanelTextNode);
}
// 0x101ecf6c
void RemovePublishParkPanel() {
    gLobbyScreens->mAdvisor->ClearAll();
    UI_SendMessage(gPublishParkPanel, 4, 0, 0);
}
// 0x101ec9b8: the publish globe's virtual slot 0x34 (the arguments are not used)
void PostRequestUploadCallbackFunction(int32_t error, const void* context) {
    (void)error;
    (void)context;
    mainmenu_callee::PublishGlobeSlot34(gLobbyScreens->mPublishGlobe);
}
