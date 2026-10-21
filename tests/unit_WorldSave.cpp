// Test library for the world's save (unit w205; libtpw_unit_WorldSave.dylib, tests/test_WorldSave.py), main app:
//   src/game/Thing.cpp, src/game/World.cpp       CWorld::Save, SAVEVAR<CWorld::WorldState>, SAVEVAR<CThing::ThingModel>, CWorld::GetStaffHQ, CWorld::GetMechanicHQ,
//                            CMacroAI::~CMacroAI, CThing::CThing(const CSerialise&)
//   src/game/World.cpp, src/game/park/Map.cpp, src/game/save/ChunkFileReader.cpp        CMapAttribFileReader::~CMapAttribFileReader and the TbPtrList<CIndividualChunkHandler> destructor (with the list
//                            members they run)
//   src/game/ControlManager.cpp, src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/rides/FlyingRide.cpp, src/game/save/FS_MasterSave.cpp  the basic SAVEVAR instances (REAL on both sides: the file is the fake)
//
// The fake file: LbFile_Write records the bytes as hex and answers from its queues ("LbFile_Write" the result, "LbFile_Write:xfer" the
// transferred count), LbFile_GetPosition answers from its queue. Every other callee of Save is a fake that records the arguments the real one
// reads: the twenty Serialise functions (the world's three parts as offsets from the world, the things as the label of their block), the
// mouse (operator new answers from the queue "new" and logs which of the two sizes it was, the default mode's constructor takes a new label, the
// mouse's AssignMouseMode logs both labels), CTRACE and GAME_ASSERTMSG. The world is one static block of the Mac's size over the fill 0xa5, the
// thing table and its heads are the unit's own globals, the thing blocks are 0x80 bytes. The destructor tests build a macro layer object or a
// list (real nodes made by AddHead) in the host's memory; operator delete is the call log's and this file keeps every freed block.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#pragma clang diagnostic ignored "-Winvalid-offsetof"   // tpw_wsv_layout takes member offsets of CWorld, which is not a standard-layout class

namespace {
bool KeepFreed(void*, bool) { return true; }
}  // namespace
#define TPW_CALLLOG_EVENTS 20000
#define TPW_CALLLOG_LABELS 512
#define TPW_CALLLOG_QUEUE_VALUES 16500
#define TPW_RELEASE_OBSERVER(p, array) KeepFreed(p, array)
#include "calllog.cpp"

#include "AdvisorQueue.h"
#include "ChunkFile.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_System.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "Mouse.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "StaffDB.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
CSaveLoad gSaveLoad;
CExitError gExitError;       // read by the chunk reader (src/game/World.cpp, src/game/park/Map.cpp, src/game/save/ChunkFileReader.cpp), which is linked for its destructors
// The balance table is read only by ChunkFile.cpp's map parsing, which this unit never runs: its storage is bytes under the global's symbol
// (a real object would need the table's constructor, destructor and vtable from files this library does not compile).
alignas(16) unsigned char gMainBalanceBytes[sizeof(CMainBalance)] __asm__("_gMainBalance");
CThingArrayEntry gThingArray[kThingArraySize];
CThingArrayEntry* gUsedThingHead;
CThingArrayEntry* gFreeThingHead;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;

