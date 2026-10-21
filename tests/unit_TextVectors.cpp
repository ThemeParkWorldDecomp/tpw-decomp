// Test library for the text formatter's vector members and line processor (libtpw_unit_TextVectors.dylib; see tests/test_TextVectors.py):
// src/ltms/TbTextFormatter.cpp, which has MslVector<CCharacterInfo>::clear and push_back, MslVector<CLineInfo>::clear and push_back,
// MslVector<TbPoint>::clear, resize, erase and insert, ProcessLine and FixParenthesis. Nothing is faked: the only callees are operator new and
// operator delete (the harness's, which count; the PowerPC side's are fakes that count the same way).
//
// A vector is described to the library as its capacity, its size and the words of its elements; the block is made with operator new, filled
// with 0xa5 and the elements written over it, so that a cell the code should not touch shows (the PowerPC side fills its blocks the same way). After an
// operation the library dumps the capacity, the size, whether the data pointer moved and the cells: all of them up to the size, and, when the block did
// not move, up to the old size as well (a cell that erase left behind is still there).
//
// Element words: a character 5 (the character, the index, the type, the width, the number), a line 3 (the two ends as tokens that are never followed,
// and the width), a point 2 (x and y). A record of ProcessLine's array is 8 words (the five fields and the three padding fields), so that the swaps are
// compared byte for byte.
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "calllog.h"
#include "TbTextFormatter.h"

namespace {

MslVector<CCharacterInfo> gChars;
MslVector<CLineInfo> gLines;
MslVector<TbPoint> gPoints;
int64_t gLastHeap[4];
CCharacterInfo* gRecords = nullptr;   // ProcessLine's array, with a guard record on each side
int32_t gRecordCount = 0;

template <class T>
MslVector<T>& Vec();
template <>
MslVector<CCharacterInfo>& Vec<CCharacterInfo>() { return gChars; }
template <>
MslVector<CLineInfo>& Vec<CLineInfo>() { return gLines; }
template <>
MslVector<TbPoint>& Vec<TbPoint>() { return gPoints; }

void Put(CCharacterInfo& cell, const uint32_t* w) {
    cell.mChar = static_cast<uint16_t>(w[0]);
    cell.mIndex = static_cast<uint16_t>(w[1]);
    cell.mType = static_cast<uint8_t>(w[2]);
    cell.mWidth = static_cast<int32_t>(w[3]);
    cell.mPosition = static_cast<uint16_t>(w[4]);
}
void Put(CLineInfo& cell, const uint32_t* w) {
    cell.mBegin = reinterpret_cast<CCharacterInfo*>(static_cast<uintptr_t>(w[0]));
    cell.mEnd = reinterpret_cast<CCharacterInfo*>(static_cast<uintptr_t>(w[1]));
    cell.mWidth = w[2];
}
void Put(TbPoint& cell, const uint32_t* w) {
    cell.x = static_cast<int32_t>(w[0]);
    cell.y = static_cast<int32_t>(w[1]);
}

constexpr int Words(const CCharacterInfo*) { return 5; }
constexpr int Words(const CLineInfo*) { return 3; }
constexpr int Words(const TbPoint*) { return 2; }

void Get(const CCharacterInfo& cell, uint32_t*& out) {
    *out++ = cell.mChar;
    *out++ = cell.mIndex;
    *out++ = cell.mType;
    *out++ = static_cast<uint32_t>(cell.mWidth);
    *out++ = cell.mPosition;
}
void Get(const CLineInfo& cell, uint32_t*& out) {
    *out++ = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cell.mBegin));
    *out++ = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cell.mEnd));
    *out++ = cell.mWidth;
}
void Get(const TbPoint& cell, uint32_t*& out) {
    *out++ = static_cast<uint32_t>(cell.x);
    *out++ = static_cast<uint32_t>(cell.y);
}

