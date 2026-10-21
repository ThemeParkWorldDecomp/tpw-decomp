// Test library for the lobby's small leaves, part 3 (libtpw_unit_LobbyLeaves3.dylib; see tests/test_LobbyLeaves3.py, unit w59):
// the ride database's one-argument GetTypeName and the two sign-name members (src/game/rides/RideDB.cpp, src/game/save/SaveLoad.cpp), CGame::GetLocalisedThemeGateNames
// (src/game/Game.cpp), FLY_Close (src/game/rides/FlyingRide.cpp) and RSSE_Close (src/game/rides/rsse.cpp).
//
// Real under them: the name builder's GetNameA and GetNameB, TPUniStrEq, the 16-bit C library (wcsncpy) and the bullfrog strings
// (the gate strings and the theme text). Fakes that record through calllog: the string table's FormatString (its text comes from
// the test), NS_MacDoze::_wcsicmp, LbMem_Free, FLY_RemoveBase, KillScript (answers from a queue of script numbers) and GAME_ASSERTMSG.
// The gate strings are RecordingString objects: their virtual operator=(const char16_t*) records the call and then assigns.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "Debug.h"
#include "Game.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPString.h"
#include "TbITable.h"
#include "Text.h"
#include "calllog.h"
#include "platform/macdoze.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files
// The text object is only a place for the string table's pointer (the name builder reads gText.mResourcePointers[20]); the storage
// has the symbol of gText so that the library does not need the text object's constructor and the font classes under it.
alignas(16) char gTextBlock[sizeof(CText)] __asm__("_gText");
CRideDatabase gRideDatabase;
alignas(16) char gGameBlock[sizeof(CGame)];   // the gate names function does not read its object

// String members the unit's sources reference and nothing here reaches.
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// (RideInfo::~RideInfo, 0x1015457c, is declared since unit w81 made ClearDatabase real: the ride records here are never destroyed by code under test)
RideInfo::~RideInfo() {}

namespace {

constexpr int32_t kTableLabel = 80;
constexpr int32_t kPool = 8;               // ride records a case can use
constexpr int32_t kScriptLabel = 10;       // label of script k
constexpr int32_t kSinLabel = 20;          // label of the flies' sine table
constexpr int32_t kGate1Label = 1;
constexpr int32_t kGate2Label = 2;

CText& Text() { return *reinterpret_cast<CText*>(gTextBlock); }

std::u16string gStrings[2048];             // the string table (OBJECT_NAMES) by number
char gTableBlob[16];                       // the string table object (only its address is used)
RideInfo gPool[kPool];                     // the ride records
std::u16string gSignA[kPool];
std::u16string gSignB[kPool];
char gScriptBlocks[8][16];                 // the scripts
char gSinBlock[16];                        // the sine table

int32_t Lower(int32_t c) { return c >= 'A' && c <= 'Z' ? c + 0x20 : c; }

std::u16string Make(const uint16_t* data, int32_t n) {
    return std::u16string(reinterpret_cast<const char16_t*>(data), static_cast<size_t>(n));
}

// The gate strings: an assignment from text is recorded (which string, what text) and then done by the real code. The heap blocks
// the real assignment moves are added up, so that the test can see the blocks of the function's own local string apart.
int32_t gAssignHeap[4];

struct RecordingString : TbDynamicStringTemplate<char16_t> {
    explicit RecordingString(const char16_t* text) : TbDynamicStringTemplate<char16_t>(text) {}
    TbDynamicStringTemplate<char16_t>& operator=(const char16_t* text) override {
        ev("Assign", {lab(this)}, {narrow(text).c_str()});
        int32_t before[4];
        for (int i = 0; i < 4; ++i) before[i] = tpw_cl_heap[i];
        TbDynamicStringTemplate<char16_t>& result = TbDynamicStringTemplate<char16_t>::operator=(text);
        for (int i = 0; i < 4; ++i) gAssignHeap[i] += tpw_cl_heap[i] - before[i];
        return result;
    }
};

// Writes zeros over the stack below the caller, so that the gate function's uninitialised buffers start as empty texts.
__attribute__((noinline)) void ScrubStack() {
    volatile char area[0x4000];
    for (size_t i = 0; i < sizeof area; ++i) area[i] = 0;
}

}  // namespace

// ---- the fakes
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {lab(this), size, id});
    const std::u16string& text = id < 2048 ? gStrings[id] : gStrings[0];
    if (size == 0) return 0;
    const uint32_t count = text.size() < size - 1 ? static_cast<uint32_t>(text.size()) : size - 1;
    for (uint32_t i = 0; i < count; ++i) out[i] = text[i];
    out[count] = 0;
    return static_cast<int32_t>(count);
}

