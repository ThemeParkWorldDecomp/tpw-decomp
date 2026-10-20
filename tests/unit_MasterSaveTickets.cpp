// Test library for the master save's ticket bookkeeping, the current theme and the park sign names (libtpw_unit_MasterSaveTickets.dylib;
// see tests/test_MasterSaveTickets.py): src/FS_MasterSave_tickets.cpp and src/FS_MasterSave_signs.cpp (unit w89), with the master
// save and the park record of unit w32 (src/FS_MasterSave_global.cpp, src/FS_MasterSave_global_ctor.cpp) and the ride names
// (src/RideDB_names.cpp) under them. Real: the bullfrog strings (the theme map's keys), the 16-bit C library (swprintf, wcslen,
// wcsncpy), the std containers of the master save.
//
// Fakes that record through calllog: the balance table (Init, UnInit and Load, whose answer and values the world scripts per theme),
// the trace output, and the string table's FormatString (its text comes from the world).
//
// The world: eight themes whose balance files the world scripts, a ride database with the rides a case gives (a record with sign
// texts) and a string table. The Python module drives it through the operations of tpw_ms_op and reads the state with tpw_ms_dump.
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "CBalanceTable.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "RideDB.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "TPEngine.h"
#include "TbFontRenderParams.h"
#include "TbITable.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "calllog.h"
#include "platform/msl.h"

using namespace calllog;

// ---- globals of other files
CText gText;
CFSGlobalMasterSave* gMasterSave;
CRideDatabase gRideDatabase;

// (the text object CText holds by value: its members are never used here, but its constructor and destructor must link)
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

// (the ride records are never destroyed by code under test)
RideInfo::~RideInfo() {}

namespace {
struct Balance {
    int32_t ok;     // Load's answer
    int32_t cost;   // slot 3
    int32_t gate;   // slot 4
};

// ---- the world's constants (the same as the Python module's)
constexpr int kThemes = 8;
const char* const kThemeNames[kThemes] = {"hallow", "space", "jungle", "fantasy", "Hollywood", "alpha", "space2", "Alpha"};
const Balance kThemeBalance[kThemes] = {{1, 1, 2601}, {1, 5, 3601}, {1, 1, 1601}, {1, 3, 4601}, {1, 2, 1150}, {1, 2, 1150}, {1, 0, 1190}, {1, 2, 1180}};
constexpr int kScratch = kThemes;   // the park id of the scratch park record
constexpr int kRides = 6;
constexpr int kBufferUnits = 0x90;   // the output buffer of the sign name operations (filled with 0xaaaa first)
constexpr int kBoughtShown = 12;     // how many members of the bought set the dump shows

Balance gBalance[kThemes];
const void* gSeeded[64];   // the park records whose award scores were given the fill (see Seed)
int gSeededCount;
// The current theme's pointer after the record it points to was deleted: the host's allocator may give the address to a new record,
// which the PowerPC side's never does, so the dump does not call such a pointer a record of the map.
const CFSGlobalParkData* gDangling;
CFSGlobalParkData* gScratchPark;
char16_t gBuffer[kBufferUnits];
RideInfo gRideRecords[kRides];
std::u16string gRideSigns[kRides][2];
std::u16string gStrings[2048];       // the string table (OBJECT_NAMES) by number
char gTableBlob[16];                 // the string table object (only its address is used)
char16_t gSignText[0x60];            // the sign text of the last op that passes one to a setter

constexpr int kTableLabel = 90;

// The theme a balance file path belongs to ("data:levels:<theme>:global.sam"), or -1.
int ThemeOfPath(const char* path) {
    const char* prefix = "data:levels:";
    const char* suffix = ":global.sam";
    if (std::strncmp(path, prefix, std::strlen(prefix)) != 0) return -1;
    std::string name(path + std::strlen(prefix));
    const std::string tail(suffix);
    if (name.size() < tail.size() || name.compare(name.size() - tail.size(), tail.size(), tail) != 0) return -1;
    name.resize(name.size() - tail.size());
    for (int i = 0; i < kThemes; ++i)
        if (name == kThemeNames[i]) return i;
    return -1;
}
}  // namespace

// ---- the trace output
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    // (the trace lines of this unit that take an argument take one text)
    const char* text = std::strstr(format, "%s") != nullptr ? va_arg(args, const char*) : nullptr;
    va_end(args);
    if (text != nullptr) ev("CTRACE", {level}, {format, text});
    else ev("CTRACE", {level}, {format});
}

