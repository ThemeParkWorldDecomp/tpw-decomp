// Test library for a ride's names, kind, excitement and stand point, and its final destroy (unit w210; libtpw_unit_ObjectInfo.dylib,
// tests/test_ObjectInfo.py), main app:
//   src/game/Object.cpp      the 14 CObject members UpgradesWhat, GetControl, GetFileName, GetUIName, GetSignNameA, GetSignNameB, SetSignNameA, SetSignNameB,
//                            IsTrackRide, GetProspectiveExcitementLevel, GetExcitementLevel, GetStandPoint and FinalDestroy
//   src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/SkirtDeform.cpp, src/game/ui/ui_staffPanel.cpp  CObject::IsCoaster (unit v5L07; GetProspectiveExcitementLevel calls it)
//   src/game/Object.cpp, src/game/ui/MessageCentre.cpp   CMsgRideDeleted's constructor (and its GetType and destructor, written)
//   src/game/rides/RideDB.cpp, src/game/save/SaveLoad.cpp     CRideDatabase::GetFileName (and the name table the file holds)
//
// Every other callee is a fake that records the same event as the PowerPC stub: the ride database's GetRideInfo (it answers the case's record and counts every
// fetch), CThing::GetThingId and CThing::FinalDestroy, the control manager's GetObjectControl (both overloads), the world's Unregister and GetBankAccount, the
// bank's MakeDeposit, the map's RemoveRegionalEffector, the message centre's PostMessage, the nothrow operator new and the message centre's constructor, the
// five CObject queries (IsDirty, GetCurrentResidualValue, GetPricePerUse, GetPrizeValue, GetChanceOfWinning), the script calls, the selected ride of the UI, the
// action recorder, the master save's constructor and sign names, the game type's constructor, the track and peep counts and the assertion. The sign names go
// through the real platform/msl.cpp. The globals of other files are defined here: gWorld (pointing into a shifted tail block), gWorldMap, gRideDatabase,
// gMessageCentre, gMasterSave, gActionRecorder and gUIInterface.
// Every wrapper runs the call under a SIGSEGV and SIGBUS guard (tpw_oin_crashed): a translation that dereferences what the stub answers (FinalDestroy does
// with GetControl's NULL) is a value of the test, not a dead worker.
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "ActionRecorder.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "ObjectControl.h"
#include "RideDB.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TbFontRenderParams.h"
#include "TbITable.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
CWorld* gWorld;
CMap* gWorldMap;
CRideDatabase gRideDatabase;
CMessageCentre* gMessageCentre;
CFSGlobalMasterSave* gMasterSave;
CActionRecorder gActionRecorder;
UIInterface gUIInterface;
CText gText;

// The name table of src/game/rides/RideDB.cpp, src/game/save/SaveLoad.cpp (the other functions of that file, never called here) needs the text object and the string table to link:
// stand-ins that do nothing.
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}

template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

int32_t TbITable::FormatString(char16_t*, uint32_t, uint32_t, ...) const { return 0; }

// The two scratch buffers the stub file defines (the program's data at 0x1034c2d8 and 0x1034c31a).
extern char16_t gObjectSignNameBuffer[0x21];
extern char16_t gObjectUINameBuffer[0x21];

// TheGameType()'s object and its guard byte (gLoansGameTypeStorage and gLoansGameTypeGuard, FS_Online.h; the Mac's data at 0x102b3aa0 and the byte at
// 0x102b3aa4): the wrappers set both, so a case can start with the game type "not yet constructed" or already constructed, as the original's callers find it.

static_assert(offsetof(CObject, mObjectType) == 0x0e && offsetof(CObject, mRotation) == 0x10 && offsetof(CObject, mF20) == 0x20 &&
              offsetof(CObject, mF24) == 0x24 && offsetof(CObject, mFlags) == 0x2e && offsetof(CObject, mF32) == 0x32 &&
              offsetof(CObject, mF34) == 0x34 && offsetof(CObject, mF4C) == 0x4c && offsetof(CObject, mF50) == 0x50 &&
              offsetof(CObject, mF54) == 0x54 && offsetof(CObject, mF58) == 0x58 && offsetof(CObject, mF59) == 0x59 &&
              offsetof(CObject, mSignA) == 0x3c4 && offsetof(CObject, mSignB) == 0x406,
              "the fields the wrappers and the module read");

