// GameShimGlue.cpp: the real definitions of the small game shims the translated code names but no translation defines: the MathLib
// calls, the lobby screens' virtual slots the base class reaches, the Outbox unmark calls, the runtime's operator delete as the
// game's classes name it, the wide string destructor glue, the 16-bit set tree destroy and USwapInplace32<long>. Only libtpw
// compiles this file: the unit test libraries list their own sources and define these names themselves (recording fakes), so
// nothing here changes what a unit test compiles.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <list>
#include <new>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "TPMain.h"
#include "email.h"
#include "lobby.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "std_deque_errorthingy.h"
#include "ui_main.h"

// ---- MathLib (the Mac's system library; the host's <cmath> answers the same).

namespace MathLib {
// 0x10239268 __fpclassifyf: the class code as the library's fp.h numbers them (an assumption from the header, not something the
// program shows; the callers only compare the class with 1, a NaN): FP_SNAN 0, FP_QNAN 1, FP_INFINITE 2, FP_ZERO 3, FP_NORMAL 4,
// FP_SUBNORMAL 5.
int __fpclassifyf(float x) {
    uint32_t bits;
    std::memcpy(&bits, &x, sizeof bits);
    switch (std::fpclassify(x)) {
        case FP_NAN:
            return (bits & 0x00400000u) != 0 ? 1 : 0;
        case FP_INFINITE:
            return 2;
        case FP_ZERO:
            return 3;
        case FP_SUBNORMAL:
            return 5;
        default:
            return 4;
    }
}
double fmod(double x, double y) { return std::fmod(x, y); }   // MathLib fmod
double sqrt(double x) { return std::sqrt(x); }                // MathLib sqrt
}  // namespace MathLib

// ---- SamsUtilities::USwapInplace32<long> (0x10192600): the Mac byte-reverses the word it reads from or writes to a file. The port
// keeps its files in host order, so the host instance leaves the word alone, as the generic instance in Draw.cpp does.

namespace SamsUtilities {
template<> void USwapInplace32<int>(int& word) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    word = static_cast<int>(__builtin_bswap32(static_cast<uint32_t>(word)));
#else
    (void)word;
#endif
}
}  // namespace SamsUtilities

// ---- The lobby's screens.

// 0x100a66b0 (12 bytes): the signed overload the lobby's callers name; the same key bit test as the unsigned one.
int32_t CLBLobby::IsKeyDown(int32_t flag) { return IsKeyDown(static_cast<uint32_t>(flag)); }

// 0x100acce0: the online globe's own Tick, which the download and publish globes' Ticks call.
void CLBScreenComponentOnlineGlobe::TickGlobe() { CLBScreenComponentOnlineGlobe::Tick(); }

// 0x100ad54c: the online globe's window messages, which the publish globe's WindowMessagesFn calls first.
void CLBScreenComponentOnlineGlobe_WindowMessagesFn(CLBScreenComponentOnlineGlobe* globe, int32_t message, int32_t a, int32_t b) {
    globe->CLBScreenComponentOnlineGlobe::WindowMessagesFn(message, a, b);
}

// The online islands screen's own virtual slots (the city lobby controller and the parser reach them as plain calls).
void OnlineIslandsSlot34(CLBScreenComponentOnlineIslands* screen) { screen->HidePanels(); }               // 0x100b4354
void OnlineIslandsSlot48(CLBScreenComponentOnlineIslands* screen) { screen->SelectCurrentIsland(); }       // 0x100b44ac
void OnlineIslandsSlot4c(CLBScreenComponentOnlineIslands* screen) { screen->StartCurrentIslandZoom(); }    // 0x100b4574

// Slot 0x48 of the base islands vtable, which the base class's window messages call: the theme islands' SelectCurrentIsland
// (0x100b3218) or the online islands' (0x100b44ac), by the object's class. The base class itself has no body.
void CLBScreenComponentIslands::Slot48() {
    if (auto* online = dynamic_cast<CLBScreenComponentOnlineIslands*>(this)) {
        online->SelectCurrentIsland();
    } else if (auto* theme = dynamic_cast<CLBScreenComponentThemeIslands*>(this)) {
        theme->SelectCurrentIsland();
    }
}

// ---- Outbox::UnmarkToSend 0x102058b4 and UnmarkToDelete 0x102059a0: the id the cursor (mF4C, the index into the list at +0x28,
// as email.cpp keeps it) names leaves the list at +0x34 / +0x40.

namespace {
bool OutboxCursorId(const Outbox* box, uint32_t* id) {
    uint32_t index;
    std::memcpy(&index, box->mF4C, sizeof index);
    if (index >= box->mF28.size()) return false;
    auto it = box->mF28.begin();
    std::advance(it, index);
    *id = *it;
    return true;
}
}  // namespace

void Outbox::UnmarkToSend() {
    uint32_t id;
    if (OutboxCursorId(this, &id)) mF34.remove(id);
}

void Outbox::UnmarkToDelete() {
    uint32_t id;
    if (OutboxCursorId(this, &id)) mF40.remove(id);
}

// ---- The runtime's operator delete (0x10238218), as each class names it. An arena block goes back to the arena; any other host
// pointer to the heap; a word below 4 GB is a Mac-style truncated address with nothing behind it, so it is left alone.

namespace {
void AnyDelete(void* p) {
    if (p == nullptr) return;
    if (tpw::InArena(p)) {
        tpw::ArenaFree(p);
    } else if (reinterpret_cast<uintptr_t>(p) > 0xffffffffu) {
        ::operator delete(p);
    }
}
}  // namespace

// ltms 0x1000f694: the "ltms" library's own operator delete, which its std::exception destructor calls.
struct LtmsStdException {
    static void operator delete(void* object);
};
void LtmsStdException::operator delete(void* object) { AnyDelete(object); }

void TPMainExc::OperatorDelete(void* p) { AnyDelete(p); }   // 0x10238218
void UI2DMapDelete(void* object) { AnyDelete(object); }      // 0x10238218

// 0x103bcbb8: the wide string destructor glue the static initialisers register with __register_global_object.
void UIMain_DestroyWString(void* object) {
    static_cast<TbDynamicStringTemplate<char16_t>*>(object)->~TbDynamicStringTemplate();
}

// ---- v51118::SetTreeDestroy 0x1016e65c: std::__tree<unsigned short>::destroy of a Mac set (MacU16Set: count, root, ..., all 32-bit
// words). The only reader of a MacU16Set's root is U16SetDtor, and nothing writes one on the host (the host sets are UshortSetTree,
// whose word at +4 is padding), so the root is 0 and this is not reached today. The word is a node pointer's low half: it is
// rebuilt with the upper half the tests set (gMslMacAddressHigh), and left alone when that is 0, as then no host node is reachable.

void v51118::SetTreeDestroy(MacU16Set* set, uint32_t node) {
    if (gMslMacAddressHigh == 0 || node == 0) return;
    auto* tree = reinterpret_cast<UshortSetTree*>(set);
    tree->destroy(reinterpret_cast<UshortSetTree::node*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | node)));
}
