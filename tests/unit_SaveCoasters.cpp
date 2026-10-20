// Test library for SaveCoasters (unit v5968; libtpw_unit_SaveCoasters.dylib, tests/test_SaveCoasters.py), main app:
//   src/TPCoaster_trains_SaveCoasters.cpp   SaveCoasters
//
// The callees outside the unit are fakes: LbFile_Write (records the file label, the length and the buffer's first 36 bytes as big-endian words,
// answers the transferred count from the queue "Write"), SaveTrainsAndCars (records the labels), UDebug (records the text) and the file swaps
// of TPCoasterSwap (the real ones: a swap only with TPW_SWAP_FILE_WORDS, else nothing). The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TPCoaster.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterInstances;
uint32_t gObjectTableLowest;
uint32_t gObjectTableFirstFree;
uint32_t gObjectTableCount;

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    uint8_t padded[36] = {0};
    std::memcpy(padded, buffer, length < 36 ? length : 36);
    int64_t w[9];
    for (int i = 0; i < 9; ++i) {
        uint32_t v;
        std::memcpy(&v, padded + 4 * i, 4);
        w[i] = __builtin_bswap32(v);
    }
    ev("Write", {lab(file), static_cast<int64_t>(length), w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8]});
    const int64_t mode = ret("Write");
    uint32_t t = length;
    if (mode == 1) t = 0;
    if (mode == 2) t = length * 2;
    if (mode == 3) t = length + 1;
    if (mode == 4) t = length - 1;
    if (transferred) *transferred = t;
    return TbError(0);
}

int32_t SaveTrainsAndCars(void* file, tag_sCoasterInstance* instance) {
    ev("SaveTrainsAndCars", {lab(file), lab(instance)});
    return 1;
}

namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

namespace TPCoasterSwap {
void USwapInplace16UShort(uint16_t& value) {
#ifdef TPW_SWAP_FILE_WORDS
    value = __builtin_bswap16(value);
#else
    (void)value;
#endif
}
void USwapInplace32Int(int32_t& word) {
#ifdef TPW_SWAP_FILE_WORDS
    word = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(word)));
#else
    (void)word;
#endif
}
void USwapInplace32ULong(uint32_t& word) {
#ifdef TPW_SWAP_FILE_WORDS
    word = __builtin_bswap32(word);
#else
    (void)word;
#endif
}
void USwapInplaceFloat(float& value) {
#ifdef TPW_SWAP_FILE_WORDS
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    bits = __builtin_bswap32(bits);
    std::memcpy(&value, &bits, 4);
#else
    (void)value;
#endif
}
}  // namespace TPCoasterSwap

extern "C" {

uint64_t tpw_sc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gCoasterInstances", &gCoasterInstances},
        {"gObjectTableLowest", &gObjectTableLowest},
        {"gObjectTableFirstFree", &gObjectTableFirstFree},
        {"gObjectTableCount", &gObjectTableCount},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sc_layout(int64_t* out) {
    int32_t n = 0;
#define O(S, M) out[n++] = static_cast<int64_t>(offsetof(S, M))
    O(tag_sCoasterInstance, mFlags);
    O(tag_sCoasterInstance, mState);
    O(tag_sCoasterInstance, mTypeID);
    O(tag_sCoasterInstance, mEntranceID);
    O(tag_sCoasterInstance, mObjectInstanceID);
    O(tag_sCoasterInstance, mNext);
    O(tag_sCoasterInstance, mX);
    O(tag_sCoasterInstance, mZ);
    O(tag_sCoasterInstance, mF2C);
    O(tag_sCoasterInstance, mBlock3C);
    O(tag_sCoasterInstance, mLastBrace);
    O(tag_sCoasterInstance, mFF0);
    O(tag_sCoasterInstance, mTrainCount);
    O(tag_sCoasterInstance, mF140);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    O(tag_sTrackBrace, mF00);
    O(tag_sTrackBrace, mFlags);
    O(tag_sTrackBrace, mPrev);
    O(tag_sTrackBrace, mWord70);
    O(tag_sTrackBrace, mFB0);
    O(tag_sTrackBrace, mFB4);
    O(tag_sTrackBrace, mFD8);
    O(tag_sTrackBrace, mFDC);
    O(tag_sTrackBrace, mAngle);
    O(tag_sTrackBrace, mFE8w);
    O(tag_sTrackBrace, mFF8);
    O(tag_sTrackBrace, mData);
    out[n++] = static_cast<int64_t>(sizeof(tag_sTrackBrace));
    O(sTrackBraceData, mFlags);
    O(sTrackBraceData, mCellX);
    O(sTrackBraceData, mCellZ);
    out[n++] = static_cast<int64_t>(sizeof(sTrackBraceData));
#undef O
    return n;
}

uint32_t tpw_sc_save(uint64_t file) { return static_cast<uint32_t>(SaveCoasters(reinterpret_cast<void*>(static_cast<uintptr_t>(file)))); }

}  // extern "C"