// ---- the balance table: Init and UnInit are recorded; Load answers per theme and fills the values the game reads
void CBalanceTable::Init() { ev("Balance.Init", {newlab(this)}); }
void CBalanceTable::UnInit() { ev("Balance.UnInit", {lab(this)}); }
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("Balance.Load", {lab(this), mode}, {path});
    const int theme = ThemeOfPath(path);
    if (theme < 0 || gBalance[theme].ok == 0) return 0;
    auto* table = static_cast<CGlobalParkDataBalance*>(this);
    table->mValues[3].mInteger = gBalance[theme].cost;
    table->mValues[4].mInteger = gBalance[theme].gate;
    return 1;
}

// ---- the string table: FormatString writes the world's text for the number
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {lab(this), size, id});
    const std::u16string& text = id < 2048 ? gStrings[id] : gStrings[0];
    if (size == 0) return 0;
    const uint32_t count = text.size() < size - 1 ? static_cast<uint32_t>(text.size()) : size - 1;
    for (uint32_t i = 0; i < count; ++i) out[i] = text[i];
    out[count] = 0;
    return static_cast<int32_t>(count);
}

extern "C" {

// Sets the globals as the Mac's data has them.
void tpw_ms_reset() {
    tpw_cl_label(kTableLabel, gTableBlob);
    gMasterSave = nullptr;
    gScratchPark = nullptr;
    for (int i = 0; i < kThemes; ++i) gBalance[i] = kThemeBalance[i];
    // the ride database: nothing cached, no rides
    std::memset(&gRideDatabase, 0, sizeof gRideDatabase);
    gRideDatabase.mRideCount = 0;
    tpw_cl_label(80, &gText);
    gText.mResourcePointers[20] = gTableBlob;
    std::memset(gParkSignNameScratch, 0, sizeof gParkSignNameScratch);
    gSeededCount = 0;
    gDangling = nullptr;
}

// The ride types of a case: ride k has the number `id` and the sign texts a and b (its name is NULL).
void tpw_ms_ride(int32_t k, int32_t id, const char16_t* a, const char16_t* b) {
    gRideRecords[k].mId = static_cast<uint16_t>(id);
    gRideSigns[k][0] = a != nullptr ? a : u"";
    gRideSigns[k][1] = b != nullptr ? b : u"";
    gRideRecords[k].mName = nullptr;
    gRideRecords[k].mSignA = gRideSigns[k][0].c_str();
    gRideRecords[k].mSignB = gRideSigns[k][1].c_str();
    gRideDatabase.mRides[k] = &gRideRecords[k];
    gRideDatabase.mRideCount = k + 1;
}

// A string of the string table.
void tpw_ms_string(int32_t id, const char16_t* text) { gStrings[id] = text; }

// The sign text the setter operations pass (up to 0x5f characters).
void tpw_ms_text(const char16_t* text) {
    std::size_t n = 0;
    for (; text[n] != 0 && n < 0x5f; ++n) gSignText[n] = text[n];
    gSignText[n] = 0;
}

void tpw_ms_heap(int64_t* out) {
    out[0] = tpw_cl_heap[0];
    out[1] = tpw_cl_heap[1];
    out[2] = tpw_cl_heap[2];
    out[3] = tpw_cl_heap[3];
}

void tpw_ms_op(int32_t op, const int64_t* a, int64_t* out);
void tpw_ms_dump(int64_t* out);

}  // extern "C"

