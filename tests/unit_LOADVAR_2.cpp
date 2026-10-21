// Test library for LOADVAR<CThoughts::ThoughtType> (unit v5736; libtpw_unit_LOADVAR_2.dylib, tests/test_LOADVAR_2.py), main app:
//   src/game/buildings/TrainingCentre.cpp
//
// The callees outside the unit (WriteSerialiseLog, LbFile_Read, GAME_ASSERTMSG) are recording fakes; gSaveLoad is defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Serialise.h"
#include "SaveLoad.h"
#include "LbFile.h"
#include "Debug.h"

using namespace calllog;

CSaveLoad gSaveLoad;

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int64_t>(state)}, {name ? name : "(null)"});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), static_cast<int64_t>(length)});
    int32_t err = static_cast<int32_t>(ret("err"));
    uint32_t n = static_cast<uint32_t>(ret("n"));
    uint32_t data = static_cast<uint32_t>(ret("data"));
    std::memcpy(buffer, &data, 4);   // the file's bytes, little-endian
    *transferred = n;
    return TbError(err);
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("assert", {condition}, {format ? format : "(null)"});
}

extern "C" {

uint64_t tpw_lv_addr(const char* name) {
    if (std::strcmp(name, "gSaveLoad") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSaveLoad));
    return 0;
}

uint64_t tpw_lv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThoughts::ThoughtType));
    return n;
}

int32_t tpw_lv_load(uint64_t file, uint64_t value, uint64_t name, int32_t noLog) {
    return LOADVAR<CThoughts::ThoughtType>(reinterpret_cast<void*>(file), reinterpret_cast<CThoughts::ThoughtType*>(value),
                                           reinterpret_cast<const char*>(name), noLog);
}

}  // extern "C"