namespace {

constexpr uintptr_t kTail = 0x1da70c;   // CWorld::mTurn: the tail block the module hands out starts here

RideInfo* gInfo = nullptr;               // what GetRideInfo answers
ObjectControl* gControl = nullptr;       // what the control manager answers
CObject* gSelected = nullptr;            // what the UI's GetSelectedObject answers
void* gNewBlock = nullptr;               // the block the fake nothrow operator new handed out last
int32_t gCtorType = 0;                   // the type the game type's constructor sets
alignas(16) unsigned char gScript[16];   // what RSSE_GetScriptPointer answers

uint64_t AddressOf(const void* pointer) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)); }
CObject* Obj(uint64_t block) { return reinterpret_cast<CObject*>(static_cast<uintptr_t>(block)); }
RideInfo* RideAt(uint64_t address) { return reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(address)); }
template <class T>
T* Ptr(uint64_t address) { return reinterpret_cast<T*>(static_cast<uintptr_t>(address)); }

// The size the Mac's operator new is asked for: the host's master save holds std containers and is larger than the Mac's 0x4c bytes.
int64_t MacSize(std::size_t size) {
    if (size == sizeof(CFSGlobalMasterSave)) return 0x4c;
    return static_cast<int64_t>(size);
}

// ---- the crash guard
sigjmp_buf gJump;
volatile sig_atomic_t gArmed = 0;
volatile sig_atomic_t gCrashed = 0;
volatile uint64_t gResult = 0;

void OnCrash(int) {
    if (gArmed) siglongjmp(gJump, 1);
    std::abort();
}

template <class F>
void Guarded(F&& body) {
    struct sigaction action;
    struct sigaction oldSegv;
    struct sigaction oldBus;
    std::memset(&action, 0, sizeof(action));
    action.sa_handler = OnCrash;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &action, &oldSegv);
    sigaction(SIGBUS, &action, &oldBus);
    gCrashed = 0;
    gResult = 0;
    if (sigsetjmp(gJump, 1) == 0) {
        gArmed = 1;
        body();
    } else {
        gCrashed = 1;
    }
    gArmed = 0;
    sigaction(SIGSEGV, &oldSegv, nullptr);
    sigaction(SIGBUS, &oldBus, nullptr);
}

}  // namespace

// ---- the fakes (the same events as the PowerPC recorders of tests/test_ObjectInfo.py)

// Every fetch of a ride record is counted: the original fetches again for every field it reads.
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return gInfo;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    return {mThingId};
}

void CThing::FinalDestroy() { ev("CThing::FinalDestroy", {lab(this)}); }

ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    ev("CControlManager::GetObjectControl", {lab(this), type.mIndex, flag});
    return gControl;
}

const ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) const {
    ev("CControlManager::GetObjectControl const", {lab(this), type.mIndex, flag});
    return gControl;
}

void CWorld::Unregister(CObject* object) { ev("CWorld::Unregister", {lab(this), lab(object)}); }

CBankAccount* CWorld::GetBankAccount() {
    ev("CWorld::GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(static_cast<uintptr_t>(0x6000));
}

void CBankAccount::MakeDeposit(uint32_t amount) { ev("CBankAccount::MakeDeposit", {lab(this), amount}); }

void CMap::RemoveRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("CMap::RemoveRegionalEffector", {lab(this), static_cast<int32_t>(effector), cell.mIndex});
}

// PostMessage logs the centre, the message's type, flags and thing id (the only message it is given is the ride deleted).
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {lab(this), static_cast<int64_t>(message.GetType()), static_cast<int64_t>(message.mFlags),
                       static_cast<int64_t>(static_cast<const CMsgRideDeleted&>(message).mThingId)});
    return 1;
}

// The message centre (0x1d0 bytes) and the master save (0x4c bytes on the Mac) are allocated with the nothrow operator new, filled with 0xa5; the queue
// "newnull" says when it fails.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {MacSize(size), static_cast<int64_t>(fail)});
    if (fail) return nullptr;
    gNewBlock = std::malloc(size);
    std::memset(gNewBlock, 0xa5, size);
    return gNewBlock;
}

// The Mac builds the 29 headers with one __construct_array call (logged once, with the array's new label); the first header stands for it.
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewBlock) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}

CSubscriptionHeader::~CSubscriptionHeader() {}

CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave", {newlab(this)}); }

void CFSGlobalMasterSave::SetSignNameA(const char16_t* text) {
    ev("CFSGlobalMasterSave::SetSignNameA", {lab(this), lab(text)}, {narrow(text).c_str()});
}

