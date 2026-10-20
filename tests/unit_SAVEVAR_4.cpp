// Test library for SAVEVAR<CThoughts::ThoughtType> (unit v5643; libtpw_unit_SAVEVAR_4.dylib, tests/test_SAVEVAR_4.py), main app:
//   src/TrackCell_SAVEVARSTRUCT_SAVEVAR_4.cpp
//
// Fakes: CSaveLoad::WriteSerialiseLog, GAME_ASSERTMSG, LbFile_Write record their arguments. gSaveLoad is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Serialise.h"
#include "SaveLoad.h"
#include "Debug.h"
#include "LbFile.h"

using namespace calllog;

CSaveLoad gSaveLoad;
static char gFileDummy[16];

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int32_t>(state)}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    uint32_t word = 0;
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);   // read as the big-endian word the PowerPC test sees
    word = (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) | (uint32_t(bytes[2]) << 8) | bytes[3];
    ev("LbFile_Write", {lab(file), static_cast<int64_t>(length), static_cast<int64_t>(word)}, {});
    int32_t err = static_cast<int32_t>(static_cast<uint32_t>(ret("LbFile_Write")));
    if (transferred) *transferred = static_cast<uint32_t>(ret("written"));
    return TbError(err);
}

extern "C" {

uint64_t tpw_sv4_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gSaveLoad", &gSaveLoad},
        {"file", gFileDummy},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sv4_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sv4_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThoughts::ThoughtType));
    return n;
}

uint32_t tpw_sv4_f(uint64_t file, uint64_t value, uint64_t name, int32_t noLog) {
    return static_cast<uint32_t>(SAVEVAR<CThoughts::ThoughtType>(reinterpret_cast<void*>(file),
        reinterpret_cast<CThoughts::ThoughtType*>(value), reinterpret_cast<const char*>(name), noLog));
}

}  // extern "C"
