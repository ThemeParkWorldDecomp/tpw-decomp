// Test library for the staff member, part 1 (unit w189; libtpw_unit_Staff.dylib, tests/test_Staff.py), main app:
//   src/game/people/staff.cpp   CStaff (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, UpdateTurn, SetState, ModelState, UpdateMonthly), the
//                   four PayGrade / StaffState serialise instances, CMsgStaffRoomEvent's destructor and GetType, and the two words of the static initialiser
//
// The callees outside it are fakes that record the same events as the PowerPC stubs: the CPeep members, the thoughts, the delayed script, the seven
// untranslated CStaff members, the world's getters, the park time, the bank account, the park analyser, the message centre (Subscribe, the headers and the
// nothrow operator new), the mouse and its modes, the other serialise instances, the trace and the assertion. The four enum serialise instances run
// real, over a fake file layer (LbFile_Read hands out the case's bytes, LbFile_Write records them) and a fake save-load log. The globals of other
// files are defined here: gWorld (pointing into a shifted tail block), gMessageCentre, gMouse and its two words, gSaveLoad.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "LbFile.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files
CWorld* gWorld;
CMessageCentre* gMessageCentre;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CSaveLoad gSaveLoad;

namespace {

using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr uintptr_t kTail = 0x1da70c;     // the world byte the tail block starts at
const char* gBase;                         // the staff member under test (offsets in the logs are from it)
void* gNewCentre = nullptr;                // the block the fake operator new made last for a message centre
CParkAnalyser* gAnalyser = nullptr;        // the analyser the world answers (shifted so that the history's total lies in the case's block)
std::vector<uint8_t> gFileData;            // the bytes LbFile_Read hands out
std::size_t gCursor = 0;

int32_t Off(const void* p) { return static_cast<int32_t>(reinterpret_cast<const char*>(p) - gBase); }
int64_t Canon(const void* pointer) {
    const int32_t label = lab(pointer);
    if (label >= 0) return label;
    return (1ll << 33) | static_cast<int64_t>(reinterpret_cast<uintptr_t>(pointer) & 0xffffffffu);
}

// The Mac's size of an object the host made: the sizes the fake operator new logs (the mouse is 1 byte, its modes 8 and 0x14 on the Mac).
int64_t MacSize(std::size_t size) {
    if (size == sizeof(CMouse)) return 1;
    if (size == sizeof(CMMDefault)) return 8;
    if (size == sizeof(CMMPickUpStaff)) return 0x14;
    return static_cast<int64_t>(size);
}

class TestMessage : public CMsgRideDeleted {
public:
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return static_cast<int32_t>(ret("GetType", 0));
    }
};

// The staff-room event under test: the delete expression logs instead of freeing (the block is read afterwards).
class TestEvent : public CMsgStaffRoomEvent {
public:
    TestEvent(uint16_t thing, int32_t arriving) : CMsgStaffRoomEvent(thing, arriving) {}
    static void operator delete(void* p) { ev("delete", {lab(p)}); }
};

// The number of "%d" in a format: the trace and the assertion log that many integer arguments.
int32_t CountInts(const char* format) {
    int32_t n = 0;
    for (const char* p = format; (p = std::strstr(p, "%d")) != nullptr; p += 2) ++n;
    return n;
}

std::string Hex(const void* data, uint32_t length) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0; i < length; ++i) {
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 15];
    }
    return out;
}

}  // namespace

// Test-only key function of CMsgRideDeleted (the Mac's is 0x10110f34); TestMessage overrides GetType.
int32_t CMsgRideDeleted::GetType() const { return 10; }
// The destructor the vtable names (the Mac's is 0x10107b28, in src/game/Object.cpp).
CMsgRideDeleted::~CMsgRideDeleted() {}