void CFSGlobalMasterSave::SetSignNameB(const char16_t* text) {
    ev("CFSGlobalMasterSave::SetSignNameB", {lab(this), lab(text)}, {narrow(text).c_str()});
}

void CActionRecorder::RecordAction(RecordedAction action, uint16_t value, uint8_t length, const char16_t* text) {
    ev("RecordAction", {lab(this), static_cast<int32_t>(action), value, length, lab(text)}, {narrow(text).c_str()});
}

// src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/SkirtDeform.cpp, src/game/ui/ui_staffPanel.cpp (CObject::IsCoaster, unit v5L07) also holds LocatorByeByeKid: its two callees, never called here.
CThing* gStaffPanelThing;
void RemoveSlideoutPanel(int16_t) {}

CObject* UIInterface::GetSelectedObject() {
    ev("UIInterface::GetSelectedObject", {lab(this)});
    return gSelected;
}

void UIInterface::SetSelectedObject(CObject* object) { ev("UIInterface::SetSelectedObject", {lab(this), lab(object)}); }

// The five queries of the ride the original asks: the answers are the case's queues.
int32_t CObject::IsDirty() const {
    ev("CObject::IsDirty", {lab(this)});
    return static_cast<int32_t>(ret("CObject::IsDirty", 0));
}

uint32_t CObject::GetCurrentResidualValue() const {
    ev("CObject::GetCurrentResidualValue", {lab(this)});
    return static_cast<uint32_t>(ret("CObject::GetCurrentResidualValue", 0));
}

uint32_t CObject::GetPricePerUse() const {
    ev("CObject::GetPricePerUse", {lab(this)});
    return static_cast<uint32_t>(ret("CObject::GetPricePerUse", 0));
}

uint32_t CObject::GetPrizeValue() const {
    ev("CObject::GetPrizeValue", {lab(this)});
    return static_cast<uint32_t>(ret("CObject::GetPrizeValue", 0));
}

uint8_t CObject::GetChanceOfWinning() const {
    ev("CObject::GetChanceOfWinning", {lab(this)});
    return static_cast<uint8_t>(ret("CObject::GetChanceOfWinning", 0));
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {script});
    return reinterpret_cast<RSSE_SCRIPT*>(gScript);
}

int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("RSSE_Getvar", {lab(script), index});
    return static_cast<int32_t>(ret("RSSE_Getvar", 0));
}

void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }

void RSSE_KillScript(int32_t handle, int32_t flag) { ev("RSSE_KillScript", {handle, flag}); }

uint32_t RSSE_GetCoasterHandle(int32_t handle) {
    ev("RSSE_GetCoasterHandle", {handle});
    return static_cast<uint32_t>(ret("RSSE_GetCoasterHandle", 0));
}

// The two floats come from the queues as bit patterns: a translation that swaps them gets the other one.
void GetCoasterPeepStats(uint32_t coaster, float* first, float* second) {
    ev("GetCoasterPeepStats", {coaster});
    const uint32_t a = static_cast<uint32_t>(ret("peep_first", 0));
    const uint32_t b = static_cast<uint32_t>(ret("peep_second", 0));
    std::memcpy(first, &a, 4);
    std::memcpy(second, &b, 4);
}

// The four counts are logged as they are given (the original zeroes them) and then set from the "tracks" queue.
int32_t BMP_CountTrackSections(int32_t ride, int32_t* a, int32_t* b, int32_t* c, int32_t* d) {
    ev("BMP_CountTrackSections", {ride, *a, *b, *c, *d});
    *a = static_cast<int32_t>(ret("tracks", 0));
    *b = static_cast<int32_t>(ret("tracks", 0));
    *c = static_cast<int32_t>(ret("tracks", 0));
    *d = static_cast<int32_t>(ret("tracks", 0));
    return 0;
}

// The game type's constructor logs and sets the type the case names.
CGameType::CGameType() {
    ev("CGameType", {1});
    mType = static_cast<GameType>(gCtorType);
}

// The assertion logs its condition, the format and the one word the original passes (the host reads the ride type; the Mac's word holds the type in its
// high halfword).
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int32_t value = va_arg(arguments, int32_t);
    va_end(arguments);
    ev("GAME_ASSERTMSG", {condition, value & 0xffff}, {format});
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_oin_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"db", &gRideDatabase},
        {"ui", &gUIInterface},
        {"rec", &gActionRecorder},
        {"script", gScript},
        {"signbuf", gObjectSignNameBuffer},
        {"uibuf", gObjectUINameBuffer},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return AddressOf(entry.address);
    return 0;
}

