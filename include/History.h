// CHistory<T, N>: the last N values of a series (main app, unit w122b). Mac layout and natural alignment: the 'temp' slot at +0, the
// ring of N values, the index of the newest, the capacity and the wrapped flag. Member names are the originals (the Serialise strings of
// CHistory<int,144> 0x100f0754). The eight constructors are the same code at eight addresses: mTemp, mCurrentEntry, mNumEntries and
// mWrappedAround are stored in that order and the ring is never written. <int,5> 0x100efb70, <int,30> 0x10110fa0, <unsigned int,30>
// 0x10111514, <int,50> 0x100efe40, <int,144> 0x100f0708, <float,144> 0x100f03c4, <unsigned char,50> 0x100ef5f8 and
// <tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, 3> 0x100f0abc (which also runs an empty element constructor, 0x100f1d7c, three times).
#pragma once
#include <cstdint>

template <class T, int N>
struct CHistory {
    CHistory() : mTemp(), mCurrentEntry(-1), mNumEntries(N), mWrappedAround(false) {}
    // Translated (unit w157, src/game/park/analyser.cpp): answers the count it summed (after the clamp to HowFarBack). Instances: <unsigned char,50>
    // 0x100ef9e8, <int,50> 0x100f0238, <unsigned int,30> 0x100f1000, <int,144> 0x1001c11c, <int,30> 0x1011132c.
    int32_t GetSumOfLast(double& sum, int32_t count) const;
    T mTemp;                 // 0x00  "mTemp" (value-initialised: 0, or 0.0f)
    T mData[N];              // "mData[i]", from +4 (+1 for unsigned char, +2 for the 16-bit tag); not written by the constructor
    int32_t mCurrentEntry;   // "mCurrentEntry": -1 while empty
    int32_t mNumEntries;     // "mNumEntries": N
    // Unit w151, translated in src/game/park/analyser.cpp (unit w157), one body per member:
    // SetAtEnd: <unsigned char,50> 0x100ef8c4, <int,50> 0x100f0114, <float,144> 0x100f069c, <int,144> 0x100f09dc, <ObjectId,3> 0x100f0e38,
    // <int,5> 0x100f2944, <int,30> 0x10111d14, <unsigned int,30> 0x10111e24.
    void SetAtEnd(T value);
    // GetValue: <ObjectId,3> 0x100f0f04, <unsigned char,50> 0x100f1dcc, <int,50> 0x100f1e68, <unsigned int,30> 0x100f1f0c, <int,5> 0x1001bf6c,
    // <int,144> 0x1001c2a8, <int,30> 0x10111d80, <float,144> 0x101f6854.
    bool GetValue(T& out, int32_t back) const;   // false when the history holds no value `back` places back
    // Unit w154, translated in src/game/park/analyser.cpp (unit w157). GetAverageOfLast: <unsigned char,50> 0x100ef930, <int,50> 0x100f0180,
    // <int,144> 0x1001c064, <int,30> 0x10111274. HowFarBack: <unsigned char,50> 0x100efb1c, <int,50> 0x100f0370, <unsigned int,30> 0x100f1134,
    // <int,5> 0x1001c010, <int,144> 0x1001c254, <int,30> 0x101114c0, <float,144> 0x101f69d8.
    void GetAverageOfLast(T& out, int32_t count) const;   // the mean of the last `count` values, written to `out`
    int32_t HowFarBack() const;                           // how many values the history holds (50 once it has wrapped)
    // Unit w157 (src/game/park/analyser.cpp). SetFromTemp: <int,144> 0x100f0a48, <int,30> 0x10111464, <unsigned int,30> 0x101117e8.
    void SetFromTemp();
    // GetHighestValue: <float,144> 0x101f68f8, <int,144> 0x101f6a2c. GetLowestValue: <int,144> 0x101f6b0c. They answer the index of the best value
    // among the first count + 1 entries read back (-1 when none beats `start`).
    int32_t GetHighestValue(T start, int32_t count);
    int32_t GetLowestValue(T start, int32_t count);
    bool mWrappedAround;     // "mWrappedAround"
    // Unit w158 (src/game/park/analyser.cpp): <unsigned char,50> 0x100ef644, <int,5> 0x100efbbc, <int,50> 0x100efe8c, <float,144> 0x100f0414,
    // <int,144> 0x100f0754, <int,30> 0x10110fec, <unsigned int,30> 0x10111560; SerialiseStruct is the <ObjectId,3> instance 0x100f0b80.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
    int32_t SerialiseStruct(void* file, int32_t save, int32_t version, uint32_t unused);
};
static_assert(sizeof(CHistory<int, 5>) == 0x24, "CHistory<int,5>");
static_assert(sizeof(CHistory<int, 30>) == 0x88, "CHistory<int,30>");
static_assert(sizeof(CHistory<uint32_t, 30>) == 0x88, "CHistory<unsigned int,30>");
static_assert(sizeof(CHistory<int, 50>) == 0xd8, "CHistory<int,50>");
static_assert(sizeof(CHistory<int, 144>) == 0x250, "CHistory<int,144>");
static_assert(sizeof(CHistory<float, 144>) == 0x250, "CHistory<float,144>");
static_assert(sizeof(CHistory<unsigned char, 50>) == 0x40, "CHistory<unsigned char,50>");

// Unit v5135 (src/game/ui/ui_graph.cpp): the analyser helpers. The range picks the lookback: 0 -> 0xc, 1 -> 0x24, 2 -> 0x90, else 0.
// MinAnalyserValue<int,144> 0x101f6030 (lowers value to the lowest entry), MaxAnalyserValue<float,144> 0x101f5f2c (the entry is cut to int),
// MaxAnalyserValue<int,144> 0x101f5e3c.
void MinAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range);
void MaxAnalyserValue(CHistory<float, 144>* history, int32_t& value, int32_t range);
void MaxAnalyserValue(CHistory<int, 144>* history, int32_t& value, int32_t range);
