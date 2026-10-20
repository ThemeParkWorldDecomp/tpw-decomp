// Test library for the online client's object (unit v5016; libtpw_unit_COnlineControl_COnlineControl.dylib,
// tests/test_COnlineControl_COnlineControl.py), main app:
//   src/COnlineControl_COnlineControl.cpp   COnlineControl::COnlineControl, ~COnlineControl, InitClient, InitGameWorld
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue: the string,
// deque, set, mutex and chat library, the online system's getters, the mouse modes, the map's entrance and TeleportAvatar. The globals of
// other files the unit reads (gOnlineSystem, gOnlineConfig, gMouse, the two mouse words, gGameWorld) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "FS_Online.h"
#include "Mouse.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "online.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files, defined here
OnlineConfig gOnlineConfig;
OnlineConfig::OnlineConfig() {}
CFSOnlineSystem* gOnlineSystem;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CWorld* gGameWorld;
static const char16_t* gFakeCustomerName;
static const char16_t* gFakePassword;

namespace {

int64_t Hash16(const char16_t* text, int64_t limit) {
    if (text == nullptr) return 0;
    int64_t n = 0;
    while (text[n] != 0 && (limit < 0 || n < limit)) n++;
    uint32_t h = static_cast<uint32_t>(n);
    for (int64_t i = 0; i < n; i++) h = h * 31u + static_cast<uint32_t>(text[i]);
    return h;
}

int64_t HashC(const char* text) {
    if (text == nullptr) return 0;
    std::size_t n = std::strlen(text);
    uint32_t h = static_cast<uint32_t>(n);
    for (std::size_t i = 0; i < n; i++) h = h * 31u + static_cast<uint32_t>(static_cast<unsigned char>(text[i]));
    return h;
}

uint32_t& WordAt(void* base, std::size_t offset) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(base) + offset); }

int64_t L(const void* p) { return lab(p); }

}  // namespace

// libc's rand, answered from the test's queue (the Mac's rand is a glue import faked on the PowerPC side the same way)
extern "C" int rand(void) { return static_cast<int>(ret("rand")); }

// ---- the unit's callees that headers declare
CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {newlab(this)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetParkEntrancePos(int32_t which) const {
    ev("GetParkEntrancePos", {L(this), which});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
    cell.mIndex = static_cast<uint16_t>(ret("entrance"));
    return cell;
}

void COnlineControl::TeleportAvatar(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) { ev("Teleport", {L(this), cell.mIndex}); }
void COnlineControl::SaveBuddy(const char* path) { ev("SaveBuddy", {L(this), HashC(path)}); }
void COnlineControl::SaveIgnore(const char* path) { ev("SaveIgnore", {L(this), HashC(path)}); }

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {L(this), L(mode)}); }

CMMOnline::CMMOnline() { ev("CMMOnline", {newlab(this)}); }
CMMOnline::~CMMOnline() {}
void CMMOnline::OnLeftClick(int32_t, int32_t) {}
void CMMOnline::OnRightClick(int32_t, int32_t) {}
void CMMOnline::OnDoubleClick(int32_t, int32_t) {}
int32_t CMMOnline::GetMode() { return 7; }
void CMMOnline::OnAssign() {}

