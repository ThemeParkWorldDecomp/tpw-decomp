// Test library for the analyser's history rings and value templates (unit w157; libtpw_unit_ParkHistory.dylib, tests/test_ParkHistory.py), main app:
//   src/ParkAnalyser_history.cpp   CHistory SetAtEnd, GetValue, HowFarBack, GetSumOfLast, GetAverageOfLast, SetFromTemp, GetHighestValue and
//                                  GetLowestValue (eight rings) and CParkAnalyserVal<int> Get and Set
//
// The only callee outside the unit is GAME_ASSERTMSG (a recording fake: the condition and the text). CTimeSig::SetTimeSig is the inline original; it
// reads gWorld, which this file defines and points at a raw block of the world's size (the test writes the turn word through tpw_ph_world).
// One wrapper per operation takes the ring by its type number (see Dispatch) and the Mac-layout block the test built.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "History.h"
#include "ThingParts2.h"
#include "TimeSig.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

struct IDXTYPEUniqueNameObjectId;
struct IDXTYPEUniqueNameThingId;
typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdTag;

// The world is raw storage: CWorld has a constructor this library does not link. SetTimeSig reads only mTurn.
alignas(CWorld) static unsigned char gTestWorldBytes[sizeof(CWorld)];
CWorld* gWorld = reinterpret_cast<CWorld*>(gTestWorldBytes);

// ---- the fake: the condition and the text, as the PowerPC stub records them
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

namespace {

// The eight rings by type number: 0 <unsigned char,50>, 1 <int,50>, 2 <float,144>, 3 <int,144>, 4 <ObjectId,3>, 5 <int,5>, 6 <int,30>, 7 <unsigned int,30>.
// Ops is the set of operations the program has an instance of (bit 0 SetAtEnd, 1 GetValue, 2 HowFarBack, 3 GetSumOfLast, 4 GetAverageOfLast, 5 SetFromTemp).
template <class T, int N> constexpr unsigned Ops() { return 0x1f; }
template <> constexpr unsigned Ops<float, 144>() { return 0x07; }
template <> constexpr unsigned Ops<int32_t, 144>() { return 0x3f; }
template <> constexpr unsigned Ops<ObjectIdTag, 3>() { return 0x03; }
template <> constexpr unsigned Ops<int32_t, 5>() { return 0x07; }
template <> constexpr unsigned Ops<int32_t, 30>() { return 0x3f; }
template <> constexpr unsigned Ops<uint32_t, 30>() { return 0x2f; }

template <class T, int N>
uint32_t Run(int32_t op, void* object, void* out, int32_t arg, uint32_t bits) {
    CHistory<T, N>& h = *static_cast<CHistory<T, N>*>(object);
    constexpr unsigned ops = Ops<T, N>();
    if constexpr ((ops & 1) != 0) {   // SetAtEnd(value): the value is the bits (a float's bits, the tag's index)
        if (op == 0) {
            T value;
            std::memcpy(&value, &bits, sizeof value);
            h.SetAtEnd(value);
        }
    }
    if constexpr ((ops & 2) != 0) {
        if (op == 1) return h.GetValue(*static_cast<T*>(out), arg) ? 1u : 0u;
    }
    if constexpr ((ops & 4) != 0) {
        if (op == 2) return static_cast<uint32_t>(h.HowFarBack());
    }
    if constexpr ((ops & 8) != 0) {
        if (op == 3) return static_cast<uint32_t>(h.GetSumOfLast(*static_cast<double*>(out), arg));
    }
    if constexpr ((ops & 16) != 0) {
        if (op == 4) h.GetAverageOfLast(*static_cast<T*>(out), arg);
    }
    if constexpr ((ops & 32) != 0) {
        if (op == 5) h.SetFromTemp();
    }
    return 0;
}

// GetHighestValue and GetLowestValue exist for three rings only.
template <class T, int N>
uint32_t RunBest(bool highest, void* object, int32_t count, uint32_t bits) {
    CHistory<T, N>& h = *static_cast<CHistory<T, N>*>(object);
    T start;
    std::memcpy(&start, &bits, sizeof start);
    if (highest) return static_cast<uint32_t>(h.GetHighestValue(start, count));
    if constexpr (!std::is_same<T, float>::value) return static_cast<uint32_t>(h.GetLowestValue(start, count));
    return 0;   // the program has no GetLowestValue of the float ring
}

}  // namespace

