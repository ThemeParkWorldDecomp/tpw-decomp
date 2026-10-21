// Test library for the park analyser's value and history save templates (unit w158; libtpw_unit_ParkSave.dylib, tests/test_ParkSave.py), main app:
//   src/game/park/analyser.cpp   CParkAnalyserVal<int>/<float>::Serialise, CHistory<T,N>::Serialise (seven instances) and
//                               CHistory<ObjectId,3>::SerialiseStruct
//
// LOADVAR, SAVEVAR (int, bool, unsigned int, unsigned char, float) and LOADVARSTRUCT, SAVEVARSTRUCT (the ObjectId tag) are fakes. Each logs its kind, the
// file's label, the value's offset from the object, noLog and the name text, then takes two answers from the case's queues: "ans" (the result; a missing
// entry is 0) and, for the LOAD forms, "val" (the value to store, of the callee's own size, native order; -1 or no entry leaves the value alone).
#include <cstddef>
#include <cstdint>
#include <cstring>

// the answer and value queues hold up to 160 entries a case (the library compiles the log itself to set the size)
#define TPW_CALLLOG_QUEUE_VALUES 256
#include "calllog.cpp"


#include "History.h"
#include "Serialise.h"
#include "ThingParts2.h"
#include "analyser.h"
#include "platform/arena.h"

using namespace calllog;

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectTag;
typedef CHistory<ObjectTag, 3> ObjectRing;

const char* gBase;   // the object under test (offsets in the log are from it)

// One call of the original's callee: the log line, then the case's answer and, for a load, the value stored.
template <class V>
int32_t Fake(const char* kind, bool load, void* file, V* value, const char* text, int32_t noLog) {
    ev(kind, {lab(file), static_cast<int64_t>(reinterpret_cast<const char*>(value) - gBase), noLog}, {text});
    const int64_t answer = ret("ans", 0);
    const int64_t stored = ret("val", -1);
    if (load && stored != -1) {
        const uint64_t bits = static_cast<uint64_t>(stored);
        std::memcpy(static_cast<void*>(value), &bits, sizeof(V));
    }
    return static_cast<int32_t>(answer);
}

// mData, mCurrentEntry, mNumEntries, mWrappedAround and the size of one history instance.
template <class H>
void HistoryLayout(int64_t* out, int32_t& n) {
    out[n++] = static_cast<int64_t>(offsetof(H, mData));
    out[n++] = static_cast<int64_t>(offsetof(H, mCurrentEntry));
    out[n++] = static_cast<int64_t>(offsetof(H, mNumEntries));
    out[n++] = static_cast<int64_t>(offsetof(H, mWrappedAround));
    out[n++] = static_cast<int64_t>(sizeof(H));
}

}  // namespace

template <> int32_t LOADVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Fake("LOADVAR<int>", true, f, v, n, l); }
template <> int32_t SAVEVAR<int32_t>(void* f, int32_t* v, const char* n, int32_t l) { return Fake("SAVEVAR<int>", false, f, v, n, l); }
template <> int32_t LOADVAR<bool>(void* f, bool* v, const char* n, int32_t l) { return Fake("LOADVAR<bool>", true, f, v, n, l); }
template <> int32_t SAVEVAR<bool>(void* f, bool* v, const char* n, int32_t l) { return Fake("SAVEVAR<bool>", false, f, v, n, l); }
template <> int32_t LOADVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) { return Fake("LOADVAR<unsigned int>", true, f, v, n, l); }
template <> int32_t SAVEVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) { return Fake("SAVEVAR<unsigned int>", false, f, v, n, l); }
template <> int32_t LOADVAR<uint8_t>(void* f, uint8_t* v, const char* n, int32_t l) { return Fake("LOADVAR<unsigned char>", true, f, v, n, l); }
template <> int32_t SAVEVAR<uint8_t>(void* f, uint8_t* v, const char* n, int32_t l) { return Fake("SAVEVAR<unsigned char>", false, f, v, n, l); }
template <> int32_t LOADVAR<float>(void* f, float* v, const char* n, int32_t l) { return Fake("LOADVAR<float>", true, f, v, n, l); }
template <> int32_t SAVEVAR<float>(void* f, float* v, const char* n, int32_t l) { return Fake("SAVEVAR<float>", false, f, v, n, l); }
template <> int32_t LOADVARSTRUCT<ObjectTag>(void* f, ObjectTag* v, const char* n, int32_t l) { return Fake("LOADVARSTRUCT<ObjectId>", true, f, v, n, l); }
template <> int32_t SAVEVARSTRUCT<ObjectTag>(void* f, ObjectTag* v, const char* n, int32_t l) { return Fake("SAVEVARSTRUCT<ObjectId>", false, f, v, n, l); }

extern "C" {

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_ps_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The object the next call works on (the log's offsets are from it).
void tpw_ps_base(uint64_t object) { gBase = reinterpret_cast<const char*>(static_cast<uintptr_t>(object)); }

// The offsets and sizes the module relies on (HOST_LAYOUT): for each history instance mData, mCurrentEntry, mNumEntries, mWrappedAround and sizeof, then
// the value's mVal, mWorkedOut and sizeof.
int32_t tpw_ps_layout(int64_t* out) {
    int32_t n = 0;
    HistoryLayout<CHistory<uint8_t, 50>>(out, n);
    HistoryLayout<CHistory<int32_t, 5>>(out, n);
    HistoryLayout<CHistory<int32_t, 50>>(out, n);
    HistoryLayout<CHistory<float, 144>>(out, n);
    HistoryLayout<CHistory<int32_t, 144>>(out, n);
    HistoryLayout<ObjectRing>(out, n);
    HistoryLayout<CHistory<int32_t, 30>>(out, n);
    HistoryLayout<CHistory<uint32_t, 30>>(out, n);
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserVal<int32_t>, mVal));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserVal<int32_t>, mWorkedOut));
    out[n++] = static_cast<int64_t>(sizeof(CParkAnalyserVal<int32_t>));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserVal<float>, mVal));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserVal<float>, mWorkedOut));
    out[n++] = static_cast<int64_t>(sizeof(CParkAnalyserVal<float>));
    return n;
}

// ---- the functions, one wrapper each (the object as an address)
#define WRAP(NAME, MEMBER, ...) \
    int32_t NAME(uint64_t object, int32_t save, int32_t version, uint32_t unused) { \
        return reinterpret_cast<__VA_ARGS__*>(static_cast<uintptr_t>(object))->MEMBER(reinterpret_cast<void*>(0x1000), save, version, unused); \
    }
WRAP(tpw_ps_val_int, Serialise, CParkAnalyserVal<int32_t>)
WRAP(tpw_ps_val_float, Serialise, CParkAnalyserVal<float>)
WRAP(tpw_ps_uchar50, Serialise, CHistory<uint8_t, 50>)
WRAP(tpw_ps_int5, Serialise, CHistory<int32_t, 5>)
WRAP(tpw_ps_int50, Serialise, CHistory<int32_t, 50>)
WRAP(tpw_ps_float144, Serialise, CHistory<float, 144>)
WRAP(tpw_ps_int144, Serialise, CHistory<int32_t, 144>)
WRAP(tpw_ps_ring3, SerialiseStruct, ObjectRing)
WRAP(tpw_ps_int30, Serialise, CHistory<int32_t, 30>)
WRAP(tpw_ps_uint30, Serialise, CHistory<uint32_t, 30>)

}  // extern "C"
