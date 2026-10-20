// Test library for the researcher (unit w188; libtpw_unit_Researcher.dylib, tests/test_Researcher.py), main app:
//   src/Researcher.cpp   CResearcher (both constructors, Init, Serialise, FinalDestroy, ReceiveMessage, UpdateTurn, ModelState, ChooseNextAction,
//                        ProcessPatrolling, ProcessResearching, ProcessIdle, DoSomeResearch), CRegionEffectCell::operator== and Serialise, and the
//                        two words of __sinit_Researcher_cpp
//
// The callees outside it are fakes that record the same events as the PowerPC stubs: the CStaff and CPeep members, the delayed script,
// CWorld::Register / Unregister / GetRand / GetResearchLab, the research lab's two calls, ESP_GetRandomSpritesFrom, the string table's FormatString,
// the serialise instances, CThing::GetThingId, memcmp (only while operator== runs), the trace and the assertion. The globals of other files are
// defined here: gWorld (pointing into a shifted tail block), gText and gMainBalance (raw storage) and gSoundBanks. __sinit_Researcher_cpp has no
// host function: the two words are constant initialised, so the wrapper reads them back.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Debug.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "TPSound.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

#ifdef __APPLE__
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
extern unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
#else
extern unsigned char gBalanceStorage[sizeof(CMainBalance)] __asm__("gMainBalance");
extern unsigned char gTextStorage[sizeof(CText)] __asm__("gText");
#endif
alignas(16) unsigned char gBalanceStorage[sizeof(CMainBalance)] = {};
alignas(16) unsigned char gTextStorage[sizeof(CText)] = {};

// ---- globals of other files
CWorld* gWorld;
int32_t gSoundBanks[8];

namespace {

constexpr uintptr_t kTail = 0x1da70c;     // the world byte the tail block starts at
const char* gBase;                         // the researcher under test (offsets in the logs are from it)
CResearchLab* gLab;                        // the research lab GetResearchLab answers with (when the queue says so)
bool gArmed;                               // memcmp logs and answers from the queue only while operator== runs
alignas(16) unsigned char gTable[64];      // the string table object (label 4)
char16_t gFormatText[0x40];                // the text FormatString writes (the case's)

int32_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gBase); }

// The number of "%d" in a format: the trace and the assertion log that many integer arguments.
int32_t CountInts(const char* format) {
    int32_t n = 0;
    for (const char* p = format; (p = std::strstr(p, "%d")) != nullptr; p += 2) ++n;
    return n;
}

}  // namespace

// memcmp: the library's own, which logs and answers from the queue only while CRegionEffectCell::operator== runs (everything else gets the plain
// comparison). The loop reads through volatile so the compiler cannot turn it back into a call of memcmp.
extern "C" int memcmp(const void* a, const void* b, size_t n) {
    if (gArmed) {
        ev("memcmp", {lab(a), lab(b), static_cast<int64_t>(n)});
        return static_cast<int>(ret("memcmp", 0));
    }
    const volatile unsigned char* x = static_cast<const volatile unsigned char*>(a);
    const volatile unsigned char* y = static_cast<const volatile unsigned char*>(b);
    for (size_t i = 0; i < n; ++i) {
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    }
    return 0;
}

// ---- the fakes: CStaff
CStaff::CStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model) {
    ev("CStaff::CStaff", {lab(this), cell.mIndex, static_cast<int32_t>(model)});
}
CStaff::CStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model, PayGrade grade, uint8_t costume) {
    ev("CStaff::CStaff4", {lab(this), cell.mIndex, static_cast<int32_t>(model), static_cast<int32_t>(grade), costume});
}
void CStaff::FinalDestroy() { ev("CStaff::FinalDestroy", {lab(this)}); }
void CStaff::ReceiveMessage(const CMsg& message) { ev("CStaff::ReceiveMessage", {lab(this), lab(&message)}); }
int32_t CStaff::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CStaff::Serialise", {lab(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CStaff::Serialise", 0));
}
void CStaff::UpdateTurn() { ev("CStaff::UpdateTurn", {lab(this)}); }
void CStaff::SetState(StaffState state) { ev("CStaff::SetState", {lab(this), static_cast<int32_t>(state)}); }
void CStaff::ModelState() { ev("CStaff::ModelState", {lab(this)}); }
int32_t CStaff::DoGeneralStaffThing() {
    ev("CStaff::DoGeneralStaffThing", {lab(this)});
    return static_cast<int32_t>(ret("CStaff::DoGeneralStaffThing", 0));
}
int32_t CStaff::TooTiredToWork() const {
    ev("CStaff::TooTiredToWork", {lab(this)});
    return static_cast<int32_t>(ret("CStaff::TooTiredToWork", 0));
}
int32_t CStaff::GetGeneralStaffState() const {
    ev("CStaff::GetGeneralStaffState", {lab(this)});
    return static_cast<int32_t>(ret("CStaff::GetGeneralStaffState", 0));
}
void CStaff::GetTiredWorking() { ev("CStaff::GetTiredWorking", {lab(this)}); }
void CStaff::GetTiredPatrolling() { ev("CStaff::GetTiredPatrolling", {lab(this)}); }
void CStaff::ProcessWalkingToRestArea() { ev("CStaff::ProcessWalkingToRestArea", {lab(this)}); }
void CStaff::ProcessResting() { ev("CStaff::ProcessResting", {lab(this)}); }

