// Test library for FLY_Save (unit v5678; libtpw_unit_FLY_Save.dylib, tests/test_FLY_Save.py), main app:
//   src/game/rides/FlyingRide.cpp   FLY_Save
//
// The callees outside the unit are fakes that record what the real ones read: _USwapCopy32, _USwapDelete, LbFile_Write,
// USwapInplace32<long>, tag_IDX_TYPE<ThingId>::Byteswap and USwapBlockStruct<FLYBOY>. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TbError.h"
#include "analyser.h"
#include "platform/samsutils.h"

using namespace calllog;

int32_t gFlyInitialised = 0;
void* gFlyBases[100];
int32_t gFlyZoneA = 0, gFlyZoneB = 0, gFlyZoneC = 0, gFlyZoneD = 0;

namespace {

int64_t Sum(const uint8_t* bytes, uint32_t length) {
    uint32_t c = 0;
    for (uint32_t i = 0; i < length; ++i) c = c * 31u + bytes[i];
    return c;
}

}  // namespace

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    int64_t labBuffer = lab(const_cast<void*>(buffer));
    int64_t sum = labBuffer == -1 ? Sum(static_cast<const uint8_t*>(buffer), length) : 0;
    ev("LbFile_Write", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), labBuffer, length, lab(transferred), sum});
    return TbError(0);
}

namespace SamsUtilities {

uint32_t* _USwapCopy32(const uint32_t* words, int32_t count) {
    ev("USwapCopy32", {lab(const_cast<uint32_t*>(words)), count, static_cast<int64_t>(words[0])});
    uint32_t* copy = static_cast<uint32_t*>(std::malloc(4 * static_cast<size_t>(count)));
    std::memcpy(copy, words, 4 * static_cast<size_t>(count));
    return copy;
}

void _USwapDelete(void* copy) {
    ev("USwapDelete", {static_cast<int64_t>(*static_cast<uint32_t*>(copy))});
    std::free(copy);
}

template<> void USwapInplace32<int32_t>(int32_t& word) {
    ev("Swap32", {static_cast<int64_t>(static_cast<uint32_t>(word))});
}

void USwapBlockStruct(FLYBOY* records, int32_t count) {
    const uint32_t* w = reinterpret_cast<const uint32_t*>(records);
    uint32_t c = 0;
    for (int32_t i = 0; i < count * 0xe4 / 4; ++i) c = c * 31u + w[i];
    ev("USwapBlockStruct", {count, static_cast<int64_t>(w[0]), static_cast<int64_t>(c)});
}

}  // namespace SamsUtilities

template<> void tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>::Byteswap() {
    ev("Byteswap16", {mIndex});
}

extern "C" {

uint64_t tpw_flysave_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gFlyInitialised", &gFlyInitialised}, {"gFlyBases", gFlyBases}, {"gFlyZoneD", &gFlyZoneD},
        {"gFlyZoneB", &gFlyZoneB},             {"gFlyZoneA", &gFlyZoneA}, {"gFlyZoneC", &gFlyZoneC},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_flysave_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_flysave_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gFlyBases[0]));
    out[n++] = static_cast<int64_t>(sizeof(gFlyBases));
    return n;
}

uint32_t tpw_flysave_save(uint64_t file) { return static_cast<uint32_t>(FLY_Save(reinterpret_cast<void*>(static_cast<uintptr_t>(file)))); }

}  // extern "C"
