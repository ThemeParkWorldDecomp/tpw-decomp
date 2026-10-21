// Fakes and the entry point for the game clock unit (src/game/FixableTime.cpp, src/game/PausableTime.cpp, src/game/VariableTime.cpp, src/game/save/SaveLoad.cpp, src/game/ui/UI_Base.cpp, with the two
// constructors and CClock). The only callees are the bullfrog clock, the file calls, the
// debug print and the platform timer; each records its arguments and answers from the
// queues the test fills (calllog.h). Nothing here reads the real clock.
#include <cstring>

#include "CFixableTime.h"
#include "CFixedTime.h"
#include "CPausableFixedTime.h"
#include "CPausableTime.h"
#include "CSaveableFixedTime.h"
#include "CSaveableTime.h"
#include "CVariableTime.h"
#include "Clock.h"
#include "LbFile.h"
#include "LbTime.h"
#include "UI_Base.h"
#include "calllog.h"
#include "platform/samsutils.h"

using namespace calllog;

// The bullfrog clock: the next reading of the queue (the last one repeats).
uint32_t LbTime_GetClock() {
    const uint32_t reading = static_cast<uint32_t>(ret("LbTime_GetClock"));
    ev("LbTime_GetClock", {reading});
    return reading;
}

// The platform timer, a 64-bit count of microseconds.
uint64_t SamsUtilities::UTimer::GetAbsolute() {
    const uint64_t microseconds = static_cast<uint64_t>(ret("GetAbsolute"));
    ev("GetAbsolute", {static_cast<int64_t>(microseconds)});
    return microseconds;
}

// Four bytes as the number they make in a little-endian file.
static uint32_t FileWord(const void* bytes) {
    const uint8_t* b = static_cast<const uint8_t*>(bytes);
    return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 | static_cast<uint32_t>(b[2]) << 16 |
           static_cast<uint32_t>(b[3]) << 24;
}

// A write: records the file, which label the buffer is (the object's saved time word is
// labelled 2 or 4), the length and the four bytes; answers the queued count and result.
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), lab(buffer), length, FileWord(buffer)});
    *transferred = static_cast<uint32_t>(ret("LbFile_Write:count"));
    return TbError(static_cast<int32_t>(ret("LbFile_Write:result")));
}

// A read: stores the first `written` bytes of the queued word (little-endian) into the
// buffer, whatever the count it then reports.
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), lab(buffer), length});
    const uint32_t word = static_cast<uint32_t>(ret("LbFile_Read:data"));
    const uint32_t written = static_cast<uint32_t>(ret("LbFile_Read:written"));
    for (uint32_t i = 0; i < written && i < 4; ++i) static_cast<uint8_t*>(buffer)[i] = static_cast<uint8_t>(word >> (8 * i));
    *transferred = static_cast<uint32_t>(ret("LbFile_Read:count"));
    return TbError(static_cast<int32_t>(ret("LbFile_Read:result")));
}

// The debug print: the format text is all the clock ever gives it.
void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }

// 0x10187994, the base class of the tick functions (not part of this unit); its vtable
// and type information are what the derived class in src/game/FixableTime.cpp, src/game/PausableTime.cpp, src/game/VariableTime.cpp, src/game/save/SaveLoad.cpp, src/game/ui/UI_Base.cpp needs.
uint32_t clockTickFunction::tickyticky() { return 0; }

