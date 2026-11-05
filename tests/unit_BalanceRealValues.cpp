// Test library for the real main balance table under the code that reads it (libtpw_unit_BalanceRealValues.dylib; tests/test_BalanceRealValues.py):
//   src/game/balance/BalanceLoader.cpp, src/game/balance/MainBalance.cpp, and the readers in src/game/people/Person.cpp and src/game/ui/ui_2dmap.cpp
//
// gMainBalance here is the real object, filled by the real reader from the bytes the test gives the fake file calls; no slot is written any
// other way. The rest is as in tests/unit_BalanceTable.cpp, tests/unit_CPerson_TooPricey.cpp and tests/unit_CUI2DMap_CreateMapSprites.cpp:
// fakes that record, answering from the test's queues.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "CBalanceTable.h"
#include "Debug.h"
#include "ExitErr.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here
uint16_t gThingIdNone;
uint16_t gObjectIdNone;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

// The bytes the fake file calls serve (set by the test).
std::vector<char> gFileText;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the reader's fakes

// The two tests' PowerPC sides name this call differently: the test says which it is running.
int32_t tpw_rv_map_asserts = 0;

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    if (tpw_rv_map_asserts != 0) {
        ev("Assert", {condition & 0xff});
    } else {
        ev("GAME_ASSERTMSG", {condition}, {format});
    }
}

void* LbMem_Alloc(uint32_t size) {
    if (ret("LbMem_Alloc", 1) == 0) {
        ev("LbMem_Alloc", {size, 0});
        return nullptr;
    }
    void* block = std::malloc(size != 0 ? size : 1);
    std::memset(block, 0xa5, size);
    ev("LbMem_Alloc", {size, newlab(block)});
    return block;
}
void LbMem_Free(void* block) {
    ev("LbMem_Free", {lab(block)});
    forget(block);
    std::free(block);
}

int32_t LbFile_GetLength(const char* name) {
    ev("LbFile_GetLength", {}, {name});
    return static_cast<int32_t>(ret("LbFile_GetLength", -1));
}
TbError LbFile_Load(const char* name, void* buffer, uint32_t capacity, int32_t* length) {
    ev("LbFile_Load", {capacity}, {name});
    const size_t count = gFileText.size() < capacity ? gFileText.size() : capacity;
    if (count != 0) std::memcpy(buffer, gFileText.data(), count);
    *length = static_cast<int32_t>(count);
    return TbError(static_cast<int32_t>(ret("LbFile_Load", 0)));
}

CExitError gExitError;
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) {
    ev("CExitError::SetError", {lab(this), static_cast<int64_t>(type), value}, {text});
}

// ---- the visitors' fakes
uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("GetExcitementLevel", {lab(static_cast<const void*>(this)), flag ? 1 : 0});
    return static_cast<uint8_t>(ret("GetExcitementLevel"));
}
uint8_t CPerson::GetPreferredExcitement() const {
    ev("GetPreferredExcitement", {lab(static_cast<const void*>(this))});
    return static_cast<uint8_t>(ret("GetPreferredExcitement"));
}

// ---- the map's fakes
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("Head", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("Head"));
    return r;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("Next", {lab(this), lab(thing)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> r;
    r.mIndex = static_cast<uint16_t>(ret("Next"));
    return r;
}

CUI2DMapSprite::CUI2DMapSprite(void* image, EUI2DMapSpriteID id, TUI2DMapIndividualShowStatus status, float scale) {
    std::memset(this, 0, sizeof(*this));
    ev("Ctor", {newlab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(image)), static_cast<int64_t>(id), status.mA, status.mB, fbits(scale)});
}

void CUI2DMap::UpdateSpritePosition(CUI2DMapSprite* sprite) {
    ev("Update", {lab(this), lab(sprite), CanonBits(sprite->mX), CanonBits(sprite->mY), lab(sprite->mThing)});
}

// ---- the test entry points

extern "C" {

// The reader, on the real table.
void* tpw_bt_main() { return &gMainBalance; }
void* tpw_bt_exit_error() { return &gExitError; }
void tpw_bt_set_file_text(const char* text, int32_t length) { gFileText.assign(text, text + length); }
int32_t tpw_bt_row_info_size() { return static_cast<int32_t>(sizeof(BalanceRowInfo)); }
void tpw_bt_init(void* object) { static_cast<CBalanceTable*>(object)->Init(); }
void tpw_bt_uninit(void* object) { static_cast<CBalanceTable*>(object)->UnInit(); }
int32_t tpw_bt_load_path(void* object, const char* path, int32_t mode) {
    return static_cast<CBalanceTable*>(object)->Load(path, static_cast<CBalanceTable::LoadMode>(mode));
}
// As tests/unit_BalanceTable.cpp's: the row information as slot numbers, the slots' eight bytes.
void tpw_bt_dump(void* object, int32_t rowCount, int32_t slotCount, int64_t* rows, uint64_t* slots) {
    CBalanceTable* table = static_cast<CBalanceTable*>(object);
    BalanceValue* values = table->GetBalanceValuesStart();
    for (int32_t i = 0; i < slotCount; ++i) slots[i] = values[i].mBits;
    BalanceRowInfo* info = static_cast<BalanceRowInfo*>(table->mRows);
    for (int32_t i = 0; i < rowCount; ++i) {
        int64_t* out = rows + 3 * i;
        if (info == nullptr) {
            out[0] = out[1] = out[2] = -3;
            continue;
        }
        out[0] = info[i].mValue != nullptr ? info[i].mValue - values : -1;
        out[1] = info[i].mStride;
        out[2] = info[i].mCount != nullptr ? info[i].mCount - values : -1;
    }
}

// The visitors.
uint64_t tpw_rv_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
void tpw_rv_lower(uint64_t p, int32_t a) { reinterpret_cast<CPerson*>(p)->LowerHappiness(static_cast<CPerson::Amount>(a)); }
void tpw_rv_raise(uint64_t p, int32_t a) { reinterpret_cast<CPerson*>(p)->RaiseHappiness(static_cast<CPerson::Amount>(a)); }
uint32_t tpw_rv_amount(uint64_t p, int32_t a) {
    return reinterpret_cast<CPerson*>(p)->GetHappinessAmount(static_cast<CPerson::Amount>(a));
}
void tpw_rv_extra(uint64_t p, uint64_t object) { reinterpret_cast<CPerson*>(p)->ExtraExcitementEffects(reinterpret_cast<const CObject*>(object)); }

// The map, under the names tests/test_CUI2DMap_CreateMapSprites.py calls.
uint64_t tpw_cm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"none", &gThingIdNone}, {"objnone", &gObjectIdNone}, {"world", &gGameWorld}, {"tarr", &gThingArray[0]}, {"bal", &gMainBalance},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cm_slot(int32_t slot) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gMainBalance.mValues[slot])); }

uint64_t tpw_cm_alloc(uint32_t size) { return tpw_rv_alloc(size); }

int32_t tpw_cm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mX));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mY));
    out[n++] = static_cast<int64_t>(offsetof(CUI2DMapSprite, mThing));
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapSprite));
    return n;
}

void tpw_cm_run(uint64_t map) { reinterpret_cast<CUI2DMap*>(static_cast<uintptr_t>(map))->CreateMapSprites(); }

}  // extern "C"
