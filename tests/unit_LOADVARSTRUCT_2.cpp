// Test library for LOADVARSTRUCT<AdvisorHistory> (unit v5649; libtpw_unit_LOADVARSTRUCT_2.dylib, tests/test_LOADVARSTRUCT_2.py), main app:
//   src/game/advisor/Advisor.cpp   LOADVARSTRUCT<AdvisorMessageState>
//
// The callees outside the unit (WriteSerialiseLog, LbFile_Read, GAME_ASSERTMSG) are recording fakes; gSaveLoad is defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorQueue.h"
#include "Serialise.h"

using namespace calllog;

CSaveLoad gSaveLoad;

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int64_t>(state)}, {name});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), static_cast<int64_t>(length)});
    uint8_t* out = static_cast<uint8_t*>(buffer);
    for (int i = 0; i < 4; ++i) {
        uint32_t w = static_cast<uint32_t>(ret(i == 0 ? "w0" : i == 1 ? "w1" : i == 2 ? "w2" : "w3"));
        out[i * 4] = w >> 24;
        out[i * 4 + 1] = w >> 16;
        out[i * 4 + 2] = w >> 8;
        out[i * 4 + 3] = w;
    }
    *transferred = static_cast<uint32_t>(ret("cnt"));
    return TbError(static_cast<int32_t>(ret("err")));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("assert", {condition}, {format});
}

extern "C" {

uint64_t tpw_lvs_addr(const char* name) {
    if (std::strcmp(name, "gSaveLoad") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSaveLoad));
    return 0;
}

uint64_t tpw_lvs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lvs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(AdvisorMessageState));
    out[n++] = static_cast<int64_t>(offsetof(AdvisorMessageState, mResponseIndex));
    out[n++] = static_cast<int64_t>(offsetof(AdvisorMessageState, mKillCount));
    return n;
}

int32_t tpw_lvs_load(uint64_t file, uint64_t value, uint64_t name, int32_t noLog) {
    return LOADVARSTRUCT<AdvisorMessageState>(reinterpret_cast<void*>(static_cast<uintptr_t>(file)),
                                              reinterpret_cast<AdvisorMessageState*>(static_cast<uintptr_t>(value)),
                                              reinterpret_cast<const char*>(static_cast<uintptr_t>(name)), noLog);
}

}  // extern "C"
