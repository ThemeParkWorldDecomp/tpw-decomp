// CHistory<T, N> and CParkAnalyserVal<int> members (main app, unit w157). The analyser keeps its series (visitors, cash, ratings ...) in rings of
// the last N values. Each member is one body here; the original has one copy per instance (and per source file it was used in), listed in
// include/History.h and below at the explicit instantiations. The bodies read mNumEntries from the object, never N, as the listings do.
// Layout (History.h): mTemp, the ring, mCurrentEntry (the newest, -1 while empty), mNumEntries, mWrappedAround.
#include <cstdint>

#include "Debug.h"
#include "History.h"
#include "ThingParts2.h"
#include "TimeSig.h"
#include "analyser.h"
#include "ppc_math.h"

// ---- CParkAnalyserVal<int> (main 0x100ef3c8 and 0x100ef400)

template <>
int32_t CParkAnalyserVal<int32_t>::Get() const {   // 0x100ef3c8
    return mVal;
}

// Stores the value, marks it worked out, then stamps the first word (a CTimeSig image) with the world's turn.
template <>
void CParkAnalyserVal<int32_t>::Set(int32_t value) {   // 0x100ef400
    mVal = value;
    mWorkedOut = true;
    reinterpret_cast<CTimeSig*>(this)->SetTimeSig();
}

// ---- CHistory

// Advances the newest index, wrapping to 0 (and flagging the wrap) at mNumEntries, then stores the value at the index re-read.
template <class T, int N>
void CHistory<T, N>::SetAtEnd(T value) {
    mCurrentEntry += 1;
    if (mCurrentEntry >= mNumEntries) {
        mCurrentEntry = 0;
        mWrappedAround = true;
    }
    mData[mCurrentEntry] = value;
}

// The value `back` places before the newest. A negative `back` passes the first two tests (plain arithmetic, as the listing does).
template <class T, int N>
bool CHistory<T, N>::GetValue(T& out, int32_t back) const {
    if (back >= mNumEntries) return false;
    if (back <= mCurrentEntry) {
        out = mData[mCurrentEntry - back];
        return true;
    }
    if (mWrappedAround) {
        out = mData[mNumEntries + mCurrentEntry - back];
        return true;
    }
    return false;
}

// How many values the ring holds.
template <class T, int N>
int32_t CHistory<T, N>::HowFarBack() const {
    return mWrappedAround ? mNumEntries : mCurrentEntry + 1;
}

// The sum of the last `count` values. The assert fires on every call whose count is above mNumEntries (and the count is then truncated anyway).
// HowFarBack is called twice when it clamps. The element is converted to double (signed, unsigned or zero-extended by its type). Answers the count.
template <class T, int N>
int32_t CHistory<T, N>::GetSumOfLast(double& sum, int32_t count) const {
    GAME_ASSERTMSG(count <= mNumEntries,
                   "CHistory - You asked for the sum of more elements than there is defined history for\n"
                   "The request will be truncated, and you'll get what we can give\n");
    int32_t n = count;
    if (HowFarBack() < count) n = HowFarBack();
    sum = 0.0;
    for (int32_t i = 0; i < n; ++i) {
        T value;
        if (GetValue(value, i)) sum += static_cast<double>(value);
    }
    return n;
}

// The mean of the last `count` values, truncated toward zero (fctiwz); 0 when nothing was summed. A one-byte type keeps the low byte.
template <class T, int N>
void CHistory<T, N>::GetAverageOfLast(T& out, int32_t count) const {
    double sum;
    const int32_t n = GetSumOfLast(sum, count);
    if (n > 0) out = static_cast<T>(ppc_fctiwz(sum / static_cast<double>(n)));
    else out = 0;
}

// Pushes mTemp onto the ring. <int,144> inlines SetAtEnd, <int,30> and <unsigned int,30> call it; the behaviour is the same.
template <class T, int N>
void CHistory<T, N>::SetFromTemp() {
    SetAtEnd(mTemp);
}

// The index (0 = newest) of the largest value among the entries 0 .. count that the ring holds, only a value above `start` counting (the first of
// equals wins); -1 when none. The loop ends at mNumEntries, after `count`, or at the first index GetValue refuses.
template <class T, int N>
int32_t CHistory<T, N>::GetHighestValue(T start, int32_t count) {
    T best = start;
    int32_t index = -1;
    for (int32_t i = 0; i < mNumEntries && i <= count; ++i) {
        T value;
        if (!GetValue(value, i)) break;
        if (value > best) {   // the float instance: fcmpo, so a NaN never wins
            best = value;
            index = i;
        }
    }
    return index;
}

// As GetHighestValue for the smallest value.
template <class T, int N>
int32_t CHistory<T, N>::GetLowestValue(T start, int32_t count) {
    T best = start;
    int32_t index = -1;
    for (int32_t i = 0; i < mNumEntries && i <= count; ++i) {
        T value;
        if (!GetValue(value, i)) break;
        if (value < best) {
            best = value;
            index = i;
        }
    }
    return index;
}