uint64_t tpw_oin_alloc(uint32_t size) { return AddressOf(tpw::ArenaAllocZeroed(size)); }

// The layout the module assumes: sizeof(CObject); the host offset of value slot 0 of a ride record, the stride between slots, the offset of the item name and
// of the UI type; sizeof(RideInfo), sizeof(ObjectControl); the world's gates thing and its word at +0x1da738; sizeof(CMsgRideDeleted).
int32_t tpw_oin_layout(int64_t* out) {
    alignas(16) static unsigned char probe[sizeof(RideInfo)];
    RideInfo* info = reinterpret_cast<RideInfo*>(probe);
    unsigned char* slot0 = reinterpret_cast<unsigned char*>(&info->Value(static_cast<RideBalanceSlot>(0)));
    unsigned char* slot1 = reinterpret_cast<unsigned char*>(&info->Value(static_cast<RideBalanceSlot>(1)));
    out[0] = sizeof(CObject);
    out[1] = slot0 - probe;
    out[2] = slot1 - slot0;
    out[3] = reinterpret_cast<unsigned char*>(&info->mItemName) - probe;
    out[4] = reinterpret_cast<unsigned char*>(&info->mUIType) - probe;
    out[5] = sizeof(RideInfo);
    out[6] = sizeof(ObjectControl);
    out[7] = offsetof(CWorld, mGatesThing) - kTail;
    out[8] = offsetof(CWorld, mF1DA738) - kTail;
    out[9] = sizeof(CMsgRideDeleted);
    return 10;
}

// ---- the world the call finds
void tpw_oin_setup(uint64_t tail, uint64_t info, uint64_t control, uint64_t selected) {
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(tail) - kTail);
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(0x7000));
    gInfo = RideAt(info);
    gControl = Ptr<ObjectControl>(control);
    gSelected = Ptr<CObject>(selected);
}

void tpw_oin_set_centre(uint64_t centre) { gMessageCentre = Ptr<CMessageCentre>(centre); }
void tpw_oin_set_master(uint64_t master) { gMasterSave = Ptr<CFSGlobalMasterSave>(master); }
void tpw_oin_set_recording(int32_t value) { gActionRecorder.mRecording = value; }
uint64_t tpw_oin_centre() { return AddressOf(gMessageCentre); }
uint64_t tpw_oin_master() { return AddressOf(gMasterSave); }

// The game type: guard set (already constructed, of the type) or not (the call constructs it, with ctortype). The type is read from the static itself, so a
// translation that skips the guard and the constructor is seen as a value, not constructed by the reader.
void tpw_oin_game(int32_t constructed, int32_t type, int32_t ctortype) {
    reinterpret_cast<CGameType*>(gLoansGameTypeStorage)->mType = static_cast<GameType>(type);
    gLoansGameTypeGuard = constructed ? 1 : 0;
    gCtorType = ctortype;
}

int32_t tpw_oin_game_type() { return static_cast<int32_t>(reinterpret_cast<CGameType*>(gLoansGameTypeStorage)->mType); }
int32_t tpw_oin_game_guard() { return gLoansGameTypeGuard != 0; }

// Never called: it makes this library emit TheGameType()'s static and guard whatever the stubs do.
void tpw_oin_game_keep() { (void)TheGameType(); }

// The ride database: the count, the cached id and ride, and the first count records (a ride record of the id, the rest zero).
void tpw_oin_db_set(int32_t count, uint32_t cached_id, uint64_t cached_ride, const uint64_t* rides) {
    gRideDatabase.mRideCount = count;
    gRideDatabase.mCachedId = static_cast<uint16_t>(cached_id);
    gRideDatabase.mCachedRide = RideAt(cached_ride);
    for (int32_t i = 0; i < count && i < 0x14a; ++i) gRideDatabase.mRides[i] = RideAt(rides[i]);
}

void tpw_oin_db_read(int64_t* out) {
    out[0] = gRideDatabase.mRideCount;
    out[1] = gRideDatabase.mCachedId;
    out[2] = static_cast<int64_t>(AddressOf(gRideDatabase.mCachedRide));
}

