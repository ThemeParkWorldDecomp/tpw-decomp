// Test library for CPerson::CalculateOptionScore (unit v5686; libtpw_unit_CPerson_CalculateOptionScore.dylib, tests/test_CPerson_CalculateOptionScore.py), main app:
//   src/CPerson_..._CPerson_CalculateOptionScore.cpp   CalculateOptionScore and the globals it defines (the two "no" halfwords and the two match tables)
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of other
// files the unit reads (gWorld, gWorldMap, gThingArray, gMainBalance) are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "image_copies.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "RideDB.h"

using namespace calllog;

// ---- globals of other files
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
// gMainBalance without its constructor (CBalanceTable::Init is not in this library): same symbol, raw storage.
alignas(16) uint8_t gMainBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");

// The match tables as loaded, for the image test (unit v5L39).
TPW_IMAGE_COPY(needs, gNeedMatchTable);
TPW_IMAGE_COPY(amounts, gAmountMatchTable);

namespace {

const void* gPersonBlock;
const void* gObjectBlock;
const void* gWorldBlock;
const void* gMapBlock;
const uint8_t* gDbBlock;
void* gWeatherBlock;
void* gCellBlock;
std::ptrdiff_t gNameOffset;

int64_t Which(const void* p) {
    if (p == gPersonBlock) return 1;
    if (p == gObjectBlock) return 2;
    if (p == gWorldBlock) return 3;
    if (p == gMapBlock) return 4;
    return 0;
}

int64_t Hash(const char* text) {
    uint32_t h = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) h = h * 31u + *p;
    return h;
}

CMainBalance* Balance() { return reinterpret_cast<CMainBalance*>(gMainBalanceStorage); }

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CObject::GetBackOfQueue() {
    ev("GetBackOfQueue", {Which(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetBackOfQueue"));
    return t;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {static_cast<int64_t>(static_cast<uint32_t>(condition)), Hash(format)});
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    int64_t which = Which(this);
    ev("GetThingId", {which});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetThingId", which, 0));
    return t;
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {Which(this)});
    return reinterpret_cast<RideInfo*>(const_cast<uint8_t*>(gDbBlock));
}

void CTRACE(uint32_t level, const char* format, ...) {
    int64_t args[12];
    int n = 0;
    args[n++] = level;
    args[n++] = Hash(format);
    va_list list;
    va_start(list, format);
    for (const char* p = format; *p; ++p) {
        if (*p != '%') continue;
        ++p;
        if (*p == 'l') {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(va_arg(list, const void*));
            args[n++] = (ptr == gDbBlock + gNameOffset) ? 1 : static_cast<int64_t>(static_cast<uint32_t>(ptr - gDbBlock));
        } else {
            args[n++] = static_cast<uint32_t>(va_arg(list, int));
        }
    }
    va_end(list);
    switch (n) {
        case 2: ev("CTRACE", {args[0], args[1]}); break;
        case 3: ev("CTRACE", {args[0], args[1], args[2]}); break;
        case 4: ev("CTRACE", {args[0], args[1], args[2], args[3]}); break;
        case 5: ev("CTRACE", {args[0], args[1], args[2], args[3], args[4]}); break;
        default: ev("CTRACE", {-1}); break;
    }
}

CRegionEffectCell* CMap::GetRegionalEffectCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("GetRegionalEffectCell", {Which(this), cell.mIndex});
    return reinterpret_cast<CRegionEffectCell*>(gCellBlock);
}

int32_t CObject::GetNumPeopleInQueue() const {
    ev("GetNumPeopleInQueue", {Which(this)});
    return static_cast<int32_t>(ret("GetNumPeopleInQueue"));
}

uint8_t CPerson::GetPreferredExcitement() const {
    ev("GetPreferredExcitement", {Which(this)});
    return static_cast<uint8_t>(ret("GetPreferredExcitement"));
}

uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("GetExcitementLevel", {Which(this), flag ? 1 : 0});
    return static_cast<uint8_t>(ret("GetExcitementLevel", flag ? 1 : 0, 0));
}

int32_t CObject::GetObjectAge() const {
    ev("GetObjectAge", {Which(this)});
    return static_cast<int32_t>(ret("GetObjectAge"));
}

CWeather* CWorld::GetWeather() {
    ev("GetWeather", {Which(this)});
    return reinterpret_cast<CWeather*>(gWeatherBlock);
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_cos_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"noBoq", &gNoBackOfQueue},
        {"noRide", &gNoThingRide},
        {"tabA", gNeedMatchTable},
        {"tabB", gAmountMatchTable},
        {"balvals", &Balance()->mValues[0]},
        {"thingarr", gThingArray},
        {"gWorldMap", &gWorldMap},
        {"gWorld", &gWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_cos_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The blocks of a case: the fakes name them.
void tpw_cos_setup(uint64_t person, uint64_t object, uint64_t world, uint64_t map, uint64_t db, uint64_t weather, uint64_t cell) {
    gPersonBlock = reinterpret_cast<const void*>(person);
    gObjectBlock = reinterpret_cast<const void*>(object);
    gWorldBlock = reinterpret_cast<const void*>(world);
    gMapBlock = reinterpret_cast<const void*>(map);
    gDbBlock = reinterpret_cast<const uint8_t*>(db);
    gWeatherBlock = reinterpret_cast<void*>(weather);
    gCellBlock = reinterpret_cast<void*>(cell);
    gNameOffset = reinterpret_cast<const uint8_t*>(reinterpret_cast<const RideInfo*>(gDbBlock)->mItemName) - gDbBlock;
    gWorld = reinterpret_cast<CWorld*>(world);
    gWorldMap = reinterpret_cast<CMap*>(map);
}

// The host's offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_cos_layout(int64_t* out) {
    alignas(16) static uint8_t buf[0x10000];
    RideInfo* ri = reinterpret_cast<RideInfo*>(buf);
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    out[n++] = reinterpret_cast<uint8_t*>(&ri->Value(static_cast<RideBalanceSlot>(78))) - buf;
    out[n++] = reinterpret_cast<uint8_t*>(&ri->Value(static_cast<RideBalanceSlot>(79))) - buf;
    out[n++] = reinterpret_cast<uint8_t*>(&ri->Value(static_cast<RideBalanceSlot>(66))) - buf;
    out[n++] = reinterpret_cast<uint8_t*>(&ri->Value(static_cast<RideBalanceSlot>(46))) - buf;
    out[n++] = reinterpret_cast<uint8_t*>(&ri->Value(static_cast<RideBalanceSlot>(107))) - buf;
    out[n++] = reinterpret_cast<uint8_t*>(ri->mItemName) - buf;
    CWeather* w = reinterpret_cast<CWeather*>(buf);
    out[n++] = reinterpret_cast<uint8_t*>(&w->mF30) - buf;
    out[n++] = static_cast<int64_t>(sizeof(CWeather));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    CRegionEffectCell* c = reinterpret_cast<CRegionEffectCell*>(buf);
    out[n++] = reinterpret_cast<uint8_t*>(&c->mpEffect[4]) - buf;
    return n;
}

// ---- the function
uint32_t tpw_cos_call(uint64_t person, uint64_t object) {
    return reinterpret_cast<CPerson*>(person)->CalculateOptionScore(reinterpret_cast<CObject*>(object));
}

}  // extern "C"
