// Test library for the save-file instances and PayPricePerUseForObject (unit v51002; libtpw_unit_SAVEVARSTRUCT_3.dylib,
// tests/test_SAVEVARSTRUCT_3.py), main app:
//   src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/people/Person.cpp (LOADVAR<ResearchCat>)
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues: WriteSerialiseLog,
// GAME_ASSERTMSG, LbFile_Read, LbFile_Write, CObject::GetPricePerUse, CObject::RecordNewTakings, CPeep::PlaySound. The globals of other
// files (gSaveLoad, gSoundBanks) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ParkMessages.h"
#include "ThingReceivers.h"
#include "Serialise.h"
#include "TPSound.h"

using namespace calllog;

CSaveLoad gSaveLoad;
int32_t gSoundBanks[8];

void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int64_t>(state)}, {name});
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), static_cast<int64_t>(length)});
    int32_t status = static_cast<int32_t>(ret("readerr"));
    uint32_t count = static_cast<uint32_t>(ret("readn"));
    uint32_t words[3] = {static_cast<uint32_t>(ret("rd0")), static_cast<uint32_t>(ret("rd1")), static_cast<uint32_t>(ret("rd2"))};
    std::memcpy(buffer, words, length < 12 ? length : 12);
    *transferred = count;
    return TbError(status);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    uint32_t words[3] = {0, 0, 0};
    std::memcpy(words, buffer, length < 12 ? length : 12);
    ev("LbFile_Write", {lab(file), static_cast<int64_t>(length), static_cast<int32_t>(words[0]), static_cast<int32_t>(words[1]),
                        static_cast<int32_t>(words[2])});
    int32_t status = static_cast<int32_t>(ret("wrerr"));
    *transferred = static_cast<uint32_t>(ret("wrn"));
    return TbError(status);
}

uint32_t CObject::GetPricePerUse() const {
    ev("GetPricePerUse", {lab(this)});
    return static_cast<uint32_t>(ret("GetPricePerUse"));
}

void CObject::RecordNewTakings(uint32_t amount) {
    ev("RecordNewTakings", {lab(this), static_cast<int64_t>(amount)});
}

void CPeep::PlaySound(int32_t category, int32_t sound) {
    ev("PlaySound", {lab(this), category, sound});
}

// Reached only by the research file's other functions (not under test); defined so the library links.
template<> int32_t LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>*, const char*, int32_t) {
    return 0;
}

template<> int32_t SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>*, const char*, int32_t) {
    return 0;
}

extern "C" {

uint64_t tpw_sv3_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gSaveLoad", &gSaveLoad},
        {"banks", &gSoundBanks[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sv3_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sv3_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CStaffHQ::StrikeType));
    out[n++] = static_cast<int64_t>(offsetof(CStaffHQ::StrikeType, mF08));
    return n;
}

// op 0 SAVEVARSTRUCT<StrikeType>, 1 LOADVARSTRUCT<StrikeType>, 2 LOADVAR<ResearchCat>, 3 SAVEVAR<PersonState>, 4 PayPricePerUseForObject (a person, b object)
uint32_t tpw_sv3_call(int32_t op, uint64_t a, uint64_t b, uint64_t c, int32_t d) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(a));
    void* q = reinterpret_cast<void*>(static_cast<uintptr_t>(b));
    const char* name = reinterpret_cast<const char*>(static_cast<uintptr_t>(c));
    switch (op) {
    case 0: return static_cast<uint32_t>(SAVEVARSTRUCT<CStaffHQ::StrikeType>(p, static_cast<CStaffHQ::StrikeType*>(q), name, d));
    case 1: return static_cast<uint32_t>(LOADVARSTRUCT<CStaffHQ::StrikeType>(p, static_cast<CStaffHQ::StrikeType*>(q), name, d));
    case 2: return static_cast<uint32_t>(LOADVAR<ResearchCat>(p, static_cast<ResearchCat*>(q), name, d));
    case 3: return static_cast<uint32_t>(SAVEVAR<CPerson::PersonState>(p, static_cast<CPerson::PersonState*>(q), name, d));
    default:
        static_cast<CPerson*>(p)->PayPricePerUseForObject(static_cast<CObject*>(q));
        return 0;
    }
}

}  // extern "C"