// ---- the message centre: Subscribe, the 29 headers (the first stands for __construct_array's call) and the nothrow operator new
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    ev("Subscribe", {lab(this), lab(&thing), type});
    return true;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = on() && ret("newnull", 0) != 0;
    ev("new", {MacSize(size), fail});
    if (fail) return nullptr;
    void* block = ::operator new(size);
    if (size == sizeof(CMessageCentre)) gNewCentre = block;
    else if (size == sizeof(CMouse)) newlab(block);   // the mouse has no constructor call: it is labelled here (and on the PowerPC side)
    return block;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// ---- the mouse and its modes
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }
CMMDefault::CMMDefault() { ev("CMMDefault.ctor", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}
CSpritePlaceMode::~CSpritePlaceMode() {}
void CSpritePlaceMode::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void CSpritePlaceMode::OnRelease() {}
CMMPickUpStaff::CMMPickUpStaff(ESP_Sprites sprite, uint8_t flag) { ev("CMMPickUpStaff.ctor", {newlab(this), static_cast<int32_t>(sprite), flag}); }
CMMPickUpStaff::~CMMPickUpStaff() {}
void CMMPickUpStaff::OnLeftClick(int32_t, int32_t) {}
void CMMPickUpStaff::OnRightClick(int32_t, int32_t) {}
int32_t CMMPickUpStaff::GetMode() { return 6; }
void CMMPickUpStaff::OnAssign() {}
void CMMPickUpStaff::OnRelease() {}

// ---- the CPeep members
CPeep::CPeep(MapId cell, CThing::ThingModel model) { ev("CPeep::CPeep", {lab(this), cell.mIndex, static_cast<int32_t>(model)}); }
void CPeep::FinalDestroy() { ev("CPeep::FinalDestroy", {lab(this)}); }
int32_t CPeep::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CPeep::Serialise", {lab(this), lab(file), save, version, unused});
    return static_cast<int32_t>(ret("CPeep::Serialise", 0));
}
void CPeep::UpdateTurn() { ev("CPeep::UpdateTurn", {lab(this)}); }
void CPeep::SetStandScript() { ev("CPeep::SetStandScript", {lab(this)}); }

// ---- the seven untranslated CStaff members
uint32_t CStaff::GetSalary() {
    ev("CStaff::GetSalary", {lab(this)});
    return static_cast<uint32_t>(ret("CStaff::GetSalary", 0));
}
ThingId CStaff::GoToRestArea() {
    ev("CStaff::GoToRestArea", {lab(this)});
    return {static_cast<uint16_t>(ret("CStaff::GoToRestArea", 0))};
}
void CStaff::StopResting() { ev("CStaff::StopResting", {lab(this)}); }
void CStaff::CancelPatrolArea() { ev("CStaff::CancelPatrolArea", {lab(this)}); }
void CStaff::ProcessWalkingToStrike() { ev("CStaff::ProcessWalkingToStrike", {lab(this)}); }
void CStaff::ProcessStriking() { ev("CStaff::ProcessStriking", {lab(this)}); }
void CStaff::ProcessBeingStunned() { ev("CStaff::ProcessBeingStunned", {lab(this)}); }

// ---- the thoughts and the delayed script (reached at the staff member + 0x30 and + 0xc)
void CThoughts::ProcessThoughtBubbles() { ev("CThoughts::ProcessThoughtBubbles", {Off(this)}); }
void CThoughts::RemoveThoughtBubble() { ev("CThoughts::RemoveThoughtBubble", {Off(this)}); }
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("CDelayedScript::SetSpriteScript", {Off(this), static_cast<int32_t>(script)}); }
void CDelayedScript::KillSpriteScript() { ev("CDelayedScript::KillSpriteScript", {Off(this)}); }