int32_t NS_MacDoze::_wcsicmp(const char16_t* a, const char16_t* b) {
    ev("_wcsicmp", {}, {narrow(a).c_str(), narrow(b).c_str()});
    for (;; ++a, ++b) {
        const int32_t ca = Lower(*a);
        const int32_t cb = Lower(*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}

// (TPString.cpp's other members call these; nothing here reaches them)
int32_t NS_MacDoze::_stricmp(const char*, const char*) { return 0; }
int32_t NS_MacDoze::WideCharToMultiByte(uint32_t, uint32_t, const char16_t*, int32_t, char*, int32_t, const char*, int32_t*) {
    return 0;
}
int32_t NS_MacDoze::MultiByteToWideChar(uint32_t, uint32_t, const char*, int32_t, char16_t*, int32_t) { return 0; }
void* LbMem_Alloc(uint32_t) { return nullptr; }

void LbMem_Free(void* block) { ev("LbMem_Free", {lab(block)}); }
void FLY_RemoveBase(int32_t index) { ev("FLY_RemoveBase", {index}); }
// (the state word is recorded too: the original has marked the system closed before it asserts)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition, gRSSEState.mInitialised}, {format});
}
RSSE_SCRIPT* KillScript(RSSE_SCRIPT* script, int32_t flag) {
    ev("KillScript", {lab(script), flag});
    const int64_t next = ret("KillScript", -1);
    return next < 0 ? nullptr : reinterpret_cast<RSSE_SCRIPT*>(gScriptBlocks[next]);
}

extern "C" {

// A string of the string table (UTF-16 code units).
void tpw_ll_string(int32_t id, const uint16_t* data, int32_t n) { gStrings[id] = Make(data, n); }

// Clears the ride database and the pool and names the string table.
void tpw_ll_reset() {
    std::memset(&gRideDatabase, 0, sizeof gRideDatabase);
    for (int32_t k = 0; k < kPool; ++k) {
        std::memset(static_cast<void*>(&gPool[k]), 0, sizeof gPool[k]);
        gSignA[k].clear();
        gSignB[k].clear();
    }
    Text().mResourcePointers[20] = gTableBlob;
    tpw_cl_label(kTableLabel, gTableBlob);
}

// Ride record k: its number, type name, and the two sign texts (each present or NULL).
void tpw_ll_ride(int32_t k, int32_t id, const uint16_t* typeName, int32_t typeLength, int32_t hasA, const uint16_t* a,
                 int32_t lengthA, int32_t hasB, const uint16_t* b, int32_t lengthB) {
    RideInfo& ride = gPool[k];
    ride.mId = static_cast<uint16_t>(id);
    for (int32_t i = 0; i < typeLength; ++i) ride.mTypeName[i] = typeName[i];
    ride.mTypeName[typeLength] = 0;
    gSignA[k] = Make(a, lengthA);
    gSignB[k] = Make(b, lengthB);
    ride.mSignA = hasA ? gSignA[k].c_str() : nullptr;
    ride.mSignB = hasB ? gSignB[k].c_str() : nullptr;
}

// The database: `count` rides, ride i being pool record order[i]; the entries after the count (up to six) are stale pointers to
// pool records, as a table that once held more rides leaves them. The cache is (cachedId, pool record cachedIndex or none).
void tpw_ll_db(int32_t count, const int32_t* order, int32_t cachedId, int32_t cachedIndex) {
    gRideDatabase.mRideCount = count;
    for (int32_t i = 0; i < 6; ++i) gRideDatabase.mRides[i] = &gPool[order[i]];
    gRideDatabase.mCachedId = static_cast<uint16_t>(cachedId);
    gRideDatabase.mCachedRide = cachedIndex < 0 ? nullptr : &gPool[cachedIndex];
}

static int32_t PoolIndex(const void* p) {
    if (p == nullptr) return -1;
    for (int32_t k = 0; k < kPool; ++k)
        if (p == &gPool[k]) return k;
    return -2;
}

// One name operation: 0 the one-argument GetTypeName (info[0] the pool record the answer lies in, -1 for NULL, -2 for elsewhere;
// the answer's text goes to `text`, 0x72 units), 1 GetSignNameA, 2 GetSignNameB (into `buffer`, `size`). info[1], info[2] are the
// cache after the call (its id, its pool record).
void tpw_ll_names(int32_t op, int32_t id, char16_t* buffer, uint32_t size, char16_t* text, int32_t* info) {
    info[0] = -1;
    if (op == 0) {
        const char16_t* answer = gRideDatabase.GetTypeName(static_cast<uint16_t>(id));
        for (int32_t k = 0; k < kPool && answer != nullptr; ++k) {
            info[0] = answer == gPool[k].mTypeName ? k : info[0];
        }
        if (answer != nullptr && info[0] < 0) info[0] = -2;
        for (int32_t i = 0; i < 0x72; ++i) text[i] = answer != nullptr ? answer[i] : 0;
    } else if (op == 1) {
        gRideDatabase.GetSignNameA(static_cast<uint16_t>(id), buffer, size);
    } else {
        gRideDatabase.GetSignNameB(static_cast<uint16_t>(id), buffer, size);
    }
    info[1] = gRideDatabase.mCachedId;
    info[2] = PoolIndex(gRideDatabase.mCachedRide);
}

// The gate names: the theme text, the two gate strings' starting texts. out receives the two strings' texts (0x200 units each, zero
// after the end), their lengths (out2[0], out2[1]) and the blocks of the function's own local string (out2[2] news, out2[3] deletes).
void tpw_ll_gate(const uint16_t* theme, int32_t themeLength, const uint16_t* first, int32_t firstLength, const uint16_t* second,
                 int32_t secondLength, int32_t scrub, uint16_t* out1, uint16_t* out2, int32_t* out) {
    const std::u16string themeText = Make(theme, themeLength);
    const TbDynamicStringTemplate<char16_t> themeString(themeText.c_str());
    RecordingString gate1(Make(first, firstLength).c_str());
    RecordingString gate2(Make(second, secondLength).c_str());
    tpw_cl_label(kGate1Label, &gate1);
    tpw_cl_label(kGate2Label, &gate2);
    for (int i = 0; i < 4; ++i) gAssignHeap[i] = 0;
    int32_t before[4];
    for (int i = 0; i < 4; ++i) before[i] = tpw_cl_heap[i];
    if (scrub) ScrubStack();
    reinterpret_cast<const CGame*>(gGameBlock)->GetLocalisedThemeGateNames(themeString, gate1, gate2);
    out[0] = gate1.mLength;
    out[1] = gate2.mLength;
    out[2] = tpw_cl_heap[1] - before[1] - gAssignHeap[1];
    out[3] = tpw_cl_heap[3] - before[3] - gAssignHeap[3];
    for (int32_t i = 0; i < 0x200; ++i) out1[i] = 0;
    for (int32_t i = 0; i < 0x200; ++i) out2[i] = 0;
    for (int32_t i = 0; i < gate1.mLength && i < 0x200; ++i) out1[i] = gate1.mData[i];
    for (int32_t i = 0; i < gate2.mLength && i < 0x200; ++i) out2[i] = gate2.mData[i];
}

// The flies: sets the globals, runs FLY_Close, reads them back. state: initialised, table present, active word, seed of the bases.
void tpw_ll_fly(int32_t initialised, int32_t hasTable, int32_t active, int32_t seed, int32_t* out) {
    gFlyInitialised = initialised;
    gFlySinTable = hasTable ? gSinBlock : nullptr;
    gFlyActive = active;
    for (int32_t i = 0; i < 100; ++i) gFlyBases[i] = reinterpret_cast<void*>(static_cast<intptr_t>(seed + i));
    tpw_cl_label(kSinLabel, gSinBlock);
    FLY_Close();
    out[0] = gFlyInitialised;
    out[1] = gFlySinTable == nullptr ? 0 : lab(gFlySinTable);
    out[2] = gFlyActive;
    for (int32_t i = 0; i < 100; ++i) out[3 + i] = static_cast<int32_t>(reinterpret_cast<intptr_t>(gFlyBases[i]));
}

// The scripts: sets the five state words and the malloc count (head = script number or -1), runs RSSE_Close, reads them back.
// The queue "KillScript" holds the answers (script numbers, -1 for NULL).
void tpw_ll_rsse(int32_t initialised, int32_t f04, int32_t f08, int32_t count, int32_t head, int32_t mallocCount, int32_t* out) {
    for (int32_t k = 0; k < 8; ++k) tpw_cl_label(kScriptLabel + k, gScriptBlocks[k]);
    gRSSEState.mInitialised = initialised;
    gRSSEState.mF04 = f04;
    gRSSEState.mF08 = f08;
    gRSSEState.mScriptCount = count;
    gRSSEState.mScripts = head < 0 ? nullptr : reinterpret_cast<RSSE_SCRIPT*>(gScriptBlocks[head]);
    gRSSEMallocCount = mallocCount;
    RSSE_Close();
    out[0] = gRSSEState.mInitialised;
    out[1] = gRSSEState.mF04;
    out[2] = gRSSEState.mF08;
    out[3] = gRSSEState.mScriptCount;
    out[4] = gRSSEState.mScripts == nullptr ? 0 : lab(gRSSEState.mScripts);
    out[5] = gRSSEMallocCount;
}

}  // extern "C"
