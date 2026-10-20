// Test library for SMachineParameters::Return (unit v5730; libtpw_unit_SMachineParameters_Return.dylib), main app:
//   src/tb_init_SMachineParameters_Output_SMachineParameters_Return.cpp
// Return calls nothing and reads no globals.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "SMachineParameters.h"

extern "C" {

uint64_t tpw_smpr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_smpr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(SMachineParameters, mVRAMSize));
    out[n++] = static_cast<int64_t>(offsetof(SMachineParameters, mVRAMSpeed));
    out[n++] = static_cast<int64_t>(offsetof(SMachineParameters, mRAMSize));
    out[n++] = static_cast<int64_t>(offsetof(SMachineParameters, mCPUFreq));
    out[n++] = static_cast<int64_t>(offsetof(SMachineParameters, mMathSpeed));
    out[n++] = static_cast<int64_t>(offsetof(SMachineParameters, mFillRate));
    out[n++] = static_cast<int64_t>(sizeof(SMachineParameters));
    return n;
}

uint32_t tpw_smpr_return(uint64_t self, uint64_t record, int32_t mode) {
    return reinterpret_cast<SMachineParameters*>(static_cast<uintptr_t>(self))
        ->Return(reinterpret_cast<SCfgRecord*>(static_cast<uintptr_t>(record)), mode);
}

}  // extern "C"
