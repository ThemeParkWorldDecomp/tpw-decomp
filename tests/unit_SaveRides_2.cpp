// Test library for SaveRides (unit v51028; libtpw_unit_SaveRides_2.dylib, tests/test_SaveRides_2.py), main app:
//   src/RideTypes_OutputAnimTimeStats_SaveRides_2.cpp   SaveRides, and the global gSaveRidesTbErrorZero it defines
//
// The callees outside the unit are recording fakes: MReleaseAllocF, MReleaseFreeF, CTRACE, OutputAnimTimeStats, LbFile_Write (which also
// keeps the bytes it was given), UDebug. The swaps are the host's (identity); USwapInplaceFloat is a no-op here as on the host.
// The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "Ptr32.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TPEngine.h"
#include "TPCoaster.h"
#include "m3d2.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances + 4];
uint32_t gRideCountA;
uint32_t gRideCountB;
uint32_t gRideFirstFree;

namespace {
std::vector<std::vector<uint8_t>> gWrites;
void* gLastBlock = nullptr;
}  // namespace

// ---- the fakes
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("alloc", {static_cast<int64_t>(size), static_cast<int64_t>(flags)});
    gLastBlock = tpw::ArenaAllocZeroed(size);
    return gLastBlock;
}

void* MReleaseFreeF(void* block) {
    ev("free", {block == gLastBlock ? 1 : 0});
    return nullptr;
}

void CTRACE(uint32_t level, const char* format, ...) {
    ev("CTRACE", {static_cast<int64_t>(level)}, {format});
}

void OutputAnimTimeStats(sAnimTimeControl* control, tag_sRideAnimSet* set) {
    int64_t setIndex = -1;
    for (uint32_t i = 1; i <= gRideCountA + gRideCountB; ++i)
        if (gRideAnimSets[i] == set) {
            setIndex = i;
            break;
        }
    int64_t index = control - static_cast<sAnimTimeControl*>(set->mTimeControls);
    ev("oats", {setIndex, index});
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("write", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file) & 0xffffffffu), static_cast<int64_t>(length), transferred != nullptr ? 1 : 0});
    int32_t error = static_cast<int32_t>(ret("werr"));
    if (transferred != nullptr) *transferred = static_cast<uint32_t>(ret("wn"));
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
    gWrites.emplace_back(bytes, bytes + length);
    return TbError(error);
}

namespace SamsUtilities {
void UDebug(const char* format, ...) {
    ev("UDebug", {}, {format});
}
}  // namespace SamsUtilities

namespace TPCoasterSwap {
void USwapInplaceFloat(float&) {}   // the host's: no swap
}  // namespace TPCoasterSwap

extern "C" {

uint64_t tpw_sr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sets", &gRideAnimSets[0]},
        {"A", &gRideCountA},
        {"B", &gRideCountB},
        {"ff", &gRideFirstFree},
        {"zero", &gSaveRidesTbErrorZero},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_sr_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

// Host offsets in the order of the module's HOST_FIELDS, then the sizes and the Mac-layout checks.
int32_t tpw_sr_layout(int64_t* out) {
    int32_t n = 0;
#define O(type, member) out[n++] = static_cast<int64_t>(offsetof(type, member))
    O(tag_sRideAnimSet, mFlags);
    O(tag_sRideAnimSet, mHeads);
    O(tag_sRideAnimSet, mF0C);
    O(tag_sRideAnimSet, mTimeCount);
    O(tag_sRideAnimSet, mTimeControls);
    O(tag_sRideAnimSet, mHoardingLevel);
    O(tag_sRideAnimSet, mFD4);
    O(tag_sRideAnimSet, mFE4);
    O(tag_sRideAnimSet, mFE8);
    O(tag_sRideAnimSet, mFEC);
    O(tag_sRideAnimSet, mFF0);
    O(tag_sRideAnimSet, mFF4);
    O(tag_sRideAnimSet, mFF8);
    O(tag_sMesh, mHeader);
    O(tag_sMesh, mF10);
    O(tag_sMesh, mF28);
    O(sEmbeddedSet, mFlags);
    O(sEmbeddedSet, mRecords);
    O(sEmbeddedSet, mF08);
    O(sEmbeddedRecord, mFlags);
    O(sEmbeddedRecord, mF0C);
#undef O
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedSet));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedRecord));
    out[n++] = static_cast<int64_t>(sizeof(tag_sM3D2Header));
    // the Mac-layout parts the module writes by Mac offset
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mAllNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodeCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mEmbeddedCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mNodes));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mMiniNodes));
    out[n++] = static_cast<int64_t>(sizeof(tag_sNode));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMiniNode));
    out[n++] = static_cast<int64_t>(sizeof(sAnimTimeControl));
    return n;
}

void tpw_sr_reset() {
    gWrites.clear();
    gLastBlock = nullptr;
}
int32_t tpw_sr_nwrites() { return static_cast<int32_t>(gWrites.size()); }
int32_t tpw_sr_wlen(int32_t i) { return static_cast<int32_t>(gWrites[i].size()); }
void tpw_sr_wdata(int32_t i, uint8_t* out) {
    if (!gWrites[i].empty()) std::memcpy(out, gWrites[i].data(), gWrites[i].size());
}

int32_t tpw_sr_save(uint64_t file) { return SaveRides(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }

}  // extern "C"
