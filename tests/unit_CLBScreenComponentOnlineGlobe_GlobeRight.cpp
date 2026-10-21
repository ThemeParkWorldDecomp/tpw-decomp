// Test library for the world globe's camera steps, list registration, small destructors and byte swaps (unit v51119;
// libtpw_unit_CLBScreenComponentOnlineGlobe_GlobeRight.dylib, tests/test_CLBScreenComponentOnlineGlobe_GlobeRight.py), main app:
//   src/game/HLSprite.cpp, src/game/online/lobby_globe.cpp, src/game/park/LLCellMouse.cpp, src/game/park/TPHField.cpp, src/game/rides/RideAnim.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read: operator delete, operator delete[],
// ios_base::~ios_base, basic_filebuf::close, the locale pointer's destructor and _USwapBlock32. The globe list is the real inline
// TbTList (the template is inline), checked by walking it. The globals of other files are defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "lobby.h"
#include "Light.h"
#include "Bitmap.h"
#include "ObjectParts.h"
#include "email.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files, defined here
unsigned char gCLightParallelVt2[0x40];
unsigned char gCLightParallelVt3[0x40];
unsigned char gCLightPointVt[0x40];
char gOstreamVt1[0x28];
uint32_t gFilebufCharVtblA[4];
uint32_t gFilebufCharVtblB[4];
uint8_t gIfsWFiles[0xfc];

// ---- the fakes
void OstreamDelete(void* object) { ev("delete", {lab(object)}); }
void Bitmap_DeleteArray(void* block) { ev("delete[]", {lab(block)}); }
void OstreamIosBaseDtor(MacOstreamIos* ios, int32_t flags) { ev("ios_base_dtor", {lab(ios), static_cast<int64_t>(flags)}); }
void OfstreamClose(void* filebuf) { ev("close", {lab(filebuf)}); }
void OfstreamLocaleDtor(void* locale, int32_t flag) { ev("locale_dtor", {lab(locale), static_cast<int64_t>(flag)}); }
namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) { ev("_USwapBlock32", {lab(words), static_cast<int64_t>(count)}); }
}  // namespace SamsUtilities

namespace {
using GList = TbTList<CLBGlobe*, CLBGlobe*>;
inline void* P(uint64_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); }
inline uint64_t U(const void* p) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)); }
inline CLBScreenComponentOnlineGlobe* Obj(uint64_t v) { return reinterpret_cast<CLBScreenComponentOnlineGlobe*>(P(v)); }
}  // namespace

extern "C" {

uint64_t tpw_gl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"pointvt", gCLightPointVt},     {"vt2", gCLightParallelVt2},      {"vt3", gCLightParallelVt3},
        {"fvtA", gFilebufCharVtblA},     {"fvtB", gFilebufCharVtblB},      {"files", gIfsWFiles},
        {"ostr", gOstreamVt1},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return U(entry.address);
    return 0;
}

uint64_t tpw_gl_base() { return U(tpw::ArenaBase()); }

uint64_t tpw_gl_alloc(uint32_t size) { return U(tpw::ArenaAllocZeroed(size)); }

// The host offsets of the Mac fields the module writes by Mac offset.
int32_t tpw_gl_layout(int64_t* out) {
    alignas(16) static uint8_t buf[1024];
    auto* o = reinterpret_cast<CLBScreenComponentOnlineGlobe*>(buf);
    int32_t n = 0;
    out[n++] = reinterpret_cast<uint8_t*>(&o->mGlobes) - buf;
    out[n++] = &o->mF0C[0] - buf;
    out[n++] = &o->mF44[0] - buf;
    out[n++] = reinterpret_cast<uint8_t*>(&o->mF70) - buf;
    out[n++] = static_cast<int64_t>(sizeof(CLBScreenComponentOnlineGlobe));
    out[n++] = static_cast<int64_t>(offsetof(Bitmap, mData));
    out[n++] = static_cast<int64_t>(sizeof(Bitmap));
    out[n++] = static_cast<int64_t>(sizeof(MapDelta));
    return n;
}