template <class T>
void Make(uint32_t capacity, uint32_t size, const uint32_t* words) {
    MslVector<T>& v = Vec<T>();
    v.mCapacity = capacity;
    v.mSize = size;
    v.mData = nullptr;
    if (capacity == 0) return;
    v.mData = static_cast<T*>(::operator new(capacity * sizeof(T)));
    std::memset(static_cast<void*>(v.mData), 0xa5, capacity * sizeof(T));
    for (uint32_t i = 0; i < size; ++i) Put(v.mData[i], words + Words(static_cast<const T*>(nullptr)) * i);
}

// Runs one operation on the vector of kind T; the dump goes to `out` (capacity, size, moved, the cell count, the cells' words). The answer is
// erase's, as an element number.
template <class T>
int64_t Run(int32_t op, const uint32_t* args, uint32_t* out);

template <class T>
int64_t RunCommon(int32_t op, const uint32_t* args, uint32_t* out) {
    MslVector<T>& v = Vec<T>();
    T* const oldData = v.mData;
    const uint32_t oldSize = v.mSize;
    int64_t answer = 0;
    const int64_t before[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    if (op == 0) {
        v.clear();
    } else if (op == 1) {
        if constexpr (!std::is_same_v<T, TbPoint>) {   // (the points have no push_back)
            T element;
            std::memset(static_cast<void*>(&element), 0, sizeof element);
            Put(element, args);
            v.push_back(element);
        }
    }
    for (int i = 0; i < 4; ++i) gLastHeap[i] = tpw_cl_heap[i] - before[i];
    out[0] = v.mCapacity;
    out[1] = v.mSize;
    out[2] = v.mData != oldData;
    uint32_t count = v.mSize;
    if (v.mData == oldData && oldSize > count) count = oldSize;
    if (v.mData == nullptr) count = 0;
    out[3] = count;
    uint32_t* cursor = out + 4;
    for (uint32_t i = 0; i < count; ++i) Get(v.mData[i], cursor);
    return answer;
}

template <>
int64_t Run<CCharacterInfo>(int32_t op, const uint32_t* args, uint32_t* out) { return RunCommon<CCharacterInfo>(op, args, out); }
template <>
int64_t Run<CLineInfo>(int32_t op, const uint32_t* args, uint32_t* out) { return RunCommon<CLineInfo>(op, args, out); }

template <>
int64_t Run<TbPoint>(int32_t op, const uint32_t* args, uint32_t* out) {
    MslVector<TbPoint>& v = gPoints;
    if (op <= 1) return RunCommon<TbPoint>(op, args, out);
    TbPoint* const oldData = v.mData;
    const uint32_t oldSize = v.mSize;
    int64_t answer = 0;
    const int64_t before[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    if (op == 2) {
        v.resize(args[0]);
    } else if (op == 3) {
        TbPoint* result = v.erase(v.mData + args[0], v.mData + args[1]);
        answer = result - v.mData;
    } else if (op == 4) {
        const TbPoint value = {static_cast<int32_t>(args[2]), static_cast<int32_t>(args[3])};
        v.insert(v.mData + args[0], args[1], value);
    }
    for (int i = 0; i < 4; ++i) gLastHeap[i] = tpw_cl_heap[i] - before[i];
    out[0] = v.mCapacity;
    out[1] = v.mSize;
    out[2] = v.mData != oldData;
    uint32_t count = v.mSize;
    if (v.mData == oldData && oldSize > count) count = oldSize;
    if (v.mData == nullptr) count = 0;
    out[3] = count;
    uint32_t* cursor = out + 4;
    for (uint32_t i = 0; i < count; ++i) Get(v.mData[i], cursor);
    return answer;
}

void FreeAll() {
    ::operator delete(gChars.mData);
    ::operator delete(gLines.mData);
    ::operator delete(gPoints.mData);
    gChars = {0, 0, nullptr};
    gLines = {0, 0, nullptr};
    gPoints = {0, 0, nullptr};
    delete[] gRecords;
    gRecords = nullptr;
}

}  // namespace

extern "C" {

// Forgets the last case's world (the blocks are let go with the harness's delete; the counters are the caller's to clear).
void tpw_tv_reset() { FreeAll(); }

// Vector `kind` (0 characters, 1 lines, 2 points) with the given capacity, size and element words.
void tpw_tv_make(int32_t kind, uint32_t capacity, uint32_t size, const uint32_t* words) {
    if (kind == 0) Make<CCharacterInfo>(capacity, size, words);
    else if (kind == 1) Make<CLineInfo>(capacity, size, words);
    else Make<TbPoint>(capacity, size, words);
}

// One operation (0 clear, 1 push_back of the element in args, 2 resize(args[0]), 3 erase(args[0], args[1]) answering the element number,
// 4 insert(args[0], args[1], the point args[2], args[3])) on vector `kind`; the dump (see above) goes to `out`.
int64_t tpw_tv_op(int32_t kind, int32_t op, const uint32_t* args, uint32_t* out) {
    for (int64_t& blocks : gLastHeap) blocks = 0;
    if (kind == 0) return Run<CCharacterInfo>(op, args, out);
    if (kind == 1) return Run<CLineInfo>(op, args, out);
    return Run<TbPoint>(op, args, out);
}

// The blocks the last operation handled (operator new, new[], delete, delete[]).
void tpw_tv_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = gLastHeap[i];
}

// A bare MslVector<CCharacterInfo> with data == NULL and the given words in its capacity and size (for the clear tests: the data is not touched).
// Nothing to do here: tpw_tv_make with capacity 0 makes it.

// ProcessLine's array: `count` records of 8 words (see above) with a guard record before and after (every byte 0xa5 at the start).
void tpw_tv_records(int32_t count, const uint32_t* words) {
    delete[] gRecords;
    gRecordCount = count;
    gRecords = new CCharacterInfo[static_cast<size_t>(count) + 2];
    std::memset(static_cast<void*>(gRecords), 0xa5, sizeof(CCharacterInfo) * (static_cast<size_t>(count) + 2));
    for (int32_t i = 0; i < count; ++i) {
        CCharacterInfo& cell = gRecords[i + 1];
        const uint32_t* w = words + 8 * i;
        cell.mChar = static_cast<uint16_t>(w[0]);
        cell.mIndex = static_cast<uint16_t>(w[1]);
        cell.mType = static_cast<uint8_t>(w[2]);
        cell.mPad5 = static_cast<uint8_t>(w[3]);
        cell.mPad6 = static_cast<uint16_t>(w[4]);
        cell.mWidth = static_cast<int32_t>(w[5]);
        cell.mPosition = static_cast<uint16_t>(w[6]);
        cell.mPadE = static_cast<uint16_t>(w[7]);
    }
}

// ProcessLine (op 0) or FixParenthesis (op 1) on the records [first, last) (element numbers 0 to count; first may be above last).
void tpw_tv_line(int32_t op, int32_t first, int32_t last) {
    CCharacterInfo* const base = gRecords + 1;
    if (op == 0) ProcessLine(base + first, base + last);
    else FixParenthesis(base + first, base + last);
}

// The records, guards included, as 8 words each.
int32_t tpw_tv_dump_records(uint32_t* out) {
    uint32_t* cursor = out;
    for (int32_t i = 0; i < gRecordCount + 2; ++i) {
        const CCharacterInfo& cell = gRecords[i];
        *cursor++ = cell.mChar;
        *cursor++ = cell.mIndex;
        *cursor++ = cell.mType;
        *cursor++ = cell.mPad5;
        *cursor++ = cell.mPad6;
        *cursor++ = static_cast<uint32_t>(cell.mWidth);
        *cursor++ = cell.mPosition;
        *cursor++ = cell.mPadE;
    }
    return static_cast<int32_t>(cursor - out);
}

}  // extern "C"
