// Test library for LOADVAR<CPerson::PersonState> (unit v5315; libtpw_unit_LOADVAR.dylib, tests/test_LOADVAR.py), main app:
//   src/CPerson_..._GetBagged_LOADVAR.cpp
//
// The callees outside the unit are fakes: CSaveLoad::WriteSerialiseLog, LbFile_Read and GAME_ASSERTMSG record their arguments;
// LbFile_Read also fills the buffer and answers from the test's queues.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Serialise.h"

using namespace calllog;

CSaveLoad gSaveLoad;
static uint8_t gData[4];

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int64_t>(state)}, {name ? name : ""});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), static_cast<int64_t>(length)});
    std::memcpy(buffer, gData, 4);
    *transferred = static_cast<uint32_t>(ret("xfer"));
    return TbError(static_cast<int32_t>(ret("read")));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("assert", {condition}, {format ? format : ""});
}

extern "C" {

uint64_t tpw_lv_addr(const char* name) {
    if (std::strcmp(name, "gSaveLoad") == 0) return reinterpret_cast<uintptr_t>(&gSaveLoad);
    if (std::strcmp(name, "data") == 0) return reinterpret_cast<uintptr_t>(gData);
    return 0;
}

uint64_t tpw_lv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CPerson::PersonState));
    return n;
}

int32_t tpw_lv_load(uint64_t file, uint64_t value, uint64_t name, int32_t noLog, uint32_t d) {
    // the bytes the fake read hands over, as the file holds them (little-endian word d)
    gData[0] = d & 0xff; gData[1] = (d >> 8) & 0xff; gData[2] = (d >> 16) & 0xff; gData[3] = (d >> 24) & 0xff;
    return LOADVAR<CPerson::PersonState>(reinterpret_cast<void*>(file), reinterpret_cast<CPerson::PersonState*>(value),
                                         reinterpret_cast<const char*>(name), noLog);
}

}  // extern "C"
