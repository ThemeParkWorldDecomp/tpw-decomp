// online.cpp (main app): COnlineControl, the online game client. Unit v5016: the constructor 0x10213870, the destructor 0x10213dd4, InitClient
// 0x102143c4 and InitGameWorld 0x10214678.
//
// The Mac object is 0xe8c bytes of 32-bit words (strings, sets and deques are one pointer each, or small blocks): the class declares
// them as opaque bytes, so the fields are reached by their Mac offsets. A pointer the object holds (the chat at +0, the avatar's person
// at +4, the two string blocks at +8 and +0xc and the character pointer inside a string block at +0xc) does not fit a 32-bit word on the
// host: the word is a handle, index + 1 into a table of pointers, 0 for NULL (as CWorld keeps its control pointer). The container and string
// members are built and torn down by library code that is faked by the tests (online_callee, include/online.h).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <unordered_map>
#include <vector>

#include "FS_Online.h"
#include "Mouse.h"
#include "World.h"
#include "analyser.h"
#include "online.h"
#include "platform/msl.h"
#include "platform/samsutils.h"

extern OnlineConfig gOnlineConfig;   // 0x102b2b70

namespace online_callee {

namespace {
std::vector<void*>& HandleTable() {
    static std::vector<void*> table;
    return table;
}
std::unordered_map<const void*, uint32_t>& HandleIndex() {
    static std::unordered_map<const void*, uint32_t> index;
    return index;
}
}  // namespace

uint32_t PointerToHandle(const void* p) {
    if (p == nullptr) return 0;
    auto found = HandleIndex().find(p);
    if (found != HandleIndex().end()) return found->second;
    HandleTable().push_back(const_cast<void*>(p));
    const uint32_t handle = static_cast<uint32_t>(HandleTable().size());
    HandleIndex()[p] = handle;
    return handle;
}

void* HandleToPointer(uint32_t handle) {
    if (handle == 0 || handle > HandleTable().size()) return nullptr;
    return HandleTable()[handle - 1];
}

}  // namespace online_callee

namespace {

const char16_t kFormatRandomName[] = u"%08x";            // 0x1025179c
const char16_t kFormatLoginName[] = u"%ls";              // 0x102517a6 (the same table, 0xa bytes on)
const char kEmptyPath[] = "";                            // 0x10250e99
const char kConnectionFailed[] = "ONLINE CHAT: connection failed!\n";   // 0x10250e9a

inline uint8_t* Bytes(COnlineControl* self) { return reinterpret_cast<uint8_t*>(self); }
inline uint32_t& Word(COnlineControl* self, std::size_t offset) { return *reinterpret_cast<uint32_t*>(Bytes(self) + offset); }
inline void* Ptr(uint32_t word) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(word)); }   // a raw word (the deque's)
inline void* Deref(uint32_t handle) { return online_callee::HandleToPointer(handle); }
inline uint32_t ToHandle(const void* pointer) { return online_callee::PointerToHandle(pointer); }

// The string's character data: a string is one handle of a block whose word at +0xc is the handle of the characters.
inline void* StringData(uint32_t stringWord) {
    const uint8_t* block = static_cast<const uint8_t*>(Deref(stringWord));
    return Deref(*reinterpret_cast<const uint32_t*>(block + 0xc));
}

// gOnlineSystem is made on first use; the original inlines this at every use.
CFSOnlineSystem* MakeOnlineSystem() {
    void* block = online_callee::OperatorNew(0x404);
    CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(block);
    if (system != nullptr) new (system) CFSOnlineSystem();
    gOnlineSystem = system;
    return system;
}

}  // namespace

// 0x10213870
COnlineControl::COnlineControl() {
    uint8_t* self = Bytes(this);
    uint32_t tempDeque[6];   // the temporary deque on the stack
    char16_t text[0x400];
    Word(this, 0) = 0;
    Word(this, 4) = 0;
    online_callee::WStringCtor(self + 8);
    online_callee::StringCtor(self + 0xc);
    self[0x10] = 0;
    self[0x11] = 0;
    self[0x12] = 0;
    self[0x13] = 0;
    self[0x14] = 0;
    online_callee::MutexCtor(self + 0x18);
    online_callee::MutexCtor(self + 0x1c);
    online_callee::ChatDataCtor(self + 0xe20);
    self[0xe60] = 0;
    Word(this, 0xe64) = 0;
    online_callee::DequeCtor(tempDeque);
    online_callee::DequeCopy(self + 0xe68, tempDeque);
    online_callee::DequeTearDown(tempDeque);
    if (tempDeque[4] != 0) online_callee::OperatorDelete(Ptr(tempDeque[4]));
    self[0xe84] = 0;
    online_callee::WStringCtor(self + 0xe88);
    const int32_t random = rand();
    swprintf(text, 0x400, kFormatRandomName, random);
    const uint32_t length = wcslen(text);
    online_callee::WStringAssign(self + 8, text, length);
    online_callee::StringAssign(self + 0xc, gOnlineConfig.mChatServer, 0, -1);
}