namespace {

constexpr int32_t kMaxBlocks = 64;
constexpr std::size_t kBlockSize = 0x80;
constexpr uint32_t kSentinelA = 0x11223344u;
constexpr uint32_t kSentinelB = 0x55667788u;

alignas(16) unsigned char gWorldBytes[sizeof(CWorld)];
alignas(16) unsigned char gBlocks[kMaxBlocks][kBlockSize];
alignas(16) unsigned char gOldMouseBytes[sizeof(CMouse)];
char gFileHandle;   // the fake file's address (a label for the log)

// The snapshot the "unchanged" test compares with: the world, the table (all rows), the heads and the blocks.
unsigned char* gSnapWorld;
CThingArrayEntry* gSnapTable;
unsigned char (*gSnapBlocks)[kBlockSize];
CThingArrayEntry* gSnapHeads[2];

// The twist of the getters: GAME_ASSERTMSG rewrites a halfword of the world (a read of it after the call would show).
uint16_t* gTwistTarget;
uint16_t gTwistValue;

// The destructor tests' object and the list inside it.
int32_t gWhat = -1;
void* gObject;
TbTList<CIndividualChunkHandler*, CIndividualChunkHandler*>* gList;
CMapAttribFileReader* gReader;

CWorld* World() { return reinterpret_cast<CWorld*>(gWorldBytes); }

template <class T>
T* Launder(T* pointer) {
    __asm__ volatile("" : "+r"(pointer));
    return pointer;
}

// A world member's offset from the world, else 0x40000000 plus the label of the object (the things' blocks).
int64_t Where(const void* p) {
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    const uintptr_t b = reinterpret_cast<uintptr_t>(gWorldBytes);
    if (a >= b && a - b < sizeof(CWorld)) return static_cast<int64_t>(a - b);
    return 0x40000000 + lab(p);
}

// 0 for NULL, the block's number plus one, -1 for anything else.
int64_t BlockNumber(const void* p) {
    if (p == nullptr) return 0;
    for (int32_t k = 0; k < kMaxBlocks; ++k)
        if (p == gBlocks[k]) return k + 1;
    return -1;
}

std::string Hex(const void* data, uint32_t size) {
    static const char kDigits[] = "0123456789abcdef";
    const auto* p = static_cast<const uint8_t*>(data);
    std::string out;
    for (uint32_t i = 0; i < size; ++i) {
        out += kDigits[p[i] >> 4];
        out += kDigits[p[i] & 15];
    }
    return out;
}

// The number of arguments a format takes ("%%" takes none).
int Args(const char* format) {
    int n = 0;
    for (const char* p = format; *p; ++p) {
        if (*p != '%') continue;
        if (p[1] == '%') {
            ++p;
            continue;
        }
        ++n;
    }
    return n;
}

CIndividualChunkHandler* Handler(int32_t k) { return reinterpret_cast<CIndividualChunkHandler*>(static_cast<uintptr_t>(0x1000 + 16 * k)); }

}  // namespace

// ---- the fakes
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
    if (gTwistTarget != nullptr) *gTwistTarget = gTwistValue;
}

void CTRACE(uint32_t level, const char* format, ...) {
    int64_t values[3] = {0, 0, 0};
    const int n = Args(format);
    va_list ap;
    va_start(ap, format);
    for (int i = 0; i < n && i < 3; ++i) values[i] = static_cast<int64_t>(va_arg(ap, uint32_t));
    va_end(ap);
    if (n == 0) ev("CTRACE", {level}, {format});
    else if (n == 1) ev("CTRACE", {level, values[0]}, {format});
    else if (n == 2) ev("CTRACE", {level, values[0], values[1]}, {format});
    else ev("CTRACE", {level, values[0], values[1], values[2]}, {format});
}

void* LbMem_Alloc(uint32_t size) {
    ev("LbMem_Alloc", {size});
    return nullptr;
}
void LbMem_Free(void* block) { ev("LbMem_Free", {lab(block)}); }
bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    return nullptr;
}
void DEBUG_MESSAGE(const char* message, uint32_t flags) { ev("DEBUG_MESSAGE", {flags}, {message}); }
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) { ev("CExitError::SetError", {type, value}, {text != nullptr ? text : ""}); }

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    std::memset(buffer, 0, length);
    *transferred = 0;
    return TbError(-1);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), length}, {Hex(buffer, length).c_str()});
    const int64_t forced = ret("LbFile_Write:xfer", -1);
    *transferred = forced >= 0 ? static_cast<uint32_t>(forced) : length;
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

int32_t LbFile_GetPosition(void* file) {
    ev("LbFile_GetPosition", {lab(file)});
    return static_cast<int32_t>(ret("LbFile_GetPosition"));
}

