// Test library for the small sound functions (unit v51124; libtpw_unit_CFormat_operator_tWAVEFORMATEX.dylib), sound program:
//   src/sound/TbSoundPipe.cpp
// Fakes: CFormat::FillWaveFormatEx, the access object's Read, and operator new[] / delete[] / delete (recording while a wrapper runs).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbMpegBase.h"

using namespace calllog;

namespace {
bool gTrack = false;

struct FakeAccess : FileAccessRead {
    int32_t Read(void* dest, uint32_t size) override {
        ev("Read", {1, dest != nullptr ? 1 : 0, static_cast<int64_t>(size)});
        return static_cast<int32_t>(ret("Read"));
    }
};
FakeAccess gAccess;
}  // namespace

void CFormat::FillWaveFormatEx() { ev("FillWaveFormatEx", {}); }

void* VoidArrayNew(size_t n) {
    ev("new[]", {static_cast<int64_t>(n)});
    return tpw::ArenaAllocZeroed(n < 4 ? 4 : static_cast<uint32_t>(n));
}
void VoidArrayDelete(void* p) { ev("delete[]", {p != nullptr ? 1 : 0}); }
void VoidObjectDelete(void* p) { ev("delete", {p != nullptr ? 1 : 0}); }

extern "C" {

uint64_t tpw_v51124_addr(const char*) { return 0; }

uint64_t tpw_v51124_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v51124_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mFilled));
    return n;
}

uint64_t tpw_v51124_fmt(uint64_t obj) {
    return reinterpret_cast<uintptr_t>(CFormat_operator_tWAVEFORMATEX(reinterpret_cast<CFormat*>(obj)));
}
int32_t tpw_v51124_read(uint32_t hasImpl, uint64_t dest, uint32_t size) {
    return CFile_Read(hasImpl ? &gAccess : nullptr, reinterpret_cast<void*>(dest), size);
}
uint64_t tpw_v51124_errctor(uint64_t obj, int32_t v) {
    return reinterpret_cast<uintptr_t>(TbError_ctor(reinterpret_cast<int32_t*>(obj), v));
}
int32_t tpw_v51124_erropl(uint64_t obj) { return TbError_operator_long(reinterpret_cast<const int32_t*>(obj)); }
uint64_t tpw_v51124_dtor(uint64_t obj, int32_t flag) {
    gTrack = true;
    void*** r = CAutoArrayPtrVoid_dtor(reinterpret_cast<void***>(obj), static_cast<int16_t>(flag));
    gTrack = false;
    return reinterpret_cast<uintptr_t>(r);
}
uint64_t tpw_v51124_ptrop(uint64_t obj) { return reinterpret_cast<uintptr_t>(CAutoArrayPtrVoid_operator_ptr(reinterpret_cast<void***>(obj))); }
uint64_t tpw_v51124_ctor(uint64_t obj, uint32_t count) {
    gTrack = true;
    void*** r = CAutoArrayPtrVoid_ctor(reinterpret_cast<void***>(obj), count);
    gTrack = false;
    return reinterpret_cast<uintptr_t>(r);
}

}  // extern "C"