// ---- the world, the park time, the bank account and the analyser
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("CWorld::GetParkAnalyser", {});
    return gAnalyser;
}
CBankAccount* CWorld::GetBankAccount() {
    ev("CWorld::GetBankAccount", {});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0x5000));
}
void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("CBankAccount::MakeWithdrawal", {lab(this), amount}); }
void CParkAnalyser::HiredSomeone() { ev("CParkAnalyser::HiredSomeone", {lab(this)}); }
TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(gWorld)), a != nullptr, b != nullptr, c != nullptr,
                        d != nullptr, e != nullptr, f != nullptr, g != nullptr, h != nullptr});
    return TbTimeStamp{ret("funny", 0)};
}
ThingId CThing::GetThingId() const {
    ev("CThing::GetThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("CThing::GetThingId", 0))};
}

// ---- the other serialise instances: the file, the field's offset from the staff member, noLog and the name; the loads of the float and the stamp
// write the case's value
template <> int32_t LOADVAR<float>(void* f, float* v, const char* n, int32_t l) {
    ev("LOADVAR<float>", {lab(f), l}, {n});
    const int32_t answer = static_cast<int32_t>(ret("vans", 0));
    if (answer == 0) {
        const uint32_t bits = static_cast<uint32_t>(ret("pvload", 0));
        std::memcpy(v, &bits, 4);
    }
    return answer;
}
template <> int32_t SAVEVAR<float>(void* f, float* v, const char* n, int32_t l) {
    uint32_t bits;
    std::memcpy(&bits, v, 4);
    ev("SAVEVAR<float>", {lab(f), l, bits}, {n});
    return static_cast<int32_t>(ret("vans", 0));
}
#define TPW_STF_VAR(KIND, NAME, TYPE)                                                                                       \
    template <> int32_t KIND<TYPE>(void* f, TYPE* v, const char* n, int32_t l) {                                           \
        ev(NAME, {lab(f), Off(v), l}, {n});                                                                                \
        return static_cast<int32_t>(ret("vans", 0));                                                                       \
    }
TPW_STF_VAR(LOADVAR, "LOADVAR<unsigned int>", uint32_t)
TPW_STF_VAR(SAVEVAR, "SAVEVAR<unsigned int>", uint32_t)
TPW_STF_VAR(LOADVAR, "LOADVAR<unsigned char>", uint8_t)
TPW_STF_VAR(SAVEVAR, "SAVEVAR<unsigned char>", uint8_t)
TPW_STF_VAR(LOADVAR, "LOADVAR<wchar_t>", char16_t)
TPW_STF_VAR(SAVEVAR, "SAVEVAR<wchar_t>", char16_t)
TPW_STF_VAR(LOADVAR, "LOADVAR<unsigned long>", MacULong)
TPW_STF_VAR(SAVEVAR, "SAVEVAR<unsigned long>", MacULong)
TPW_STF_VAR(LOADVARSTRUCT, "LOADVARSTRUCT<MapId>", MapId)
TPW_STF_VAR(SAVEVARSTRUCT, "SAVEVARSTRUCT<MapId>", MapId)
TPW_STF_VAR(LOADVARSTRUCT, "LOADVARSTRUCT<ThingId>", ThingId)
TPW_STF_VAR(SAVEVARSTRUCT, "SAVEVARSTRUCT<ThingId>", ThingId)
TPW_STF_VAR(SAVEVARSTRUCT, "SAVEVARSTRUCT<TbTimeStamp>", TbTimeStamp)
template <> int32_t LOADVARSTRUCT<TbTimeStamp>(void* f, TbTimeStamp* v, const char* n, int32_t l) {
    ev("LOADVARSTRUCT<TbTimeStamp>", {lab(f), Off(v), l}, {n});
    const int32_t answer = static_cast<int32_t>(ret("vans", 0));
    if (answer == 0) {
        const int64_t value = ret("stampload", 0);
        std::memcpy(v, &value, 8);
    }
    return answer;
}

// ---- the file layer and the save-load log (the four enum instances run real over them)
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    const std::size_t available = gFileData.size() > gCursor ? gFileData.size() - gCursor : 0;
    uint32_t count = available < length ? static_cast<uint32_t>(available) : length;
    const std::size_t taken = count;
    const int64_t forced = ret("LbFile_Read:xfer", -1);
    if (forced >= 0) count = static_cast<uint32_t>(forced);
    *transferred = count;
    const int32_t result = static_cast<int32_t>(ret("LbFile_Read"));
    if (count == length && result >= 0) {
        std::vector<uint8_t> bytes(length, 0);
        std::memcpy(bytes.data(), gFileData.data() + (gFileData.size() > gCursor ? gCursor : gFileData.size()), taken < length ? taken : length);
        std::memcpy(buffer, bytes.data(), length);
    }
    gCursor += taken;
    return TbError(result);
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), length}, {Hex(buffer, length).c_str()});
    const int64_t forced = ret("LbFile_Write:xfer", -1);
    *transferred = forced >= 0 ? static_cast<uint32_t>(forced) : length;
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

