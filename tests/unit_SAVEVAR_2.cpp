// Test library for SAVEVAR<ESP_Sprites> and __sinit_Peep_cpp (unit SAVEVAR_2; libtpw_unit_SAVEVAR_2.dylib, tests/test_SAVEVAR_2.py), main app:
//   src/CPeep_IsLocalAreaUnchangedSinceBecomingStranded_SAVEVAR_2.cpp
//
// Fakes: CSaveLoad::WriteSerialiseLog, GAME_ASSERTMSG, LbFile_Write, __register_global_object. Defined here: gSaveLoad, gNeighbourSteps.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CPeep_SAVEVAR_2.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TbError.h"
#include "ThingReceivers.h"

using namespace calllog;

CSaveLoad gSaveLoad;
CNeighbourStep gNeighbourSteps[9];

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int64_t>(state)}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("Assert", {condition}, {format}); }

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    // The bytes written (the file's) as a big-endian word, the PowerPC side's reading (v5L15: the host no longer reverses the value).
    const uint8_t* p = static_cast<const uint8_t*>(buffer);
    uint32_t word = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
    ev("LbFile_Write", {lab(file), static_cast<int64_t>(word), static_cast<int64_t>(length)});
    *transferred = static_cast<uint32_t>(ret("wn"));
    return TbError(static_cast<int32_t>(ret("wr")));
}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {static_cast<int64_t>(reinterpret_cast<char*>(object) - reinterpret_cast<char*>(gNeighbourSteps)),
                    destructor == static_cast<void*>(gPeepMapDeltaDtorVector) ? 1 : 0,
                    static_cast<int64_t>(reinterpret_cast<char*>(node) - reinterpret_cast<char*>(gPeepSinitNodes))});
}

extern "C" {

uint64_t tpw_sv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"zero", &gPeepTbErrorZero}, {"saveload", &gSaveLoad}, {"steps", gNeighbourSteps},
        {"nodes", gPeepSinitNodes},  {"w498", &gPeepC498},     {"w49c", &gPeepC49C},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CNeighbourStep));
    out[n++] = static_cast<int64_t>(sizeof(gPeepSinitNodes));
    return n;
}

uint32_t tpw_sv_save(uint64_t file, uint64_t value, uint64_t name, int32_t noLog) {
    return static_cast<uint32_t>(SAVEVAR<ESP_Sprites>(reinterpret_cast<void*>(file), reinterpret_cast<ESP_Sprites*>(value),
                                                      reinterpret_cast<const char*>(name), noLog));
}

void tpw_sv_sinit() { __sinit_Peep_cpp(); }

}  // extern "C"
