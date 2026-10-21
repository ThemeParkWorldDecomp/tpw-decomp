// Test library for GAMESYSTEM_Save and GAMESYSTEM_Load (unit v5151; libtpw_unit_GAMESYSTEM_Load.dylib, tests/test_GAMESYSTEM_Load.py), main app:
//   src/game/GameSys.cpp
//
// LbFile_Read, LbFile_Write and SamsUtilities::UDebug are recording fakes; the clock globals of GameSys.cpp are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "GameSys.h"
#include "LbFile.h"
#include "platform/samsutils.h"

using namespace calllog;

int32_t gGameSys_103bc178;
int32_t gTickCount;
int32_t gTickNow;
int32_t gLastTurnTime;
int32_t gLastFlyTime;
int32_t gTickTime;

namespace {
int gWrites = 0;
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("Read", {lab(file), static_cast<int64_t>(length)});
    uint32_t v = static_cast<uint32_t>(ret("RdV"));
    uint32_t n = static_cast<uint32_t>(ret("RdN"));
    std::memcpy(buffer, &v, 4);
    *transferred = n;
    return TbError(0);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    uint32_t v;
    std::memcpy(&v, buffer, 4);
    // the sixth write is an uninitialised stack word on the Mac: not compared
    if (gWrites++ == 5) v = 0;
    ev("Write", {lab(file), static_cast<int64_t>(length), static_cast<int64_t>(v)});
    *transferred = static_cast<uint32_t>(ret("WrN"));
    return TbError(0);
}

void SamsUtilities::UDebug(const char* text, ...) {
    ev("UDebug", {}, {text});
}

extern "C" {

uint64_t tpw_gsl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gGameSys_103bc178", &gGameSys_103bc178}, {"gTickCount", &gTickCount},       {"gTickNow", &gTickNow},
        {"gLastTurnTime", &gLastTurnTime},         {"gLastFlyTime", &gLastFlyTime},   {"gTickTime", &gTickTime},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gsl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_gsl_reset() { gWrites = 0; }

int32_t tpw_gsl_layout(int64_t*) { return 0; }

int32_t tpw_gsl_save(uint64_t file) { return GAMESYSTEM_Save(reinterpret_cast<void*>(file)); }
int32_t tpw_gsl_load(uint64_t file) { return GAMESYSTEM_Load(reinterpret_cast<void*>(file)); }

}  // extern "C"