// ---- the CPeep members
int32_t CPeep::PeepWalks() {
    ev("CPeep::PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::PeepWalks", 0));
}
void CPeep::PlaySound(int32_t category, int32_t sound) { ev("CPeep::PlaySound", {lab(this), category, sound}); }
int32_t CPeep::SetRandomDestination() {
    ev("CPeep::SetRandomDestination", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::SetRandomDestination", 0));
}

// ---- the delayed script (reached at the researcher + 0xc)
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("CDelayedScript::SpawnSpriteScript", {Off(this), static_cast<int32_t>(script), set, index});
}
void CDelayedScript::SetSpriteScript(ScriptIdx script) {
    // The turn the researcher stored before this call (the Mac stores it first), as the field's value.
    ev("CDelayedScript::SetSpriteScript", {Off(this), static_cast<int32_t>(script),
                                           static_cast<int64_t>(*reinterpret_cast<const uint32_t*>(gBase + 0x210))});
}

// ---- the world, the research lab and the sprites
void CWorld::Register(CResearcher* staff) { ev("CWorld::Register", {lab(staff)}); }
void CWorld::Unregister(CResearcher* staff) { ev("CWorld::Unregister", {lab(staff)}); }
uint32_t CWorld::GetRand() {
    ev("CWorld::GetRand", {});
    return static_cast<uint32_t>(ret("CWorld::GetRand", 0));
}
CResearchLab* CWorld::GetResearchLab() {
    ev("CWorld::GetResearchLab", {});
    return ret("CWorld::GetResearchLab", 0) != 0 ? gLab : nullptr;
}
void CResearchLab::UpdateActiveStatus() { ev("CResearchLab::UpdateActiveStatus", {lab(this)}); }
void CResearchLab::ContributeResearch(uint32_t points) { ev("CResearchLab::ContributeResearch", {lab(this), points}); }
int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    ev("ESP_GetRandomSpritesFrom", {static_cast<int32_t>(set)});
    return static_cast<int32_t>(ret("ESP_GetRandomSpritesFrom", 0));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("CThing::GetThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("CThing::GetThingId", 0))};
}

// ---- the string table (FormatString writes the case's text, cut to the size)
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("TbITable::FormatString", {lab(this), size, id});
    if (size == 0) return 0;
    uint32_t count = 0;
    while (gFormatText[count] != 0 && count < size - 1) {
        out[count] = gFormatText[count];
        ++count;
    }
    out[count] = 0;
    return static_cast<int32_t>(count);
}

