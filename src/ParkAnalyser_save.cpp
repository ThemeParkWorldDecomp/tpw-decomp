// The park analyser's save-file members for its value and history templates (main app, analyser.cpp range; unit w158): ten functions in
// 5,280 bytes. All of them read or write the same fields in the same order, the load branch calling LOADVAR and the save branch SAVEVAR, and
// only when 0 <= version < 2 (two signed compares); any other version, and a call that answers nonzero, are described below.
//
//   CParkAnalyserVal<T>::Serialise  <int> 0x100ef278, <float> 0x100ef4a8 (276 bytes each): mVal, then mWorkedOut (mTimeSig is not saved).
//   CHistory<T, N>::Serialise: mCurrentEntry, mNumEntries, mWrappedAround, mTemp, then the elements 0 .. mNumEntries - 1 under the text
//     "mData[i]" (the literal, not a formatted index). <unsigned char,50> 0x100ef644 (584 bytes: the element address needs no shift, the
//     other instances 592), <int,5> 0x100efbbc, <int,50> 0x100efe8c, <float,144> 0x100f0414, <int,144> 0x100f0754, <int,30> 0x10110fec,
//     <unsigned int,30> 0x10111560. One body serves them all: the instances differ in the element's size and its load and save calls.
//   CHistory<ObjectId,3>::SerialiseStruct 0x100f0b80 (592 bytes): the same with LOADVARSTRUCT / SAVEVARSTRUCT of the 16-bit tag for mTemp
//     and the elements.
//
// A call that answers nonzero ends the member at once with 1; the end of the member answers 0 (so does a version outside 0 .. 1, with no
// call made). `unused` (the fourth argument) is never read. noLog is 0 in every call.
//
// The element loop reloads mNumEntries from the object before every iteration, as the listings do (lwz 0x1c(r28) after each call), so a loaded
// element that overlaps the count changes the bound. The element address is the object's address plus the data offset plus i * sizeof(T) for every
// i, also from i = N on, where it is the next fields of the object (mCurrentEntry at i = N and mNumEntries at i = N + 1 for the 4-byte rings): the
// walk is byte arithmetic on the object, never mData[i].
#include <cstddef>
#include <cstdint>

#include "History.h"
#include "Serialise.h"
#include "ThingParts2.h"
#include "analyser.h"

namespace {

// The strings of the original (the table at 0x10240129 + 0x1dd ..).
const char kVal[] = "mVal";
const char kWorkedOut[] = "mWorkedOut";
const char kCurrentEntry[] = "mCurrentEntry";
const char kNumEntries[] = "mNumEntries";
const char kWrappedAround[] = "mWrappedAround";
const char kTemp[] = "mTemp";
const char kData[] = "mData[i]";

template <class V>
int32_t Var(bool save, void* file, V* value, const char* name) {
    return save ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}

template <class V>
int32_t VarStruct(bool save, void* file, V* value, const char* name) {
    return save ? SAVEVARSTRUCT<V>(file, value, name, 0) : LOADVARSTRUCT<V>(file, value, name, 0);
}

// The common walk of the three history forms; Elem is LOADVAR / SAVEVAR for the plain elements and the STRUCT pair for the tag.
template <class H, class T, class Elem>
int32_t SerialiseHistory(H* self, void* file, int32_t saveFlag, int32_t version, Elem elem) {
    if (version < 0 || version >= 2) return 0;
    const bool save = saveFlag != 0;
    if (Var<int32_t>(save, file, &self->mCurrentEntry, kCurrentEntry) != 0) return 1;
    if (Var<int32_t>(save, file, &self->mNumEntries, kNumEntries) != 0) return 1;
    if (Var<bool>(save, file, &self->mWrappedAround, kWrappedAround) != 0) return 1;
    if (elem(save, file, &self->mTemp, kTemp) != 0) return 1;
    for (int32_t i = 0; i < self->mNumEntries; ++i) {
        T* element = reinterpret_cast<T*>(reinterpret_cast<char*>(self) + offsetof(H, mData) + static_cast<size_t>(i) * sizeof(T));
        if (elem(save, file, element, kData) != 0) return 1;
    }
    return 0;
}

}  // namespace

template <class T>
int32_t CParkAnalyserVal<T>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (version < 0 || version >= 2) return 0;
    const bool saving = save != 0;
    if (Var<T>(saving, file, &mVal, kVal) != 0) return 1;
    if (Var<bool>(saving, file, &mWorkedOut, kWorkedOut) != 0) return 1;
    return 0;
}

template <class T, int N>
int32_t CHistory<T, N>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    return SerialiseHistory<CHistory<T, N>, T>(this, file, save, version, [](bool s, void* f, T* p, const char* n) { return Var<T>(s, f, p, n); });
}

template <class T, int N>
int32_t CHistory<T, N>::SerialiseStruct(void* file, int32_t save, int32_t version, uint32_t unused) {
    return SerialiseHistory<CHistory<T, N>, T>(this, file, save, version, [](bool s, void* f, T* p, const char* n) { return VarStruct<T>(s, f, p, n); });
}

template int32_t CParkAnalyserVal<int32_t>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CParkAnalyserVal<float>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<uint8_t, 50>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<int32_t, 5>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<int32_t, 50>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<float, 144>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<int32_t, 144>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<int32_t, 30>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<uint32_t, 30>::Serialise(void*, int32_t, int32_t, uint32_t);
template int32_t CHistory<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>, 3>::SerialiseStruct(void*, int32_t, int32_t, uint32_t);