template <class T, int N>
static void RingLayout(int64_t* out, int32_t& n) {
    typedef CHistory<T, N> H;
    out[n++] = static_cast<int64_t>(sizeof(H));
    out[n++] = static_cast<int64_t>(offsetof(H, mData));
    out[n++] = static_cast<int64_t>(offsetof(H, mCurrentEntry));
    out[n++] = static_cast<int64_t>(offsetof(H, mNumEntries));
    out[n++] = static_cast<int64_t>(offsetof(H, mWrappedAround));
}
extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_ph_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gTestWorldBytes));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_ph_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT): per ring sizeof, mData, mCurrentEntry, mNumEntries, mWrappedAround; then the world's
// mTurn and the Val's mTimeSig, mVal, mWorkedOut.
#define RING_LAYOUT(T, N) RingLayout<T, N>(out, n);
int32_t tpw_ph_layout(int64_t* out) {
    int32_t n = 0;
    RING_LAYOUT(unsigned char, 50)
    RING_LAYOUT(int32_t, 50)
    RING_LAYOUT(float, 144)
    RING_LAYOUT(int32_t, 144)
    RING_LAYOUT(ObjectIdTag, 3)
    RING_LAYOUT(int32_t, 5)
    RING_LAYOUT(int32_t, 30)
    RING_LAYOUT(uint32_t, 30)
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserVal<int32_t>, mTimeSig));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserVal<int32_t>, mVal));
    out[n++] = static_cast<int64_t>(offsetof(CParkAnalyserVal<int32_t>, mWorkedOut));
    return n;
}

// One operation (0 SetAtEnd, 1 GetValue, 2 HowFarBack, 3 GetSumOfLast, 4 GetAverageOfLast, 5 SetFromTemp) on ring `type`; the answer's low 32 bits.
uint32_t tpw_ph_op(int32_t type, int32_t op, uint64_t object, uint64_t out, int32_t arg, uint32_t bits) {
    void* o = reinterpret_cast<void*>(static_cast<uintptr_t>(object));
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(out));
    switch (type) {
        case 0: return Run<unsigned char, 50>(op, o, p, arg, bits);
        case 1: return Run<int32_t, 50>(op, o, p, arg, bits);
        case 2: return Run<float, 144>(op, o, p, arg, bits);
        case 3: return Run<int32_t, 144>(op, o, p, arg, bits);
        case 4: return Run<ObjectIdTag, 3>(op, o, p, arg, bits);
        case 5: return Run<int32_t, 5>(op, o, p, arg, bits);
        case 6: return Run<int32_t, 30>(op, o, p, arg, bits);
        default: return Run<uint32_t, 30>(op, o, p, arg, bits);
    }
}

// GetHighestValue (highest 1) or GetLowestValue on <float,144> (type 2) or <int,144> (type 3).
uint32_t tpw_ph_best(int32_t type, int32_t highest, uint64_t object, int32_t count, uint32_t bits) {
    void* o = reinterpret_cast<void*>(static_cast<uintptr_t>(object));
    if (type == 2) return RunBest<float, 144>(highest != 0, o, count, bits);
    return RunBest<int32_t, 144>(highest != 0, o, count, bits);
}

// CParkAnalyserVal<int>: 0 Get, 1 Set(value).
uint32_t tpw_ph_val(int32_t op, uint64_t object, int32_t value) {
    CParkAnalyserVal<int32_t>& v = *reinterpret_cast<CParkAnalyserVal<int32_t>*>(static_cast<uintptr_t>(object));
    if (op == 0) return static_cast<uint32_t>(v.Get());
    v.Set(value);
    return 0;
}

// The three tag_IDX_TYPE default constructors: a default-initialisation (placement new, no initialiser) of the tag in the block; the block must stay as it was.
void tpw_ph_tag(int32_t which, uint64_t block) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(block));
    if (which == 0) new (p) tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
    else if (which == 1) new (p) tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
    else new (p) tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
}

}  // extern "C"