namespace {
// The answer and the dump are built in a fixed array and copied out (no heap traffic).
int64_t gOutBuf[4096];
std::size_t gOutCount;
void Put(int64_t value) {
    if (gOutCount < 4000) gOutBuf[gOutCount++] = value;
}
void PutText16(const char16_t* text, int units) {
    bool ended = false;
    for (int i = 0; i < units; ++i) {
        if (!ended && text[i] == 0) ended = true;
        Put(ended ? 0 : text[i]);
    }
}
void PutText8(const char* text, int units) {
    bool ended = false;
    for (int i = 0; i < units; ++i) {
        if (!ended && text[i] == 0) ended = true;
        Put(ended ? 0 : static_cast<unsigned char>(text[i]));
    }
}
void Flush(int64_t* out) {
    out[0] = static_cast<int64_t>(gOutCount);
    for (std::size_t i = 0; i < gOutCount; ++i) out[1 + i] = gOutBuf[i];
}

CFSGlobalParkData* ParkOf(int which) {
    if (which == kScratch) return gScratchPark;
    if (which < 0 || which >= kThemes || gMasterSave == nullptr) return nullptr;
    const auto found = gMasterSave->mThemes.find(TbDynamicStringTemplate<char>(kThemeNames[which]));
    return found == gMasterSave->mThemes.end() ? nullptr : found->second;
}

void PutPark(const CFSGlobalParkData* park) {
    Put(park != nullptr);
    if (park == nullptr) return;
    for (int i = 0; i < 6; ++i) Put(park->mTickets[i]);
    for (int i = 0; i < 4; ++i) Put(park->mAwards[i]);
    for (int i = 0; i < 4; ++i) Put(park->mAwardScores[i]);
    Put(park->mBalance != nullptr);
    Put(park->mHasSignNames);
    Put(park->mAllResearchCompleted);
    PutText8(park->mName.mData, 12);
    for (int i = 0; i < 0x21; ++i) Put(park->mSignNameA[i]);   // both sign arrays whole: what follows a terminator and the last unit show
    for (int i = 0; i < 0x21; ++i) Put(park->mSignNameB[i]);
}

// The PowerPC side makes every object over 0xa5 bytes; the host's memory is whatever malloc gives. The members the original's constructors
// leave alone (a park's award scores, the save's current theme) are given the same bytes here, so that the dumps can compare them.
constexpr uint32_t kFill = 0xa5a5a5a5u;
void Seed(CFSGlobalParkData* park) {
    if (park == nullptr) return;
    for (int i = 0; i < gSeededCount; ++i)
        if (gSeeded[i] == park) return;
    for (int i = 0; i < 4; ++i) park->mAwardScores[i] = static_cast<int32_t>(kFill);
    // (the constructor sets only the first unit of each sign name)
    for (int i = 1; i < 0x21; ++i) {
        park->mSignNameA[i] = static_cast<char16_t>(kFill);
        park->mSignNameB[i] = static_cast<char16_t>(kFill);
    }
    if (gSeededCount < 64) gSeeded[gSeededCount++] = park;
}

// A park that is about to be deleted can be made again at its address.
void Unseed(const void* park) {
    for (int i = 0; i < gSeededCount; ++i) {
        if (gSeeded[i] == park) {
            gSeeded[i] = gSeeded[--gSeededCount];
            return;
        }
    }
}

void UnseedAll() {
    if (gMasterSave == nullptr) return;
    for (const auto& entry : gMasterSave->mThemes) {
        Unseed(entry.second);
        if (entry.second == gMasterSave->mCurrent) gDangling = entry.second;
    }
}

void FillBuffer() {
    for (char16_t& unit : gBuffer) unit = 0xaaaa;
}
void PutBuffer() {
    for (int i = 0; i < kBufferUnits; ++i) Put(gBuffer[i]);
}
}  // namespace

