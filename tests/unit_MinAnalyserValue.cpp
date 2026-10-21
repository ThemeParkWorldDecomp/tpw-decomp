// Test library for the analyser min/max helpers and the errorthingy cursor += (unit v5135), src/game/ui/ui_graph.cpp.
// The CHistory members and the cursor -= are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "History.h"
#include "std_deque_errorthingy.h"

using namespace calllog;

namespace {
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}
}  // namespace

template <>
int32_t CHistory<int, 144>::GetLowestValue(int32_t start, int32_t count) {
    ev("GetLowestI", {lab(this), start, count});
    return static_cast<int32_t>(ret("GetLowestI"));
}
template <>
int32_t CHistory<int, 144>::GetHighestValue(int32_t start, int32_t count) {
    ev("GetHighestI", {lab(this), start, count});
    return static_cast<int32_t>(ret("GetHighestI"));
}
template <>
int32_t CHistory<float, 144>::GetHighestValue(float start, int32_t count) {
    ev("GetHighestF", {lab(this), CanonBits(start), count});
    return static_cast<int32_t>(ret("GetHighestF"));
}
template <>
bool CHistory<int, 144>::GetValue(int32_t& out, int32_t back) const {
    ev("GetValueI", {lab(const_cast<CHistory<int, 144>*>(this)), back});
    out = static_cast<int32_t>(ret("GetValueI"));
    return true;
}
template <>
bool CHistory<float, 144>::GetValue(float& out, int32_t back) const {
    ev("GetValueF", {lab(const_cast<CHistory<float, 144>*>(this)), back});
    uint32_t bits = static_cast<uint32_t>(ret("GetValueF"));
    std::memcpy(&out, &bits, 4);
    return true;
}

namespace errdeque_callee {
void CursorSub(MslErrorDequeCursor* cursor, int32_t count) { ev("CursorSub", {lab(cursor), count}); }
}  // namespace errdeque_callee

extern "C" {

uint64_t tpw_mav_addr(const char*) { return 0; }

// The cursor and the block table hold 32-bit words, so the blocks must sit below 4 GB: a fixed mapping handed out in 64-byte pieces.
uint64_t tpw_mav_alloc(uint32_t size) {
    void* block = std::calloc(1, size < 16 ? 16 : size);
    uint64_t addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
    gMslMacAddressHigh = addr >> 32;   // the cursor keeps the low half; the translation restores this upper half
    return addr;
}

int32_t tpw_mav_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CHistory<int, 144>));
    out[n++] = static_cast<int64_t>(sizeof(MslErrorDequeCursor));
    out[n++] = static_cast<int64_t>(offsetof(MslErrorDequeCursor, mSlot));
    return n;
}

void tpw_mav_min(uint64_t history, uint64_t value, int32_t range) {
    MinAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(history), *reinterpret_cast<int32_t*>(value), range);
}
void tpw_mav_maxf(uint64_t history, uint64_t value, int32_t range) {
    MaxAnalyserValue(reinterpret_cast<CHistory<float, 144>*>(history), *reinterpret_cast<int32_t*>(value), range);
}
void tpw_mav_maxi(uint64_t history, uint64_t value, int32_t range) {
    MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(history), *reinterpret_cast<int32_t*>(value), range);
}
uint64_t tpw_mav_add(uint64_t cursor, int32_t count) {
    return reinterpret_cast<uintptr_t>(MslErrorDequeCursorAddErrorthingy(reinterpret_cast<MslErrorDequeCursor*>(cursor), count));
}

}  // extern "C"
