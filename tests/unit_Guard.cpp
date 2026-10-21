// Test library for the guard (unit w182; libtpw_unit_Guard.dylib, tests/test_Guard.py), main app:
//   src/game/people/guard.cpp   CGuard (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, ProsecutePerp, IsAvailable, IsCarrying, UpdateTurn,
//                   ChooseNextAction, ModelState, SetState, ProcessIdle, ProcessPatrolling, ProcessProsecuting, ProcessWalkingToEntrance,
//                   ProcessReturningToPatrol)
//
// The callees outside it are fakes that record the same events as the PowerPC stubs: the CStaff, CPeep and CPerson members, the delayed script,
// CWorld::Register / Unregister / GetRand / GetParkAnalyser, the map's effectors and entrance cells, the park analyser, the sound and script calls,
// the string table's FormatString, the four serialise instances, CThing::GetThingId, the message centre (Subscribe, the headers and the nothrow
// operator new), the trace and the assertion. The globals of other files are defined here: gWorld (pointing into a shifted tail block),
// gWorldMap, gText and gMainBalance (raw storage), gSoundBanks, gThingArray and gMessageCentre.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "MainBalance.h"
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
CMap* gWorldMap;
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];
CMessageCentre* gMessageCentre;

namespace {

constexpr uintptr_t kTail = 0x1da70c;     // the world byte the tail block starts at
const char* gBase;                         // the guard under test (offsets in the logs are from it)
void* gNewCentre = nullptr;                // the block the fake operator new made last (its first header logs the event)
alignas(16) unsigned char gTable[64];      // the string table object (label 4)
char16_t gFormatText[0x40];                // the text FormatString writes (the case's)

int32_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gBase); }
int64_t Canon(const void* pointer) {
    const int32_t label = lab(pointer);
    if (label >= 0) return label;
    return (1ll << 33) | static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer) & 0xffffffffu);
}

class TestMessage : public CMsgDeath {
public:
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return static_cast<int32_t>(ret("GetType", 0));
    }
};

}  // namespace

// Test-only key function and destructor of CMsgDeath (src/game/ui/MessageCentre.cpp, src/game/Thing.cpp is not linked); TestMessage overrides GetType.
int32_t CMsgDeath::GetType() const { return 0x1b; }
CMsgDeath::~CMsgDeath() {}

// ---- the message centre: Subscribe, the 29 headers (the first stands for __construct_array's call) and the nothrow operator new
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    ev("Subscribe", {lab(this), lab(&thing), type});
    return true;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = on() && ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = ::operator new(size);
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

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
void CStaff::GetTiredWorking() { ev("CStaff::GetTiredWorking", {lab(this)}); }
void CStaff::GetTiredPatrolling() { ev("CStaff::GetTiredPatrolling", {lab(this)}); }
void CStaff::ProcessWalkingToRestArea() { ev("CStaff::ProcessWalkingToRestArea", {lab(this)}); }
void CStaff::ProcessResting() { ev("CStaff::ProcessResting", {lab(this)}); }

// ---- the CPeep and CPerson members
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CPeep::LastCellVisited() {
    ev("CPeep::LastCellVisited", {lab(this)});
    return {static_cast<uint16_t>(ret("CPeep::LastCellVisited", 0))};
}
int32_t CPeep::PeepWalks() {
    ev("CPeep::PeepWalks", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::PeepWalks", 0));
}
void CPeep::PlaySound(int32_t category, int32_t sound) { ev("CPeep::PlaySound", {lab(this), category, sound}); }
int32_t CPeep::SetRandomDestination() {
    ev("CPeep::SetRandomDestination", {lab(this)});
    return static_cast<int32_t>(ret("CPeep::SetRandomDestination", 0));
}
int32_t CPeep::SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CPeep::SetDest", {lab(this), cell.mIndex});
    return static_cast<int32_t>(ret("CPeep::SetDest", 0));
}
void CPerson::GetBagged() { ev("CPerson::GetBagged", {lab(this)}); }
void CPerson::GetDebagged(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("CPerson::GetDebagged", {lab(this), cell.mIndex}); }
int32_t CPerson::IsCatchable() const {
    ev("CPerson::IsCatchable", {lab(this)});
    return static_cast<int32_t>(ret("CPerson::IsCatchable", 0));
}

// ---- the delayed script (reached at the guard + 0xc)
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("CDelayedScript::SpawnSpriteScript", {Off(this), static_cast<int32_t>(script), set, index});
}
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("CDelayedScript::SetSpriteScript", {Off(this), static_cast<int32_t>(script)}); }