extern "C" {

void tpw_ms_op(int32_t op, const int64_t* a, int64_t* out) {
    gOutCount = 0;
    const int which = static_cast<int>(a[0]);
    switch (op) {
        case 10:
            gMasterSave = new CFSGlobalMasterSave;
            gDangling = nullptr;
            gMasterSave->mCurrent = reinterpret_cast<CFSGlobalParkData*>(static_cast<uintptr_t>(kFill));
            break;
        case 11: gMasterSave->ResetGlobalData(); break;
        case 13: {
            const TbDynamicStringTemplate<char> name(kThemeNames[which]);
            CFSGlobalParkData* park = gMasterSave->GetInfoOn(name);
            Put(park == nullptr);
            Put(park != nullptr && park == ParkOf(which));
            break;
        }
        case 14: {
            const TbDynamicStringTemplate<char> name(kThemeNames[which]);
            CFSGlobalParkData* park = gMasterSave->CreateNewTheme(name);
            Put(park != nullptr && park == ParkOf(which));
            break;
        }
        case 15: {
            const TbDynamicStringTemplate<char> name(kThemeNames[which]);
            gScratchPark = new CFSGlobalParkData(name);
            break;
        }
        case 34: gBalance[which] = {static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])}; break;
        case 37:
            for (int i = 0; i < 4; ++i) gMasterSave->mGlobalTickets[i] = (a[0] >> i) & 1;
            for (int i = 0; i < 2; ++i) gMasterSave->mSecretTickets[i] = (a[1] >> i) & 1;
            gMasterSave->mExtraKeys = static_cast<int32_t>(a[2]);
            gMasterSave->mTicketsSpent = static_cast<int32_t>(a[3]);
            break;
        case 38: {
            CFSGlobalParkData* park = ParkOf(which);
            for (int i = 0; i < 6; ++i) park->mTickets[i] = (a[1] >> i) & 1;
            for (int i = 0; i < 4; ++i) park->mAwards[i] = (a[2] >> i) & 1;
            break;
        }
        // the master save's counters and queries
        case 50: Put(gMasterSave->HowManyTickets()); break;
        case 51: Put(gMasterSave->HowManyTicketsIncludingSecrets()); break;
        case 52: Put(gMasterSave->SpendingPower()); break;
        case 53: Put(gMasterSave->SpendTicketOn(static_cast<uint16_t>(a[0]), static_cast<int32_t>(a[1]))); break;
        case 54: Put(gMasterSave->HasBeenBought(static_cast<uint16_t>(a[0]))); break;
        case 55: Put(gMasterSave->HaveTicket(static_cast<GoldenTicketTypeLocal>(a[0]))); break;
        case 56: Put(gMasterSave->HaveTicket(static_cast<GoldenTicketTypeSecret>(a[0]))); break;
        case 57: Put(gMasterSave->EarnedTicket(static_cast<GoldenTicketTypeLocal>(a[0]))); break;
        case 58: Put(gMasterSave->EarnedTicket(static_cast<GoldenTicketTypeGlobal>(a[0]), static_cast<int32_t>(a[1]))); break;
        case 59: Put(gMasterSave->EarnedTicket(static_cast<GoldenTicketTypeSecret>(a[0]))); break;
        case 60: Put(gMasterSave->CheckTicketTypeWon()); break;
        case 61: {
            const TbDynamicStringTemplate<char> name(kThemeNames[which]);
            gDangling = nullptr;
            gMasterSave->ChangeCurrentTheme(name);
            break;
        }
        case 62: gMasterSave->SetSignNameA(gSignText); break;
        case 63: gMasterSave->SetSignNameB(gSignText); break;
        case 64: gMasterSave->SetSwearFilter(a[0] != 0); break;
        case 65: Put(gMasterSave->IsSwearFilterOn()); break;
        case 66:
            UnseedAll();
            gMasterSave->ClearThemeMap();
            break;
        case 67: Put(gMasterSave->HasAllResearchBeenCompleted()); break;
        case 68: gMasterSave->SetAllResearchCompletedInCurrentTheme(); break;
        case 69: Put(gMasterSave->IsFirstTimePlayer()); break;
        case 70: gMasterSave->ClearFirstTimePlayer(); break;
        case 71:
            UnseedAll();
            delete gMasterSave;
            gMasterSave = nullptr;
            break;
        // one park record's members
        case 72: Put(ParkOf(which)->HaveTicket(static_cast<GoldenTicketTypeLocal>(a[1]))); break;
        case 73: Put(ParkOf(which)->EarnedTicket(static_cast<GoldenTicketTypeLocal>(a[1]))); break;
        case 74: Put(ParkOf(which)->AwardInThisPark(static_cast<GoldenTicketTypeGlobal>(a[1]))); break;
        case 75: Put(ParkOf(which)->ScoreForAward(static_cast<GoldenTicketTypeGlobal>(a[1]))); break;
        case 76: ParkOf(which)->ClearAward(static_cast<GoldenTicketTypeGlobal>(a[1])); break;
        case 77: ParkOf(which)->SetAward(static_cast<GoldenTicketTypeGlobal>(a[1]), static_cast<int32_t>(a[2])); break;
        case 78:
            FillBuffer();
            ParkOf(which)->GetSignNameA(gBuffer, static_cast<uint32_t>(a[1]));
            PutBuffer();
            break;
        case 79:
            FillBuffer();
            ParkOf(which)->GetSignNameB(gBuffer, static_cast<uint32_t>(a[1]));
            PutBuffer();
            break;
        case 80: ParkOf(which)->SetSignNameA(gSignText); break;
        case 81: ParkOf(which)->SetSignNameB(gSignText); break;
        case 82:
            Unseed(gScratchPark);
            if (gMasterSave != nullptr && gMasterSave->mCurrent == gScratchPark) gDangling = gScratchPark;
            delete gScratchPark;
            gScratchPark = nullptr;
            break;
        // raw state: one byte of the save (0..3 global tickets, 4..5 secret tickets, 6 swear filter, 7 first-time player, 8 easy mode)
        case 83: {
            const uint8_t value = static_cast<uint8_t>(a[1]);
            if (which < 4) gMasterSave->mGlobalTickets[which] = value;
            else if (which < 6) gMasterSave->mSecretTickets[which - 4] = value;
            else if (which == 6) gMasterSave->mSwearFilter = value;
            else if (which == 7) gMasterSave->mFirstTimePlayer = value;
            else gMasterSave->mEasyMode = value;
            break;
        }
        // raw state of a park: kind 0 ticket byte, 1 award byte, 2 award score, 3 research flag, 4 sign flag
        case 84: {
            CFSGlobalParkData* park = ParkOf(which);
            const int index = static_cast<int>(a[2]);
            if (a[1] == 0) park->mTickets[index] = static_cast<uint8_t>(a[3]);
            else if (a[1] == 1) park->mAwards[index] = static_cast<uint8_t>(a[3]);
            else if (a[1] == 2) park->mAwardScores[index] = static_cast<int32_t>(a[3]);
            else if (a[1] == 3) park->mAllResearchCompleted = static_cast<uint8_t>(a[3]);
            else park->mHasSignNames = static_cast<uint8_t>(a[3]);
            break;
        }
        // the destructors' first part (the deleting flag 0 on the PowerPC side): the object is taken apart, its block is not freed
        case 87:
            UnseedAll();
            gMasterSave->~CFSGlobalMasterSave();
            gMasterSave = nullptr;
            break;
        case 88:
            Unseed(gScratchPark);
            if (gMasterSave != nullptr && gMasterSave->mCurrent == gScratchPark) gDangling = gScratchPark;
            gScratchPark->~CFSGlobalParkData();
            gScratchPark = nullptr;
            break;
        case 85:
            gDangling = nullptr;
            gMasterSave->mCurrent = nullptr;
            break;
        case 86:
            gDangling = nullptr;
            gMasterSave->mCurrent = ParkOf(which);
            break;
        default: break;
    }
    if (gMasterSave != nullptr)
        for (const auto& entry : gMasterSave->mThemes) Seed(entry.second);
    Seed(gScratchPark);
    Flush(out);
}

