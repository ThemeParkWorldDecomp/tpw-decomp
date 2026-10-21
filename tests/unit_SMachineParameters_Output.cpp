// Test library for SMachineParameters::Output and ::Suggest (unit v5092; libtpw_unit_SMachineParameters_Output.dylib,
// tests/test_SMachineParameters_Output.py), engine:
//   src/engine/tb_init.cpp   the two functions, gSMachineCfgData, gSMachineOutputCount
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues: fopen,
// fprintf, fflush, fclose (libc, overridden in this library), the render system's GetVRAMSize, the draw primitive's
// GetPixelFillRate, CTextureCache::TestUploadSpeed, CTransformBase::TestMathSpeed and SMachineParameters::Return. The
// globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "SMachineParameters.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_resource_system.h"
#include "tb_texturecache.h"

using namespace calllog;

// ---- globals of other files
CSystem gei_System;
CDirectDraw* gei_DDraw = nullptr;
CTextureCache* gei_TCache = nullptr;
CTransformBase* gei_MathUnit = nullptr;
CDrawPrimitive* gei_DrawPrimitive = nullptr;

namespace {

uint32_t Fnv(const char* s) {
    uint32_t h = 0x811C9DC5u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 0x01000193u;
    return h;
}

float BitsToFloat(int64_t queued) {
    uint32_t bits = static_cast<uint32_t>(queued);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

struct FakeRender : CRenderSystem {
    int32_t ClearViewport() override { return 0; }
    int32_t FlipScreen() override { return 0; }
    int32_t TempShutdown() override { return 0; }
    int32_t ReInitialize() override { return 0; }
    int32_t GetTextureMemory() override { return 0; }
    int32_t GetVRAMSize() override {
        ev("GetVRAMSize", {lab(this)});
        return static_cast<int32_t>(ret("GetVRAMSize"));
    }
    void* CreateTextureBase() override { return nullptr; }
};

struct FakePrim : CDrawPrimitive {
    int32_t Init() override { return 0; }
    int32_t SetBlendingMode(int32_t) override { return 0; }
    int32_t SetTexture(int32_t, void*) override { return 0; }
    int32_t Execute(SPrimitive*) override { return 0; }
    int32_t ExecuteShadow(SPrimitive*) override { return 0; }
    int32_t SetFogParameters(float, float, uint32_t) override { return 0; }
    int32_t BeginDraw() override { return 0; }
    int32_t EndDraw() override { return 0; }
    int32_t DrawTestPoly() override { return 0; }
    int32_t DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) override { return 0; }
    float GetPixelFillRate() override {
        ev("GetPixelFillRate", {lab(this)});
        return BitsToFloat(ret("GetPixelFillRate"));
    }
};

FakeRender gFakeRender;
FakePrim gFakePrim;

}  // namespace

// ---- fakes of functions outside the unit (the same event names and arguments as the module's PowerPC stubs)
CSystem::CSystem() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}

double CTextureCache::TestUploadSpeed(int32_t size, int32_t count) {
    ev("TestUploadSpeed", {lab(this), size, count});
    return static_cast<double>(BitsToFloat(ret("TestUploadSpeed")));
}

float CTransformBase::TestMathSpeed() {
    ev("TestMathSpeed", {lab(this)});
    return BitsToFloat(ret("TestMathSpeed"));
}

uint32_t SMachineParameters::Return(SCfgRecord* record, int32_t mode) {
    ev("Return", {lab(this), static_cast<int64_t>(reinterpret_cast<uint8_t*>(record) - gSMachineCfgData), mode});
    return static_cast<uint32_t>(ret("Return"));
}

// libc, overridden: the file pointer is whatever the queue says and is never dereferenced
extern "C" {

FILE* fopen(const char* path, const char* mode) {
    ev("fopen", {Fnv(path), Fnv(mode)});
    return reinterpret_cast<FILE*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("fopen"))));
}

int fprintf(FILE* file, const char* format, ...) {
    int count = 0;
    for (const char* p = format; *p; ++p)
        if (*p == '%') ++count;
    ev("fprintf", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file))), Fnv(format), count});
    va_list ap;
    va_start(ap, format);
    for (const char* p = format; *p; ++p) {
        if (*p != '%') continue;
        ++p;
        if (*p == 's') {
            const char* s = va_arg(ap, const char*);
            ev("arg", {1, Fnv(s), 0});
        } else if (*p == 'd') {
            int v = va_arg(ap, int);
            ev("arg", {2, static_cast<uint32_t>(v), 0});
        } else {
            double d = va_arg(ap, double);
            uint64_t bits;
            std::memcpy(&bits, &d, 8);
            if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull && (bits & 0xfffffffffffffull) != 0)
                bits = 0x7ff8000000000000ull;
            ev("arg", {3, static_cast<int64_t>(bits >> 32), static_cast<int64_t>(bits & 0xffffffffull)});
        }
    }
    va_end(ap);
    return 0;
}

int fflush(FILE* file) {
    ev("fflush", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)))});
    return 0;
}

int fclose(FILE* file) {
    ev("fclose", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)))});
    return 0;
}

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_smp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sys.mClocksPerMs", &gei_System.mClocksPerMs},
        {"sys.mTotalPhys", &gei_System.mTotalPhys},
        {"sys.mRenderer", &gei_System.mRenderer},
        {"sys.mF20", &gei_System.mF20},
        {"sys.mRenderSystem", &gei_System.mRenderSystem},
        {"ddraw", &gei_DDraw},
        {"tcache", &gei_TCache},
        {"mathunit", &gei_MathUnit},
        {"prim", &gei_DrawPrimitive},
        {"count", &gSMachineOutputCount},
        {"o.prim", static_cast<CDrawPrimitive*>(&gFakePrim)},
        {"o.rs", static_cast<CRenderSystem*>(&gFakeRender)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_smp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_smp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(SMachineParameters, mVRAMSize));
    out[n++] = static_cast<int64_t>(offsetof(SMachineParameters, mFillRate));
    out[n++] = static_cast<int64_t>(sizeof(SMachineParameters));
    return n;
}

void tpw_smp_output(uint64_t self) { reinterpret_cast<SMachineParameters*>(self)->Output(); }
uint32_t tpw_smp_suggest(uint64_t self) { return static_cast<uint32_t>(reinterpret_cast<SMachineParameters*>(self)->Suggest()); }

}  // extern "C"