CMMDefault::CMMDefault() { ev("CMMDefault", {newlab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

void SamsUtilities::UDebug(const char* text, ...) { ev("UDebug", {HashC(text)}); }

// ---- the callees no header declares
namespace online_callee {

void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size < 0x2000 ? 0x2000 : size);
}
void OperatorDelete(void* p) {
    const uintptr_t raw = reinterpret_cast<uintptr_t>(p);
    ev("delete", {L(p), raw < 0x10000 ? static_cast<int64_t>(raw) : 0});
}
void WStringCtor(void* self) { ev("wstr_ctor", {L(self)}); }
void WStringCtorFrom(void* self, const char16_t* text) {
    const int64_t token = Hash16(text, -1);
    ev("wstr_from", {L(self), token});
    WordAt(self, 0) = static_cast<uint32_t>(token);
}
void WStringDtor(void* self, int16_t flag) { ev("wstr_dtor", {L(self), flag}); }
void WStringAssign(void* self, const char16_t* text, uint32_t n) { ev("wstr_assign", {L(self), n, Hash16(text, n)}); }
void StringCtor(void* self) { ev("str_ctor", {L(self)}); }
void StringDtor(void* self, int16_t flag) { ev("str_dtor", {L(self), flag}); }
void StringAssign(void* self, const std::string& other, uint32_t pos, int32_t count) {
    ev("str_assign", {L(self), HashC(other.c_str()), pos, count});
}
void DequeCtor(void* self) {
    ev("deque_ctor", {L(self)});
    WordAt(self, 0x10) = static_cast<uint32_t>(ret("deque10"));
}
void DequeCopy(void* self, const void* other) { ev("deque_copy", {L(self), L(other)}); }
void DequeTearDown(void* self) { ev("deque_td", {L(self)}); }
void ListDtor(void* self, int16_t flag) { ev("list_dtor", {L(self), flag}); }
void NocaseSetDtor(void* self, int16_t flag) { ev("nset_dtor", {L(self), flag}); }
void BuddyMapDtor(void* self, int16_t flag) { ev("map_dtor", {L(self), flag}); }
void WStringSetDtor(void* self, int16_t flag) { ev("wset_dtor", {L(self), flag}); }
void MutexCtor(void* self) { ev("mutex_ctor", {L(self)}); }
void MutexDtor(void* self, int16_t flag) { ev("mutex_dtor", {L(self), flag}); }
void ChatDataCtor(void* self) { ev("chatdata_ctor", {L(self)}); }
void IPBaseCtor(void* self) { ev("ip_ctor", {L(self)}); }
int32_t IPSystemStartup(void* self) {
    ev("ip_startup", {L(self)});
    return static_cast<int32_t>(ret("Startup"));
}
void IPSystemShutdown(void* self) { ev("ip_shutdown", {L(self)}); }
void ChatCtor(void* self) { ev("Chat", {newlab(self)}); }
void ChatCallbackFn(int32_t, void*, void*) {}
int32_t ChatConnect(void* chat, const char* server, uint16_t port, void (*callback)(int32_t, void*, void*), char* extra) {
    ev("Connect", {L(chat), HashC(server), port, callback == &ChatCallbackFn ? 1 : 0, L(extra)});
    return static_cast<int32_t>(ret("Connect"));
}
void ChatCreatePlayer(void* chat, const void* name, int32_t customerId, int32_t playerId, const void* password, void* context) {
    ev("CreatePlayer", {L(chat), WordAt(const_cast<void*>(name), 0), customerId, playerId, WordAt(const_cast<void*>(password), 0), L(context)});
}
void ChatSetHearingRange(void* chat, uint32_t range, void* context) { ev("hearing", {L(chat), range, L(context)}); }
void ChatEnumPlayers(void* chat, void* context) { ev("enum", {L(chat), L(context)}); }
void ChatRelease(void* chat) { ev("chat_release", {L(chat)}); }
void ChatDtor(void* chat, int16_t flag) { ev("chat_dtor", {L(chat), flag}); }
const char16_t* LoginCustomerName(CFSOnlineSystem* sys) {
    ev("cust_name", {L(sys)});
    return gFakeCustomerName;
}
const char16_t* LoginPassword(CFSOnlineSystem* sys) {
    ev("password", {L(sys)});
    return gFakePassword;
}
int32_t LoginPlayerID(CFSOnlineSystem* sys) {
    ev("player_id", {L(sys)});
    return static_cast<int32_t>(ret("pid"));
}
int32_t LoginCustomerID(CFSOnlineSystem* sys) {
    ev("cust_id", {L(sys)});
    return static_cast<int32_t>(ret("cid"));
}
uint32_t AvatarID(CFSOnlineSystem* sys) {
    ev("avatar", {L(sys)});
    return static_cast<uint32_t>(ret("avatar"));
}
void COnlinePersonCtor(void* self, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, uint32_t avatar, void* name, bool flag) {
    ev("COnlinePerson", {newlab(self), cell.mIndex, avatar, L(name), flag ? 1 : 0});
}

}  // namespace online_callee

extern "C" {

uint64_t tpw_ocs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem}, {"gMouse", &gMouse},           {"gMouseWord1E0", &gMouseWord1E0},
        {"gMouseWord1E4", &gMouseWord1E4}, {"gGameWorld", &gGameWorld},   {"custName", &gFakeCustomerName},
        {"password", &gFakePassword},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint32_t tpw_ocs_handle(uint64_t p) { return online_callee::PointerToHandle(reinterpret_cast<const void*>(static_cast<uintptr_t>(p))); }
uint64_t tpw_ocs_deref(uint32_t h) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(online_callee::HandleToPointer(h))); }

uint64_t tpw_ocs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ocs_lab(uint64_t p) { return static_cast<int32_t>(lab(reinterpret_cast<const void*>(static_cast<uintptr_t>(p)))); }

// gOnlineConfig's chat server and port (the host's std::string is not the Mac's pointer)
void tpw_ocs_set_config(const char* server, int32_t port) {
    gOnlineConfig.mChatServer = server;
    gOnlineConfig.mChatPort = static_cast<int16_t>(port);
}

int32_t tpw_ocs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    return n;
}

void tpw_ocs_ctor(uint64_t p) { new (reinterpret_cast<void*>(static_cast<uintptr_t>(p))) COnlineControl(); }
void tpw_ocs_dtor(uint64_t p) { reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(p))->~COnlineControl(); }
void tpw_ocs_init_client(uint64_t p) { reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(p))->InitClient(); }
void tpw_ocs_init_world(uint64_t p) { reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(p))->InitGameWorld(); }

}  // extern "C"