// The state after an operation: the master save with its theme map in order, the current theme's place in it, the bought set, the
// scratch park, the sign work areas, the output buffer and the ride database's cache.
void tpw_ms_dump(int64_t* out) {
    gOutCount = 0;
    Put(gMasterSave != nullptr);
    if (gMasterSave != nullptr) {
        for (int i = 0; i < 4; ++i) Put(gMasterSave->mGlobalTickets[i]);
        Put(gMasterSave->mTicketsSpent);
        Put(gMasterSave->mExtraKeys);
        Put(gMasterSave->mEasyMode);
        Put(gMasterSave->mSwearFilter);
        Put(gMasterSave->mSecretTickets[0]);
        Put(gMasterSave->mSecretTickets[1]);
        Put(gMasterSave->mFirstTimePlayer);
        Put(static_cast<int64_t>(gMasterSave->mBought.size()));
        int shown = 0;
        for (uint16_t thing : gMasterSave->mBought) {
            if (shown++ < kBoughtShown) Put(thing);
        }
        Put(static_cast<int64_t>(gMasterSave->mThemes.size()));
        // the current theme: its place in the map's order, -1 for none, -2 for a pointer that is not a record of the map
        int64_t place = gMasterSave->mCurrent == nullptr ? -1 : -2;
        int index = 0;
        for (const auto& entry : gMasterSave->mThemes) {
            if (entry.second == gMasterSave->mCurrent && entry.second != gDangling) place = index;
            ++index;
        }
        Put(place);
        for (const auto& entry : gMasterSave->mThemes) {
            PutText8(entry.first.mData, 12);
            PutPark(entry.second);
        }
    }
    PutPark(gScratchPark);
    for (int i = 0; i < 0x21; ++i) Put(gParkSignNameScratch[i]);   // the whole work area, what follows a terminator too
    PutText16(gRideDatabase.mNames.mPartA, 24);
    PutText16(gRideDatabase.mNames.mPartB, 24);
    PutText16(gRideDatabase.mNames.mWhole, 24);
    Put(gRideDatabase.mCachedId);
    int cached = -1;
    for (int i = 0; i < kRides; ++i)
        if (gRideDatabase.mCachedRide == &gRideRecords[i]) cached = i;
    Put(cached);
    Flush(out);
}

}  // extern "C"