// op: 0 GlobeLeft, 1 GlobeRight, 2 GlobeUp, 3 GlobeDown, 4 Render, 5 ProcessCancel, 6 Init
uint32_t tpw_gl_globe(int32_t op, uint64_t obj) {
    CLBScreenComponentOnlineGlobe* o = Obj(obj);
    switch (op) {
        case 0: o->GlobeLeft(); break;
        case 1: o->GlobeRight(); break;
        case 2: o->GlobeUp(); break;
        case 3: o->GlobeDown(); break;
        case 4: o->CLBScreenComponentOnlineGlobe::Render(); break;
        case 5: return o->CLBScreenComponentOnlineGlobe::ProcessCancel();
        case 6: o->CLBScreenComponentOnlineGlobe::Init(); break;
    }
    return 0;
}

// op: 0 Register, 1 UnRegister; the value stands for a globe pointer (never followed)
void tpw_gl_listop(int32_t op, uint64_t obj, uint32_t value) {
    CLBScreenComponentOnlineGlobe* o = Obj(obj);
    CLBGlobe* globe = reinterpret_cast<CLBGlobe*>(static_cast<uintptr_t>(value));
    if (op == 0) o->Register(globe);
    else o->UnRegister(globe);
}

uint64_t tpw_gl_list_new() { return U(new GList()); }
void tpw_gl_list_add(uint64_t list, uint32_t value) {
    static_cast<GList*>(P(list))->AddTail(reinterpret_cast<CLBGlobe*>(static_cast<uintptr_t>(value)));
}
// out: count, forward length, forward values, backward length, backward values
int32_t tpw_gl_list_dump(uint64_t list, int64_t* out, int32_t cap) {
    GList* l = static_cast<GList*>(P(list));
    int32_t n = 0;
    out[n++] = l->mCount;
    int32_t fpos = n++;
    int32_t c = 0;
    for (GList::CNode* nd = l->mHead; nd != nullptr && c < 40; nd = nd->mNext, ++c)
        out[n++] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(nd->mData));
    out[fpos] = c;
    int32_t bpos = n++;
    c = 0;
    for (GList::CNode* nd = l->mTail; nd != nullptr && c < 40; nd = nd->mPrev, ++c)
        out[n++] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(nd->mData));
    out[bpos] = c;
    (void)cap;
    return n;
}

// which: 0 Bitmap, 1 MapDelta, 2 basic_filebuf<char>, 3 basic_ios<char>, 4 CLightPoint
uint64_t tpw_gl_dtor(int32_t which, uint64_t self, int32_t flag) {
    void* p = P(self);
    int16_t f = static_cast<int16_t>(flag);
    switch (which) {
        case 0: return U(Bitmap_DtorMac(static_cast<Bitmap*>(p), f));
        case 1: return U(MapDelta_DtorMac(static_cast<MapDelta*>(p), f));
        case 2: return U(BasicFilebufChar_DtorMac(static_cast<BasicFilebufChar*>(p), f));
        case 3: return U(BasicIosChar_DtorMac(p, f));
        case 4: return U(CLightPoint_DtorMac(static_cast<CLightPoint*>(p), f));
    }
    return 0;
}

// which: 0 16<short>, 1 32<long> 0x100de448, 2 32<long> 0x10079a64, 3 32<unsigned long> 0x10079968, 4 32<unsigned long> 0x1006b6ac,
// 5 32<int>, 6 16<unsigned short>
void tpw_gl_swap(int32_t which, uint64_t p) {
    void* q = P(p);
    switch (which) {
        case 0: v51119_swap::USwapInplace16_short_100de404(*static_cast<int16_t*>(q)); break;
        case 1: v51119_swap::USwapInplace32_long_100de448(*static_cast<int32_t*>(q)); break;
        case 2: v51119_swap::USwapInplace32_long_10079a64(*static_cast<int32_t*>(q)); break;
        case 3: v51119_swap::USwapInplace32_ulong_10079968(*static_cast<uint32_t*>(q)); break;
        case 4: v51119_swap::USwapInplace32_ulong_1006b6ac(*static_cast<uint32_t*>(q)); break;
        case 5: v51119_swap::USwapInplace32_int_1006b668(*static_cast<int32_t*>(q)); break;
        case 6: v51119_swap::USwapInplace16_ushort_1006b620(*static_cast<uint16_t*>(q)); break;
    }
}

void tpw_gl_block(uint64_t p, int32_t count) { v51119_swap::USwapBlock32_long_100799b0(static_cast<int32_t*>(P(p)), count); }

}  // extern "C"