namespace {

// One operation: the object it runs on, an integer, a rate, and the answer.
struct Operation {
    const char* name;
    uint32_t (*run)(void* object, uint32_t argument, double rate, double* rateOut, void* file);
};

CFixableTime* Fixable(void* object) { return static_cast<CFixableTime*>(static_cast<CSaveableTime*>(object)); }

const Operation kOperations[] = {
    {"CFixedTime::ReadFixedTime", [](void* o, uint32_t, double, double*, void*) { return static_cast<CFixedTime*>(o)->ReadFixedTime(); }},
    {"CVariableTime::SetTimeRate", [](void* o, uint32_t, double r, double*, void*) { static_cast<CVariableTime*>(o)->SetTimeRate(r); return 0u; }},
    {"CVariableTime::IncreaseTimeRate", [](void* o, uint32_t, double, double*, void*) { static_cast<CVariableTime*>(o)->IncreaseTimeRate(); return 0u; }},
    {"CVariableTime::DecreaseTimeRate", [](void* o, uint32_t, double, double*, void*) { static_cast<CVariableTime*>(o)->DecreaseTimeRate(); return 0u; }},
    {"CVariableTime::GetTimeRate", [](void* o, uint32_t, double, double* out, void*) { *out = static_cast<CVariableTime*>(o)->GetTimeRate(); return 0u; }},
    {"CVariableTime::ReadVariableTime", [](void* o, uint32_t, double, double*, void*) { return static_cast<CVariableTime*>(o)->ReadVariableTime(); }},
    {"CPausableTime::Pause", [](void* o, uint32_t, double, double*, void*) { static_cast<CPausableTime*>(o)->Pause(); return 0u; }},
    {"CPausableTime::UnPause", [](void* o, uint32_t, double, double*, void*) { static_cast<CPausableTime*>(o)->UnPause(); return 0u; }},
    {"CPausableTime::TogglePause", [](void* o, uint32_t, double, double*, void*) { static_cast<CPausableTime*>(o)->TogglePause(); return 0u; }},
    {"CPausableTime::GetPauseFlag", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(static_cast<CPausableTime*>(o)->GetPauseFlag()); }},
    {"CPausableTime::ReadPausableTime", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(static_cast<CPausableTime*>(o)->ReadPausableTime()); }},
    {"CPausableFixedTime::Pause", [](void* o, uint32_t, double, double*, void*) { static_cast<CPausableFixedTime*>(o)->Pause(); return 0u; }},
    {"CPausableFixedTime::UnPause", [](void* o, uint32_t, double, double*, void*) { static_cast<CPausableFixedTime*>(o)->UnPause(); return 0u; }},
    {"CPausableFixedTime::TogglePause", [](void* o, uint32_t, double, double*, void*) { static_cast<CPausableFixedTime*>(o)->TogglePause(); return 0u; }},
    {"CPausableFixedTime::ReadPausableFixedTime", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(static_cast<CPausableFixedTime*>(o)->ReadPausableFixedTime()); }},
    {"CFixableTime::UseFixedTime", [](void* o, uint32_t a, double, double*, void*) { Fixable(o)->UseFixedTime(a); return 0u; }},
    {"CFixableTime::CancelFixedTime", [](void* o, uint32_t, double, double*, void*) { Fixable(o)->CancelFixedTime(); return 0u; }},
    {"CFixableTime::NextFrame", [](void* o, uint32_t, double, double*, void*) { Fixable(o)->NextFrame(); return 0u; }},
    {"CFixableTime::ReadFixableTime", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(Fixable(o)->ReadFixableTime()); }},
    {"CSaveableTime::Save", [](void* o, uint32_t, double, double*, void* f) { return static_cast<uint32_t>(static_cast<CSaveableTime*>(o)->Save(f)); }},
    {"CSaveableTime::Load", [](void* o, uint32_t, double, double*, void* f) { return static_cast<uint32_t>(static_cast<CSaveableTime*>(o)->Load(f)); }},
    {"CSaveableTime::ReadSaveableTime", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(static_cast<CSaveableTime*>(o)->ReadSaveableTime()); }},
    {"CSaveableTime::AdjustTime", [](void* o, uint32_t, double, double*, void*) { static_cast<CSaveableTime*>(o)->AdjustTime(); return 0u; }},
    {"CSaveableFixedTime::Save", [](void* o, uint32_t, double, double*, void* f) { return static_cast<uint32_t>(static_cast<CSaveableFixedTime*>(o)->Save(f)); }},
    {"CSaveableFixedTime::Load", [](void* o, uint32_t, double, double*, void* f) { return static_cast<uint32_t>(static_cast<CSaveableFixedTime*>(o)->Load(f)); }},
    {"CSaveableFixedTime::ReadSaveableFixedTime", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(static_cast<CSaveableFixedTime*>(o)->ReadSaveableFixedTime()); }},
    {"CSaveableFixedTime::AdjustTime", [](void* o, uint32_t, double, double*, void*) { static_cast<CSaveableFixedTime*>(o)->AdjustTime(); return 0u; }},
    {"timeGetTimeClockTickFunction::tickyticky", [](void*, uint32_t, double, double*, void*) {
         timeGetTimeClockTickFunction tick;
         return tick.timeGetTimeClockTickFunction::tickyticky();
     }},
    // The clock the game uses (src/game/park/Clock.cpp), on top of the real time classes.
    {"CClock::Read", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(static_cast<CClock*>(o)->Read()); }},
    {"CClock::ReadFixed", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(static_cast<CClock*>(o)->ReadFixed()); }},
    {"CClock::Pause", [](void* o, uint32_t, double, double*, void*) { static_cast<CClock*>(o)->Pause(); return 0u; }},
    {"CClock::UnPause", [](void* o, uint32_t, double, double*, void*) { static_cast<CClock*>(o)->UnPause(); return 0u; }},
    {"CClock::TogglePause", [](void* o, uint32_t, double, double*, void*) { static_cast<CClock*>(o)->TogglePause(); return 0u; }},
    {"CClock::GetPauseFlag", [](void* o, uint32_t, double, double*, void*) { return static_cast<uint32_t>(static_cast<CClock*>(o)->GetPauseFlag()); }},
    {"CClock::Save", [](void* o, uint32_t, double, double*, void* f) { return static_cast<uint32_t>(static_cast<CClock*>(o)->Save(f)); }},
    {"CClock::Load", [](void* o, uint32_t, double, double*, void* f) { return static_cast<uint32_t>(static_cast<CClock*>(o)->Load(f)); }},
    {"CClock::PreSave", [](void* o, uint32_t, double, double*, void*) { static_cast<CClock*>(o)->PreSave(); return 0u; }},
    {"CClock::PostLoad", [](void* o, uint32_t, double, double*, void*) { static_cast<CClock*>(o)->PostLoad(); return 0u; }},
};

}  // namespace

extern "C" {

// Runs the named operation on the object; returns r3's value and, for GetTimeRate, the
// double through rateOut. An unknown name answers 0xdeadbeef.
uint32_t tpw_gc_run(const char* name, void* object, uint32_t argument, double rate, void* file, double* rateOut) {
    for (const Operation& operation : kOperations)
        if (std::strcmp(operation.name, name) == 0) return operation.run(object, argument, rate, rateOut, file);
    return 0xdeadbeefu;
}

// Records where the script is, so the two logs can be compared event by event.
void tpw_gc_mark(int32_t index) { ev("op", {index}); }

// The sizes the test lays its objects out with.
uint32_t tpw_gc_size(int32_t what) {
    switch (what) {
    case 0: return sizeof(CSaveableTime);
    case 1: return sizeof(CSaveableFixedTime);
    case 2: return sizeof(CClock);
    default: return 0;
    }
}

}  // extern "C"