// ---- the world and the map
void CWorld::Register(CGuard* staff) { ev("CWorld::Register", {lab(staff)}); }
void CWorld::Unregister(CGuard* staff) { ev("CWorld::Unregister", {lab(staff)}); }
uint32_t CWorld::GetRand() {
    ev("CWorld::GetRand", {});
    return static_cast<uint32_t>(ret("CWorld::GetRand", 0));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("CWorld::GetParkAnalyser", {});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x4000));
}
void CParkAnalyser::IveMisbehaved(int32_t caught) { ev("CParkAnalyser::IveMisbehaved", {caught}); }
void CMap::AddRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CMap::AddRegionalEffector", {static_cast<int32_t>(effector), cell.mIndex});
}
void CMap::RemoveRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CMap::RemoveRegionalEffector", {static_cast<int32_t>(effector), cell.mIndex});
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetTicketBoothPos(int32_t which) const {
    ev("CMap::GetTicketBoothPos", {which});
    return {static_cast<uint16_t>(ret("CMap::GetTicketBoothPos", 0))};
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetParkEntrancePos(int32_t which) const {
    ev("CMap::GetParkEntrancePos", {which});
    return {static_cast<uint16_t>(ret("CMap::GetParkEntrancePos", 0))};
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("CThing::GetThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("CThing::GetThingId", 0))};
}

// ---- sprites
int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    ev("ESP_GetRandomSpritesFrom", {static_cast<int32_t>(set)});
    return static_cast<int32_t>(ret("ESP_GetRandomSpritesFrom", 0));
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

// ---- the serialise instances (the four the guard uses): the file, the field's offset from the guard, noLog and the name
template <> int32_t LOADVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) {
    ev("LOADVAR<unsigned int>", {lab(f), Off(v), l}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
template <> int32_t SAVEVAR<uint32_t>(void* f, uint32_t* v, const char* n, int32_t l) {
    ev("SAVEVAR<unsigned int>", {lab(f), Off(v), l}, {n});
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

// ---- the runtime
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
// The one format with an argument ("%d") logs it as a second integer.
void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strstr(format, "%d") != nullptr) {
        va_list args;
        va_start(args, format);
        const uint32_t a = va_arg(args, uint32_t);
        va_end(args);
        ev("CTRACE", {static_cast<int64_t>(level), a}, {format});
    } else {
        ev("CTRACE", {static_cast<int64_t>(level)}, {format});
    }
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_gd_addr(const char* name) {
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
uint64_t tpw_gd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// A death message whose GetType answers the queue "GetType" and whose dying id is `id`.
uint64_t tpw_gd_makemsg(uint32_t id) {
    TestMessage* m = new TestMessage();
    m->mThingId = static_cast<uint16_t>(id);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m));
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_gd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CGuard));
    out[n++] = static_cast<int64_t>(offsetof(CMainBalance, mValues));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry) == 0x14 ? 0x14 : sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    return n;
}

// The state a case sets before the call: the guard under test, the world's tail block (the world pointer is shifted back to it), the table, the text
// FormatString writes and the message centre (0 or a block).
void tpw_gd_setup(uint64_t guard, uint64_t tail, const char* text, uint64_t centre) {
    gBase = reinterpret_cast<const char*>(guard);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(0x5000));
    reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[14] = gTable;
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
    gNewCentre = nullptr;
    std::memset(gFormatText, 0, sizeof(gFormatText));
    for (int32_t i = 0; i < 0x3f && text[i] != 0; ++i) gFormatText[i] = static_cast<unsigned char>(text[i]);
}
void tpw_gd_setrow(int32_t row, uint64_t block) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(block)); }
int64_t tpw_gd_centre() { return Canon(gMessageCentre); }

// ---- the functions, one wrapper each with C types only
static CGuard* G(uint64_t block) { return reinterpret_cast<CGuard*>(static_cast<uintptr_t>(block)); }
uint64_t tpw_gd_ctor1(uint64_t block, uint32_t cell) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) CGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}));
}
uint64_t tpw_gd_ctor2(uint64_t block, uint32_t cell, int32_t grade, uint32_t flag, int32_t name) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block)))
                                           CGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)}, grade, static_cast<uint8_t>(flag), name));
}
void tpw_gd_init(uint64_t block) { G(block)->Init(); }
void tpw_gd_receive(uint64_t block, uint64_t message) { G(block)->ReceiveMessage(*reinterpret_cast<const CMsg*>(static_cast<uintptr_t>(message))); }
int32_t tpw_gd_serialise(uint64_t block, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return G(block)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}
void tpw_gd_final(uint64_t block) { G(block)->FinalDestroy(); }
void tpw_gd_prosecute(uint64_t block, uint32_t perp) { G(block)->ProsecutePerp(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(perp)}); }
int32_t tpw_gd_isavailable(uint64_t block) { return G(block)->IsAvailable() ? 1 : 0; }
int32_t tpw_gd_iscarrying(uint64_t block) { return G(block)->IsCarrying() ? 1 : 0; }
void tpw_gd_update(uint64_t block) { G(block)->UpdateTurn(); }
void tpw_gd_choose(uint64_t block) { G(block)->ChooseNextAction(); }
void tpw_gd_model(uint64_t block) { G(block)->ModelState(); }
void tpw_gd_setstate(uint64_t block, int32_t state) { G(block)->SetState(static_cast<CStaff::StaffState>(state)); }
void tpw_gd_idle(uint64_t block) { G(block)->ProcessIdle(); }
void tpw_gd_patrol(uint64_t block) { G(block)->ProcessPatrolling(); }
void tpw_gd_prosecuting(uint64_t block) { G(block)->ProcessProsecuting(); }
void tpw_gd_toentrance(uint64_t block) { G(block)->ProcessWalkingToEntrance(); }
void tpw_gd_toreturn(uint64_t block) { G(block)->ProcessReturningToPatrol(); }

}  // extern "C"
