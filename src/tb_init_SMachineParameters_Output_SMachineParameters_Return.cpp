#include <cstdint>
#include <cstdio>
#include <cstring>
#include "SMachineParameters.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_resource_system.h"
#include "tb_texturecache.h"

// Compares the record's limits (flag bits 0..5 of its word at 4) against the measured values; the record pointer never
// advances in the original, so every pass reads the same record. Answers the record's word 0 (0 when count <= 0).
uint32_t SMachineParameters::Return(SCfgRecord* record, int32_t mode) {
    const uint8_t* b = record->mBytes;
    auto word = [&](int off) {
        uint32_t v;
        memcpy(&v, b + off, 4);
        return v;
    };
    auto flt = [&](int off) {
        float v;
        memcpy(&v, b + off, 4);
        return v;
    };
    uint32_t result = 0;
    for (int32_t n = mode; n > 0; n--) {
        uint32_t flags = word(4);
        if ((flags & 1) && static_cast<int32_t>(word(8)) > mVRAMSize) break;
        if ((flags & 2) && flt(0xc) > mVRAMSpeed) break;
        if ((flags & 4) && static_cast<int32_t>(word(0x10)) > mRAMSize) break;
        if ((flags & 8) && static_cast<int32_t>(word(0x14)) > mCPUFreq) break;
        if (flags & 0x10) {
            float v = static_cast<float>(static_cast<double>(static_cast<int32_t>(word(0x18))));
            if (v > mMathSpeed) break;
        }
        if ((flags & 0x20) && flt(0x1c) > mFillRate) break;
        result = word(0);
    }
    return result;
}