// ---- the three tag_IDX_TYPE default constructors (0x100e2ef0 MapId, 0x100f1d7c ObjectId, 0x10116ccc ThingId) are a lone blr: the tag is an aggregate
// (analyser.h) and its implicit trivial default constructor is the translation.

// ---- the instances the program has (the manifest's addresses)
struct IDXTYPEUniqueNameObjectId;
typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdTag;
typedef CHistory<ObjectIdTag, 3> HistoryObjectId;

template void CHistory<unsigned char, 50>::SetAtEnd(unsigned char);           // 0x100ef8c4
template void CHistory<int32_t, 50>::SetAtEnd(int32_t);                        // 0x100f0114
template void CHistory<float, 144>::SetAtEnd(float);                           // 0x100f069c
template void CHistory<int32_t, 144>::SetAtEnd(int32_t);                       // 0x100f09dc
template void HistoryObjectId::SetAtEnd(ObjectIdTag);                          // 0x100f0e38
template void CHistory<int32_t, 5>::SetAtEnd(int32_t);                         // 0x100f2944
template void CHistory<int32_t, 30>::SetAtEnd(int32_t);                        // 0x10111d14
template void CHistory<uint32_t, 30>::SetAtEnd(uint32_t);                      // 0x10111e24

template bool HistoryObjectId::GetValue(ObjectIdTag&, int32_t) const;          // 0x100f0f04
template bool CHistory<unsigned char, 50>::GetValue(unsigned char&, int32_t) const;   // 0x100f1dcc
template bool CHistory<int32_t, 50>::GetValue(int32_t&, int32_t) const;        // 0x100f1e68
template bool CHistory<uint32_t, 30>::GetValue(uint32_t&, int32_t) const;      // 0x100f1f0c
template bool CHistory<int32_t, 5>::GetValue(int32_t&, int32_t) const;         // 0x1001bf6c
template bool CHistory<int32_t, 144>::GetValue(int32_t&, int32_t) const;       // 0x1001c2a8
template bool CHistory<int32_t, 30>::GetValue(int32_t&, int32_t) const;        // 0x10111d80
template bool CHistory<float, 144>::GetValue(float&, int32_t) const;           // 0x101f6854

template int32_t CHistory<unsigned char, 50>::HowFarBack() const;              // 0x100efb1c
template int32_t CHistory<int32_t, 50>::HowFarBack() const;                    // 0x100f0370
template int32_t CHistory<uint32_t, 30>::HowFarBack() const;                   // 0x100f1134
template int32_t CHistory<int32_t, 5>::HowFarBack() const;                     // 0x1001c010
template int32_t CHistory<int32_t, 144>::HowFarBack() const;                   // 0x1001c254
template int32_t CHistory<int32_t, 30>::HowFarBack() const;                    // 0x101114c0
template int32_t CHistory<float, 144>::HowFarBack() const;                     // 0x101f69d8

template int32_t CHistory<unsigned char, 50>::GetSumOfLast(double&, int32_t) const;   // 0x100ef9e8
template int32_t CHistory<int32_t, 50>::GetSumOfLast(double&, int32_t) const;  // 0x100f0238
template int32_t CHistory<uint32_t, 30>::GetSumOfLast(double&, int32_t) const; // 0x100f1000
template int32_t CHistory<int32_t, 144>::GetSumOfLast(double&, int32_t) const; // 0x1001c11c
template int32_t CHistory<int32_t, 30>::GetSumOfLast(double&, int32_t) const;  // 0x1011132c

template void CHistory<unsigned char, 50>::GetAverageOfLast(unsigned char&, int32_t) const;   // 0x100ef930
template void CHistory<int32_t, 50>::GetAverageOfLast(int32_t&, int32_t) const;   // 0x100f0180
template void CHistory<int32_t, 144>::GetAverageOfLast(int32_t&, int32_t) const;  // 0x1001c064
template void CHistory<int32_t, 30>::GetAverageOfLast(int32_t&, int32_t) const;   // 0x10111274

template void CHistory<int32_t, 144>::SetFromTemp();                           // 0x100f0a48
template void CHistory<int32_t, 30>::SetFromTemp();                            // 0x10111464
template void CHistory<uint32_t, 30>::SetFromTemp();                           // 0x101117e8

template int32_t CHistory<float, 144>::GetHighestValue(float, int32_t);        // 0x101f68f8
template int32_t CHistory<int32_t, 144>::GetHighestValue(int32_t, int32_t);    // 0x101f6a2c
template int32_t CHistory<int32_t, 144>::GetLowestValue(int32_t, int32_t);     // 0x101f6b0c
