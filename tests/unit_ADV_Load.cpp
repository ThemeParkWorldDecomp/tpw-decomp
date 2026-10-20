// Test library for the advisor's save file part (unit v5685; libtpw_unit_ADV_Load.dylib, tests/test_ADV_Load.py), main app:
//   src/ThingLoop_ADV_Load.cpp   ADV_Save, ADV_Load
//
// The callees outside the unit are recording fakes: LbFile_Read (answers the bytes of the case's queue "fb"), LbFile_Write (records the
// bytes as words), the platform's _USwapCopy32 / _USwapDelete / _USwapBlock32 (a little-endian host: copies, no swapping). The advisor's
// globals (defined in other files) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Advisor.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

SAdvisorState gAdvisorState;
int32_t gAdvisorStaticInUse;
int32_t gAdvisorStaticBuffer[250];

// The case's file (the queue of the call log is too short for it): bytes after the end read as 0.
static const uint8_t* gFileBytes;
static uint32_t gFileSize, gFilePos;

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("read", {static_cast<int64_t>(lab(file)), static_cast<int64_t>(length), transferred ? 1 : 0});
    uint8_t* out = static_cast<uint8_t*>(buffer);
    for (uint32_t i = 0; i < length; ++i) out[i] = gFilePos < gFileSize ? gFileBytes[gFilePos++] : 0;
    if (transferred) *transferred = length;
    return TbError(0);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("write", {static_cast<int64_t>(lab(file)), static_cast<int64_t>(length), transferred ? 1 : 0});
    const uint8_t* in = static_cast<const uint8_t*>(buffer);
    const uint32_t count = length / 4;
    for (uint32_t j = 0; j < count; ++j) {
        uint32_t word;
        std::memcpy(&word, in + 4 * j, 4);
        if (length == 0x168 && j == 80) word = word != 0;
        ev("wd", {static_cast<int64_t>(j), static_cast<int64_t>(word)});
    }
    if (transferred) *transferred = length;
    return TbError(0);
}

namespace SamsUtilities {

void _USwapBlock32(uint32_t*, int32_t) {}

uint32_t* _USwapCopy32(const uint32_t* words, int32_t count) {
    ev("USwapCopy32", {count});
    uint32_t* copy = new uint32_t[count > 0 ? count : 1];
    if (count > 0) std::memcpy(copy, words, 4u * static_cast<uint32_t>(count));
    return copy;
}

void _USwapDelete(void* copy) {
    ev("USwapDelete", {0});
    delete[] static_cast<uint32_t*>(copy);
}

}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_adv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"state", &gAdvisorState},
        {"inuse", &gAdvisorStaticInUse},
        {"sbuf", gAdvisorStaticBuffer},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A block from the C library (the ADV_Load under test frees the old data pointer with free()).
uint64_t tpw_adv_malloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(std::calloc(size ? size : 4, 1))); }

void tpw_adv_setfile(const uint8_t* bytes, uint32_t size) {
    gFileBytes = bytes;
    gFileSize = size;
    gFilePos = 0;
}

int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(SAdvisorState, mSyncData));
    out[n++] = static_cast<int64_t>(offsetof(SAdvisorState, mAccessoriesOn));
    out[n++] = static_cast<int64_t>(offsetof(SAdvisorState, mSyncSize));
    out[n++] = static_cast<int64_t>(offsetof(SAdvisorState, mPausedSound));
    out[n++] = static_cast<int64_t>(sizeof(SAdvisorState));
    return n;
}

void tpw_adv_load(uint64_t file) { ADV_Load(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }
void tpw_adv_save(uint64_t file) { ADV_Save(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }

}  // extern "C"
