// Test library for CParkTime::Serialise (unit v5222; libtpw_unit_CParkTime_Serialise.dylib, tests/test_CParkTime_Serialise.py), main app:
//   src/game/people/ParkTime.cpp
//
// The six callees (SAVEVAR / LOADVAR of int and MacULong, SAVEVARSTRUCT / LOADVARSTRUCT of TbTimeStamp) are fakes that record the file, the offset of
// the value inside the object, the name (as its index in the five names) and the no-log flag, and answer from a queue.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ParkTime.h"
#include "Serialise.h"

using namespace calllog;

namespace {

uintptr_t gObject = 0;

int64_t NameIndex(const char* name) {
    static const char* const names[] = {"mFunnyTimeStart", "mSessionStart", "mMonthAtLastUpdate", "mDayAtLastUpdate", "mFunnySecsPerRealSec"};
    for (int i = 0; i < 5; ++i)
        if (std::strcmp(names[i], name) == 0) return i;
    return -1;
}

int32_t Fake(const char* event, void* file, void* value, const char* name, int32_t noLog) {
    ev(event, {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)),
               static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gObject), NameIndex(name), static_cast<int64_t>(noLog)});
    return static_cast<int32_t>(ret(event));
}

}  // namespace

template<> int32_t SAVEVARSTRUCT<TbTimeStamp>(void* f, TbTimeStamp* v, const char* n, int32_t l) { return Fake("SaveStamp", f, v, n, l); }
template<> int32_t LOADVARSTRUCT<TbTimeStamp>(void* f, TbTimeStamp* v, const char* n, int32_t l) { return Fake("LoadStamp", f, v, n, l); }
template<> int32_t SAVEVAR<int>(void* f, int* v, const char* n, int32_t l) { return Fake("SaveInt", f, v, n, l); }
template<> int32_t LOADVAR<int>(void* f, int* v, const char* n, int32_t l) { return Fake("LoadInt", f, v, n, l); }
template<> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) { return Fake("SaveUlong", f, v, n, l); }
template<> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) { return Fake("LoadUlong", f, v, n, l); }

extern "C" {

uint64_t tpw_cpt_addr(const char*) { return 0; }

uint64_t tpw_cpt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cpt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CParkTime, mMonthAtLastUpdate));
    out[n++] = static_cast<int64_t>(offsetof(CParkTime, mDayAtLastUpdate));
    out[n++] = static_cast<int64_t>(offsetof(CParkTime, mFunnySecsPerRealSec));
    out[n++] = static_cast<int64_t>(sizeof(CParkTime));
    return n;
}

int32_t tpw_cpt_serialise(uint64_t object, uint32_t file, int32_t save, int32_t version, uint32_t unused) {
    gObject = static_cast<uintptr_t>(object);
    return reinterpret_cast<CParkTime*>(gObject)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}

}  // extern "C"
