// Test library for SAVEVAR<ResearchCat> (unit v5045; libtpw_unit_SAVEVAR.dylib, tests/test_SAVEVAR.py), main app:
//   src/ParkMessages_SAVEVAR.cpp
//
// The callees (WriteSerialiseLog, GAME_ASSERTMSG, LbFile_Write) are recording fakes; gSaveLoad is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Serialise.h"
#include "ThingReceivers.h"

using namespace calllog;

CSaveLoad gSaveLoad;

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int32_t>(state)}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    uint32_t data = 0;
    std::memcpy(&data, buffer, 4);
    ev("LbFile_Write", {lab(file), static_cast<int64_t>(data), static_cast<int64_t>(length), static_cast<int64_t>(*transferred)});
    int32_t result = static_cast<int32_t>(ret("wres"));
    *transferred = static_cast<uint32_t>(ret("wcnt"));
    return TbError(result);
}

extern "C" {

uint64_t tpw_sv_addr(const char*) { return 0; }

uint64_t tpw_sv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_sv_gsave() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSaveLoad)); }

int32_t tpw_sv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(ResearchCat));
    return n;
}

int32_t tpw_sv_f(uint64_t file, uint64_t value, uint64_t name, int32_t noLog) {
    return SAVEVAR<ResearchCat>(reinterpret_cast<void*>(file), reinterpret_cast<ResearchCat*>(value), reinterpret_cast<const char*>(name), noLog);
}

}  // extern "C"
