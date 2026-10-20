// Unit w219: host wrappers and recording fakes for the converters in another file.
#include "TbRaConvert.h"
#include "calllog.h"
#include <cstdlib>
#include <cstring>
#include <new>
#include <cstddef>
using namespace calllog;
namespace {
bool gTraceHeap = false;
bool gHeapBusy = false;
void Format(const char* name, const void* object, const TbPixFormat& f, const TbPalette& p) {
    ev(name, {lab(object), f.mRedMask, f.mGreenMask, f.mBlueMask, f.mAlphaMask, f.mBitsPerPixel,
              f.mPalettised, lab(&p)});
}
void Line(const char* name, const void* object, uint8_t* d, uint8_t* s, int32_t n) {
    ev(name, {lab(object), lab(d), lab(s), n});
}
class Recorder : public CConverterBase {
public:
    ~Recorder() override { ev("rec.dtor", {lab(this)}); }
    void ConvertLineUp(uint8_t* d, uint8_t* s, int32_t n) override { Line("up", this, d, s, n); }
    void ConvertLineDown(uint8_t* d, uint8_t* s, int32_t n) override { Line("down", this, d, s, n); }
};
}
CPalToMaskConverter::CPalToMaskConverter() = default;
CPalToMaskConverter::~CPalToMaskConverter() { ev("pal.dtor", {lab(this)}); }
void CPalToMaskConverter::Init(const TbPalette& p, const TbPixFormat& f) { Format("pal.init", this, f, p); }
void CPalToMaskConverter::ConvertLineUp(uint8_t* d, uint8_t* s, int32_t n) { Line("pal.up", this, d, s, n); }
void CPalToMaskConverter::ConvertLineDown(uint8_t* d, uint8_t* s, int32_t n) { Line("pal.down", this, d, s, n); }
CMaskToPalConverter::CMaskToPalConverter(const TbPixFormat& f, const TbPalette& p) { Format("mask.ctor", this, f, p); }
CMaskToPalConverter::~CMaskToPalConverter() { ev("mask.dtor", {lab(this)}); }
void CMaskToPalConverter::Init(const TbPixFormat& f, const TbPalette& p) { Format("mask.init", this, f, p); }
void CMaskToPalConverter::ConvertLineUp(uint8_t* d, uint8_t* s, int32_t n) { Line("mask.up", this, d, s, n); }
void CMaskToPalConverter::ConvertLineDown(uint8_t* d, uint8_t* s, int32_t n) { Line("mask.down", this, d, s, n); }

// Abstract bases have no complete-object destructor on this ABI; call the base-subobject entry.
extern "C" void RcvBaseDtor(CConverterBase*) asm("__ZN14CConverterBaseD2Ev");