// ---- the serialise instances: the file, the field's offset from the researcher (the cell's own offset for the region ones), noLog and the name
template <> int32_t LOADVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) {
    ev("LOADVAR<unsigned long>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t SAVEVAR<MacULong>(void* f, MacULong* v, const char* n, int32_t l) {
    ev("SAVEVAR<unsigned long>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>(void* f, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>* v, const char* n, int32_t l) {
    ev("LOADVARSTRUCT<ThingId>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>(void* f, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>* v, const char* n, int32_t l) {
    ev("SAVEVARSTRUCT<ThingId>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t LOADVAR<short>(void* f, short* v, const char* n, int32_t l) {
    ev("LOADVAR<short>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t SAVEVAR<short>(void* f, short* v, const char* n, int32_t l) {
    ev("SAVEVAR<short>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}

// ---- the runtime: the "%d" arguments are logged after the level (CTRACE) or the condition (GAME_ASSERTMSG)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t v[4] = {condition, 0, 0, 0};
    const int32_t n = CountInts(format);
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < n; ++i) v[1 + i] = va_arg(args, uint32_t);
    va_end(args);
    switch (n) {
    case 0: ev("GAME_ASSERTMSG", {v[0]}, {format}); break;
    case 1: ev("GAME_ASSERTMSG", {v[0], v[1]}, {format}); break;
    case 2: ev("GAME_ASSERTMSG", {v[0], v[1], v[2]}, {format}); break;
    default: ev("GAME_ASSERTMSG", {v[0], v[1], v[2], v[3]}, {format}); break;
    }
}
void CTRACE(uint32_t level, const char* format, ...) {
    int64_t v[4] = {static_cast<int64_t>(level), 0, 0, 0};
    const int32_t n = CountInts(format);
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < n; ++i) v[1 + i] = va_arg(args, uint32_t);
    va_end(args);
    switch (n) {
    case 0: ev("CTRACE", {v[0]}, {format}); break;
    case 1: ev("CTRACE", {v[0], v[1]}, {format}); break;
    case 2: ev("CTRACE", {v[0], v[1], v[2]}, {format}); break;
    default: ev("CTRACE", {v[0], v[1], v[2], v[3]}, {format}); break;
    }
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_resr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"banks", gSoundBanks},
        {"balance_values", &reinterpret_cast<CMainBalance*>(gBalanceStorage)->mValues[0]},
        {"table", gTable},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_resr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_resr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CResearcher));
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CRegionEffectCell));
    return n;
}

// The state a case sets before the call: the researcher under test, the world's tail block (the world pointer is shifted back to it), the lab, the
// table and the text FormatString writes.
void tpw_resr_setup(uint64_t researcher, uint64_t tail, const char* text, uint64_t labBlock) {
    gBase = reinterpret_cast<const char*>(researcher);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    gLab = reinterpret_cast<CResearchLab*>(static_cast<uintptr_t>(labBlock));
    reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[15] = gTable;
    std::memset(gFormatText, 0, sizeof(gFormatText));
    for (int32_t i = 0; i < 0x3f && text[i] != 0; ++i) gFormatText[i] = static_cast<unsigned char>(text[i]);
}
// __sinit_Researcher_cpp: the host has no function; the two words are constant initialised, so the wrapper only reads them back.
void tpw_resr_sinit(uint32_t* out) {
    out[0] = gResearcherSinitLow;
    out[1] = gResearcherSinitHigh;
}

// ---- the functions, one wrapper each with C types only
static CResearcher* H(uint64_t block) { return reinterpret_cast<CResearcher*>(static_cast<uintptr_t>(block)); }
static CRegionEffectCell* Cell(uint64_t block) { return reinterpret_cast<CRegionEffectCell*>(static_cast<uintptr_t>(block)); }
uint64_t tpw_resr_ctor1(uint64_t block, uint32_t cell) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) CResearcher(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}));
}
uint64_t tpw_resr_ctor2(uint64_t block, uint32_t cell, int32_t grade, uint32_t flag, int32_t name) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block)))
                                           CResearcher(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}, grade, static_cast<uint8_t>(flag), name));
}
void tpw_resr_init(uint64_t block, uint32_t sprite) { H(block)->Init(static_cast<uint8_t>(sprite)); }
void tpw_resr_recv(uint64_t block, uint64_t message) { H(block)->ReceiveMessage(*reinterpret_cast<const CMsg*>(static_cast<uintptr_t>(message))); }
int32_t tpw_resr_ser(uint64_t block, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return H(block)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}
void tpw_resr_final(uint64_t block) { H(block)->FinalDestroy(); }
void tpw_resr_update(uint64_t block) { H(block)->UpdateTurn(); }
void tpw_resr_choose(uint64_t block) { H(block)->ChooseNextAction(); }
void tpw_resr_model(uint64_t block) { H(block)->ModelState(); }
void tpw_resr_idle(uint64_t block) { H(block)->ProcessIdle(); }
void tpw_resr_patrol(uint64_t block) { H(block)->ProcessPatrolling(); }
void tpw_resr_research(uint64_t block) { H(block)->ProcessResearching(); }
void tpw_resr_dosome(uint64_t block) { H(block)->DoSomeResearch(); }
int32_t tpw_resr_eq(uint64_t a, uint64_t b) {
    gArmed = true;
    const bool same = *Cell(a) == *Cell(b);
    gArmed = false;
    return same ? 1 : 0;
}
int32_t tpw_resr_rser(uint64_t block, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return Cell(block)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}

}  // extern "C"
