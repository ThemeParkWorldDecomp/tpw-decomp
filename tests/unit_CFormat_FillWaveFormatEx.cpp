// Test library for CFormat::FillWaveFormatEx (unit v5274; libtpw_unit_CFormat_FillWaveFormatEx.dylib), main sound library:
//   src/TbMpegBase_CFormat_FillWaveFormatEx.cpp
// No callees and no globals.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbMpegBase.h"

extern "C" {

uint64_t tpw_cfmt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cfmt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mFilled));
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mChannels));
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mSamplesPerSec));
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mAvgBytesPerSec));
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mBlockAlign));
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mBitsPerSample));
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mExtraSize));
    out[n++] = static_cast<int64_t>(offsetof(CFormat, mInfo));
    out[n++] = static_cast<int64_t>(offsetof(CFormatInfo, mRate));
    out[n++] = static_cast<int64_t>(offsetof(CFormatInfo, mBits));
    out[n++] = static_cast<int64_t>(offsetof(CFormatInfo, mFlags));
    return n;
}

void tpw_cfmt_fill(uint64_t object) { reinterpret_cast<CFormat*>(static_cast<uintptr_t>(object))->FillWaveFormatEx(); }

}  // extern "C"
