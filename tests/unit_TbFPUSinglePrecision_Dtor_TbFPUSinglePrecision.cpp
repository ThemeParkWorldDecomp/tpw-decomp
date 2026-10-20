// Test library for the TbFPUSinglePrecision destructor (unit v5740).
// TbFPUFlag::Reset and operator delete are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbFPUPrecision.h"

using namespace calllog;

void TbFPUFlag::Reset() {
    ev("Reset", {mSaved, mSet, mClear});
}

extern "C" {

uint32_t tpw_fpusp_dtor(int32_t has, uint32_t saved, uint32_t set, uint32_t clear, int32_t flag) {
    if (!has) return 0;
    alignas(16) unsigned char buf[32];
    TbFPUSinglePrecision* p = new (buf) TbFPUSinglePrecision();
    p->mSaved = static_cast<uint16_t>(saved);
    p->mSet = static_cast<uint16_t>(set);
    p->mClear = static_cast<uint16_t>(clear);
    p->~TbFPUSinglePrecision();
    if (static_cast<int16_t>(flag) > 0) ev("delete", {});
    return 1;
}

}  // extern "C"