// The twenty Serialise functions Save reaches (the first three are the world's parts, the rest the things').
#define TPW_SERIALISE_FAKE(Class)                                                                          \
    int32_t Class::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {                 \
        ev(#Class "::Serialise", {Where(this), lab(file), save, version, unused});                        \
        return static_cast<int32_t>(ret(#Class "::Serialise"));                                           \
    }
TPW_SERIALISE_FAKE(CControlManager)
TPW_SERIALISE_FAKE(CMacroAI)
TPW_SERIALISE_FAKE(CMap)
TPW_SERIALISE_FAKE(CPerson)
TPW_SERIALISE_FAKE(CObject)
TPW_SERIALISE_FAKE(CMechanic)
TPW_SERIALISE_FAKE(CHandyman)
TPW_SERIALISE_FAKE(CEntertainer)
TPW_SERIALISE_FAKE(CGuard)
TPW_SERIALISE_FAKE(CResearcher)
TPW_SERIALISE_FAKE(CStaffHQ)
TPW_SERIALISE_FAKE(CMechanicHQ)
TPW_SERIALISE_FAKE(CAdvisor)
TPW_SERIALISE_FAKE(CTagSystem)
TPW_SERIALISE_FAKE(CParkAnalyser)
TPW_SERIALISE_FAKE(CResearchLab)
TPW_SERIALISE_FAKE(CWeather)
TPW_SERIALISE_FAKE(CBankAccount)
TPW_SERIALISE_FAKE(CUIMsgReceiver)
TPW_SERIALISE_FAKE(CChallengeHQ)

// The mouse: the Mac makes the mouse (1 byte) and its default mode (8 bytes) with the plain operator new; the host's sizes differ, so the log
// holds which of the two it was (1 the mouse, 2 the mode). The block is never freed.
static_assert(sizeof(CMouse) != sizeof(CMMDefault), "the two sizes must tell the allocations apart");
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    ev("new", {size == sizeof(CMouse) ? 1 : (size == sizeof(CMMDefault) ? 2 : 0)});
    if (ret("new", 1) == 0) return nullptr;
    return std::calloc(1, size);
}

CMMDefault::CMMDefault() { ev("CMMDefault::CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("CMouse::AssignMouseMode", {lab(this), lab(mode)}); }

// The staff database's destructor (the Mac passes a flag that is not positive: no delete there).
CStaffDB::~CStaffDB() { ev("CStaffDB::~CStaffDB", {lab(this), 0}); }

extern "C" {

// The host address of an object by the module's name for it (the labels' keys).
uint64_t tpw_wsv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"file", &gFileHandle},
        {"gSaveLoad", &gSaveLoad},
        {"oldmouse", gOldMouseBytes},
        {"object", nullptr},
    };
    if (std::strcmp(name, "object") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gObject));
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The offsets and sizes the module relies on (HOST_LAYOUT): the world's size, the 27 members of its tail, the map and control manager
// offsets, the thing kind's offset and the table's size.
int32_t tpw_wsv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CWorld));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mStartTime));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA710));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mPersonCount));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mStaffHQThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMechanicHQThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mAdvisorThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTagSystemThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkAnalyserThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mResearchLabThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mWeatherThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mBankAccountThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mUIMsgReceiverThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mArrivalVehicleThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mBusThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mSeaplaneThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mFerryThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mGatesThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mLightsThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mChallengeHQThing));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA738));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73C));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73E));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA740));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA742));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA744));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA746));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(World()->GetMap()) - reinterpret_cast<uintptr_t>(World()));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(World()->GetControlManager()) - reinterpret_cast<uintptr_t>(World()));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mKind));
    out[n++] = static_cast<int64_t>(kThingArraySize);
    return n;
}

// ---- the world, the blocks, the table, the mouse
uint64_t tpw_wsv_world() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gWorldBytes)); }
uint64_t tpw_wsv_block(int32_t k) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gBlocks[k])); }

// One row of the table: the thing's block (number plus one, 0 for none), the index word, and the next used, previous used and next free rows
// (row plus one, 0 for none). Every row is whole: nothing in it is left from an earlier case (tpw_wsv_reset).
void tpw_wsv_row(int32_t row, int32_t blockPlusOne, uint32_t index, int32_t nextPlusOne, int32_t prevPlusOne, int32_t freePlusOne) {
    CThingArrayEntry& entry = gThingArray[row];
    entry.mThing = blockPlusOne != 0 ? static_cast<void*>(gBlocks[blockPlusOne - 1]) : nullptr;
    entry.mF04[0] = index;
    entry.mF04[1] = entry.mF04[2] = entry.mF04[3] = 0;
    entry.mNextUsed = nextPlusOne != 0 ? &gThingArray[nextPlusOne - 1] : nullptr;
    entry.mPrevUsed = prevPlusOne != 0 ? &gThingArray[prevPlusOne - 1] : nullptr;
    entry.mNextFree = freePlusOne != 0 ? &gThingArray[freePlusOne - 1] : nullptr;
}

