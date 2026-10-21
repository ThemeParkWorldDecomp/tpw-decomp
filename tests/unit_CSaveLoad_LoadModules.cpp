// Test library for CSaveLoad::LoadModules (unit v5206; libtpw_unit_CSaveLoad_LoadModules.dylib, tests/test_CSaveLoad_LoadModules.py), main app:
//   src/game/save/SaveLoad.cpp   CSaveLoad::LoadModules
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue. The globals of
// other files the function reads or writes are defined here (zero at start like the Mac's bss). The message centre is made by
// `new CMessageCentre` (as the other sources do): the Mac's operator new and __construct_array are faked on the PowerPC side so that
// they log the 29 header constructions, which the fake CSubscriptionHeader constructor logs here. TheScoring() is the shared
// singleton of src/game/advisor/Advisor.cpp; its fake here does what the Mac code inlined in LoadModules does (constructor and registration
// behind the guard byte).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"

#include "ActionRecorder.h"
#include "AdvisorScoring.h"
#include "Cheat.h"
#include "CMessageCentre.h"
#include "Clock.h"
#include "Debug.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "UIInterface.h"
#include "VanillaTime.h"
#include "World.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files, defined here
CSaveLoad gSaveLoad;
// (raw storage under the program's symbol names: the objects are only addresses here, and their constructors live in files this
// library does not compile)
alignas(16) uint8_t gActionRecorderStorage[sizeof(CActionRecorder)] __asm__("_gActionRecorder");
alignas(16) uint8_t gClockStorage[sizeof(CClock)] __asm__("_gClock");
alignas(16) uint8_t gVanillaTimeStorage[sizeof(CVanillaTime)] __asm__("_gVanillaTime");
alignas(16) uint8_t gUIInterfaceStorage[sizeof(UIInterface)] __asm__("_gUIInterface");
CWorld* gGameWorld;
CMessageCentre* gMessageCentre;
alignas(4) uint8_t gGameWinCheat[12];
int8_t gGameWinCheatGuard;

namespace {
int8_t gScoringGuard;
alignas(16) uint8_t gScoringStorage[sizeof(CAdvisorScoring)];
}  // namespace

// ---- the fakes (event names and arguments as the PowerPC recorders in the test module)
int32_t LbFile_GetPosition(void* file) {
    ev("GetPosition", {lab(file)});
    return static_cast<int32_t>(ret("GetPosition"));
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), static_cast<int32_t>(length)});
    uint32_t tag = static_cast<uint32_t>(ret("ReadTag"));
    uint32_t count = static_cast<uint32_t>(ret("ReadCount"));
    std::memcpy(buffer, &tag, 4);   // the file's bytes: the tag little endian
    *transferred = count;
    return TbError(0);
}

void CSaveLoad::OpenLogfile(int32_t state) { ev("OpenLogfile", {lab(this), state}); }
void CSaveLoad::CloseLogfile() { ev("CloseLogfile", {lab(this)}); }

int32_t CActionRecorder::Serialise(void* file, int32_t saving, int32_t version, uint32_t unused) {
    ev("ActionSerialise", {lab(this), lab(file), saving, version, static_cast<int64_t>(unused)});
    return static_cast<int32_t>(ret("ActionSerialise"));
}

void DEBUG_MESSAGE(const char* message, uint32_t flags) { ev("DEBUG_MESSAGE", {static_cast<int64_t>(flags)}, {message}); }

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {static_cast<int64_t>(level), value}, {format});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void SamsUtilities::UDebug(const char* format, ...) {
    // (the PowerPC stub logs an extra event unless the text is "read failed")
    ev("UDebug", {});
    if (std::strcmp(format, "read failed") != 0) ev("UDebug_other_text", {});
}

int32_t CWorld::Load(void* file) {
    ev("CWorld_Load", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("CWorld_Load"));
}

int32_t SCR_LoadAllScripts(void* file) {
    ev("SCR_LoadAllScripts", {lab(file)});
    return static_cast<int32_t>(ret("SCR_LoadAllScripts"));
}

int32_t PAR_LoadStatus(void* file) {
    ev("PAR_LoadStatus", {lab(file)});
    return static_cast<int32_t>(ret("PAR_LoadStatus"));
}