uint64_t tpw_oin_ride_new(uint32_t id) {
    RideInfo* record = static_cast<RideInfo*>(tpw::ArenaAllocZeroed(sizeof(RideInfo)));
    record->mId = static_cast<uint16_t>(id);
    return AddressOf(record);
}

int32_t tpw_oin_crashed() { return gCrashed; }

// ---- the functions under test (a pointer is a 64 bit word; the id arguments are the plain halfwords)
uint64_t tpw_oin_upgrades_what(uint64_t block) {
    Guarded([&] { gResult = Obj(block)->UpgradesWhat().mIndex; });
    return gResult;
}

uint64_t tpw_oin_get_control(uint64_t block) {
    Guarded([&] { gResult = AddressOf(Obj(block)->GetControl()); });
    return gResult;
}

uint64_t tpw_oin_get_file_name(uint64_t block) {
    Guarded([&] { gResult = AddressOf(Obj(block)->GetFileName()); });
    return gResult;
}

void tpw_oin_ui_name(uint64_t block, uint64_t out, uint32_t size) {
    Guarded([&] { Obj(block)->GetUIName(Ptr<char16_t>(out), size); });
}

void tpw_oin_sign_a(uint64_t block, uint64_t out, uint32_t size) {
    Guarded([&] { Obj(block)->GetSignNameA(Ptr<char16_t>(out), size); });
}

void tpw_oin_sign_b(uint64_t block, uint64_t out, uint32_t size) {
    Guarded([&] { Obj(block)->GetSignNameB(Ptr<char16_t>(out), size); });
}

// The master save's pointer is logged after the call (its label tells whether the translation made one, kept the old one or stored NULL).
void tpw_oin_set_sign_a(uint64_t block, uint64_t text) {
    Guarded([&] { Obj(block)->SetSignNameA(Ptr<const char16_t>(text)); });
    ev("gMasterSave", {lab(gMasterSave)});
}

void tpw_oin_set_sign_b(uint64_t block, uint64_t text) {
    Guarded([&] { Obj(block)->SetSignNameB(Ptr<const char16_t>(text)); });
    ev("gMasterSave", {lab(gMasterSave)});
}

int32_t tpw_oin_is_coaster(uint64_t block) {
    Guarded([&] { gResult = static_cast<uint32_t>(Obj(block)->IsCoaster()); });
    return static_cast<int32_t>(gResult);
}

int32_t tpw_oin_is_track_ride(uint64_t block) {
    Guarded([&] { gResult = static_cast<uint32_t>(Obj(block)->IsTrackRide()); });
    return static_cast<int32_t>(gResult);
}

int32_t tpw_oin_prospective(uint64_t block, int32_t a, int32_t b, int32_t c) {
    Guarded([&] { gResult = static_cast<uint32_t>(Obj(block)->GetProspectiveExcitementLevel(a, b, c)); });
    return static_cast<int32_t>(gResult);
}

// The flag is the Mac's register: the original tests its low byte.
uint32_t tpw_oin_excitement(uint64_t block, uint32_t flag_word) {
    Guarded([&] { gResult = Obj(block)->GetExcitementLevel((flag_word & 0xff) != 0); });
    return static_cast<uint32_t>(gResult);
}

void tpw_oin_stand_point(uint64_t block, int32_t which, uint64_t x, uint64_t y) {
    Guarded([&] { Obj(block)->GetStandPoint(which, Ptr<MapPos>(x), Ptr<MapPos>(y)); });
}

// The message centre is logged after the call (its label tells whether the translation made one).
void tpw_oin_final_destroy(uint64_t block) {
    Guarded([&] { Obj(block)->FinalDestroy(); });
    ev("gMessageCentre", {lab(gMessageCentre)});
}

// CMsgRideDeleted's constructor on a block, and the message read back: its type (the virtual GetType), flags and thing id.
void tpw_oin_msg_make(uint64_t block, uint32_t id) {
    Guarded([&] {
        new (Ptr<void>(block)) CMsgRideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(id)});
    });
}

void tpw_oin_msg_read(uint64_t block, int64_t* out) {
    const CMsgRideDeleted* message = Ptr<const CMsgRideDeleted>(block);
    out[0] = message->GetType();
    out[1] = message->mFlags;
    out[2] = message->mThingId;
}

// CRideDatabase::GetFileName of the plain id.
uint64_t tpw_oin_db_file_name(uint32_t id) {
    Guarded([&] { gResult = AddressOf(gRideDatabase.GetFileName(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(id)})); });
    return gResult;
}

}  // extern "C"