// The state every case starts from: the world and the blocks over the fill 0xa5, the table in its default (no thing, the row's own index, no
// links), the heads NULL, no mouse, the sentinels in the mouse words, no twist and no destructor object.
void tpw_wsv_reset() {
    std::memset(gWorldBytes, 0xa5, sizeof(gWorldBytes));
    std::memset(gBlocks, 0xa5, sizeof(gBlocks));
    for (int32_t row = 0; row < kThingArraySize; ++row) tpw_wsv_row(row, 0, static_cast<uint32_t>(row), 0, 0, 0);
    gUsedThingHead = nullptr;
    gFreeThingHead = nullptr;
    gMouse = nullptr;
    gMouseWord1E0 = static_cast<int32_t>(kSentinelA);
    gMouseWord1E4 = static_cast<int32_t>(kSentinelB);
    gTwistTarget = nullptr;
    gTwistValue = 0;
    gObject = nullptr;
    gList = nullptr;
    gReader = nullptr;
}

void tpw_wsv_heads(int32_t usedPlusOne, int32_t freePlusOne) {
    gUsedThingHead = usedPlusOne != 0 ? &gThingArray[usedPlusOne - 1] : nullptr;
    gFreeThingHead = freePlusOne != 0 ? &gThingArray[freePlusOne - 1] : nullptr;
}

// The mouse before the call (0: none, 1: the old one) and the two words under the sentinels.
void tpw_wsv_mouse(int32_t which) {
    gMouse = which == 1 ? reinterpret_cast<CMouse*>(gOldMouseBytes) : nullptr;
    gMouseWord1E0 = static_cast<int32_t>(kSentinelA);
    gMouseWord1E4 = static_cast<int32_t>(kSentinelB);
}
int32_t tpw_wsv_get_mouse() {
    if (gMouse == nullptr) return 0;
    return gMouse == reinterpret_cast<CMouse*>(gOldMouseBytes) ? 1 : 2;
}
void tpw_wsv_get_words(uint32_t* out) {
    out[0] = static_cast<uint32_t>(gMouseWord1E0);
    out[1] = static_cast<uint32_t>(gMouseWord1E4);
}

// The snapshot: copies of the world, the table, the heads and the blocks, and the test that none of them changed.
void tpw_wsv_snapshot() {
    if (gSnapWorld == nullptr) {
        gSnapWorld = static_cast<unsigned char*>(std::malloc(sizeof(CWorld)));
        gSnapTable = static_cast<CThingArrayEntry*>(std::malloc(sizeof(gThingArray)));
        gSnapBlocks = static_cast<unsigned char(*)[kBlockSize]>(std::malloc(sizeof(gBlocks)));
    }
    std::memcpy(gSnapWorld, gWorldBytes, sizeof(CWorld));
    std::memcpy(gSnapTable, gThingArray, sizeof(gThingArray));
    std::memcpy(gSnapBlocks, gBlocks, sizeof(gBlocks));
    gSnapHeads[0] = gUsedThingHead;
    gSnapHeads[1] = gFreeThingHead;
}
int32_t tpw_wsv_unchanged() {
    return std::memcmp(gSnapWorld, gWorldBytes, sizeof(CWorld)) == 0 && std::memcmp(gSnapTable, gThingArray, sizeof(gThingArray)) == 0 &&
           std::memcmp(gSnapBlocks, gBlocks, sizeof(gBlocks)) == 0 && gSnapHeads[0] == gUsedThingHead && gSnapHeads[1] == gFreeThingHead;
}

// ---- the functions, one wrapper each with C types only
int32_t tpw_wsv_save() { return Launder(World())->Save(&gFileHandle); }

