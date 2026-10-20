// Test library for SAVEVARSTRUCT<CThoughts::EventHistType> (unit v5067; libtpw_unit_SAVEVARSTRUCT.dylib, tests/test_SAVEVARSTRUCT.py), main app:
//   src/TrackCell_SAVEVARSTRUCT.cpp
//
// Callees: CSaveLoad::WriteSerialiseLog, GAME_ASSERTMSG, LbFile_Write are recording fakes; gSaveLoad is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Serialise.h"

using namespace calllog;

CSaveLoad gSaveLoad;

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int32_t>(state)}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    const uint8_t* b = static_cast<const uint8_t*>(buffer);
    ev("LbFile_Write", {lab(file), b[0], b[1], b[2], b[3], length}, {});
    *transferred = static_cast<uint32_t>(ret("written"));
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

extern "C" {

uint64_t tpw_sv_addr(const char* name) {
    if (std::strcmp(name, "gSaveLoad") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSaveLoad));
    return 0;
}

uint64_t tpw_sv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CThoughts::EventHistType, mSecond));
    out[n++] = static_cast<int64_t>(sizeof(CThoughts::EventHistType));
    return n;
}

int32_t tpw_sv_save(uint64_t file, uint64_t value, uint64_t name, int32_t noLog) {
    return SAVEVARSTRUCT<CThoughts::EventHistType>(reinterpret_cast<void*>(file), reinterpret_cast<CThoughts::EventHistType*>(value),
                                                   reinterpret_cast<const char*>(name), noLog);
}

}  // extern "C"