extern "C" {
uint64_t tpw_rcv_vptr(int32_t kind) {
    static CMaskConverter mask;
    static CPalToMaskConverter pal;
    alignas(CMaskToPalConverter) static uint8_t storage[sizeof(CMaskToPalConverter)];
    static CMaskToPalConverter* toPal = new (storage) CMaskToPalConverter(*reinterpret_cast<TbPixFormat*>(storage), *reinterpret_cast<TbPalette*>(storage));
    static CInverseMap inverse;
    static Recorder recorder;
    void* p = kind == 1 ? static_cast<void*>(&mask) : kind == 2 ? static_cast<void*>(&pal) : kind == 3 ? static_cast<void*>(toPal) : kind == 5 ? static_cast<void*>(&inverse) : static_cast<void*>(&recorder);
    return *reinterpret_cast<uintptr_t*>(p);
}
uint64_t tpw_rcv_global() { return reinterpret_cast<uintptr_t>(&gRaConverter); }
uint64_t tpw_rcv_stat(int32_t i) {
    void* p[] = {&gRedloopRxx, &gGreenloopHere, &gGreenloopMin, &gGreenloopMax, &gGreenloopGinc,
                &gGreenloopGxx, &gGreenloopGcdist, &gGreenloopGcdp, &gGreenloopGcrgbp,
                &gBlueloopHere, &gBlueloopMin, &gBlueloopMax, &gBlueloopBinc};
    return reinterpret_cast<uintptr_t>(p[i]);
}
// The shared Python world uses this Mac-word to native-offset map.
int32_t tpw_rcv_layout(int32_t kind, uint32_t* out) {
    if (kind == 0) {
        uint32_t a[] = {sizeof(CConverter), offsetof(CConverter, mConverter), offsetof(CConverter, mSourceBytes), offsetof(CConverter, mDestBytes), offsetof(CConverter, mKind)};
        std::memcpy(out, a, sizeof a);
        return 5;
    }
    if (kind == 1) {
        CMaskConverter o;
        out[0] = sizeof(o);
        for (int i = 0; i < 20; ++i) out[i + 1] = reinterpret_cast<uint8_t*>(&o.mShift[0]) - reinterpret_cast<uint8_t*>(&o) + i * 4;
        return 21;
    }
    CInverseMap o;
    void* fields[] = {&o.cindex, &o.bcenter, &o.gcenter, &o.rcenter, &o.gdist, &o.rdist, &o.cdist, &o.cbinc, &o.cginc, &o.crinc,
                      &o.gdp, &o.rdp, &o.cdp, &o.grgbp, &o.rrgbp, &o.crgbp, &o.gstride, &o.rstride, &o.x, &o.xsqr, &o.colormax};
    out[0] = sizeof(o);
    for (int i = 0; i < 21; ++i) out[i + 1] = reinterpret_cast<uint8_t*>(fields[i]) - reinterpret_cast<uint8_t*>(&o);
    return 22;
}
uint64_t tpw_rcv_call(int32_t op, void* obj, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e, uintptr_t f) {
    gTraceHeap = true;
    uint64_t result = 0;
    switch (op) {
    case 0: new (obj) CConverter; break;
    case 1: static_cast<CConverter*>(obj)->Init(reinterpret_cast<TbPixFormat*>(a), reinterpret_cast<TbPixFormat*>(b), reinterpret_cast<TbPalette*>(c), d); break;
    case 2: static_cast<CConverter*>(obj)->ConvBuf(reinterpret_cast<uint8_t*>(a), reinterpret_cast<uint8_t*>(b), c); break;
    case 3: static_cast<CConverter*>(obj)->ConvertArea(reinterpret_cast<uint8_t*>(a), reinterpret_cast<uint8_t*>(b), c, d, e, f); break;
    case 4: static_cast<CConverter*>(obj)->Release(); break;
    case 5: if (obj) { RcvBaseDtor(static_cast<CConverterBase*>(obj)); if (int16_t(a) > 0) ::operator delete(obj); } break;
    case 6: static_cast<CMaskConverter*>(obj)->Init(*reinterpret_cast<TbPixFormat*>(a), *reinterpret_cast<TbPixFormat*>(b)); break;
    case 7: static_cast<CMaskConverter*>(obj)->GetShiftBits(a, b, reinterpret_cast<uint32_t*>(c), reinterpret_cast<uint32_t*>(d)); break;
    case 8: static_cast<CMaskConverter*>(obj)->CMaskConverter::ConvertLineUp(reinterpret_cast<uint8_t*>(a), reinterpret_cast<uint8_t*>(b), c); break;
    case 9: static_cast<CMaskConverter*>(obj)->CMaskConverter::ConvertLineDown(reinterpret_cast<uint8_t*>(a), reinterpret_cast<uint8_t*>(b), c); break;
    case 10: if (obj) { if (int16_t(a) > 0) delete static_cast<CMaskConverter*>(obj); else static_cast<CMaskConverter*>(obj)->~CMaskConverter(); } break;
    case 11: new (obj) CInverseMap; break;
    case 12: if (obj) { if (int16_t(a) > 0) delete static_cast<CInverseMap*>(obj); else static_cast<CInverseMap*>(obj)->~CInverseMap(); } break;
    case 13: static_cast<CInverseMap*>(obj)->InvCMap(a, b, *reinterpret_cast<TbPalette*>(c), d, reinterpret_cast<uint32_t*>(e), reinterpret_cast<uint8_t*>(f)); break;
    case 14: result = static_cast<CInverseMap*>(obj)->redloop(); break;
    case 15: result = static_cast<CInverseMap*>(obj)->greenloop(a); break;
    case 16: result = static_cast<CInverseMap*>(obj)->blueloop(a); break;
    case 17: static_cast<CInverseMap*>(obj)->maxfill(reinterpret_cast<uint32_t*>(a), b); break;
    case 18: result = static_cast<TbRAConv*>(obj)->Move(*reinterpret_cast<TbRenderArea*>(a), *reinterpret_cast<TbRenderArea*>(b), reinterpret_cast<TbPalette*>(c)).mCode; break;
    case 19: if (obj) { if (int16_t(a) > 0) delete static_cast<TbRAConv*>(obj); else static_cast<TbRAConv*>(obj)->~TbRAConv(); } break;
    case 20: new (&gRaConverter) CConverter; break;
    }
    gTraceHeap = false;
    return result;
}
// Deleting-destructor tests need malloc-compatible storage that remains readable after a fake delete.
uint64_t tpw_rcv_alloc(uint32_t n) { return reinterpret_cast<uintptr_t>(::operator new(n)); }
}

// The stock recorder counts allocations but does not fill or label them. Adapt its
// allocator at inclusion time so the same recorder still owns all event storage.
namespace std {
void* RcvMalloc(size_t size) {
    void* p = std::malloc(size);
    if (p) std::memset(p, 0xa5, size);
    if (gTraceHeap && !gHeapBusy && p) {
        gHeapBusy = true;
        uint32_t n = size == sizeof(CMaskConverter) ? 0x54 : size == sizeof(CPalToMaskConverter) ? 0x430 : size == sizeof(CMaskToPalConverter) ? 0x43c : size;
        ev("new", {newlab(p), n});
        gHeapBusy = false;
    }
    return p;
}
}
namespace std {
void RcvFree(void* p) {
    if (!gTraceHeap) std::free(p); // The emulator's fake delete retains its block too.
}
}
using std::RcvMalloc;
using std::RcvFree;
#define free RcvFree
#define malloc RcvMalloc
#include "calllog.cpp"
#undef malloc
#undef free

// Observe only pixel-row copies while a game function runs; recorder-internal copies
// remain ordinary memory operations. Volatile bytes prevent recursion into memcpy.
extern "C" void* memcpy(void* d, const void* s, std::size_t n) {
    if (gTraceHeap && !gHeapBusy) {
        auto dl = lab(d);
        auto sl = lab(s);
        if (dl >= 30 && dl < 46 && sl >= 30 && sl < 46) {
            gHeapBusy = true;
            ev("memcpy", {dl, sl, static_cast<int64_t>(n)});
            gHeapBusy = false;
        }
    }
    auto* dest = static_cast<volatile uint8_t*>(d);
    auto* source = static_cast<const volatile uint8_t*>(s);
    for (std::size_t i = 0; i < n; ++i) dest[i] = source[i];
    return d;
}