// SAVEVAR of the world state (0) or the thing model (1): the value is read from and written back to *value.
int32_t tpw_wsv_savevar(int32_t which, uint32_t* value, const char* name, int32_t noLog) {
    if (which == 0) {
        CWorld::WorldState copy = static_cast<CWorld::WorldState>(*value);
        const int32_t answer = SAVEVAR<CWorld::WorldState>(&gFileHandle, &copy, name, noLog);
        *value = static_cast<uint32_t>(copy);
        return answer;
    }
    CThing::ThingModel copy = static_cast<CThing::ThingModel>(*value);
    const int32_t answer = SAVEVAR<CThing::ThingModel>(&gFileHandle, &copy, name, noLog);
    *value = static_cast<uint32_t>(copy);
    return answer;
}

// The two headquarters getters (0 staff, 1 mechanic): the block number of the answer.
int64_t tpw_wsv_get_hq(int32_t which) {
    CWorld* world = Launder(World());
    return BlockNumber(which == 0 ? static_cast<void*>(world->GetStaffHQ()) : static_cast<void*>(world->GetMechanicHQ()));
}
void tpw_wsv_twist(int32_t offset, uint32_t value) {
    gTwistTarget = offset >= 0 ? reinterpret_cast<uint16_t*>(gWorldBytes + offset) : nullptr;
    gTwistValue = static_cast<uint16_t>(value);
}

// CThing's serialise constructor, placed over block k: the block number of the answer.
int64_t tpw_wsv_thing_ctor(int32_t k) {
    alignas(8) static unsigned char dummy[16];
    const CSerialise& serialise = *reinterpret_cast<const CSerialise*>(dummy);
    CThing* const thing = new (gBlocks[k]) CThing(serialise);
    return BlockNumber(thing);
}

// ---- the destructors: an object of the kind (0 macro layer, 1 chunk reader, 2 pointer list) with `nodes` nodes (the head holds Handler(0)).
void tpw_wsv_build(int32_t what, int32_t nodes) {
    gWhat = what;
    gList = nullptr;
    gReader = nullptr;
    gObject = nullptr;
    if (what == 0) {
        gObject = new CMacroAI;
    } else if (what == 1) {
        gReader = new CMapAttribFileReader(nullptr);
        for (int32_t i = nodes - 1; i >= 0; --i) gReader->AddChunkHandler(Handler(i));
        gList = gReader;
        gObject = gReader;
    } else {
        TbPtrList<CIndividualChunkHandler>* list = new TbPtrList<CIndividualChunkHandler>;
        for (int32_t i = nodes - 1; i >= 0; --i) list->AddHead(Handler(i));
        gList = list;
        gObject = list;
    }
}
uint64_t tpw_wsv_node(int32_t k) {
    TbTList<CIndividualChunkHandler*, CIndividualChunkHandler*>::CNode* node = gList->mHead;
    for (int32_t i = 0; i < k && node != nullptr; ++i) node = node->mNext;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(node));
}
// The destruction: the deleting form (the Mac's flag is positive) or the destructor alone.
void tpw_wsv_destroy(int32_t what, int32_t deleting) {
    if (what == 0) {
        CMacroAI* const object = static_cast<CMacroAI*>(gObject);
        if (deleting) delete object; else object->~CMacroAI();
    } else if (what == 1) {
        if (deleting) delete gReader; else gReader->~CMapAttribFileReader();
    } else {
        TbPtrList<CIndividualChunkHandler>* const list = static_cast<TbPtrList<CIndividualChunkHandler>*>(gObject);
        if (deleting) delete list; else list->~TbPtrList();
    }
}
// What the destruction leaves: the list's count and whether its head and tail are NULL, the reader's magic and its handler's chunk id.
void tpw_wsv_state(uint32_t* out) {
    out[0] = gList != nullptr ? static_cast<uint32_t>(gList->mCount) : 0;
    out[1] = gList != nullptr ? (gList->mHead == nullptr ? 1 : 0) : 0;
    out[2] = gList != nullptr ? (gList->mTail == nullptr ? 1 : 0) : 0;
    out[3] = gReader != nullptr ? gReader->mMagic : 0;
    out[4] = gReader != nullptr ? gReader->mHandler.mChunkId : 0;
}

}  // extern "C"