void PAR_KillAndHideAll2DObjects() { ev("PAR_KillAndHide", {}); }

CSubscriptionHeader::CSubscriptionHeader() { ev("SubscriptionHeader", {lab(this)}); }
CSubscriptionHeader::~CSubscriptionHeader() {}

int32_t CMessageCentre::Load(void*& file) {
    ev("CMessageCentre_Load", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("CMessageCentre_Load"));
}

int32_t CClock::Load(void* file) {
    ev("CClock_Load", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("CClock_Load"));
}

bool CVanillaTime::Load(void* file) {
    ev("CVanillaTime_Load", {lab(this), lab(file)});
    return ret("CVanillaTime_Load") != 0;
}

int32_t GAMESYSTEM_Load(void* file) {
    ev("GAMESYSTEM_Load", {lab(file)});
    return static_cast<int32_t>(ret("GAMESYSTEM_Load"));
}

int32_t LoadRides(void* file) {
    ev("LoadRides", {lab(file)});
    return static_cast<int32_t>(ret("LoadRides"));
}

int32_t BMP_Load(void* file) {
    ev("BMP_Load", {lab(file)});
    return static_cast<int32_t>(ret("BMP_Load"));
}

int32_t FLY_Load(void* file) {
    ev("FLY_Load", {lab(file)});
    return static_cast<int32_t>(ret("FLY_Load"));
}

int32_t RSSE_Load(void* file) {
    ev("RSSE_Load", {lab(file)});
    return static_cast<int32_t>(ret("RSSE_Load"));
}

int32_t LoadCamera(void* file) {
    ev("LoadCamera", {lab(file)});
    return static_cast<int32_t>(ret("LoadCamera"));
}

int32_t LoadCoasters(void* file) {
    ev("LoadCoasters", {lab(file)});
    return static_cast<int32_t>(ret("LoadCoasters"));
}

void ADV_Load(void* file) { ev("ADV_Load", {lab(file)}); }

void SOUND_LoadStatus(void* file) { ev("SOUND_LoadStatus", {lab(file)}); }

CCheat::CCheat() { ev("CCheat_ctor", {lab(this)}); }

int32_t CCheat::Load(void*& file) {
    ev("CCheat_Load", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("CCheat_Load"));
}

int32_t CAdvisorScoring::Load(void*& file) {
    ev("CAdvisorScoring_Load", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("CAdvisorScoring_Load"));
}

CAdvisorScoring& TheScoring() {
    CAdvisorScoring* scoring = reinterpret_cast<CAdvisorScoring*>(gScoringStorage);
    if (gScoringGuard == 0) {
        // (the constructor is only logged: CAdvisorScoring::CAdvisorScoring is in a file this library does not compile)
        ev("CAdvisorScoring_ctor", {lab(scoring)});
        // the Mac's __register_global_object(object, destructor TVector 0x102657e0, node 0x1037ff04)
        ev("register_global_object", {lab(scoring), 0x102657e0, 0x1037ff04});
        gScoringGuard = 1;
    }
    return *scoring;
}

void UIInterface::Load(void* file) { ev("UIInterface_Load", {lab(this), lab(file)}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_lm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gSaveLoad", &gSaveLoad},
        {"gActionRecorder", gActionRecorderStorage},
        {"gClock", gClockStorage},
        {"gVanillaTime", gVanillaTimeStorage},
        {"gUIInterface", gUIInterfaceStorage},
        {"gGameWorld", &gGameWorld},
        {"gMessageCentre", &gMessageCentre},
        {"gGameWinCheat", gGameWinCheat},
        {"gGameWinCheatGuard", &gGameWinCheatGuard},
        {"gScoringStorage", gScoringStorage},
        {"gScoringGuard", &gScoringGuard},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_lm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    out[n++] = static_cast<int64_t>(sizeof(CCheat));
    return n;
}

// CSaveLoad::LoadModules on the global gSaveLoad; `file` is the address of the file object, `type` the load type.
int32_t tpw_lm_run(uint64_t file, int32_t type) {
    return gSaveLoad.LoadModules(*reinterpret_cast<bfl::FileSystem::CFile*>(static_cast<uintptr_t>(file)),
                                 static_cast<CSaveLoad::LoadType>(type));
}

}  // extern "C"
