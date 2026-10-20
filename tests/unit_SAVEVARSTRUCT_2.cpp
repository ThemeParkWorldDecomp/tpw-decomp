// Test library for SAVEVARSTRUCT<AdvisorHistory>, CAdvisorScoring::LastMinuteCheck and CAdvisorRequest::Serialise (unit v5937;
// libtpw_unit_SAVEVARSTRUCT_2.dylib, tests/test_SAVEVARSTRUCT_2.py), main app:
//   src/Serialise_basic_SAVEVARSTRUCT_2.cpp
//
// The callees outside the unit are recording fakes: names are hashed (FNV-1a) into the event so that a wrong string shows. The globals of
// other files (gSaveLoad, the message table) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Serialise.h"

// First, before TimeSig.h (through AdvisorQueue.h) instantiates them: an explicit specialization must precede any instantiation.
namespace {
uintptr_t gBase = 0;   // the request being serialised: the fakes log the value's offset in it
int64_t FnvEarly(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}
}  // namespace
template<> int32_t SAVEVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog) {
    calllog::ev("SAVEVAR_ulong", {calllog::lab(file), static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gBase), noLog, FnvEarly(name)});
    return static_cast<int32_t>(calllog::ret("ret"));
}
template<> int32_t LOADVAR<MacULong>(void* file, MacULong* value, const char* name, int32_t noLog) {
    calllog::ev("LOADVAR_ulong", {calllog::lab(file), static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gBase), noLog, FnvEarly(name)});
    return static_cast<int32_t>(calllog::ret("ret"));
}

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "Serialise.h"
#include "TbError.h"

using namespace calllog;

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingId;

CSaveLoad gSaveLoad;
AdvisorMessageRecord gAdvisorMessageBlock[kAdvisorMessageCount + 2];

namespace {

int64_t Fnv(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h;
}

uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }


}  // namespace

// ---- CSaveLoad / Debug / LbFile fakes
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, Fnv(name), static_cast<int32_t>(state)});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition, Fnv(format)}); }

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t value = va_arg(args, int);
    va_end(args);
    ev("CTRACE", {level, value, Fnv(format)});
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    const uint8_t* p = static_cast<const uint8_t*>(buffer);
    ev("LbFile_Write", {lab(file), length, Be32(p), Be32(p + 4), uint32_t(p[8]) << 24, Be32(p + 12)});
    int32_t code = static_cast<int32_t>(ret("wr"));
    *transferred = static_cast<uint32_t>(ret("wn"));
    return TbError(code);
}

// ---- scoring fakes
bool CAdvisorScoring::IsThisValidInTheCurrentMode(AdvisorMessage message) const {
    ev("IsValid", {lab(this), message, mWord4, mWord8});
    return ret("valid") != 0;
}

int32_t CAdvisorScoring::_ADVISOR_NO_SCORE() {   // the target record's scorer
    ev("Scorer", {lab(this), 1, mWord4, mWord8});
    mWord4 = static_cast<uint16_t>(ret("w4"));
    mWord8 = static_cast<uint32_t>(ret("w8"));
    return static_cast<int32_t>(ret("score"));
}

int32_t CAdvisorScoring::_ADVISOR_WELCOME() {   // any other record's scorer
    ev("Scorer", {lab(this), 0, mWord4, mWord8});
    mWord4 = static_cast<uint16_t>(ret("w4"));
    mWord8 = static_cast<uint32_t>(ret("w8"));
    return static_cast<int32_t>(ret("score"));
}

// ---- the save templates of the request
#define FAKE(KIND, NAME, TYPE)                                                              \
    template<> int32_t KIND<TYPE>(void* file, TYPE* value, const char* name, int32_t noLog) { \
        ev(NAME, {lab(file), static_cast<int64_t>(reinterpret_cast<uintptr_t>(value) - gBase), noLog, Fnv(name)}); \
        return static_cast<int32_t>(ret("ret"));                                             \
    }
FAKE(SAVEVARSTRUCT, "SAVEVARSTRUCT_thing", ThingId)
FAKE(LOADVARSTRUCT, "LOADVARSTRUCT_thing", ThingId)
FAKE(SAVEVAR, "SAVEVAR_msg", AdvisorMessage)
FAKE(LOADVAR, "LOADVAR_msg", AdvisorMessage)
FAKE(SAVEVAR, "SAVEVAR_int", int32_t)
FAKE(LOADVAR, "LOADVAR_int", int32_t)
FAKE(SAVEVAR, "SAVEVAR_bool", bool)
FAKE(LOADVAR, "LOADVAR_bool", bool)

extern "C" {

uint64_t tpw_adv_saveload() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSaveLoad)); }

uint64_t tpw_adv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_adv_layout(int64_t* out) {
    int32_t n = 0;
    CAdvisorScoring* s = static_cast<CAdvisorScoring*>(tpw::ArenaAllocZeroed(sizeof(CAdvisorScoring)));
    out[n++] = reinterpret_cast<char*>(&s->mWord4) - reinterpret_cast<char*>(s);
    out[n++] = reinterpret_cast<char*>(&s->mWord8) - reinterpret_cast<char*>(s);
    out[n++] = static_cast<int64_t>(offsetof(AdvisorMessageRecord, mMessage));
    out[n++] = static_cast<int64_t>(sizeof(AdvisorMessageState));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorRequest));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorRequest, mMessage));
    out[n++] = static_cast<int64_t>(offsetof(CAdvisorRequest, mScoreOverridden));
    return n;
}

void tpw_adv_setmsg(int32_t index, int32_t value) { gAdvisorMessages[index].mMessage = value; }

uint32_t tpw_adv_serstruct(uint64_t file, uint64_t state, uint64_t name, int32_t noLog) {
    return static_cast<uint32_t>(SAVEVARSTRUCT<AdvisorMessageState>(reinterpret_cast<void*>(file),
                                                                    reinterpret_cast<AdvisorMessageState*>(state),
                                                                    reinterpret_cast<const char*>(name), noLog));
}

uint32_t tpw_adv_lmc(uint64_t object, int32_t message, uint64_t thingId, uint64_t value) {
    for (int32_t i = -1; i < kAdvisorMessageCount; i++)
        gAdvisorMessages[i].mScorer = (i == message) ? &CAdvisorScoring::_ADVISOR_NO_SCORE : &CAdvisorScoring::_ADVISOR_WELCOME;
    CAdvisorScoring* self = reinterpret_cast<CAdvisorScoring*>(object);
    return static_cast<uint32_t>(self->LastMinuteCheck(static_cast<AdvisorMessage>(message),
                                                      *reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>*>(thingId),
                                                      *reinterpret_cast<uint32_t*>(value)));
}

uint32_t tpw_adv_ser(uint64_t object, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    gBase = static_cast<uintptr_t>(object);
    return static_cast<uint32_t>(
        reinterpret_cast<CAdvisorRequest*>(object)->Serialise(reinterpret_cast<void*>(file), save, version, unused));
}

}  // extern "C"
