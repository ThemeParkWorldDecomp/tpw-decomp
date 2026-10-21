// Test library for LOADVAR and SAVEVAR of AdvisorMessage (unit v5L05; libtpw_unit_AdvisorMessageVar.dylib,
// tests/test_AdvisorMessageVar.py), main app: src/game/advisor/Advisor.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues: WriteSerialiseLog,
// GAME_ASSERTMSG, LbFile_Read, LbFile_Write. gSaveLoad is defined here.
#include <cstddef>
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

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), static_cast<int64_t>(length)});
    int32_t status = static_cast<int32_t>(ret("readerr"));
    uint32_t count = static_cast<uint32_t>(ret("readn"));
    uint32_t word = static_cast<uint32_t>(ret("rd0"));
    std::memcpy(buffer, &word, length < 4 ? length : 4);
    *transferred = count;
    return TbError(status);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    uint32_t word = 0;
    std::memcpy(&word, buffer, length < 4 ? length : 4);
    ev("LbFile_Write", {lab(file), static_cast<int64_t>(length), static_cast<int32_t>(word)});
    int32_t status = static_cast<int32_t>(ret("wrerr"));
    *transferred = static_cast<uint32_t>(ret("wrn"));
    return TbError(status);
}

extern "C" {

uint64_t tpw_amv_addr(const char* name) {
    if (std::strcmp(name, "gSaveLoad") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSaveLoad));
    return 0;
}

uint64_t tpw_amv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// op 0 LOADVAR<AdvisorMessage>, 1 SAVEVAR<AdvisorMessage>
uint32_t tpw_amv_call(int32_t op, uint64_t a, uint64_t b, uint64_t c, int32_t d) {
    void* file = reinterpret_cast<void*>(static_cast<uintptr_t>(a));
    AdvisorMessage* value = reinterpret_cast<AdvisorMessage*>(static_cast<uintptr_t>(b));
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(c));
    if (op == 0) return static_cast<uint32_t>(LOADVAR<AdvisorMessage>(file, value, name, d));
    return static_cast<uint32_t>(SAVEVAR<AdvisorMessage>(file, value, name, d));
}

}  // extern "C"
