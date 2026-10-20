// Unit v5135: analyser min/max helpers and the errorthingy deque cursor +=.
#include <cstdint>

#include "History.h"
#include "std_deque_errorthingy.h"

uint64_t gMslMacAddressHigh = 0;

namespace {
// The jump chain of the listings: 0 -> 0xc, 1 -> 0x24, 2 -> 0x90, anything else 0.
int32_t Lookback(int32_t range) {
    if (range == 1) return 0x24;
    if (range < 1) return range >= 0 ? 0xc : 0;
    return range < 3 ? 0x90 : 0;
}
}  // namespace

// 0x101f6030
void MinAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range) {
    int32_t index = history->GetLowestValue(0, Lookback(range));
    if (index != -1) {
        int32_t entry;
        history->GetValue(entry, index);
        if (entry < value) value = entry;
    }
}

// 0x101f5f2c: the entry is truncated to int; no check for -1.
void MaxAnalyserValue(CHistory<float, 144>* history, int32_t& value, int32_t range) {
    int32_t index = history->GetHighestValue(0.0f, Lookback(range));
    float entry;
    history->GetValue(entry, index);
    int32_t truncated = static_cast<int32_t>(entry);
    if (truncated > value) value = truncated;
}

// 0x101f5e3c
void MaxAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range) {
    int32_t index = history->GetHighestValue(0, Lookback(range));
    int32_t entry;
    history->GetValue(entry, index);
    if (entry > value) value = entry;
}

// 0x101f5adc
MslErrorDequeCursor* MslErrorDequeCursorAddErrorthingy(MslErrorDequeCursor* cursor, int32_t count) {
    if (count < 0) {
        errdeque_callee::CursorSub(cursor, -count);
        return cursor;
    }
    const uint32_t* table = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | cursor->mCdeque));
    uint32_t position = table[2] + (cursor->mWord4 + static_cast<uint32_t>(count));
    uint32_t capacity = table[0];
    if (position < capacity)
        cursor->mSlot += static_cast<uint32_t>(count) * 4;
    else
        cursor->mSlot = table[3] + (position - capacity) * 4;
    cursor->mWord4 += static_cast<uint32_t>(count);
    return cursor;
}