// ---- the runtime: the "%d" arguments are logged after the condition
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t v[4] = {condition, 0, 0, 0};
    const int32_t n = CountInts(format);
    va_list args;
    va_start(args, format);
    for (int32_t i = 0; i < n && i < 3; ++i) v[1 + i] = va_arg(args, uint32_t);
    va_end(args);
    switch (n) {
    case 0: ev("GAME_ASSERTMSG", {v[0]}, {format}); break;
    case 1: ev("GAME_ASSERTMSG", {v[0], v[1]}, {format}); break;
    case 2: ev("GAME_ASSERTMSG", {v[0], v[1], v[2]}, {format}); break;
    default: ev("GAME_ASSERTMSG", {v[0], v[1], v[2], v[3]}, {format}); break;
    }
}
void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_stf_addr(const char* name) {
    if (std::strcmp(name, "gSaveLoad") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gSaveLoad));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_stf_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
// A ride-deleted message whose GetType answers the queue "GetType" and whose deleted id is `id`.
uint64_t tpw_stf_makemsg(uint32_t id) {
    TestMessage* m = new TestMessage();
    m->mThingId = static_cast<uint16_t>(id);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m));
}
// A staff-room event (the delete expression logs "delete").
uint64_t tpw_stf_makeevent(uint32_t thing, int32_t arriving) {
    TestEvent* m = new TestEvent(static_cast<uint16_t>(thing), arriving);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m));
}
// The fields the inline constructor sets, read back (the type, the flags, the id and the arriving word).
void tpw_stf_eventfields(uint64_t block, int64_t* out) {
    const CMsgStaffRoomEvent* m = reinterpret_cast<const CMsgStaffRoomEvent*>(static_cast<uintptr_t>(block));
    out[0] = m->CMsgStaffRoomEvent::GetType();
    out[1] = m->mFlags;
    out[2] = m->mThingId;
    out[3] = m->mArriving;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_stf_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CStaff));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    return n;
}

// The state a case sets before the call: the staff member under test, the world's tail block (the world pointer is shifted back to it), the analyser's
// block (the pointer is shifted back so that the history's total lies at its start), the message centre (0 or a block), the mouse (0 or a block), the
// two words of the mouse, and the file's bytes.
void tpw_stf_setup(uint64_t staff, uint64_t tail, uint64_t analyser, uint64_t centre, uint64_t mouse, int32_t word1, int32_t word2, const uint8_t* file,
                   uint32_t fileLength) {
    gBase = reinterpret_cast<const char*>(staff);
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    const uintptr_t total = reinterpret_cast<uintptr_t>(&reinterpret_cast<CParkAnalyser*>(0)->mHistory1F100[3].mTemp);
    gAnalyser = reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(analyser) - total);
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
    gMouse = reinterpret_cast<CMouse*>(static_cast<uintptr_t>(mouse));
    gMouseWord1E0 = word1;
    gMouseWord1E4 = word2;
    gNewCentre = nullptr;
    gFileData.assign(file, file + fileLength);
    gCursor = 0;
}
// The label of the shifted analyser pointer (the code under test only ever calls members on it).
uint64_t tpw_stf_analyser() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gAnalyser)); }
int64_t tpw_stf_centre() { return Canon(gMessageCentre); }
int64_t tpw_stf_mouse() { return Canon(gMouse); }
void tpw_stf_mousewords(int32_t* out) {
    out[0] = gMouseWord1E0;
    out[1] = gMouseWord1E4;
}
// __sinit_staff_cpp: the host has no function; the two words are constant initialised, so the wrapper only reads them back.
void tpw_stf_sinit(uint32_t* out) {
    out[0] = gStaffSinitLow;
    out[1] = gStaffSinitHigh;
}

