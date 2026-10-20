// Test library for the unit v5963 functions (libtpw_unit_RegisterBullfrogWindowClass_2.dylib, tests/test_RegisterBullfrogWindowClass_2.py), main app:
//   src/CreateDirectDrawObject_RegisterBullfrogWindowClass_2.cpp   RegisterBullfrogWindowClass, ScreenFPUHandler_Dtor,
//   __sinit_TbSystemInfo_cpp, LbRect_Contains, LbRect_Bounding, CRefpack::MemCompress
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues:
// LbApp_GetHInstance, LbApp_GetWindowIcon, NS_MacDoze::{ZeroMemory, LoadIcon, GetStockObject, RegisterClass}, CPU::detect,
// TbFPUFlag::Reset, operator delete (ScreenFPUHandler_OperatorDelete), operator new[] / delete[] (Refpack_*), and the virtual
// CompressWorkspaceSize. The globals of other files the unit reads (gWinScrnClassName, gScreenFPUHandler) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "WinScrn.h"
#include "TbFPUPrecision.h"
#include "bfl_Refpack.h"
#include "platform/macdoze.h"

using namespace calllog;

// ---- globals of other files, defined here
char gWinScrnClassName[64];
_ScreenFPUHandler gScreenFPUHandler;
const void* const kScreenFPUHandlerVtable[2] = {nullptr, nullptr};

namespace {
inline int64_t I(uint64_t v) { return static_cast<int64_t>(static_cast<uint32_t>(v)); }
inline int64_t P(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }
bfl::compress::CRefpack gRefpack;
uint8_t* gLastWork = nullptr;
}  // namespace

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
void* LbApp_GetHInstance() {
    ev("GetHInstance", {0});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("hinst"))));
}

int16_t LbApp_GetWindowIcon() {
    ev("GetWindowIcon", {0});
    return static_cast<int16_t>(ret("icon"));
}

int32_t WindowProc(GrafPort*, uint32_t, uint32_t, int32_t*) { return 0; }

namespace NS_MacDoze {
void ZeroMemory(void* p, uint32_t size) {
    ev("ZeroMemory", {I(size)});
    std::memset(p, 0, size);
}
void* LoadIcon(void* instance, const char* name) {
    ev("LoadIcon", {P(instance), P(name)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("loadicon"))));
}
void* GetStockObject(int32_t index) {
    ev("GetStockObject", {I(index)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("stock"))));
}
int32_t RegisterClass(const tagWNDCLASSA* wc) {
    ev("RegisterClass", {I(wc->style), wc->lpfnWndProc == &WindowProc ? 1 : 0, I(wc->cbClsExtra), I(wc->cbWndExtra),
                         P(wc->hInstance), P(wc->hIcon), P(wc->hCursor), P(wc->hbrBackground), P(wc->lpszMenuName),
                         wc->lpszClassName == gWinScrnClassName ? 1 : 0});
    return static_cast<int32_t>(ret("regclass"));
}
}  // namespace NS_MacDoze

void bullfrog::CPU::detect() { ev("CPU::detect", {lab(this)}); }

void TbFPUFlag::Reset() { ev("TbFPUFlag::Reset", {lab(this)}); }

void ScreenFPUHandler_OperatorDelete(void* block) { ev("delete", {lab(block)}); }

uint8_t* Refpack_NewArray(uint32_t size) {
    ev("new[]", {I(size)});
    gLastWork = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(size < 0xc0000u ? 0xc0000u : size));
    return gLastWork;
}

void Refpack_DeleteArray(void* block) { ev("delete[]", {block == gLastWork ? 1 : 0}); }

// The CRefpack members the unit does not translate: only CompressWorkspaceSize is reached (through the virtual call).
bfl::compress::CCompressionBase::~CCompressionBase() {}
bfl::compress::CRefpack::~CRefpack() {}
int32_t bfl::compress::CRefpack::MemDecompress(uint8_t*, uint8_t*, uint8_t*) const { return 0; }
int32_t bfl::compress::CRefpack::CompressWorkspaceSize() const {
    ev("CompressWorkspaceSize", {this == &gRefpack ? 1 : 0});
    return static_cast<int32_t>(ret("wsize"));
}
int32_t bfl::compress::CRefpack::DecompressWorkspaceSize() const { return 0; }
int32_t bfl::compress::CRefpack::DecompressedSize(uint8_t*) const { return 0; }

extern "C" {

uint64_t tpw_rb2_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"guard", &gRegisterBullfrogWindowClassGuard},
        {"done", &gRegisterBullfrogWindowClassDone},
        {"classname", gWinScrnClassName},
        {"fpu", &gScreenFPUHandler},
        {"fpu4", &gScreenFPUHandler.mVtable},
        {"vtA", kScreenFPUHandlerVtable},
        {"vtB", kScreenFPUHandlerFlagVtable},
        {"cpu", &gTbSystemInfoCPU},
        {"f1c", gTbSystemInfoF1C},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rb2_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_rb2_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(_ScreenFPUHandler, mF00));
    out[n++] = static_cast<int64_t>(offsetof(_ScreenFPUHandler, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(_ScreenFPUHandler, mF08));
    out[n++] = static_cast<int64_t>(offsetof(_ScreenFPUHandler, mF0A));
    out[n++] = static_cast<int64_t>(offsetof(_ScreenFPUHandler, mF0C));
    out[n++] = static_cast<int64_t>(sizeof(_ScreenFPUHandler));
    out[n++] = static_cast<int64_t>(sizeof(bullfrog::CPU));
    return n;
}

void tpw_rb2_register() { RegisterBullfrogWindowClass(); }
uint64_t tpw_rb2_dtor(uint64_t self, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        ScreenFPUHandler_Dtor(reinterpret_cast<_ScreenFPUHandler*>(static_cast<uintptr_t>(self)), flag)));
}
void tpw_rb2_sinit() { __sinit_TbSystemInfo_cpp(); }
uint32_t tpw_rb2_contains(uint64_t a, uint64_t b) {
    return LbRect_Contains(reinterpret_cast<const TbRect*>(static_cast<uintptr_t>(a)),
                           reinterpret_cast<const TbRect*>(static_cast<uintptr_t>(b)))
               ? 1u
               : 0u;
}
void tpw_rb2_bounding(uint64_t a, uint64_t b) {
    LbRect_Bounding(reinterpret_cast<TbRect*>(static_cast<uintptr_t>(a)), reinterpret_cast<const TbRect*>(static_cast<uintptr_t>(b)));
}
int32_t tpw_rb2_compress(uint64_t dst, uint64_t src, uint32_t length, uint64_t work) {
    return gRefpack.MemCompress(reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(dst)),
                                reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(src)), length,
                                reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(work)));
}

}  // extern "C"
