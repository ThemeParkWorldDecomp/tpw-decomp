// Test library for CSaveLoad::SaveModules (unit v5628; libtpw_unit_CSaveLoad_SaveModules.dylib, tests/test_CSaveLoad_SaveModules.py), main app:
//   src/SaveLoad_CSaveLoad_SaveModules.cpp   CSaveLoad::SaveModules and gSaveModulesTagTestWords
//
// Every callee is a fake that records the arguments the real one reads (pointers as labels, the tag write's four bytes as a big-endian
// word) and answers from the test's queue; the savers that take the file by reference may hand back the second test file. TheScoring
// is the shared static the original inlines: its fake builds it behind a guard byte as the original's code does (the constructor and
// the destructor registration as events). The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "AdvisorScoring.h"
#include "CFixedTime.h"
#include "CMessageCentre.h"
#include "CVariableTime.h"
#include "Cheat.h"
#include "Clock.h"
#include "Debug.h"
#include "GameSys.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TPCamera.h"
#include "TPSound.h"
#include "UIInterface.h"
#include "VanillaTime.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
// gClock's member time is built at load; the fakes never read it, so its constructor is empty here (the real one is src/CFixedTime.cpp's).
CFixedTime::CFixedTime() {}
CVariableTime::CVariableTime() {}
CActionRecorder gActionRecorder;
CSaveLoad gSaveLoad;
CWorld* gGameWorld = nullptr;
CMessageCentre* gMessageCentre = nullptr;
CClock gClock;
CVanillaTime gVanillaTime;
UIInterface gUIInterface;
int8_t gGameWinCheatGuard = 0;
alignas(4) uint8_t gGameWinCheat[12];