// ---- the functions, one wrapper each with C types only
static CStaff* S(uint64_t block) { return reinterpret_cast<CStaff*>(static_cast<uintptr_t>(block)); }
uint64_t tpw_stf_ctor1(uint64_t block, uint32_t cell, int32_t model) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block)))
                                           CStaff(MapId{static_cast<uint16_t>(cell)}, static_cast<CThing::ThingModel>(model)));
}
uint64_t tpw_stf_ctor2(uint64_t block, uint32_t cell, int32_t model, int32_t grade, uint32_t costume) {
    return reinterpret_cast<uintptr_t>(new (reinterpret_cast<void*>(static_cast<uintptr_t>(block)))
                                           CStaff(MapId{static_cast<uint16_t>(cell)}, static_cast<CThing::ThingModel>(model), static_cast<CStaff::PayGrade>(grade),
                                                  static_cast<uint8_t>(costume)));
}
void tpw_stf_init(uint64_t block) { S(block)->Init(); }
void tpw_stf_final(uint64_t block) { S(block)->FinalDestroy(); }
void tpw_stf_receive(uint64_t block, uint64_t message) { S(block)->ReceiveMessage(*reinterpret_cast<const CMsg*>(static_cast<uintptr_t>(message))); }
int32_t tpw_stf_serialise(uint64_t block, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    return S(block)->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}
void tpw_stf_update(uint64_t block) { S(block)->UpdateTurn(); }
void tpw_stf_setstate(uint64_t block, int32_t state) { S(block)->SetState(static_cast<CStaff::StaffState>(state)); }
void tpw_stf_model(uint64_t block) { S(block)->ModelState(); }
void tpw_stf_monthly(uint64_t block) { S(block)->UpdateMonthly(); }
// The four enum instances: kind 0 LOADVAR<PayGrade>, 1 LOADVAR<StaffState>, 2 SAVEVAR<PayGrade>, 3 SAVEVAR<StaffState>.
int32_t tpw_stf_enumvar(int32_t kind, uint64_t file, uint64_t value, const char* name, int32_t noLog) {
    void* f = reinterpret_cast<void*>(static_cast<uintptr_t>(file));
    void* v = reinterpret_cast<void*>(static_cast<uintptr_t>(value));
    switch (kind) {
    case 0: return LOADVAR(f, static_cast<CStaff::PayGrade*>(v), name, noLog);
    case 1: return LOADVAR(f, static_cast<CStaff::StaffState*>(v), name, noLog);
    case 2: return SAVEVAR(f, static_cast<CStaff::PayGrade*>(v), name, noLog);
    default: return SAVEVAR(f, static_cast<CStaff::StaffState*>(v), name, noLog);
    }
}
// CMsgStaffRoomEvent: the destructor with the flag (positive: the delete expression of the deleting form) and GetType.
void tpw_stf_msgdtor(uint64_t block, int32_t flag) {
    TestEvent* m = reinterpret_cast<TestEvent*>(static_cast<uintptr_t>(block));
    if (static_cast<int16_t>(flag) > 0) delete m;
    else m->~TestEvent();
}
int32_t tpw_stf_msgtype(uint64_t block) { return reinterpret_cast<const CMsgStaffRoomEvent*>(static_cast<uintptr_t>(block))->CMsgStaffRoomEvent::GetType(); }

}  // extern "C"
