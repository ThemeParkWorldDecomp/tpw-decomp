// Unit v51119: the world globe's camera steps and list registration (CLBScreenComponentOnlineGlobe), and the small
// destructors and byte swaps the neighbouring addresses hold.
#define TPW_GLOBE_INIT_REAL 1
#include <cstring>
#include "lobby.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "TPSound.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "Bitmap.h"
#include "ObjectParts.h"
#include "Light.h"
#include "email.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "platform/samsutils.h"
#include "platform/arena.h"

namespace {
inline uint32_t A(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
// The word at (pointer word + off): the Mac's 32-bit pointers are offsets from the arena's base on the host.
inline uint32_t& W(uint32_t base, uint32_t off) {
    return *reinterpret_cast<uint32_t*>(tpw::ArenaBase() + static_cast<uintptr_t>(base) + off);
}
inline float LoadF(const uint8_t* p) {
    float f;
    std::memcpy(&f, p, 4);
    return f;
}
inline void StoreF(uint8_t* p, float f) { std::memcpy(p, &f, 4); }
}  // namespace

// ---- CLBScreenComponentOnlineGlobe
// 0x100acc98
void CLBScreenComponentOnlineGlobe::Init() { mF70 = 0; }

// 0x100ad4cc: nothing
void CLBScreenComponentOnlineGlobe::Render() {}

// 0x100ad690
uint32_t CLBScreenComponentOnlineGlobe::ProcessCancel() { return 0; }

// 0x100adb54: the offset at 0x48 less the step at 0x14 (single precision)
void CLBScreenComponentOnlineGlobe::GlobeLeft() { StoreF(&mF44[4], LoadF(&mF44[4]) - LoadF(&mF0C[8])); }
// 0x100adba8
void CLBScreenComponentOnlineGlobe::GlobeRight() { StoreF(&mF44[4], LoadF(&mF44[4]) + LoadF(&mF0C[8])); }
// 0x100adbfc: the offset at 0x50
void CLBScreenComponentOnlineGlobe::GlobeUp() { StoreF(&mF44[12], LoadF(&mF44[12]) + LoadF(&mF0C[8])); }
// 0x100adc50
void CLBScreenComponentOnlineGlobe::GlobeDown() { StoreF(&mF44[12], LoadF(&mF44[12]) - LoadF(&mF0C[8])); }

// 0x100adff4
void CLBScreenComponentOnlineGlobe::Register(CLBGlobe* globe) { mGlobes->AddTail(globe); }

// 0x100ae064
void CLBScreenComponentOnlineGlobe::UnRegister(CLBGlobe* globe) {
    POSITION position = mGlobes->Find(globe);
    if (position != nullptr) mGlobes->RemoveAt(position);
}

// ---- destructors with the hidden delete flag (a signed 16-bit value)
__attribute__((weak)) void Bitmap_DeleteArray(void* block) { ::operator delete[](block); }

// 0x100ca4bc
Bitmap* Bitmap_DtorMac(Bitmap* self, int16_t flag) {
    if (self != nullptr) {
        if (self->mData != nullptr) Bitmap_DeleteArray(self->mData);
        self->mWidth = 0;
        self->mHeight = 0;
        self->mBytesPerPixel = 0;
        self->mData = nullptr;
        if (flag > 0) OstreamDelete(self);
    }
    return self;
}

// 0x10082c88
MapDelta* MapDelta_DtorMac(MapDelta* self, int16_t flag) {
    if (self != nullptr && flag > 0) OstreamDelete(self);
    return self;
}

// 0x100700e4
BasicFilebufChar* BasicFilebufChar_DtorMac(BasicFilebufChar* self, int16_t flag) {
    if (self != nullptr) {
        self->mVtable = A(gFilebufCharVtblB);
        uint32_t files = A(gIfsWFiles);
        if (self->mFile != files && self->mFile != files + 0x54 && self->mFile != files + 0xa8) OfstreamClose(self);
        self->mVtable = A(gFilebufCharVtblA);
        if (A(self) + 0x1c != 0) OfstreamLocaleDtor(&self->mLocale, -1);
        if (flag > 0) OstreamDelete(self);
    }
    return self;
}

// 0x1006ff50
void* BasicIosChar_DtorMac(void* self, int16_t flag) {
    if (self != nullptr) {
        *static_cast<uint32_t*>(self) = A(gOstreamVt1);
        OstreamIosBaseDtor(static_cast<MacOstreamIos*>(self), 0);
        if (flag > 0) OstreamDelete(self);
    }
    return self;
}

// 0x1006427c: the Mac object is 0x74 bytes: +0 pointer to the CLight part's vtable record, +4 the class's vtable, +8 the shared
// CMapLinkBase, +0x10 the CLight part. Every pointer is a 32-bit word.
CLightPoint* CLightPoint_DtorMac(CLightPoint* self, int16_t flag) {
    if (self == nullptr) return self;
    uint32_t me = tpw::ArenaOffset(self);
    uint32_t vt2 = A(gCLightParallelVt2);
    uint32_t vt3 = A(gCLightParallelVt3);
    uint32_t vt1 = A(gCLightPointVt);
    W(me, 4) = vt1;
    W(W(me, 0), 0x5c) = vt1 + 0x24;
    W(W(W(me, 0), 0), 0) = vt1 + 0xc;
    uint32_t q = W(W(me, 0), 0);
    W(q, 4) = (me + 8) - q;
    uint32_t p = W(me, 0);
    W(p, 0x60) = (me + 0x10) - p;
    if (flag != 0) {
        if (me + 0x10 != 0) {
            W(me, 0x6c) = vt3;
            W(W(me, 0x10), 0) = vt3 + 0x18;
            uint32_t r = W(me, 0x10);
            W(r, 4) = (me + 0x70) - r;
        }
        if (me + 8 != 0) W(me, 8) = vt2;
    }
    if (flag > 0) OstreamDelete(self);
    return self;
}

// ---- SamsUtilities byte swaps: the value's bytes reversed in place
namespace v51119_swap {
void USwapInplace16_short_100de404(int16_t& v) { v = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(v))); }
void USwapInplace32_long_100de448(int32_t& v) { v = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(v))); }
void USwapInplace32_long_10079a64(int32_t& v) { v = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(v))); }
void USwapInplace32_ulong_10079968(uint32_t& v) { v = __builtin_bswap32(v); }
void USwapInplace32_ulong_1006b6ac(uint32_t& v) { v = __builtin_bswap32(v); }
void USwapInplace32_int_1006b668(int32_t& v) { v = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(v))); }
void USwapInplace16_ushort_1006b620(uint16_t& v) { v = __builtin_bswap16(v); }
// 0x100799b0: hands both arguments to _USwapBlock32
void USwapBlock32_long_100799b0(int32_t* words, int32_t count) {
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(words), count);
}
}  // namespace v51119_swap
