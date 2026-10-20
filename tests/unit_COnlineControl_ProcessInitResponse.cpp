// Test library for COnlineControl::ProcessInitResponse (unit v5248; libtpw_unit_COnlineControl_ProcessInitResponse.dylib,
// tests/test_COnlineControl_ProcessInitResponse.py), main app:
//   src/COnlineControl_..._COnlineControl_ProcessInitResponse.cpp   COnlineControl::ProcessInitResponse
//
// Every callee outside the unit is a fake that records what the real one reads (pointers as a class: 0 NULL, 1 the object under test, 2 the
// control in gOnlineControl, 3 the system in gOnlineSystem, 4 the world, 5 the last block from operator new, 6 the park info, 7 the city
// info, 99 other) and answers from the test. The strings the code builds hold the hash of their text in their first word.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdarg>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "Text.h"
#include "UI_pump.h"
#include "World.h"
#include "online.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CFSOnlineSystem* gOnlineSystem;
COnlineControl* gOnlineControl;
CWorld* gGameWorld;
// gText is raw storage under the symbol's name: CText's members have constructors this library does not link.
alignas(16) uint8_t gTextStorage[sizeof(CText)] __asm__("_gText");

namespace {

uintptr_t gThis, gLastNew, gPark, gCity;

uint32_t Hash(const uint8_t* text) {
    uint32_t h = 2166136261u;
    for (; *text; ++text) h = (h ^ *text) * 16777619u;
    return h;
}
uint32_t HashW(const char16_t* text) {
    uint32_t h = 2166136261u;
    for (; *text; ++text) h = (h ^ static_cast<uint32_t>(*text)) * 16777619u;
    return h;
}
int64_t Cls(const void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    if (v == 0) return 0;
    if (v == gThis) return 1;
    if (v == reinterpret_cast<uintptr_t>(gOnlineControl)) return 2;
    if (v == reinterpret_cast<uintptr_t>(gOnlineSystem)) return 3;
    if (v == reinterpret_cast<uintptr_t>(gGameWorld)) return 4;
    if (v == gLastNew) return 5;
    if (v == gPark) return 6;
    if (v == gCity) return 7;
    return 99;
}
uint32_t FirstWord(const void* p) { return *static_cast<const uint32_t*>(p); }

}  // namespace

// ---- the fakes
namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {static_cast<int64_t>(Hash(reinterpret_cast<const uint8_t*>(format)))}); }
}  // namespace SamsUtilities

namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {static_cast<int64_t>(size)});
    void* p = tpw::ArenaAllocZeroed(size < 4 ? 4 : size);
    gLastNew = reinterpret_cast<uintptr_t>(p);
    return p;
}
uint32_t AvatarID(CFSOnlineSystem* sys) {
    ev("GetAvatarID", {Cls(sys)});
    return static_cast<uint32_t>(ret("GetAvatarID"));
}
void WStringCtorFrom(void* self, const char16_t* text) {
    uint32_t h = HashW(text);
    *static_cast<uint32_t*>(self) = h;
    ev("WStrCtor", {static_cast<int64_t>(h)});
}
void WStringDtor(void* self, int16_t flag) { ev("WStrDtor", {static_cast<int64_t>(FirstWord(self)), static_cast<uint16_t>(flag)}); }
void StringDtor(void* self, int16_t flag) { ev("StrDtor", {static_cast<int64_t>(FirstWord(self)), static_cast<uint16_t>(flag)}); }
}  // namespace online_callee

namespace init_callee {
void StringFromChars(void* self, const char* text) {
    uint32_t h = Hash(reinterpret_cast<const uint8_t*>(text));
    *static_cast<uint32_t*>(self) = h;
    ev("StrCtor", {static_cast<int64_t>(h)});
}
void* ReturnCityInfo(uint32_t cityList, int32_t id) {
    ev("ReturnCityInfo", {static_cast<int64_t>(cityList), static_cast<int64_t>(static_cast<uint32_t>(id))});
    return reinterpret_cast<void*>(gCity);
}
void ChatSetPlayerData(uint32_t chat, int32_t avatar, void* context) {
    ev("SetPlayerData", {static_cast<int64_t>(chat), static_cast<int64_t>(static_cast<uint32_t>(avatar)), Cls(context)});
}
void ChatCreateRoom(uint32_t chat, const void* name, const void* topic, const void* city, const void* password, int32_t size, void* context) {
    ev("CreateRoom", {static_cast<int64_t>(chat), static_cast<int64_t>(FirstWord(name)), static_cast<int64_t>(FirstWord(topic)),
                      static_cast<int64_t>(FirstWord(city)), static_cast<int64_t>(FirstWord(password)),
                      static_cast<int64_t>(static_cast<uint32_t>(size)), Cls(context)});
}
void LockChatPanelOff() { ev("LockOff", {}); }
}  // namespace init_callee

CFSOnlineSystem::CFSOnlineSystem() { ev("SysCtor", {Cls(this)}); }
CFSFTPParkInfo* CFSOnlineSystem::GetFTPDownloadParkInfo() {
    ev("GetFTP", {Cls(this)});
    return reinterpret_cast<CFSFTPParkInfo*>(gPark);
}
void AddUIError(int32_t error, TbITable* table) {
    ev("AddUIError", {static_cast<int64_t>(static_cast<uint32_t>(error)), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(table)))});
}
COnlineControl::COnlineControl() { ev("CtrlCtor", {Cls(this)}); }
void bthread_mutex::lock() { ev("lock", {Cls(reinterpret_cast<uint8_t*>(this) - 0x1c)}); }
void bthread_mutex::unlock() { ev("unlock", {Cls(reinterpret_cast<uint8_t*>(this) - 0x1c)}); }
void COnlineControl::AddContact(std::u16string& name, int32_t flag) {
    ev("AddContact", {Cls(this), static_cast<int64_t>(reinterpret_cast<uint8_t*>(&name) - reinterpret_cast<uint8_t*>(this)),
                      static_cast<int64_t>(static_cast<uint32_t>(flag))});
}
void CWorld::StartingPopulationForOnline() { ev("StartingPop", {Cls(this)}); }
bool COnlineControl::LoadBuddy(const char* path) {
    ev("LoadBuddy", {Cls(this), static_cast<int64_t>(Hash(reinterpret_cast<const uint8_t*>(path)))});
    return false;
}
bool COnlineControl::LoadIgnore(const char* path) {
    ev("LoadIgnore", {Cls(this), static_cast<int64_t>(Hash(reinterpret_cast<const uint8_t*>(path)))});
    return false;
}

extern "C" {

uint64_t tpw_pir_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineControl", &gOnlineControl},
        {"gOnlineSystem", &gOnlineSystem},
        {"gGameWorld", &gGameWorld},
        {"gTextTable", &reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[19]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pir_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The Ptr32 word of a block's address (an offset from the arena base).
uint32_t tpw_pir_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_pir_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    out[n++] = static_cast<int64_t>(offsetof(COnlineControl, mClientReady));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

void tpw_pir_run(uint64_t self, uint64_t arguments, uint64_t park, uint64_t city) {
    gThis = static_cast<uintptr_t>(self);
    gPark = static_cast<uintptr_t>(park);
    gCity = static_cast<uintptr_t>(city);
    gLastNew = 0;
    reinterpret_cast<COnlineControl*>(self)->ProcessInitResponse(reinterpret_cast<void*>(static_cast<uintptr_t>(arguments)));
}

}  // extern "C"