namespace {

alignas(8) uint8_t gTestFile1[16];
alignas(8) uint8_t gTestFile2[16];
alignas(8) uint8_t gTestWorld[16];
alignas(8) uint8_t gTestMessageCentre[sizeof(CMessageCentre)];
alignas(8) uint8_t gTestScoring[sizeof(CAdvisorScoring)];
int8_t gTestScoringGuard = 0;          // the Mac's 0x102b4100
alignas(8) uint8_t gTestScoringDtor[8];   // stands for the destructor's TVector 0x102657e0
alignas(8) uint8_t gTestScoringNode[8];   // stands for the destructor chain node 0x1037ff04

int64_t S(int64_t value) { return static_cast<int32_t>(value); }

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
int32_t LbFile_GetPosition(void* file) {
    ev("LbFile_GetPosition", {lab(file)});
    return static_cast<int32_t>(ret("LbFile_GetPosition"));
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    const uint8_t* b = static_cast<const uint8_t*>(buffer);
    const int64_t word = (static_cast<int64_t>(b[0]) << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    ev("LbFile_Write", {lab(file), word, S(length), lab(transferred)});
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

void CSaveLoad::OpenLogfile(int32_t state) { ev("CSaveLoad::OpenLogfile", {lab(this), state}); }
void CSaveLoad::CloseLogfile() { ev("CSaveLoad::CloseLogfile", {lab(this)}); }

int32_t CActionRecorder::Serialise(void* file, int32_t saving, int32_t version, uint32_t unused) {
    ev("CActionRecorder::Serialise", {lab(this), lab(file), saving, version, static_cast<int64_t>(unused)});
    return static_cast<int32_t>(ret("CActionRecorder::Serialise"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {S(level), value}, {format});
}

void DEBUG_MESSAGE(const char* message, uint32_t flags) { ev("DEBUG_MESSAGE", {static_cast<int64_t>(flags)}, {message}); }

int32_t CWorld::Save(void* file) {
    ev("CWorld::Save", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("CWorld::Save"));
}

void SCR_SaveAllScripts(void* file) { ev("SCR_SaveAllScripts", {lab(file)}); }

int32_t PAR_SaveStatus(void* file, int32_t withParticles) {
    ev("PAR_SaveStatus", {lab(file), withParticles});
    return static_cast<int32_t>(ret("PAR_SaveStatus"));
}

CSubscriptionHeader::CSubscriptionHeader() { ev("CSubscriptionHeader", {newlab(this)}); }
CSubscriptionHeader::~CSubscriptionHeader() { ev("~CSubscriptionHeader", {lab(this)}); }

int32_t CMessageCentre::Save(void*& file) const {
    ev("CMessageCentre::Save", {lab(this), lab(file)});
    if (ret("CMessageCentre::Save.file") != 0) file = gTestFile2;
    return static_cast<int32_t>(ret("CMessageCentre::Save"));
}

int32_t CClock::Save(void* file) {
    ev("CClock::Save", {lab(this), lab(file)});
    return static_cast<int32_t>(ret("CClock::Save"));
}

bool CVanillaTime::Save(void* file) {
    ev("CVanillaTime::Save", {lab(this), lab(file)});
    return ret("CVanillaTime::Save") != 0;
}

int32_t GAMESYSTEM_Save(void* file) {
    ev("GAMESYSTEM_Save", {lab(file)});
    return static_cast<int32_t>(ret("GAMESYSTEM_Save"));
}

int32_t SaveRides(void* file) {
    ev("SaveRides", {lab(file)});
    return static_cast<int32_t>(ret("SaveRides"));
}

int32_t BMP_Save(void* file) {
    ev("BMP_Save", {lab(file)});
    return static_cast<int32_t>(ret("BMP_Save"));
}

int32_t FLY_Save(void* file) {
    ev("FLY_Save", {lab(file)});
    return static_cast<int32_t>(ret("FLY_Save"));
}

int32_t RSSE_Save(void* file) {
    ev("RSSE_Save", {lab(file)});
    return static_cast<int32_t>(ret("RSSE_Save"));
}

int32_t SaveCamera(void* file) {
    ev("SaveCamera", {lab(file)});
    return static_cast<int32_t>(ret("SaveCamera"));
}

int32_t SaveCoasters(void* file) {
    ev("SaveCoasters", {lab(file)});
    return static_cast<int32_t>(ret("SaveCoasters"));
}

void ADV_Save(void* file) { ev("ADV_Save", {lab(file)}); }
void SOUND_SaveStatus(void* file) { ev("SOUND_SaveStatus", {lab(file)}); }

CCheat::CCheat() { ev("CCheat::CCheat", {lab(this)}); }

int32_t CCheat::Save(void*& file) const {
    ev("CCheat::Save", {lab(this), lab(file)});
    if (ret("CCheat::Save.file") != 0) file = gTestFile2;
    return static_cast<int32_t>(ret("CCheat::Save"));
}

// The original's inline code for the static: guard byte tested as a signed byte; constructor; __register_global_object(object,
// destructor, node); guard set to 1.
CAdvisorScoring& TheScoring() {
    if (gTestScoringGuard == 0) {
        ev("CAdvisorScoring::CAdvisorScoring", {lab(gTestScoring)});
        ev("__register_global_object", {lab(gTestScoring), lab(gTestScoringDtor), lab(gTestScoringNode)});
        gTestScoringGuard = 1;
    }
    return *reinterpret_cast<CAdvisorScoring*>(gTestScoring);
}

int32_t CAdvisorScoring::Save(void*& file) const {
    ev("CAdvisorScoring::Save", {lab(this), lab(file)});
    if (ret("CAdvisorScoring::Save.file") != 0) file = gTestFile2;
    return static_cast<int32_t>(ret("CAdvisorScoring::Save"));
}

void UIInterface::Save(void* file) { ev("UIInterface::Save", {lab(this), lab(file)}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"file1", gTestFile1},
        {"file2", gTestFile2},
        {"gSaveLoad", &gSaveLoad},
        {"gActionRecorder", &gActionRecorder},
        {"world", gTestWorld},
        {"mc", gTestMessageCentre},
        {"gClock", &gClock},
        {"gVanillaTime", &gVanillaTime},
        {"gGameWinCheat", gGameWinCheat},
        {"scoring", gTestScoring},
        {"gUIInterface", &gUIInterface},
        {"node", gTestScoringNode},
        {"dtor", gTestScoringDtor},
        {"gGameWorld", &gGameWorld},
        {"gMessageCentre", &gMessageCentre},
        {"cheatGuard", &gGameWinCheatGuard},
        {"scoringGuard", &gTestScoringGuard},
        {"tagWords", gSaveModulesTagTestWords},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable).
uint64_t tpw_sm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The sizes the module relies on (HOST_LAYOUT).
int32_t tpw_sm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    out[n++] = static_cast<int64_t>(sizeof(CSubscriptionHeader));
    out[n++] = static_cast<int64_t>(sizeof(CCheat));
    return n;
}

// ---- the function
int32_t tpw_sm_save_modules(uint64_t file, int32_t type) {
    return gSaveLoad.SaveModules(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), static_cast<CSaveLoad::LoadType>(type));
}

}  // extern "C"