// 0x10213dd4. The original also frees the object when its hidden flag is positive; the host's caller does that.
COnlineControl::~COnlineControl() {
    uint8_t* self = Bytes(this);
    if (Word(this, 0) != 0) {
        online_callee::ChatRelease(Deref(Word(this, 0)));
        online_callee::ChatDtor(Deref(Word(this, 0)), 1);
        Word(this, 0) = 0;
        SaveBuddy(kEmptyPath);
        SaveIgnore(kEmptyPath);
    }
    if (self[0x13] == 1) {
        uint32_t ip[4];
        online_callee::IPBaseCtor(ip);
        online_callee::IPSystemShutdown(ip);
    }
    online_callee::WStringDtor(self + 0xe88, -1);
    online_callee::DequeTearDown(self + 0xe68);
    if (Word(this, 0xe78) != 0) online_callee::OperatorDelete(Ptr(Word(this, 0xe78)));
    online_callee::ListDtor(self + 0xe54, -1);
    online_callee::NocaseSetDtor(self + 0xe44, -1);
    online_callee::BuddyMapDtor(self + 0xe34, -1);
    online_callee::WStringSetDtor(self + 0xe28, -1);
    online_callee::WStringDtor(self + 0xe24, -1);
    online_callee::WStringDtor(self + 0xe20, -1);
    online_callee::MutexDtor(self + 0x1c, -1);
    online_callee::MutexDtor(self + 0x18, -1);
    online_callee::StringDtor(self + 0xc, -1);
    online_callee::WStringDtor(self + 8, -1);
}

// 0x102143c4
void COnlineControl::InitClient() {
    uint8_t* self = Bytes(this);
    uint32_t ip[4];
    online_callee::IPBaseCtor(ip);
    if (online_callee::IPSystemStartup(ip) == 0) self[0x13] = 1;
    if (gOnlineSystem == nullptr) MakeOnlineSystem();
    const char16_t* loginName = online_callee::LoginCustomerName(gOnlineSystem);
    swprintf(reinterpret_cast<char16_t*>(self + 0x220), 0x400, kFormatLoginName, loginName);
    const uint32_t length = wcslen(reinterpret_cast<char16_t*>(self + 0x220));
    online_callee::WStringAssign(self + 8, reinterpret_cast<char16_t*>(self + 0x220), length);
    if (Word(this, 0) == 0) {
        void* block = online_callee::OperatorNew(0xc);
        if (block != nullptr) online_callee::ChatCtor(block);
        Word(this, 0) = ToHandle(block);
    }
    const char* server = static_cast<const char*>(StringData(Word(this, 0xc)));
    const uint16_t port = static_cast<uint16_t>(gOnlineConfig.mChatPort);
    if (online_callee::ChatConnect(Deref(Word(this, 0)), server, port, online_callee::ChatCallbackFn, nullptr) == 0) {
        SamsUtilities::UDebug(kConnectionFailed);
    } else {
        uint32_t nameString;
        uint32_t passwordString;
        online_callee::WStringCtorFrom(&nameString, static_cast<const char16_t*>(StringData(Word(this, 8))));
        if (gOnlineSystem == nullptr) MakeOnlineSystem();
        const char16_t* password = online_callee::LoginPassword(gOnlineSystem);
        online_callee::WStringCtorFrom(&passwordString, password);
        if (gOnlineSystem == nullptr) MakeOnlineSystem();
        CFSOnlineSystem* system = gOnlineSystem;   // (the original keeps this word, even when it is NULL)
        if (system == nullptr) MakeOnlineSystem();
        const int32_t playerId = online_callee::LoginPlayerID(gOnlineSystem);
        const int32_t customerId = online_callee::LoginCustomerID(system);
        online_callee::ChatCreatePlayer(Deref(Word(this, 0)), &nameString, customerId, playerId, &passwordString, this);
        online_callee::WStringDtor(&passwordString, -1);
        online_callee::WStringDtor(&nameString, -1);
    }
    self[0xe84] = 0;
}

// 0x10214678
void COnlineControl::InitGameWorld() {
    uint8_t* self = Bytes(this);
    CMap* map = reinterpret_cast<CMap*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8);
    const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell = map->GetParkEntrancePos(0);
    void* personBlock = online_callee::OperatorNew(0x3a8);
    if (personBlock != nullptr) {
        if (gOnlineSystem == nullptr) MakeOnlineSystem();
        const uint32_t avatar = online_callee::AvatarID(gOnlineSystem);
        online_callee::COnlinePersonCtor(personBlock, cell, avatar, self + 8, true);
    }
    Word(this, 4) = ToHandle(personBlock);
    TeleportAvatar(cell);
    void* modeBlock = online_callee::OperatorNew(8);
    CMMOnline* online = static_cast<CMMOnline*>(modeBlock);
    if (online != nullptr) new (online) CMMOnline();
    if (gMouse == nullptr) {
        void* mouseBlock = online_callee::OperatorNew(1);
        CMouse* mouse = static_cast<CMouse*>(mouseBlock);
        if (mouse != nullptr) {
            void* defaultBlock = online_callee::OperatorNew(8);
            CMMDefault* defaultMode = static_cast<CMMDefault*>(defaultBlock);
            if (defaultMode != nullptr) new (defaultMode) CMMDefault();
            mouse->AssignMouseMode(defaultMode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(online);
    online_callee::ChatSetHearingRange(Deref(Word(this, 0)), 0xa00, this);
    online_callee::ChatEnumPlayers(Deref(Word(this, 0)), this);
}
