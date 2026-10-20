// Test library for CParkAnalyser::Serialise (unit w162; libtpw_unit_ParkSerialise.dylib, tests/test_ParkSerialise.py), main app:
//   src/ParkAnalyser_serialise.cpp   CParkAnalyser::Serialise 0x100e3004
//
// Every callee is a fake that writes nothing. A member call logs its kind, the object's offset from the analyser, the file's label, save, version and
// unused; a VAR call logs its kind, the file's label, the value's offset, noLog and the name text. All of them share one call counter (reset per
// case by tpw_pw_setup): the call whose number equals the case's `fail` answers `failValue`, every other call answers 0.
#include <cstddef>
#include <cstdint>
#include <cstring>

// the log holds a full run (1,091 events) with room to spare
#include "calllog.cpp"

#include "History.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "analyser.h"
#include "platform/arena.h"

using namespace calllog;

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectTag;

const char* gBase;       // the analyser under test (offsets in the log are from it)
int32_t gCount;          // calls so far in this case
int32_t gFail = -1;      // the number of the call that fails (-1: none)
int32_t gFailValue;      // what the failing call answers

// The answer of the next call: the case's failValue for call number gFail, else 0.
int32_t Answer() {
    const int32_t number = gCount++;
    return number == gFail ? gFailValue : 0;
}

// One member call of the original (this, file, save, version, unused).
int32_t Member(const char* kind, const void* object, void* file, int32_t save, int32_t version, uint32_t unused) {
    ev(kind, {static_cast<int64_t>(reinterpret_cast<const char*>(object) - gBase), lab(file), save, version, unused}, {});
    return Answer();
}

// One VAR call of the original (file, value, name, noLog).
int32_t Var(const char* kind, void* file, const void* value, const char* text, int32_t noLog) {
    ev(kind, {lab(file), static_cast<int64_t>(reinterpret_cast<const char*>(value) - gBase), noLog}, {text});
    return Answer();
}

}  // namespace

// CThing::Serialise 0x1012e010 (untranslated)
int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CThing::Serialise", this, file, save, version, unused); }

// CParkAnalyserCell::Serialise 0x100e60a0, CParkAnalyserVal<int>::Serialise 0x100ef278
int32_t CParkAnalyserCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CParkAnalyserCell::Serialise", this, file, save, version, unused); }
template <> int32_t CParkAnalyserVal<int>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CParkAnalyserVal<int>::Serialise", this, file, save, version, unused); }

// the six history instances
template <> int32_t CHistory<unsigned char, 50>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CHistory<unsigned char,50>::Serialise", this, file, save, version, unused); }
template <> int32_t CHistory<int, 5>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CHistory<int,5>::Serialise", this, file, save, version, unused); }
template <> int32_t CHistory<int, 50>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CHistory<int,50>::Serialise", this, file, save, version, unused); }
template <> int32_t CHistory<float, 144>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CHistory<float,144>::Serialise", this, file, save, version, unused); }
template <> int32_t CHistory<int, 144>::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CHistory<int,144>::Serialise", this, file, save, version, unused); }
template <> int32_t CHistory<ObjectTag, 3>::SerialiseStruct(void* file, int32_t save, int32_t version, uint32_t unused) { return Member("CHistory<ObjectId,3>::SerialiseStruct", this, file, save, version, unused); }

// LOADVAR / SAVEVAR<int> 0x1000ef18 / 0x1000f4b0 and LOADVARSTRUCT / SAVEVARSTRUCT<TbTimeStamp> 0x100f1468 / 0x100f1b1c
template <> int32_t LOADVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Var("LOADVAR<int>", f, v, n, l); }
template <> int32_t SAVEVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Var("SAVEVAR<int>", f, v, n, l); }
template <> int32_t LOADVARSTRUCT<TbTimeStamp>(void* f, TbTimeStamp* v, const char* n, int32_t l) { return Var("LOADVARSTRUCT<TbTimeStamp>", f, v, n, l); }
template <> int32_t SAVEVARSTRUCT<TbTimeStamp>(void* f, TbTimeStamp* v, const char* n, int32_t l) { return Var("SAVEVARSTRUCT<TbTimeStamp>", f, v, n, l); }

extern "C" {

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_pw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The analyser the next call works on, and the case's failing call and its answer; the call counter restarts.
void tpw_pw_setup(uint64_t object, int32_t fail, int32_t failValue) {
    gBase = reinterpret_cast<const char*>(static_cast<uintptr_t>(object));
    gCount = 0;
    gFail = fail;
    gFailValue = failValue;
}

// The offsets and sizes the module relies on (HOST_LAYOUT): sizeof(CParkAnalyser), mCells, mLastOfObjectType, mHistory1F100, mOpinionHistory,
// mLastStrike, mLongestStay.
int32_t tpw_pw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CParkAnalyser));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mCells));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mLastOfObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mHistory1F100));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mOpinionHistory));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mLastStrike));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyser, mLongestStay));
    return n;
}

// The function under test; the file is the label address 0x1000.
int32_t tpw_pw_serialise(uint64_t object, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(object))->Serialise(reinterpret_cast<void*>(0x1000), save, version, unused);
}

}  // extern "C"
