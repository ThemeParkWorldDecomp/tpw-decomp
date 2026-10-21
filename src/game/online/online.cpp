// online.cpp: name proven by __sinit_online_cpp in the main application.

#include "analyser.h"
#include "CMsg.h"
#include "Debug.h"
#include "email.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_UserInfo.h"
#include "GameSys.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "Mouse.h"
#include "MslSubscriptionTree.h"
#include "MslWStringTree.h"
#include "online.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "Ptr32.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"
#include "std_basic_streambuf_wchar.h"
#include "std_cdeque.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTList.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "WinScrn.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

// online.cpp / OnlinePerson.cpp (main app), unit v51009:
//   VoteNotifyCallback 0x10213590, SwearFilter::Init 0x1020d1bc, stristr 0x1020cbec, IPBase::ShowError 0x1020cac8,
//   FTPClient::ProcessEvents 0x1020c8cc.

// ---- VoteNotifyCallback: the vote connection's callback; sends command 0x2711 and the first argument to the online control
// (made on first use) as a vector of two Arguments (three words each: char string, wide string, number).
static void PushArgument(uint32_t* vec, int32_t number) {
    uint32_t argument[3];
    online_callee::StringCtor(&argument[0]);
    online_callee::WStringCtor(&argument[1]);
    argument[2] = static_cast<uint32_t>(number);
    online_callee::ArgumentVectorPushBack(vec, argument);
    online_callee::WStringDtor(&argument[1], -1);
    online_callee::StringDtor(&argument[0], -1);
}

// 0x10213590 (the last two arguments are not read)
void VoteNotifyCallback(int32_t a, int32_t b, void* c, void* d) {
    uint32_t vec[3] = {0, 0, 0};
    PushArgument(vec, 0x2711);
    PushArgument(vec, a);
    if (gOnlineControl == nullptr) {
        void* block = online_callee::OperatorNew(0xe8c);
        if (block != nullptr) new (block) COnlineControl();
        gOnlineControl = static_cast<COnlineControl*>(block);
    }
    gOnlineControl->CallbackFn(0, vec);
    online_callee::ArgumentVectorTearDown(vec);
}

// online.cpp (main app): the park-goto callbacks of the online client.

namespace {
// Strings of the data section at 0x10250e7f (the TOC slot -0x395c).
const char kGotoFailed[] = "goto ftp download failed\n";

COnlineControl* Control() {
    if (gOnlineControl == nullptr) {
        void* block = online_callee::OperatorNew(0xe8c);
        if (block != nullptr) new (block) COnlineControl();
        gOnlineControl = static_cast<COnlineControl*>(block);
    }
    return gOnlineControl;
}
}  // namespace

// 0x10213728
int32_t GotoPark_OK(void*) {
    Control()->GotoStoredPerson();
    return 1;
}

// 0x102137bc
void GotoCallbackFn(int32_t status, const void*) {
    if (status == 0) {
        GS_SetCurrentGameStatus(GS_LEAVE_PARK);
    } else {
        Control()->GotoFailed();
        SamsUtilities::UDebug(kGotoFailed);
    }
}

// online.cpp (main app): COnlineControl, the online game client. Unit v5016: the constructor 0x10213870, the destructor 0x10213dd4, InitClient
// 0x102143c4 and InitGameWorld 0x10214678.
//
// The Mac object is 0xe8c bytes of 32-bit words (strings, sets and deques are one pointer each, or small blocks): the class declares
// them as opaque bytes, so the fields are reached by their Mac offsets. A pointer the object holds (the chat at +0, the avatar's person
// at +4, the two string blocks at +8 and +0xc and the character pointer inside a string block at +0xc) does not fit a 32-bit word on the
// host: the word is a handle, index + 1 into a table of pointers, 0 for NULL (as CWorld keeps its control pointer). The container and string
// members are built and torn down by library code that is faked by the tests (online_callee, include/online.h).

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

// COnlineControl (online.cpp): Shutdown, WantToShutDown, Update, CallbackFn and the first-person moves.
// The Mac object keeps 32-bit pointers at fixed offsets; they are read raw (the arena is 32-bit addressable).

namespace {

template <class T>
T* Field(void* object, size_t offset) { return reinterpret_cast<T*>(static_cast<char*>(object) + offset); }

void* PointerAt(void* object, size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<char*>(object) + offset, 4);
    return online_callee::HandleToPointer(word);
}

int32_t Halfword(MapPos p) { return static_cast<int32_t>(static_cast<int16_t>((p.mCell << 8) | p.mOffset)); }

}  // namespace

// 0x10214840
void COnlineControl::Shutdown() {
    bthread_mutex* mutex = Field<bthread_mutex>(this, 0x1c);
    mutex->lock();
    *Field<uint8_t>(this, 0x11) = 1;
    *Field<uint8_t>(this, 0x12) = 1;
    mutex->unlock();
}

// 0x102148bc
bool COnlineControl::WantToShutDown() {
    bthread_mutex* mutex = Field<bthread_mutex>(this, 0x1c);
    mutex->lock();
    bool answer = *Field<bool>(this, 0x12);
    mutex->unlock();
    return answer;
}

// 0x10214944
void COnlineControl::Update() {
    ProcessQueue();
    if (PointerAt(this, 0xe64) != nullptr && *Field<uint8_t>(this, 0xe60) != 0) {
        SamsUtilities::UDebug("We have a vote connection, but we are done voting, release and kill it\n");
        online_callee::VoteRelease(PointerAt(this, 0xe64));
        online_callee::VoteDtor(PointerAt(this, 0xe64), 1);
        *Field<uint32_t>(this, 0xe64) = 0;
        *Field<uint8_t>(this, 0xe60) = 0;
        SamsUtilities::UDebug("Vote connection released, ready for new vote\n");
        online_callee::DisableVoteButton(false);
    }
}

// online.cpp (main app): ChatCallbackFn 0x10214a0c, the chat library's callback. Errors -5 and -4 raise the "connection lost" flags
// and the error dialog 0x4c; every other id goes to the lazily created COnlineControl's CallbackFn. The second argument is unused.

static COnlineControl* OnlineControlOnFirstUse() {
    if (gOnlineControl == nullptr) {
        COnlineControl* created = static_cast<COnlineControl*>(online_callee::OperatorNew(0xe8c));
        if (created != nullptr) new (created) COnlineControl();
        gOnlineControl = created;
    }
    return gOnlineControl;
}

// 0x10214a0c
void ChatCallbackFn(int32_t id, void* unused, void* arguments) {
    (void)unused;
    if (id < -3 && id >= -5) {
        COnlineControl* control = OnlineControlOnFirstUse();
        char* bytes = reinterpret_cast<char*>(control);
        bthread_mutex* mutex = reinterpret_cast<bthread_mutex*>(bytes + 0x1c);
        mutex->lock();
        *reinterpret_cast<uint8_t*>(bytes + 0x11) = 1;
        *reinterpret_cast<uint8_t*>(bytes + 0x12) = 1;
        mutex->unlock();
        AddUIError(0x4c, nullptr);
    } else {
        OnlineControlOnFirstUse()->CallbackFn(id, arguments);
    }
}

// 0x10214b70
void COnlineControl::CallbackFn(int32_t id, void* arguments) {
    bthread_mutex* mutex = Field<bthread_mutex>(this, 0x18);
    mutex->lock();
    online_callee::ArgumentDequePushBack(Field<void>(this, 0xe68), arguments);
    mutex->unlock();
}

// COnlineControl::ProcessQueue 0x10214c34, AddBuddy 0x1021baf8 and LoadBuddy 0x1021d004 (main app, online.cpp; unit v5695).
// ProcessQueue hands the queued answers of the chat server (vectors of Argument in a deque at 0xe68) to the vote, normal or init handler.
// AddBuddy puts a name into the buddy map (0xe34) or updates its two flag bytes, then tells the UI. LoadBuddy reads the buddy file
// line by line into AddBuddy; like LoadIgnore it builds a narrow ":buddy.dat" string that is never used (the file name comes from the
// wide path string).

using namespace ignore_callee;

namespace {

inline uint32_t Word32(const void* address) {
    uint32_t value;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

const char kBuddyDat[] = ":buddy.dat";                                      // 0x10251480
const char kFormatPath[] = "%ls";                                           // 0x1025135d
const char kIosFailure[] = "ios_base failure in clear";                     // 0x1025148b
const char kExceptionTypes[] = "!std::exception!!std::ios_base::failure!!"; // 0x102517e6
const char16_t kPathSuffix[] = u".bud";                                      // 0x102517dc

// One look at the deque's first record through the block table (the original calls operator[] again for every use).
uint8_t* FirstRecord(COnlineControl* self) {
    void* slot = errdeque_callee::CdequeIndex(Bytes(self) + 0xe6c, Word(self, 0xe7c) / Word(self, 0xe68));
    const uint32_t first = Word(self, 0xe7c);
    const uint32_t blockSize = Word(self, 0xe68);
    uint8_t* block = Ptr32<uint8_t>{Word32(slot)}.get();
    return block + (first - first / blockSize * blockSize) * 0xc;
}

void SetFailed(IgnoreIos* ios) {
    ios->mState |= 4;
    if (ios->mStreamBuf == 0) ios->mState |= 1;
    if ((ios->mState & ios->mExceptions) != 0) {
        MslThrowObject object;
        object.mVtbl = gIosFailureVtbl;
        char* block = NewArray(static_cast<uint32_t>(std::strlen(kIosFailure)) + 1);
        RefPtrCtor(&object.mMessage, block);
        StrCpy(object.mMessage, kIosFailure);
        MslThrow(kExceptionTypes, &object, gIosFailureDtorVec);
    }
}

void BuildPath(COnlineControl* self, const char* path, void* text, IgnorePathString* filePath, char* buffer) {
    if (gUserInfo == nullptr) {
        void* block = online_callee::OperatorNew(0xd4);
        if (block != nullptr) UserInfoCtor(block);
        gUserInfo = static_cast<CFSUserInfo*>(block);
    }
    IgnorePathString online;
    ReturnOnlinePath(&online, gUserInfo);
    PathCopy(filePath, &online);
    PathDtor(&online, -1);
    PathAppendString(filePath, reinterpret_cast<const uint8_t*>(&gTPGlobals) + 0x44c0);
    // this + 8 is the player name (a std::wstring: one word, the representation's data pointer at +0xc, native pointer at +0x10 here)
    const uint8_t* rep = static_cast<const uint8_t*>(online_callee::HandleToPointer(Word(self, 8)));
    const char16_t* name;
    std::memcpy(&name, rep + 0x10, sizeof(name));
    PathAppendText(filePath, name);
    PathAppendText(filePath, kPathSuffix);
    CharStringAssign(text, path, static_cast<uint32_t>(std::strlen(path)));
    CharStringAppend(text, kBuddyDat, static_cast<uint32_t>(std::strlen(kBuddyDat)));
    SprintfWide(buffer, kFormatPath, filePath->mData);
}

}  // namespace

// 0x10214c34
void COnlineControl::ProcessQueue() {
    reinterpret_cast<bthread_mutex*>(Bytes(this) + 0x18)->lock();
    while (Word(this, 0xe80) != 0) {
        uint8_t* record = FirstRecord(this);
        bool vote = false;
        if (Word32(record + 4) != 0) {
            record = FirstRecord(this);
            const uint8_t* arguments = Ptr32<uint8_t>{Word32(record + 8)}.get();
            vote = Word32(arguments + 8) == 0x2711;
        }
        if (vote) {
            if (gOnlineControl == nullptr) {
                void* block = online_callee::OperatorNew(0xe8c);
                if (block != nullptr) new (block) COnlineControl();
                gOnlineControl = static_cast<COnlineControl*>(block);
            }
            COnlineControl* target = gOnlineControl;
            record = FirstRecord(this);
            const uint8_t* arguments = Ptr32<uint8_t>{Word32(record + 8)}.get();
            target->ProcessVoteResponse(static_cast<int32_t>(Word32(arguments + 0xc + 8)));
        } else if (Bytes(this)[0x10] != 0) {
            record = FirstRecord(this);
            ProcessResponse(record);
        } else {
            record = FirstRecord(this);
            ProcessInitResponse(record);
        }
        online_callee::ArgumentDequePopFront(Bytes(this) + 0xe68);
    }
    reinterpret_cast<bthread_mutex*>(Bytes(this) + 0x18)->unlock();
}

// COnlineControl::ProcessInitResponse (0x10214e80): the chat client's answer to the init request. The first Argument's command number picks
// a case of the jump table at 0x102b1b68 (55 entries; a number past 0x36 and the entries that point at 0x10215480 take the default).

namespace {

const char kMsgB2[] = "ONLINE CHAT: Created a player, let's join a park.\n";            // +0xb2
const char kFormatD[] = "%d";           // +0xe5
const char kMsgE8[] = "ONLINE CHAT: Denied because of childlock\n";                    // +0xe8
const char kMsg112[] = "ONLINE CHAT: Denied because authorisation failed\n";           // +0x112
const char kMsg144[] = "ONLINE CHAT: Denied because we are blacmarked out\n";          // +0x144
const char kMsg177[] = "ONLINE CHAT: Denied because we are already in (duplicate player - get your own account)\n";   // +0x177
const char kMsg1d0[] = "ONLINE CHAT: Denied because the park is full\n";               // +0x1d0
const char kMsg1fe[] = "ONLINE CHAT: Denied because the server has run out of memory! HELP!\n";   // +0x1fe
const char kMsg243[] = "ONLINE CHAT: Denied because of a parameter error.\n";          // +0x243
const char kMsg276[] = "ONLINE CHAT: The park has been created successfully\n";        // +0x276
const char kMsg2ab[] = "ONLINE CHAT: Loading buddy and ignore lists\n";                // +0x2ab
const char kMsg2d8[] = "ONLINE CHAT: Dunno what is going on here, but it can't be a good thing\n";   // +0x2d8
const char16_t kRandomName[] = u"";      // 0x1025179c + 0x12 (an empty string)

// The jump table at 0x102b1b68: command number -> case (0 = the default at 0x10215480, 11 = the epilogue at 0x102154dc).
int CaseOf(uint32_t command) {
    switch (command) {
    case 1: case 7: case 44: return 11;
    case 40: return 8;    // 0x10215340 (0xa2)
    case 41: return 7;    // 0x102152d0 (0xa3)
    case 43: return 5;    // 0x102151f0 (0xa5)
    case 45: return 6;    // 0x10215260 (0xa8)
    case 48: return 9;    // 0x102153b0 (0xaa)
    case 49: return 2;    // 0x102150f4 (childlock)
    case 50: return 3;    // 0x10215110 (0xae)
    case 51: return 4;    // 0x10215180 (0xaf)
    case 52: return 1;    // 0x10214ec8 (created a player)
    case 54: return 10;   // 0x10215420 (the park was created)
    default: return 0;    // 0x10215480 (a number past 0x36 too)
    }
}

inline uint32_t Word_2(const void* base, uint32_t offset) {
    return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(base) + offset);
}
// A 32-bit pointer word of a Mac-layout block (an offset into the arena on the host).
inline const char16_t* WideAt(const void* base, uint32_t offset) {
    return reinterpret_cast<const Ptr32<const char16_t>*>(static_cast<const uint8_t*>(base) + offset)->get();
}

// "if (*gOnlineSystem == NULL) *gOnlineSystem = new CFSOnlineSystem": the original repeats it before every use.
CFSOnlineSystem* System() {
    if (gOnlineSystem == nullptr) {
        void* memory = online_callee::OperatorNew(0x404);
        CFSOnlineSystem* created = nullptr;
        if (memory != nullptr) created = new (memory) CFSOnlineSystem();
        gOnlineSystem = created;
    }
    return gOnlineSystem;
}

COnlineControl* Control_2() {
    if (gOnlineControl == nullptr) {
        void* memory = online_callee::OperatorNew(0xe8c);
        COnlineControl* created = nullptr;
        if (memory != nullptr) created = new (memory) COnlineControl();
        gOnlineControl = created;
    }
    return gOnlineControl;
}

// The tail shared by the default and the error cases: both flag bytes set under the lock at 0x1c.
void SetFlags() {
    COnlineControl* control = Control_2();
    uint8_t* bytes = reinterpret_cast<uint8_t*>(control);
    bthread_mutex* mutex = reinterpret_cast<bthread_mutex*>(bytes + 0x1c);
    mutex->lock();
    bytes[0x11] = 1;
    bytes[0x12] = 1;
    mutex->unlock();
}

void ErrorCase(const char* message, int32_t error) {
    SamsUtilities::UDebug(message);
    AddUIError(error, static_cast<TbITable*>(gText.mResourcePointers[19]));   // gText + 0x598
    SetFlags();
}

}  // namespace

// 0x10214e80
void COnlineControl::ProcessInitResponse(void* arguments) {
    const uint8_t* data = *reinterpret_cast<uint8_t* const*>(static_cast<const uint8_t*>(arguments) + 8);   // vector's first element
    const uint32_t command = Word_2(data, 8);
    switch (CaseOf(command)) {
    case 1: {
        SamsUtilities::UDebug(kMsgB2);
        uint8_t* info = reinterpret_cast<uint8_t*>(System()->GetFTPDownloadParkInfo());
        char buffer[16];
        std::sprintf(buffer, kFormatD, static_cast<int>(Word_2(info, 8)));
        uint64_t text;
        init_callee::StringFromChars(&text, buffer);
        init_callee::ChatSetPlayerData(Word_2(this, 0), static_cast<int32_t>(online_callee::AvatarID(System())), this);
        info = reinterpret_cast<uint8_t*>(System()->GetFTPDownloadParkInfo());
        uint64_t name;
        online_callee::WStringCtorFrom(&name, WideAt(info, 0x14));
        uint64_t topic;
        online_callee::WStringCtorFrom(&topic, kRandomName);
        CFSOnlineSystem* system = System();
        System();
        info = reinterpret_cast<uint8_t*>(System()->GetFTPDownloadParkInfo());
        uint8_t* city = static_cast<uint8_t*>(init_callee::ReturnCityInfo(Word_2(system, 0xe0), static_cast<int32_t>(Word_2(info, 0x48))));
        uint64_t cityName;
        online_callee::WStringCtorFrom(&cityName, WideAt(city, 0x14));
        init_callee::ChatCreateRoom(Word_2(this, 0), &name, &topic, &cityName, &text, 0x100, this);
        online_callee::WStringDtor(&cityName, -1);
        online_callee::WStringDtor(&topic, -1);
        online_callee::WStringDtor(&name, -1);
        online_callee::StringDtor(&text, -1);
        break;
    }
    case 2:
        SamsUtilities::UDebug(kMsgE8);
        mClientReady = 1;
        init_callee::LockChatPanelOff();
        break;
    case 3: ErrorCase(kMsg112, 0xae); break;
    case 4: ErrorCase(kMsg144, 0xaf); break;
    case 5: ErrorCase(kMsg177, 0xa5); break;
    case 6: ErrorCase(kMsg1d0, 0xa8); break;
    case 7: ErrorCase(kMsg1fe, 0xa3); break;
    case 8: ErrorCase(kMsg243, 0xa2); break;
    case 9: ErrorCase(kMsg243, 0xaa); break;
    case 10:
        mClientReady = 1;
        AddContact(*reinterpret_cast<std::u16string*>(reinterpret_cast<uint8_t*>(this) + 8), 0);
        gGameWorld->StartingPopulationForOnline();
        SamsUtilities::UDebug(kMsg276);
        SamsUtilities::UDebug(kMsg2ab);
        LoadBuddy(kEmptyPath);
        LoadIgnore(kEmptyPath);
        break;
    case 11: break;   // a table entry that points at the epilogue
    default:
        SamsUtilities::UDebug(kMsg2d8);
        SetFlags();
        break;
    }
}

// COnlineControl::ProcessResponse 0x10215560 (8376 bytes, online.cpp; unit v5638): the chat server's responses. The first Argument's
// command number minus 0xb picks a case of the jump table at 0x102b1c44 (72 entries); the gaps and anything past 0x52 return at once.
// Every wide string is one word on the Mac (MSL's rep pointer, the text pointer at rep+0xc); here they are std::u16string objects the
// callees fill. The fields are reached by Mac offset (see online.h).

namespace {

using WString = std::u16string;

// Strings of the data section at 0x10250e7f (the TOC slot -0x395c) and their offsets.
const char kTraceTeleport[] = "Pos for teleport - x: %d, y:%d\n";                 // +0x320
const char kTraceBeforeUnhide[] = "Pos before unhide - x: %d, y:%d\n";            // +0x340
const char kTraceUnhide[] = "Pos for unhide - x: %d, y:%d\n";                     // +0x361
const char kTraceAfterUnhide[] = "Pos after unhide & teleport - x: %d, y:%d\n";   // +0x37f
const char kMsgBadZ[] = "What is going on here then? Unrecognised Z value in online BOSS_CHAT_MOVE\n";   // +0x3aa
const char kMsgConnectedHere[] = "One of your buddies entered this park - BOSS_CHAT_CONNECTED\n";      // +0x3f5
const char kMsgConnectedChat[] = "One of your buddies entered a chat park - BOSS_CHAT_CONNECTED\n";    // +0x432
const char kMsgEnteredPark[] = "One of your buddies entered this park - BOSS_CHAT_ENTERED_PARK\n";    // +0x471
const char kMsgGotoBusy[] = "Goto already in progress. Can't goto again!\n";                         // +0x4b1

constexpr uint32_t kWhole = 0xffffffffu;   // npos

uint8_t* Base(COnlineControl* self) { return reinterpret_cast<uint8_t*>(self); }

uint32_t Word_3(const void* block, size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const uint8_t*>(block) + offset, 4);
    return word;
}

void* PointerAt_2(const void* object, size_t offset) { return online_callee::HandleToPointer(Word_3(object, offset)); }

// A signed halfword at a Mac offset (lha): the high byte first.
int32_t HalfAt(const void* block, size_t offset) {
    const uint8_t* p = static_cast<const uint8_t*>(block) + offset;
    return static_cast<int16_t>(static_cast<uint16_t>((p[0] << 8) | p[1]));
}

int32_t Halfword_2(MapPos pos) { return static_cast<int16_t>(static_cast<uint16_t>((pos.mCell << 8) | pos.mOffset)); }

// srawi 8 + addze: the cell is the value / 256 rounded toward zero, the offset what is left.
MapPos ToMapPos(int32_t value) {
    const int32_t cell = value / 256;
    MapPos pos;
    pos.mCell = static_cast<uint8_t>(cell);
    pos.mOffset = static_cast<uint8_t>(value - cell * 256);
    return pos;
}

// gText's TbITable of UI strings (the word at 0x598).
const TbITable* Table() { return static_cast<const TbITable*>(gText.mResourcePointers[19]); }

char16_t* Buffer(COnlineControl* self) { return reinterpret_cast<char16_t*>(Base(self) + 0x220); }

// The buffer at 0x220 as a wide string, given to Feedback.
void FeedbackBuffer(COnlineControl* self) {
    WString message;
    online_callee::WStringCtorFrom(&message, Buffer(self));
    self->Feedback(message);
    online_callee::WStringDtor(&message, -1);
}

// The Argument's wide string copied into `name` (BStrVal, copy, the temporary destroyed).
void NameOf(WString* name, const Argument* argument) {
    WString value;
    response_callee::ArgumentBStrVal(&value, argument);
    online_callee::WStringCopy(name, &value, 0, kWhole);
    online_callee::WStringDtor(&value, -1);
}

// Text `id` with no tag, then Feedback.
void FormatPlain(COnlineControl* self, uint32_t id) {
    Table()->FormatString(Buffer(self), 0x400, id);
    FeedbackBuffer(self);
}

// Text `id` with the name as tag `index`, then Feedback.
void FormatName(COnlineControl* self, const WString& name, uint32_t id, uint32_t index) {
    TbUniStringVTag tag(index, name.c_str());
    Table()->FormatString(Buffer(self), 0x400, id, &tag);
    FeedbackBuffer(self);
}

// Text `id` with the second Argument's string as tag 0, then Feedback.
void FormatArgument(COnlineControl* self, const Argument* args, uint32_t id) {
    WString value;
    response_callee::ArgumentBStrVal(&value, &args[1]);
    TbUniStringVTag tag(0, value.c_str());
    Table()->FormatString(Buffer(self), 0x400, id, &tag);
    online_callee::WStringDtor(&value, -1);
    FeedbackBuffer(self);
}

// Text `id` with the strings of Arguments 1 and 2 as tags 0 and 1; the text goes to ReceiveText in `colour`, or to Feedback when the
// colour is negative.
void FormatTwo(COnlineControl* self, const Argument* args, uint32_t id, int32_t colour) {
    WString first;
    response_callee::ArgumentBStrVal(&first, &args[1]);
    WString second;
    response_callee::ArgumentBStrVal(&second, &args[2]);
    TbUniStringVTag tag0(0, first.c_str());
    TbUniStringVTag tag1(1, second.c_str());
    Table()->FormatString(Buffer(self), 0x400, id, &tag0, &tag1);
    online_callee::WStringDtor(&second, -1);
    online_callee::WStringDtor(&first, -1);
    WString message;
    online_callee::WStringCtorFrom(&message, Buffer(self));
    if (colour < 0)
        self->Feedback(message);
    else
        self->ReceiveText(message, colour);
    online_callee::WStringDtor(&message, -1);
}

// The buddy map's node for the name (a nocase copy as the key); the end node is this + 0xe38.
uint8_t* FindBuddy(COnlineControl* self, const WString& name) {
    WString key;
    online_callee::NocaseWStringCopy(&key, &name, 0, kWhole);
    uint8_t* node = static_cast<uint8_t*>(online_callee::BuddyTreeFind(Base(self) + 0xe34, &key));
    online_callee::NocaseWStringDtor(&key, 0);
    return node;
}

// GetDataForUser of `key`'s nocase copy, then the UI's buddy (or contact) entry for `text`.
void UpdateUser(COnlineControl* self, const WString& key, const char16_t* text, bool contact) {
    WString nocase;
    online_callee::NocaseWStringCopy(&nocase, &key, 0, kWhole);
    const uint32_t flags = self->GetDataForUser(reinterpret_cast<const nocase_string&>(nocase));
    if (contact)
        response_callee::UIOnlineUpdateContact(&gUIInterface, text, static_cast<int32_t>(flags));
    else
        response_callee::UIOnlineUpdateBuddy(&gUIInterface, text, static_cast<int32_t>(flags));
    online_callee::NocaseWStringDtor(&nocase, 0);
}

// The buddy entry from the second Argument's string, read twice (the text from the first copy, the key from the second).
void UpdateBuddyFromArgument(COnlineControl* self, const Argument* args) {
    WString text;
    response_callee::ArgumentBStrVal(&text, &args[1]);
    WString key;
    response_callee::ArgumentBStrVal(&key, &args[1]);
    UpdateUser(self, key, text.c_str(), false);
    online_callee::WStringDtor(&key, -1);
    online_callee::WStringDtor(&text, -1);
}

// 0x1021573c: a visitor moves. Arguments: 1 the name, 2 x, 3 y, 4 the mode (0 walk, 1 teleport, 2 hide, 3 unhide).
void Move(COnlineControl* self, const Argument* args) {
    WString name;
    NameOf(&name, &args[1]);
    COnlinePerson* person = self->GetPerson(name);
    if (person == nullptr || person == PointerAt_2(self, 4)) {
        online_callee::WStringDtor(&name, -1);
        return;
    }
    const MapPos x = ToMapPos(static_cast<int32_t>(args[2].mCommand));
    const MapPos y = ToMapPos(static_cast<int32_t>(args[3].mCommand));
    const int32_t mode = static_cast<int32_t>(args[4].mCommand);
    CThing* thing = reinterpret_cast<CThing*>(person);
    CPeep* peep = reinterpret_cast<CPeep*>(person);
    if (mode == 0) {
        self->SetDestination(thing->GetThingId(), x, y);
    } else if (mode == 1) {
        self->Teleport(thing->GetThingId(), x, y);
        CTRACE(3, kTraceTeleport, Halfword_2(x), Halfword_2(y));
    } else if (mode == 2) {
        peep->HideMySprite();
    } else if (mode == 3) {
        CTRACE(3, kTraceBeforeUnhide, HalfAt(person, 4), HalfAt(person, 6));
        peep->DrawMySpriteAt(0.0f, 0.0f, 0.0f, 0, static_cast<CDelayedScript::ScriptIdx>(3));
        SCR_SetFloatVariable(static_cast<int32_t>(Word_3(person, 0xc)), 8, 1.5f);
        self->Teleport(thing->GetThingId(), x, y);
        CTRACE(3, kTraceUnhide, Halfword_2(x), Halfword_2(y));
        CTRACE(3, kTraceAfterUnhide, HalfAt(person, 4), HalfAt(person, 6));
    } else {
        SamsUtilities::UDebug(kMsgBadZ);
    }
    online_callee::WStringDtor(&name, -1);
}

// 0x10215f68: a buddy connected (in this park when the name is in the contact set).
void BuddyConnected(COnlineControl* self, const Argument* args) {
    uint8_t* node;
    {
        WString value;
        response_callee::ArgumentBStrVal(&value, &args[1]);
        node = FindBuddy(self, value);
        online_callee::WStringDtor(&value, -1);
    }
    if (node == Base(self) + 0xe38) return;
    void* contact;
    {
        WString value;
        response_callee::ArgumentBStrVal(&value, &args[1]);
        contact = response_callee::WStringSetFind(Base(self) + 0xe28, &value);
        online_callee::WStringDtor(&value, -1);
    }
    const bool here = contact != Base(self) + 0xe2c;
    {
        WString value;
        response_callee::ArgumentBStrVal(&value, &args[1]);
        TbUniStringVTag tag(0, value.c_str());
        Table()->FormatString(Buffer(self), 0x400, here ? 0x6f : 0x70, &tag);
        online_callee::WStringDtor(&value, -1);
    }
    WString message;
    online_callee::WStringCtorFrom(&message, Buffer(self));
    self->Feedback(message);
    SamsUtilities::UDebug(here ? kMsgConnectedHere : kMsgConnectedChat);
    node[0x11] = here ? 1 : 0;
    online_callee::WStringDtor(&message, -1);
    node[0x10] = 1;
    UpdateBuddyFromArgument(self, args);
}

// 0x1021620c: a buddy disconnected.
void BuddyDisconnected(COnlineControl* self, const Argument* args) {
    uint8_t* node;
    {
        WString value;
        response_callee::ArgumentBStrVal(&value, &args[1]);
        node = FindBuddy(self, value);
        online_callee::WStringDtor(&value, -1);
    }
    if (node == Base(self) + 0xe38) return;
    {
        WString value;
        response_callee::ArgumentBStrVal(&value, &args[1]);
        TbUniStringVTag tag(0, value.c_str());
        Table()->FormatString(Buffer(self), 0x400, 0x71, &tag);
        online_callee::WStringDtor(&value, -1);
    }
    WString message;
    online_callee::WStringCtorFrom(&message, Buffer(self));
    self->Feedback(message);
    node[0x11] = 0;
    node[0x10] = 0;
    UpdateBuddyFromArgument(self, args);
    online_callee::WStringDtor(&message, -1);
}

// 0x102163b4: someone entered the park. Arguments: 1 the name, 2 (not read by CreateVisitor), 3 the avatar.
void EnteredPark(COnlineControl* self, const Argument* args) {
    WString name;
    NameOf(&name, &args[1]);
    self->CreateVisitor(name, -1, -1, static_cast<int32_t>(args[3].mCommand), static_cast<int32_t>(args[2].mCommand));
    self->AddContact(name, 0);
    uint8_t* node = FindBuddy(self, name);
    if (node != Base(self) + 0xe38) {
        node[0x11] = 1;
        UpdateUser(self, name, name.c_str(), false);
        SamsUtilities::UDebug(kMsgEnteredPark);
    }
    online_callee::WStringDtor(&name, -1);
}

// 0x102164f4: someone left the park.
void LeftPark(COnlineControl* self, const Argument* args) {
    WString name;
    NameOf(&name, &args[1]);
    self->DestroyVisitor(name);
    self->RemoveContact(name);
    uint8_t* node = FindBuddy(self, name);
    if (node != Base(self) + 0xe38) {
        node[0x11] = 0;
        UpdateUser(self, name, name.c_str(), false);
    }
    online_callee::WStringDtor(&name, -1);
}

// 0x102155b8: someone is in the park (not the player). Arguments: 1 the name, 2 (not read), 3 the avatar, 4 x, 5 y.
void Visitor(COnlineControl* self, const Argument* args) {
    bool same;
    {
        WString value;
        response_callee::ArgumentBStrVal(&value, &args[1]);
        same = response_callee::WStringEqual(value, Base(self) + 8);
        online_callee::WStringDtor(&value, -1);
    }
    if (same) return;
    WString name;
    NameOf(&name, &args[1]);
    self->CreateVisitor(name, static_cast<int32_t>(args[4].mCommand), static_cast<int32_t>(args[5].mCommand),
                        static_cast<int32_t>(args[3].mCommand), static_cast<int32_t>(args[2].mCommand));
    self->AddContact(name, 0);
    online_callee::WStringDtor(&name, -1);
}

// 0x1021660c: a buddy added (flags: 1 always, 2 when there are three Arguments and the third's number is not 0, 4 in the contact set).
void BuddyAdded(COnlineControl* self, const ArgumentVector* vector, const Argument* args) {
    WString name;
    NameOf(&name, &args[1]);
    int32_t flags = 0;
    if (vector->mSize >= 3 && args[2].mCommand != 0) flags |= 2;
    if (response_callee::WStringSetFind(Base(self) + 0xe28, &name) != Base(self) + 0xe2c) flags |= 4;
    flags |= 1;
    if (self->AddBuddy(name, flags)) FormatName(self, name, 0x6a, 0);
    online_callee::WStringDtor(&name, -1);
}

// 0x102168c8: a name ignored (only when it is not in the ignore set yet).
void Ignored(COnlineControl* self, const Argument* args) {
    WString name;
    NameOf(&name, &args[1]);
    void* found;
    {
        WString key;
        online_callee::NocaseWStringCopy(&key, &name, 0, kWhole);
        found = online_callee::NocaseSetFind(Base(self) + 0xe44, &key);
        online_callee::NocaseWStringDtor(&key, 0);
    }
    if (found != Base(self) + 0xe48) {
        online_callee::WStringDtor(&name, -1);
        return;
    }
    {
        WString key;
        online_callee::NocaseWStringCopy(&key, &name, 0, kWhole);
        response_callee::NocaseSetInsertOne(Base(self) + 0xe44, &key);
        online_callee::NocaseWStringDtor(&key, 0);
    }
    TbUniStringVTag tag(0, name.c_str());
    Table()->FormatString(Buffer(self), 0x400, 0x62, &tag);
    WString message;
    online_callee::WStringCtorFrom(&message, Buffer(self));
    self->Feedback(message);
    UpdateUser(self, name, name.c_str(), true);
    UpdateUser(self, name, name.c_str(), false);
    online_callee::WStringDtor(&message, -1);
    online_callee::WStringDtor(&name, -1);
}

// 0x10216adc: a name no longer ignored (only when it is in the ignore set).
void Unignored(COnlineControl* self, const Argument* args) {
    WString name;
    NameOf(&name, &args[1]);
    void* found;
    {
        WString key;
        online_callee::NocaseWStringCopy(&key, &name, 0, kWhole);
        found = online_callee::NocaseSetFind(Base(self) + 0xe44, &key);
        online_callee::NocaseWStringDtor(&key, 0);
    }
    if (found != Base(self) + 0xe48) {
        response_callee::NocaseSetErase(Base(self) + 0xe44, found);
        TbUniStringVTag tag(0, name.c_str());
        Table()->FormatString(Buffer(self), 0x400, 0x63, &tag);
        WString message;
        online_callee::WStringCtorFrom(&message, Buffer(self));
        self->Feedback(message);
        UpdateUser(self, name, name.c_str(), true);
        UpdateUser(self, name, name.c_str(), false);
        online_callee::WStringDtor(&message, -1);
    }
    online_callee::WStringDtor(&name, -1);
}

// 0x10216e98: Arguments 1, 2 and 3 as tags 0, 4 and 5 of text 0x98.
void ThreeTags(COnlineControl* self, const Argument* args) {
    WString name;
    NameOf(&name, &args[1]);
    WString second;
    response_callee::ArgumentBStrVal(&second, &args[2]);
    WString third;
    response_callee::ArgumentBStrVal(&third, &args[3]);
    TbUniStringVTag tag0(0, name.c_str());
    TbUniStringVTag tag4(4, second.c_str());
    TbUniStringVTag tag5(5, third.c_str());
    Table()->FormatString(Buffer(self), 0x400, 0x98, &tag0, &tag4, &tag5);
    online_callee::WStringDtor(&third, -1);
    online_callee::WStringDtor(&second, -1);
    FeedbackBuffer(self);
    online_callee::WStringDtor(&name, -1);
}

// 0x10216fe0: go to a player's park. Argument 4's char string is the park id; when it is the park being downloaded, the avatar is
// teleported next to the named person instead.
void GotoPark(COnlineControl* self, const Argument* args) {
    if (Base(self)[0xe84] == 1) {
        SamsUtilities::UDebug(kMsgGotoBusy);
        return;
    }
    MslCharString text;
    chat_satellite_callee::ArgumentStrVal(&text, &args[4]);
    const int32_t park = std::atoi(text.mRep->data);
    online_callee::StringDtor(&text, -1);
    CFSFTPParkInfo* info = System()->GetFTPDownloadParkInfo();
    if (park != static_cast<int32_t>(Word_3(info, 8))) {
        System()->GotoPlayerRequestFTPDownloadByID(park, GotoCallbackFn, nullptr);
        Base(self)[0xe84] = 1;
        return;
    }
    WString name;
    NameOf(&name, &args[1]);
    COnlinePerson* person = self->GetPerson(name);
    if (person != nullptr) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(person);
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
        cell.mIndex = static_cast<uint16_t>(p[4] + (static_cast<uint32_t>(p[6]) << 7) + 1);
        self->TeleportAvatar(cell);
    }
    online_callee::WStringDtor(&name, -1);
}

}  // namespace

// 0x10215560
void COnlineControl::ProcessResponse(void* arguments) {
    const ArgumentVector* vector = static_cast<const ArgumentVector*>(arguments);
    const Argument* args = vector->mData;
    switch (args[0].mCommand) {
    case 0x0b:
        Table()->FormatString(Buffer(this), 0x400, 0x60);
        {
            WString message;
            online_callee::WStringCtorFrom(&message, Buffer(this));
            Feedback(message);
            EarmuffsButtonDown(true);
            online_callee::WStringDtor(&message, -1);
        }
        break;
    case 0x0c:
        Table()->FormatString(Buffer(this), 0x400, 0x61);
        {
            WString message;
            online_callee::WStringCtorFrom(&message, Buffer(this));
            Feedback(message);
            EarmuffsButtonDown(false);
            online_callee::WStringDtor(&message, -1);
        }
        break;
    case 0x0f:
        if (vector->mSize >= 2 && args[1].mCommand != 0) FormatPlain(this, 0x79);
        break;
    case 0x10:
        if (vector->mSize == 2 && args[1].mCommand != 0) FormatPlain(this, 0x7a);
        break;
    case 0x11: FormatTwo(this, args, 0x58, 1); break;
    case 0x12: FormatTwo(this, args, 0x5d, 4); break;
    case 0x13: {
        FormatTwo(this, args, 0x59, 2);
        WString value;
        response_callee::ArgumentBStrVal(&value, &args[1]);
        online_parse_callee::WStringAssignRange(Base(this) + 0xe24, &value, 0, kWhole);
        online_callee::WStringDtor(&value, -1);
        break;
    }
    case 0x16:
    case 0x18:
    case 0x1a:
    case 0x1c: FormatTwo(this, args, 0x5c, 3); break;
    case 0x1e:
    case 0x1f:
    case 0x2e:
    case 0x2f:
    case 0x39:
    case 0x3c:
    case 0x45: {
        // A name as tag 0 (tag 1 for 0x1e), after RemoveBuddy for 0x39.
        uint32_t id = 0;
        uint32_t index = 0;
        switch (args[0].mCommand) {
        case 0x1e: id = 0xa0; index = 1; break;
        case 0x1f: id = 0x74; break;
        case 0x2e: id = 0x6e; break;
        case 0x2f: id = 0x66; break;
        case 0x39: id = 0x6c; break;
        case 0x3c: id = 0x73; break;
        default: id = 0x64; break;
        }
        WString name;
        NameOf(&name, &args[1]);
        if (args[0].mCommand == 0x39) RemoveBuddy(name);
        FormatName(this, name, id, index);
        online_callee::WStringDtor(&name, -1);
        break;
    }
    case 0x22: Move(this, args); break;
    case 0x23: BuddyConnected(this, args); break;
    case 0x24: BuddyDisconnected(this, args); break;
    case 0x25: EnteredPark(this, args); break;
    case 0x26: LeftPark(this, args); break;
    case 0x27: Visitor(this, args); break;
    case 0x2a:
        if (vector->mSize > 1) FormatArgument(this, args, 0x5e);
        break;
    case 0x38: BuddyAdded(this, vector, args); break;
    case 0x3a: Ignored(this, args); break;
    case 0x3b: Unignored(this, args); break;
    case 0x3d: ThreeTags(this, args); break;
    case 0x3f: FormatTwo(this, args, 0x5a, -1); break;
    case 0x40: FormatTwo(this, args, 0x5b, -1); break;
    case 0x41:
    case 0x42: FormatArgument(this, args, 0x7d); break;
    case 0x44: FormatPlain(this, 0x68); break;
    case 0x47: FormatPlain(this, 0x5f); break;
    case 0x4f:
        UpdateBuddyListToServer();
        UpdateIgnoreListToServer();
        break;
    case 0x51: FormatArgument(this, args, 0x67); break;
    case 0x52: GotoPark(this, args); break;
    default: break;   // the table's gaps (0xd, 0xe, 0x14, 0x15, ...) and every number outside 0xb..0x52
    }
}

// 0x102177bc
void COnlineControl::EnteringFirstPerson(MapPos x, MapPos y) {
    CPeep* peep = static_cast<CPeep*>(PointerAt(this, 4));
    peep->HideMySprite();
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CThing*>(PointerAt(this, 4))->GetThingId();
    Teleport(thing, x, y);
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 1, this);
}

// 0x10217884
void COnlineControl::LeavingFirstPerson(MapPos x, MapPos y) {
    CPeep* peep = static_cast<CPeep*>(PointerAt(this, 4));
    peep->DrawMySpriteAt(0.0f, 0.0f, 0.0f, 0, static_cast<CDelayedScript::ScriptIdx>(3));
    uint32_t handle;
    std::memcpy(&handle, reinterpret_cast<char*>(peep) + 0xc, 4);
    SCR_SetFloatVariable(static_cast<int32_t>(handle), 8, 1.5f);
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 1, this);
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CThing*>(PointerAt(this, 4))->GetThingId();
    Teleport(thing, x, y);
}

// 0x1021797c
void COnlineControl::UpdateFirstPersonPos(MapPos x, MapPos y) {
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 0, this);
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CThing*>(PointerAt(this, 4))->GetThingId();
    Teleport(thing, x, y);
}

// Unit v51062: four small COnlineControl members, Argument::StrVal / BStrVal, and the library functions around them (wide streambuf,
// ios and ostream destructors, ctype<wchar_t>::do_widen, the buddy map's erase_one, nocase_string's destructor).

namespace {

// A 32-bit pointer word of the Mac object, read raw (arena memory is 32-bit addressable).
void* PointerAt_3(void* object, size_t offset) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(*Field<uint32_t>(object, offset)));
}

// A MapPos is passed in the high halfword of its register; the code reads it back with lha (signed).
int32_t Halfword_3(MapPos pos) {
    return static_cast<int16_t>((static_cast<uint32_t>(pos.mCell) << 8) | pos.mOffset);
}

}  // namespace

// 0x10217a38: mode 2, the position is the halfwords at 4 and 6 of the own avatar (the word at 4)
void COnlineControl::HideAvatar() {
    uint32_t avatarWord = *Field<uint32_t>(this, 4);   // an arena offset on the host
    void* avatar = avatarWord != 0 ? static_cast<void*>(tpw::gArenaBase + avatarWord) : nullptr;
    online_callee::ChatMovePlayer(PointerAt_3(this, 0), *Field<int16_t>(avatar, 4), *Field<int16_t>(avatar, 6), 2, this);
}

// 0x10217aa8
void COnlineControl::UnHideAvatar(MapPos x, MapPos y) {
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 3, this);
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = reinterpret_cast<CThing*>(PointerAt(this, 4))->GetThingId();
    Teleport(thing, x, y);
}

// COnlineControl: IgnoreOff, IgnoreOn, BuddyOff, SendText, RemoveContact, RemoveBuddy, UpdateUserList (unit v5897).
// Strings the Mac keeps as one word are opaque buffers here; the callees (library code) are the online_callee functions.

namespace {

constexpr uint32_t kParserVtableWord = 0x102b1b2c;   // TOC -0x5edc: the address CParser's destructor leaves in the object

// A host tree node of the contact set: the node base, then the std::wstring (the Mac has it at node + 0xc).
struct ContactNode : MslRedBlackTree::node_base {
    std::u16string mValue;
};

// An Argument is three words: 0 the char string, 4 the wide string, 8 the command number.
// Pushes an Argument that carries only a command number.
void PushCommand(void* vec, uint32_t command) {
    alignas(8) uint8_t argument[16] = {};
    online_callee::StringCtor(argument);
    online_callee::WStringCtor(argument + 4);
    std::memcpy(argument + 8, &command, 4);
    online_callee::ArgumentVectorPushBack(vec, argument);
    online_callee::WStringDtor(argument + 4, -1);
    online_callee::StringDtor(argument, -1);
}

// Pushes an Argument that carries a copy of the wide string.
void PushText(void* vec, const void* text) {
    alignas(8) uint8_t copy[16] = {};
    alignas(8) uint8_t argument[16] = {};
    online_callee::WStringCopy(copy, text, 0, 0xffffffffu);
    online_callee::ArgumentCtor(argument, copy);
    online_callee::ArgumentVectorPushBack(vec, argument);
    online_callee::WStringDtor(argument + 4, -1);
    online_callee::StringDtor(argument, -1);
    online_callee::WStringDtor(copy, -1);
}

// The command (a number and the name) goes through DoCommand.
void SendNameCommand(COnlineControl* self, uint32_t command, const std::u16string& name) {
    alignas(8) uint8_t vec[16] = {};
    PushCommand(vec, command);
    PushText(vec, &name);
    self->DoCommand(vec);
    online_callee::ArgumentVectorTearDown(vec);
}

}  // namespace

// 0x10217b5c: parses the chat line; when the parser pushed nothing the line is sent as a plain chat command (0x17)
void COnlineControl::SendText(std::u16string text) {
    if (text.empty()) return;   // the original tests the first word of the string's representation (its length)
    alignas(8) uint8_t vec[16] = {};
    alignas(8) uint8_t output[16] = {};
    alignas(8) uint8_t parser[0x80] = {};
    alignas(8) uint8_t copy[16] = {};
    online_callee::WStringCtor(output);
    COnlineParser::Construct(reinterpret_cast<COnlineParser*>(parser), vec, output);
    online_callee::WStringCopy(copy, &text, 0, 0xffffffffu);
    reinterpret_cast<COnlineParser*>(parser)->DoParse(copy);
    online_callee::WStringDtor(copy, -1);
    uint32_t pushed;
    std::memcpy(&pushed, vec + 4, 4);
    if (pushed == 0) {
        PushCommand(vec, 0x17);
        PushText(vec, output);
    }
    DoCommand(vec);
    CTokenListClear(reinterpret_cast<CTokenList*>(parser + 0x30));
    std::memcpy(parser, &kParserVtableWord, 4);
    online_parser_callee::ParserDtor(parser, 0);
    online_callee::WStringDtor(output, -1);
    online_callee::ArgumentVectorTearDown(vec);
}

// online.cpp (main app): COnlineControl, the avatar moves, the chat text and the contact lookups (unit v5285).
//   CreateVisitor 0x102182c0, SetAvatarDestination 0x10218140, TeleportAvatar 0x1021802c, IsThisMe 0x10217f34, ReceiveText 0x10217dbc,
//   Feedback 0x1021b784, GetDataForUser 0x1021b854, AddContact 0x1021b950.
// Fields of the object are read raw at their Mac offsets (32-bit pointer words); the library objects (strings, trees, the chat, the UI) are
// reached through online_callee, which the tests fake.

namespace {

using ThingTag = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using MapTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

uint32_t Word32_2(const void* base, size_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

template <class T>
T* ToPtr(uint32_t word) {
    // The object's pointer words (the Chat at 0, the peep at 4) are handles on the host (online_callee::HandleToPointer, as the constructor stores them).
    return static_cast<T*>(online_callee::HandleToPointer(word));
}

// The global master save, made on first use (the original does it inline at both places).
CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) {
        void* block = online_callee::OperatorNew(0x4c);
        online_callee::MasterSaveCtor(block);
        gMasterSave = static_cast<CFSGlobalMasterSave*>(block);
    }
    return gMasterSave;
}

}  // namespace

// 0x10217dbc: filters the text when the swear filter is on (the master save is handed to Filter as the filter, as the original does), then shows it.
void COnlineControl::ReceiveText(std::u16string& text, int32_t colour) {
    if (MasterSave()->IsSwearFilterOn()) {
        reinterpret_cast<SwearFilter*>(MasterSave())->Filter(text, true);
    }
    online_callee::UIOnlineReceiveStr(&gUIInterface, text.c_str(), colour);
}

// 0x10217f34: the name against the player's own (field 8), both as case-insensitive copies.
bool COnlineControl::IsThisMe(const char16_t* name) {
    alignas(8) uint8_t plain[16] = {};
    alignas(8) uint8_t first[16] = {};
    alignas(8) uint8_t second[16] = {};
    online_callee::WStringCtorFrom(plain, name);
    online_callee::NocaseWStringCopy(first, plain, 0, 0xffffffffu);
    online_callee::WStringDtor(plain, -1);
    online_callee::NocaseWStringCopy(second, reinterpret_cast<uint8_t*>(this) + 8, 0, 0xffffffffu);
    bool same = online_callee::NocaseWStringEqual(first, second);
    online_callee::NocaseWStringDtor(second, 0);
    online_callee::NocaseWStringDtor(first, 0);
    return same;
}

// 0x1021802c
void COnlineControl::TeleportAvatar(MapTag cell) {
    ThingTag thing = ToPtr<CThing>(Word32_2(this, 4))->GetThingId();
    if (!online_callee::TeleportCell(this, thing, cell)) return;
    uint32_t v = static_cast<uint32_t>(cell.mIndex) - 1;
    int16_t x = static_cast<int16_t>(((v & 0x7f) << 8) | 0x80);
    int16_t y = static_cast<int16_t>((((v >> 7) & 0xff) << 8) | 0x80);
    online_callee::ChatMovePlayer(ToPtr<void>(Word32_2(this, 0)), x, y, 1, this);
    NoLongerAFK(true);
}

// 0x10218140
void COnlineControl::SetAvatarDestination(MapTag cell) {
    NoLongerAFK(true);
    ThingTag thing = ToPtr<CThing>(Word32_2(this, 4))->GetThingId();
    if (!SetDestination(thing, cell)) return;
    uint32_t v = static_cast<uint32_t>(cell.mIndex) - 1;
    online_callee::ChatMovePlayer(ToPtr<void>(Word32_2(this, 0)), static_cast<int32_t>((v << 8) & 0x7f00),
                                  static_cast<int32_t>((v << 1) & 0xffffff00u), 0, this);
}

// 0x1021823c: mode 1
void COnlineControl::UpdateAvatarPos(MapPos x, MapPos y) {
    online_callee::ChatMovePlayer(PointerAt_3(this, 0), Halfword_3(x), Halfword_3(y), 1, this);
}

// 0x102182c0: makes the visitor's COnlinePerson (the fourth int is not read). y == -1: the park entrance 1; else the cell x/256 + (y/256 << 7) + 1.
void COnlineControl::CreateVisitor(std::u16string& name, int32_t x, int32_t y, int32_t avatar, int32_t unused) {
    (void)unused;
    MapTag cell;
    if (y == -1) {
        CMap* map = reinterpret_cast<CMap*>(reinterpret_cast<uint8_t*>(gGameWorld) + 0x2d8);
        cell = map->GetParkEntrancePos(1);
    } else {
        uint32_t sum = static_cast<uint32_t>(x / 256) + (static_cast<uint32_t>(y / 256) << 7) + 1;
        cell.mIndex = static_cast<uint16_t>(sum);
    }
    void* person = online_callee::OperatorNew(0x3a8);
    online_callee::COnlinePersonCtor(person, cell, static_cast<uint32_t>(avatar), &name, false);
}

// 0x102183f4
void COnlineControl::DestroyVisitor(std::u16string& name) {
    COnlinePerson* person = GetPerson(name);
    if (person != nullptr) reinterpret_cast<CThing*>(person)->KillMe();
}

// online.cpp (main app): COnlineControl chat commands Locate, Page, Goto, BlackMark and BuddyOn (unit v5023).
// Each builds a vector of two Arguments (the command number, then a copy of the name) and hands it to DoCommand.

namespace {

// An Argument as the code builds it: char string word, wide string word, command number.
struct ArgumentWords {
    uint32_t mString;    // 0
    uint32_t mWString;   // 4
    int32_t mType;       // 8
};

void SendCommand(COnlineControl* self, int32_t type, std::u16string& name) {
    uint32_t vec[3] = {0, 0, 0};
    ArgumentWords first;
    online_callee::StringCtor(&first.mString);
    online_callee::WStringCtor(&first.mWString);
    first.mType = type;
    online_callee::ArgumentVectorPushBack(vec, &first);
    online_callee::WStringDtor(&first.mWString, -1);
    online_callee::StringDtor(&first.mString, -1);

    uint32_t copy;
    ArgumentWords second;
    online_callee::WStringCopy(&copy, &name, 0, 0xffffffffu);
    online_callee::ArgumentCtor(&second, &copy);
    online_callee::ArgumentVectorPushBack(vec, &second);
    online_callee::WStringDtor(&second.mWString, -1);
    online_callee::StringDtor(&second.mString, -1);
    online_callee::WStringDtor(&copy, -1);
    self->DoCommand(vec);
    online_callee::ArgumentVectorTearDown(vec);
}

}  // namespace

// 0x102184a0
void COnlineControl::BlackMark(std::u16string& name) { SendCommand(this, 0xe, name); }

// 0x1021864c
void COnlineControl::Page(std::u16string& name) { SendCommand(this, 0x11, name); }

// 0x102187f4
void COnlineControl::Locate(std::u16string& name) { SendCommand(this, 0x14, name); }

// 0x1021899c
void COnlineControl::Goto(std::u16string& name) { SendCommand(this, 0x13, name); }

// Unit v5121: COnlineControl's VoteForThisPark, EarmuffsOn, EarmuffsOff, GetPerson, SetDestination (two), Teleport (two).

uint16_t gNullThingId = 0;   // 0x102b1d64

namespace {

// The three-word Argument the commands push: 0 the char string, 4 the wide string, 8 the command number (see online.h, unit v5023).
void SendCommand_2(COnlineControl* self, uint32_t command) {
    uint32_t vec[3] = {0, 0, 0};
    uint32_t argument[3];
    online_callee::StringCtor(&argument[0]);
    online_callee::WStringCtor(&argument[1]);
    argument[2] = command;
    online_callee::ArgumentVectorPushBack(vec, argument);
    online_callee::WStringDtor(&argument[1], -1);
    online_callee::StringDtor(&argument[0], -1);
    self->DoCommand(vec);
    online_callee::ArgumentVectorTearDown(vec);
}

const char kIndexZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10251361

}  // namespace

// 0x10218b44
void COnlineControl::VoteForThisPark() {
    SendCommand_2(this, 0x15);
}

// 0x10218c44
void COnlineControl::BuddyOn(std::u16string& name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    uint32_t key;
    online_callee::NocaseWStringCopy(&key, &name, 0, 0xffffffffu);
    void* found = online_callee::BuddyTreeFind(self + 0xe34, &key);
    online_callee::NocaseWStringDtor(&key, 0);
    if (found == self + 0xe38) SendCommand(this, 0xf, name);
}

// 0x10218e3c: sends command 0xf when the name is in the buddy map (0xe34)
void COnlineControl::BuddyOff(std::u16string& name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t key[16] = {};
    online_callee::NocaseWStringCopy(key, &name, 0, 0xffffffffu);
    void* found = online_callee::BuddyTreeFind(self + 0xe34, key);
    online_callee::NocaseWStringDtor(key, 0);
    if (found == self + 0xe38) return;
    SendNameCommand(this, 0xf, name);
}

// 0x10219034: sends command 0xd when the name is not in the ignore set (0xe44)
void COnlineControl::IgnoreOn(std::u16string& name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t key[16] = {};
    online_callee::NocaseWStringCopy(key, &name, 0, 0xffffffffu);
    void* found = online_callee::NocaseSetFind(self + 0xe44, key);
    online_callee::NocaseWStringDtor(key, 0);
    if (found != self + 0xe48) return;
    SendNameCommand(this, 0xd, name);
}

// 0x1021922c: sends command 0xd when the name is in the ignore set (0xe44)
void COnlineControl::IgnoreOff(std::u16string& name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t key[16] = {};
    online_callee::NocaseWStringCopy(key, &name, 0, 0xffffffffu);
    void* found = online_callee::NocaseSetFind(self + 0xe44, key);
    online_callee::NocaseWStringDtor(key, 0);
    if (found == self + 0xe48) return;
    SendNameCommand(this, 0xd, name);
}

// 0x10219424
void COnlineControl::EarmuffsOn() {
    if (reinterpret_cast<uint8_t*>(this)[0xe50] == 0) SendCommand_2(this, 0xc);
}

// 0x1021952c
void COnlineControl::EarmuffsOff() {
    if (reinterpret_cast<uint8_t*>(this)[0xe50] != 0) SendCommand_2(this, 0xc);
}

// 0x10219634: the flag argument is overwritten (r4 = this) and never read.
void COnlineControl::NoLongerAFK(bool) {
    *Field<uint8_t>(this, 0xe51) = 0;
    docommand_callee::ChatNotAFK(*Field<uint32_t>(this, 0), this);
}

// 0x10217684 Argument::StrVal: the narrow string at the Argument's start, copied whole.
namespace chat_satellite_callee {
void ArgumentStrVal(MslCharString* result, const Argument* argument) {
    msl_char_string::construct(result, reinterpret_cast<const MslCharString*>(argument), 0, 0xffffffffu);
}
}  // namespace chat_satellite_callee

// 0x10217764 Argument::BStrVal: the wide string at the Argument's offset 4, copied whole.
namespace docommand_callee {
void ArgumentBStrVal(void* result, const void* argument) {
    online_callee::WStringCopy(result, static_cast<const char*>(argument) + 4, 0, 0xffffffffu);
}
}  // namespace docommand_callee

namespace nocase_callee {
// 0x102176d8 nocase_string::~nocase_string with the delete flag (the original answers this).
void StringDtor(nocase_string* self, int16_t deleteFlag) {
    if (self != nullptr) {
        online_callee::NocaseWStringDtor(self, 0);
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
}

// 0x1021eda4 std::allocator<set<nocase_string> node>::deallocate: the size is unused.
void Deallocate(void* allocator, void* node, unsigned long count) {
    (void)allocator;
    (void)count;
    online_callee::OperatorDelete(node);
}
}  // namespace nocase_callee

namespace ignore_callee {
// 0x1021cbb8 std::basic_ios<wchar_t>::~basic_ios (the original answers this).
void IosDtor(void* ios, int16_t flag) {
    if (ios != nullptr) {
        *static_cast<const void**>(ios) = gIfsWVtblE;
        ifstream_callee::IosBaseDtor(ios, 0);
        if (flag > 0) ifstream_callee::Delete(ios);
    }
}

// 0x1021cc68 std::basic_ostream<wchar_t>::~basic_ostream (the original answers this). The stream's first word points to the virtual base
// table; the ios part starts at Mac offset 8 (mRest here). A non-zero flag, positive or not, tears the ios part down.
void OstreamDtor(IgnoreStream* stream, int16_t flag) {
    if (stream != nullptr) {
        void* ios = stream->mRest;
        stream->mSlot = gOfsWVtblB;
        stream->mIos->mVtable = reinterpret_cast<const char*>(gOfsWVtblB) + 0xc;
        stream->mIos->mBaseOffset = static_cast<int32_t>(static_cast<char*>(ios) - reinterpret_cast<char*>(stream->mIos));
        if (flag != 0) {
            *static_cast<const void**>(ios) = gIfsWVtblE;
            ifstream_callee::IosBaseDtor(ios, 0);
        }
        if (flag > 0) ifstream_callee::Delete(stream);
    }
}
}  // namespace ignore_callee

// 0x1021d584 std::ctype<wchar_t>::do_widen(char): sign extension.
char16_t CtypeWcharDoWiden(const void*, char c) {
    return static_cast<char16_t>(static_cast<int8_t>(c));
}

// 0x1021e14c ~basic_streambuf<wchar_t> (the original answers this).
BasicStreambufWchar* BasicStreambufWchar::destroy(int16_t flag) {
    if (this != nullptr) {
        mVtable = gIfsWVtblC;
        LocaleRefPtrDestroy(&mLocale, -1);
        if (flag > 0) ifstream_callee::Delete(this);
    }
    return this;
}

// 0x1021e2d4
void BasicStreambufWchar::imbue(const void* loc) {
    mLocale = *static_cast<const LocaleRefPtr*>(loc);
}

BasicStreambufWchar* BasicStreambufWchar::setbuf(char16_t*, int32_t) { return this; }   // 0x1021e378
int32_t BasicStreambufWchar::sync() { return 0; }                                         // 0x1021e3cc
int32_t BasicStreambufWchar::showmanyc() { return 0; }                                    // 0x1021e420
int32_t BasicStreambufWchar::underflow() { return 0xffff; }                               // 0x1021e47c
int32_t BasicStreambufWchar::pbackfail(int32_t) { return 0xffff; }                        // 0x1021e5a8
int32_t BasicStreambufWchar::overflow(int32_t) { return 0xffff; }                         // 0x1021e608

// 0x1021e8e8 erase_one: the first node whose pair is not below the key (value_compare is at this + 8), erased when the key is not below it.
uint32_t BuddyTreeEraseOneCount(BuddyTree* self, const void* key) {
    BuddyTreeNode* end = reinterpret_cast<BuddyTreeNode*>(&self->mRoot);
    BuddyTreeNode* found = end;
    BuddyTreeNode* node = self->mRoot;
    while (node != nullptr) {
        if (!buddy_erase_callee::ValueCompareNodeKey(&self->mField8, node->mPair, key)) {
            found = node;
            node = node->mLeft;
        } else {
            node = node->mRight;
        }
    }
    if (found == end || buddy_erase_callee::ValueCompareKeyNode(&self->mField8, key, found->mPair)) found = end;
    if (found == end) return 0;
    BuddyTreeErase(self, found);
    return 1;
}

// 0x1021969c
COnlinePerson* COnlineControl::GetPerson(std::u16string& name) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> cur = gGameWorld->GetUsedThingHead();
    while (cur.mIndex != gNullThingId) {
        uint16_t index = cur.mIndex;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kIndexZeroMessage);
        CThing* thing = static_cast<CThing*>(gThingArray[index].mThing);
        if (thing->mKind == 0x12) {
            COnlinePerson* person = reinterpret_cast<COnlinePerson*>(thing);
            if (person->IsThisMyName(name)) return person;
        }
        cur = gGameWorld->GetNextUsedThing(thing);
    }
    return nullptr;
}

// 0x102197ec
bool COnlineControl::SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing,
                                    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kIndexZeroMessage);
    CThing* t = static_cast<CThing*>(gThingArray[thing.mIndex].mThing);
    if (t->mKind != 0x12) return false;
    return reinterpret_cast<COnlinePerson*>(t)->SetDest(cell) != 0;
}

// 0x10219908
bool COnlineControl::SetDestination(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kIndexZeroMessage);
    CThing* t = static_cast<CThing*>(gThingArray[thing.mIndex].mThing);
    if (t->mKind != 0x12) return false;
    return reinterpret_cast<COnlinePerson*>(t)->SetDest(x, y) != 0;
}

// 0x10219a10: the cell number minus one splits into a low part of 7 bits (x) and the rest (y), each with the offset byte 0x80.
void COnlineControl::Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing,
                              tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    uint32_t v = static_cast<uint32_t>(cell.mIndex) - 1u;
    MapPos x{static_cast<uint8_t>(v & 0x7f), 0x80};
    MapPos y{static_cast<uint8_t>(v >> 7), 0x80};
    Teleport(thing, x, y);
}

// 0x10219af4
// The original answers whether the thing is an online person; the header declares void, so the answer lives in this helper.
bool OnlineControlTeleportThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kIndexZeroMessage);
    CThing* t = static_cast<CThing*>(gThingArray[thing.mIndex].mThing);
    if (t->mKind != 0x12) return false;
    reinterpret_cast<COnlinePerson*>(t)->Teleport(x, y);
    return true;
}

void COnlineControl::Teleport(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, MapPos x, MapPos y) {
    OnlineControlTeleportThing(thing, x, y);
}

// COnlineControl::DoCommand 0x10219bec (online.cpp, main app): runs one chat command, the vector of Argument the parser built (the first
// Argument's command number picks a case of the jump table at 0x102b1d68). Fields (Mac offsets; 32-bit words are arena offsets, Ptr32):
// 0x00 Chat*, 0x08 the player's name, 0x220 the feedback buffer (0x400 wide chars), 0xe20 the last tell target, 0xe24 the last teller,
// 0xe28 the contact set (first node word at 0xe30, end node this + 0xe2c), 0xe34 the buddy map (end this + 0xe38), 0xe44 the ignore set
// (end this + 0xe48), 0xe50 earmuffs byte, 0xe51 AFK byte, 0xe64 Vote*, 0xe84 goto-in-progress byte, 0xe88 the goto target.

// 0x102b1b58: the 16 bytes DoCommand copies to the stack and sends with Vote::SendVote.
uint32_t gVoteSendData[4] = {0x5c200a41u, 0x298111d3u, 0xadf400e0u, 0x18904fd5u};

namespace {

using Slot = uint64_t;   // a std::wstring (or nocase string) on the stack: one 32-bit word, 8 bytes kept

uint32_t Word_4(const void* block, size_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(block) + offset, 4);
    return value;
}

void SetWord(void* block, size_t offset, uint32_t value) { std::memcpy(static_cast<uint8_t*>(block) + offset, &value, 4); }

void* Ptr_2(uint32_t word) { return word != 0 ? static_cast<void*>(tpw::gArenaBase + word) : nullptr; }

uint32_t Addr32(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }

const IME::MslWideStringRep* Rep(const void* slot) { return static_cast<const IME::MslWideStringRep*>(Ptr_2(Word_4(slot, 0))); }

std::u16string& AsString(Slot& slot) { return *reinterpret_cast<std::u16string*>(&slot); }

const char kGotoBusy[] = "Goto already in progress. Can't goto again!\n";                          // 0x10250e7f + 0x4b1
const char kIgnoreSelf[] = "You can't ignore yourself\n";                                          // + 0x525
const char kBlackmarkSelf[] = "You can't blackmark yourself\n";                                    // + 0x540
const char kVoteBusy[] = "There is a vote going on already. We can't vote until that's done\n";    // + 0x55e
const char kVoteCreated[] = "Vote connection created\n";                                            // + 0x5a1
const char kVoteConnected[] = "Vote connected\n";                                                   // + 0x5ba
const char kVoteSent[] = "Vote sent\n";                                                             // + 0x5ca

// gOnlineSystem made on first use; answers the value read before creating it (as the original's inlined accessor does).
CFSOnlineSystem* EnsureSystem() {
    CFSOnlineSystem* before = gOnlineSystem;
    if (before == nullptr) {
        void* p = online_callee::OperatorNew(0x404);
        if (p != nullptr) docommand_callee::OnlineSystemCtor(p);
        gOnlineSystem = static_cast<CFSOnlineSystem*>(p);
    }
    return before;
}

void EnsureMasterSave() {
    if (gMasterSave == nullptr) {
        void* p = online_callee::OperatorNew(0x4c);
        if (p != nullptr) online_callee::MasterSaveCtor(p);
        gMasterSave = static_cast<CFSGlobalMasterSave*>(p);
    }
}

TbITable* Table_2() { return static_cast<TbITable*>(gText.mResourcePointers[19]); }   // gText + 0x598

}  // namespace

void COnlineControl::DoCommand(void* arguments) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    ArgumentVector* vector = static_cast<ArgumentVector*>(arguments);
    char16_t* buffer = reinterpret_cast<char16_t*>(self + 0x220);

    auto count = [&]() { return vector->mSize; };
    auto arg = [&](uint32_t i) -> const void* { return &vector->mData[i]; };
    auto chat = [&]() { return Word_4(self, 0); };
    // The buffer as a string, handed to Feedback.
    auto feed = [&]() {
        Slot text = 0;
        online_callee::WStringCtorFrom(&text, buffer);
        Feedback(AsString(text));
        online_callee::WStringDtor(&text, -1);
    };
    auto report = [&](uint32_t id) {
        Table_2()->FormatString(buffer, 0x400, id);
        feed();
    };
    auto reportWith = [&](uint32_t id, const char16_t* text) {
        TbUniStringVTag tag(0, text);
        Table_2()->FormatString(buffer, 0x400, id, &tag);
        feed();
    };
    // Whether Argument 1's wide string is not empty.
    auto nonEmpty = [&]() {
        Slot text = 0;
        docommand_callee::ArgumentBStrVal(&text, arg(1));
        bool result = Rep(&text)->mLength != 0;
        online_callee::WStringDtor(&text, -1);
        return result;
    };
    // Argument 1's text to Chat::Say / RoomShout when it is not empty.
    auto sayChecked = [&](bool shout, bool flag) {
        if (!nonEmpty()) return;
        Slot text = 0;
        docommand_callee::ArgumentBStrVal(&text, arg(1));
        if (shout)
            docommand_callee::ChatRoomShout(chat(), &text, flag, this);
        else
            docommand_callee::ChatSay(chat(), &text, flag, this);
        online_callee::WStringDtor(&text, -1);
    };
    // Argument 1's text to one of Chat's name commands.
    auto nameCommand = [&](void (*command)(uint32_t, const void*, void*)) {
        Slot name = 0;
        docommand_callee::ArgumentBStrVal(&name, arg(1));
        command(chat(), &name, this);
        online_callee::WStringDtor(&name, -1);
    };
    auto tellTwo = [&](bool flag) {
        Slot name = 0;
        docommand_callee::ArgumentBStrVal(&name, arg(1));
        Slot text = 0;
        docommand_callee::ArgumentBStrVal(&text, arg(2));
        docommand_callee::ChatTell(chat(), &name, &text, flag, this);
        online_callee::WStringDtor(&text, -1);
        online_callee::WStringDtor(&name, -1);
    };
    // Tell the one at 0xe20 / 0xe24, or say there is none.
    auto replyTo = [&](size_t field, uint32_t noneId, uint32_t whoId) {
        if (count() > 1) {
            if (Rep(self + field)->mLength == 0) {
                report(noneId);
                return;
            }
            Slot text = 0;
            docommand_callee::ArgumentBStrVal(&text, arg(1));
            docommand_callee::ChatTell(chat(), self + field, &text, false, this);
            online_callee::WStringDtor(&text, -1);
            return;
        }
        const IME::MslWideStringRep* rep = Rep(self + field);
        if (rep->mLength == 0)
            report(noneId);
        else
            reportWith(whoId, rep->mData);
    };
    // Argument 1 as a nocase string, and the player's name as one.
    auto nocaseArg = [&](Slot* out) {
        Slot text = 0;
        docommand_callee::ArgumentBStrVal(&text, arg(1));
        online_callee::NocaseWStringCopy(out, &text, 0, 0xffffffffu);
        online_callee::WStringDtor(&text, -1);
    };

    if (vector->mSize == 0) return;
    if (vector->mData[0].mCommand != 0x12) {
        self[0xe51] = 0;
        docommand_callee::ChatNotAFK(chat(), this);
    }
    const uint32_t command = vector->mData[0].mCommand;
    if (command > 0x18) return;

    switch (command) {
    case 0x0:   // say
        if (count() <= 1) return;
        sayChecked(false, false);
        return;
    case 0x1:   // tell
        if (count() > 1) {
            tellTwo(false);
            Slot name = 0;
            docommand_callee::ArgumentBStrVal(&name, arg(1));
            online_parse_callee::WStringAssignRange(self + 0xe20, &name, 0, 0xffffffffu);
            online_callee::WStringDtor(&name, -1);
            return;
        }
        if (Rep(self + 0xe20)->mLength == 0)
            report(0xb3);
        else
            reportWith(0x56, Rep(self + 0xe20)->mData);
        return;
    case 0x2:   // reply
        replyTo(0xe24, 0xb2, 0x57);
        return;
    case 0x3:   // retell
        replyTo(0xe20, 0xb3, 0x56);
        return;
    case 0x4:   // shout
        if (self[0xe50] != 0) {
            report(0x5f);
            return;
        }
        sayChecked(true, false);
        return;
    case 0x5:   // emote
        sayChecked(false, true);
        return;
    case 0x6:   // emote tell
        if (count() > 1)
            tellTwo(true);
        else
            report(0x72);
        return;
    case 0x7:   // emote shout
        if (self[0xe50] != 0) {
            report(0x5f);
            return;
        }
        sayChecked(true, true);
        return;
    case 0xa:   // hearing range
        online_callee::ChatSetHearingRange(Ptr_2(chat()), static_cast<const Argument*>(arg(1))->mCommand, this);
        return;
    case 0xc:   // earmuffs
        if (self[0xe50] != 0) {
            self[0xe50] = 0;
            docommand_callee::ChatEarmuffsOff(chat(), this);
        } else {
            self[0xe50] = 1;
            docommand_callee::ChatEarmuffsOn(chat(), this);
        }
        return;
    case 0xd: {   // ignore
        if (count() <= 1) {
            report(0x72);
            return;
        }
        Slot text = 0;
        docommand_callee::ArgumentBStrVal(&text, arg(1));
        Slot key = 0;
        online_callee::NocaseWStringCopy(&key, &text, 0, 0xffffffffu);
        void* found = online_callee::NocaseSetFind(self + 0xe44, &key);
        online_callee::NocaseWStringDtor(&key, 0);
        online_callee::WStringDtor(&text, -1);
        if (found != self + 0xe48) {
            nameCommand(docommand_callee::ChatUnIgnore);
            return;
        }
        Slot who = 0;
        nocaseArg(&who);
        Slot me = 0;
        online_callee::NocaseWStringCopy(&me, self + 8, 0, 0xffffffffu);
        if (online_callee::NocaseWStringEqual(&who, &me)) {
            SamsUtilities::UDebug(kIgnoreSelf);
            report(0xb1);
        } else if (nonEmpty()) {
            Slot name = 0;
            docommand_callee::ArgumentBStrVal(&name, arg(1));
            ignore_callee::ChatIgnore(chat(), reinterpret_cast<const ignore_callee::IgnoreWString*>(&name), this);
            online_callee::WStringDtor(&name, -1);
        }
        online_callee::NocaseWStringDtor(&me, 0);
        online_callee::NocaseWStringDtor(&who, 0);
        return;
    }
    case 0xe: {   // blackmark
        if (count() <= 1) {
            report(0x69);
            return;
        }
        Slot who = 0;
        nocaseArg(&who);
        Slot me = 0;
        online_callee::NocaseWStringCopy(&me, self + 8, 0, 0xffffffffu);
        if (online_callee::NocaseWStringEqual(&who, &me)) {
            SamsUtilities::UDebug(kBlackmarkSelf);
            report(0xb0);
        } else if (nonEmpty()) {
            nameCommand(docommand_callee::ChatBlackmark);
        }
        online_callee::NocaseWStringDtor(&me, 0);
        online_callee::NocaseWStringDtor(&who, 0);
        return;
    }
    case 0xf: {   // buddy
        if (count() <= 1) {
            report(0x72);
            return;
        }
        if (!nonEmpty()) {
            report(0x72);
            return;
        }
        Slot text = 0;
        docommand_callee::ArgumentBStrVal(&text, arg(1));
        Slot key = 0;
        online_callee::NocaseWStringCopy(&key, &text, 0, 0xffffffffu);
        void* found = online_callee::BuddyTreeFind(self + 0xe34, &key);
        online_callee::NocaseWStringDtor(&key, 0);
        online_callee::WStringDtor(&text, -1);
        if (found != self + 0xe38) {
            nameCommand(docommand_callee::ChatRemoveBuddy);
            return;
        }
        Slot who = 0;
        nocaseArg(&who);
        Slot me = 0;
        online_callee::NocaseWStringCopy(&me, self + 8, 0, 0xffffffffu);
        if (online_callee::NocaseWStringEqual(&who, &me))
            report(0x9b);
        else
            nameCommand(docommand_callee::ChatAddBuddy);
        online_callee::NocaseWStringDtor(&me, 0);
        online_callee::NocaseWStringDtor(&who, 0);
        return;
    }
    case 0x11:   // page
        if (count() > 1)
            nameCommand(docommand_callee::ChatPage);
        else
            report(0x72);
        return;
    case 0x12: {   // AFK
        Slot text = 0;
        if (self[0xe51] != 0) {
            self[0xe51] = 0;
            Table_2()->FormatString(buffer, 0x400, 0x7c);
            online_callee::WStringCtorFrom(&text, buffer);
            docommand_callee::ChatSay(chat(), &text, false, this);
        } else {
            Table_2()->FormatString(buffer, 0x400, 0x7b);
            online_callee::WStringCtorFrom(&text, buffer);
            docommand_callee::ChatSay(chat(), &text, false, this);
            self[0xe51] = 1;
            docommand_callee::ChatAFK(chat(), this);
        }
        online_callee::WStringDtor(&text, -1);
        return;
    }
    case 0x13: {   // goto
        if (self[0xe84] == 1) {
            SamsUtilities::UDebug(kGotoBusy);
            return;
        }
        if (count() <= 1) {
            report(0x72);
            return;
        }
        Slot who = 0;
        nocaseArg(&who);
        Slot me = 0;
        online_callee::NocaseWStringCopy(&me, self + 8, 0, 0xffffffffu);
        if (online_callee::NocaseWStringEqual(&who, &me)) {
            report(0x9c);
        } else if (nonEmpty()) {
            Slot target = 0;
            docommand_callee::ArgumentBStrVal(&target, arg(1));
            online_parse_callee::WStringAssignRange(self + 0xe88, &target, 0, 0xffffffffu);
            online_callee::WStringDtor(&target, -1);
            bool found = false;
            Slot key = 0;
            nocaseArg(&key);
            uint32_t node = Word_4(self, 0xe30);
            while (node != Addr32(self + 0xe2c)) {
                Slot other = 0;
                online_callee::NocaseWStringCopy(&other, static_cast<uint8_t*>(Ptr_2(node)) + 0xc, 0, 0xffffffffu);
                if (online_callee::NocaseWStringEqual(&key, &other)) {
                    found = true;
                    online_callee::NocaseWStringDtor(&other, 0);
                    break;
                }
                online_callee::NocaseWStringDtor(&other, 0);
                ignore_callee::TreeIncrement(&node);
            }
            if (found) {
                Slot name = 0;
                docommand_callee::ArgumentBStrVal(&name, arg(1));
                Slot copy = 0;
                online_callee::WStringCtorFrom(&copy, Rep(&name)->mData);
                docommand_callee::ChatGoto(chat(), &copy, this);
                online_callee::WStringDtor(&copy, -1);
                online_callee::WStringDtor(&name, -1);
            } else {
                Slot name = 0;
                docommand_callee::ArgumentBStrVal(&name, arg(1));
                TbUniStringVTag tag(0, Rep(&name)->mData);
                Table_2()->FormatString(buffer, 0x400, 0x7f, &tag);
                online_callee::WStringDtor(&name, -1);
                dlgboxparams params;
                params.mText = buffer;
                params.mRed = 0xff;
                params.mGreen = 0xff;
                params.mBlue = 0xff;
                params.mValue8 = 4;
                params.mValueC = 0;
                UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(1), GotoPark_OK, static_cast<dlgbutton>(0),
                             nullptr, false);
            }
            online_callee::NocaseWStringDtor(&key, 0);
        }
        online_callee::NocaseWStringDtor(&me, 0);
        online_callee::NocaseWStringDtor(&who, 0);
        return;
    }
    case 0x14:   // locate
        if (count() > 1 && nonEmpty()) {
            Slot name = 0;
            docommand_callee::ArgumentBStrVal(&name, arg(1));
            Slot copy = 0;
            online_callee::WStringCtorFrom(&copy, Rep(&name)->mData);
            docommand_callee::ChatLocate(chat(), &copy, this);
            online_callee::WStringDtor(&copy, -1);
            online_callee::WStringDtor(&name, -1);
            return;
        }
        report(0x72);
        return;
    case 0x15: {   // vote
        EnsureSystem();
        CFSOnlineSystem* system = EnsureSystem();
        CFSFTPParkInfo* info = gOnlineSystem->GetFTPDownloadParkInfo();
        void* city = init_callee::ReturnCityInfo(Word_4(system, 0xe0), static_cast<int32_t>(Word_4(info, 0x48)));
        if (Word_4(city, 0x60) != 0) return;
        if (Word_4(self, 0xe64) != 0) {
            SamsUtilities::UDebug(kVoteBusy);
            return;
        }
        uint32_t data[4];
        std::memcpy(data, gVoteSendData, sizeof data);
        void* vote = online_callee::OperatorNew(0xc);
        if (vote != nullptr) docommand_callee::VoteCtor(vote);
        SetWord(self, 0xe64, Addr32(vote));
        if (Word_4(self, 0xe64) == 0) return;
        SamsUtilities::UDebug(kVoteCreated);
        online_callee::DisableVoteButton(true);
        int32_t connected = docommand_callee::VoteConnect(Ptr_2(Word_4(self, 0xe64)), const_cast<char*>(gOnlineConfig.mVoteServer.c_str()), nullptr,
                                                          static_cast<uint16_t>(gOnlineConfig.mVotePort), VoteNotifyCallback);
        if (connected != 1) return;
        SamsUtilities::UDebug(kVoteConnected);
        EnsureSystem();
        CFSOnlineSystem* forPlayer = EnsureSystem();
        CFSOnlineSystem* forCustomer = EnsureSystem();
        const char16_t* password = online_callee::LoginPassword(gOnlineSystem);
        int32_t customer = online_callee::LoginCustomerID(forCustomer);
        int32_t player = online_callee::LoginPlayerID(forPlayer);
        docommand_callee::VoteSetSessionInfo(Ptr_2(Word_4(self, 0xe64)), 9, player, customer, password);
        EnsureSystem();
        info = gOnlineSystem->GetFTPDownloadParkInfo();
        docommand_callee::VoteSendVote(Ptr_2(Word_4(self, 0xe64)), static_cast<int32_t>(Word_4(info, 8)), data, 1, nullptr);
        SamsUtilities::UDebug(kVoteSent);
        return;
    }
    case 0x16:   // swear filter
        EnsureMasterSave();
        if (gMasterSave->IsSwearFilterOn()) {
            EnsureMasterSave();
            gMasterSave->SetSwearFilter(false);
            report(0x85);
        } else {
            EnsureMasterSave();
            gMasterSave->SetSwearFilter(true);
            report(0x84);
        }
        return;
    case 0x18:   // help
        report(0x9a);
        return;
    case 0x17:   // say (checked)
        if (count() > 1)
            sayChecked(false, false);
        else
            report(0xa1);
        return;
    default:
        return;
    }
}

// 0x1021b784: the text in the feedback colour 7.
void COnlineControl::Feedback(std::u16string& text) {
    std::u16string copy;
    online_callee::WStringCopy(&copy, &text, 0, 0xffffffffu);
    ReceiveText(copy, 7);
    online_callee::WStringDtor(&copy, -1);
}

// 0x1021b854
uint32_t COnlineControl::GetDataForUser(const nocase_string& key) const {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(this);
    uint32_t result = 0;
    const uint8_t* node = static_cast<const uint8_t*>(online_callee::BuddyTreeFind(const_cast<uint8_t*>(base) + 0xe34, &key));
    if (node != base + 0xe38) {
        result |= 1;
        if (node[0x10] != 0) result |= 2;
        if (node[0x11] != 0) result |= 4;
    }
    node = static_cast<const uint8_t*>(online_callee::NocaseSetFind(const_cast<uint8_t*>(base) + 0xe44, &key));
    if (node != base + 0xe48) result |= 8;
    return result;
}

// 0x1021b950: the flag is not read (the UI is always given 0).
void COnlineControl::AddContact(std::u16string& name, int32_t flag) {
    (void)flag;
    online_callee::UIOnlineAddContact(&gUIInterface, name.c_str(), 0);
    online_callee::WStringSetInsertOne(reinterpret_cast<uint8_t*>(this) + 0xe28, name);
}

// 0x1021ba28
void COnlineControl::RemoveContact(std::u16string& name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    online_callee::UIOnlineRemoveContact(&gUIInterface, name.c_str());
    reinterpret_cast<WStringTree*>(self + 0xe28)->erase_one(reinterpret_cast<const MslWideString&>(name));
}

// 0x1021baf8
bool COnlineControl::AddBuddy(std::u16string& name, int32_t flags) {
    uint8_t* self = Bytes(this);
    const uint8_t direct = (flags & 2) != 0 ? 1 : 0;
    const uint8_t second = (flags & 4) != 0 ? 1 : 0;
    uint64_t key = 0;
    online_callee::NocaseWStringCopy(&key, &name, 0, 0xffffffffu);
    uint8_t* node = static_cast<uint8_t*>(online_callee::BuddyTreeFind(self + 0xe34, &key));
    online_callee::NocaseWStringDtor(&key, 0);
    bool added;
    if (node != self + 0xe38) {
        node[0x10] = direct;
        node[0x11] = second;
        added = false;
    } else {
        uint64_t copy = 0;
        BuddyEntry entry{};
        online_callee::NocaseWStringCopy(&copy, &name, 0, 0xffffffffu);
        online_callee::NocaseWStringCopy(&entry.mKey, &copy, 0, 0xffffffffu);
        entry.mDirect = direct;
        entry.mFlag = second;
        online_callee::BuddyTreeInsert(self + 0xe34, &entry);
        online_callee::NocaseWStringDtor(&entry.mKey, 0);
        online_callee::NocaseWStringDtor(&copy, 0);
        added = true;
    }
    uint64_t lookup = 0;
    online_callee::NocaseWStringCopy(&lookup, &name, 0, 0xffffffffu);
    const uint32_t data = GetDataForUser(*reinterpret_cast<const nocase_string*>(&lookup));
    online_callee::NocaseWStringDtor(&lookup, 0);
    // the name is a pointer to the representation, whose data pointer is at +0xc (native pointer at +0x10 here)
    const char16_t* text;
    const uint8_t* rep = *reinterpret_cast<const uint8_t* const*>(&name);
    std::memcpy(&text, rep + 0x10, sizeof(text));
    online_callee::UIOnlineAddBuddy(&gUIInterface, text, static_cast<int32_t>(data));
    rep = *reinterpret_cast<const uint8_t* const*>(&name);
    std::memcpy(&text, rep + 0x10, sizeof(text));
    online_callee::UIOnlineUpdateContact(&gUIInterface, text, static_cast<int32_t>(data));
    return added;
}

// 0x1021bdcc
void COnlineControl::RemoveBuddy(std::u16string& name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t key[16] = {};
    online_callee::NocaseWStringCopy(key, &name, 0, 0xffffffffu);
    online_callee::BuddyTreeEraseOne(self + 0xe34, key);
    online_callee::NocaseWStringDtor(key, 0);
    alignas(8) uint8_t second[16] = {};
    online_callee::NocaseWStringCopy(second, &name, 0, 0xffffffffu);
    uint32_t data = GetDataForUser(reinterpret_cast<const nocase_string&>(second));
    online_callee::NocaseWStringDtor(second, 0);
    online_callee::UIOnlineRemoveBuddy(&gUIInterface, name.c_str());
    online_callee::UIOnlineUpdateContact(&gUIInterface, name.c_str(), static_cast<int32_t>(data));
}

// 0x1021bf30: every contact goes to the contact list with its buddy / online / ignore bits
void COnlineControl::UpdateUserList() const {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(this);
    uint32_t first;
    std::memcpy(&first, self + 0xe30, 4);
    // the first node's link is a 32-bit arena offset (Ptr32 convention)
    MslRedBlackTree::node_base* node =
        first != 0 ? reinterpret_cast<MslRedBlackTree::node_base*>(tpw::ArenaBase() + first) : nullptr;
    while (node != reinterpret_cast<const MslRedBlackTree::node_base*>(self + 0xe2c)) {
        const std::u16string& value = static_cast<ContactNode*>(node)->mValue;
        alignas(8) uint8_t key[16] = {};
        online_callee::NocaseWStringCopy(key, &value, 0, 0xffffffffu);
        uint32_t data = GetDataForUser(reinterpret_cast<const nocase_string&>(key));
        online_callee::NocaseWStringDtor(key, 0);
        online_callee::UIOnlineAddContact(&gUIInterface, static_cast<ContactNode*>(node)->mValue.c_str(), static_cast<int32_t>(data));
        MslRedBlackTree::increment(node);
    }
}

// Unit v5903: the buddy / ignore list helpers of COnlineControl, GotoFailed, GotoStoredPerson, the pair<const nocase_string, BuddyInfo>
// destructor 0x1021bd24 and the basic_ofstream<wchar_t> destructor 0x1021ce74.

// 0x102b1f08 (TOC -0x5eb4): the vtable the ofstream destructor stores last (initial word 0x102b1f00).
uint32_t gOfsWVtblB[4];

namespace {
inline uint32_t RdW(const void* base, uint32_t off) {
    return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(base) + off);
}
// The object's pointer words (node links, the person, the key reps) are arena offsets (Ptr32); 0 is null.
inline void* P32(uint32_t v) {
    Ptr32<void> p;
    p.mOffset = v;
    return p.get();
}
inline uint32_t A32(const void* p) { return tpw::ArenaOffset(p); }
}  // namespace

// 0x1021c02c: every buddy to the UI, with its data word.
void COnlineControl::UpdateBuddyList() const {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(this);
    uint32_t it = RdW(self, 0xe40);
    while (it != A32(self) + 0xe38) {
        uint32_t data = GetDataForUser(*static_cast<const nocase_string*>(P32(it + 0xc)));
        uint32_t rep = RdW(P32(it + 0xc), 0);
        online_callee::UIOnlineAddBuddy(&gUIInterface, static_cast<const char16_t*>(P32(RdW(P32(rep), 0xc))),
                                        static_cast<int32_t>(data));
        ignore_callee::TreeIncrement(&it);
    }
}

// 0x1021c0ec: every name of the buddy map (end node 0xe38, first node word 0xe40).
void COnlineControl::UpdateBuddyListToServer() {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(this);
    uint32_t it = RdW(self, 0xe40);
    while (it != A32(self) + 0xe38) {
        docommand_callee::ChatAddBuddy(RdW(self, 0), P32(it + 0xc), this);
        ignore_callee::TreeIncrement(&it);
    }
}

// 0x1021c190: every name of the ignore set (end node 0xe48, first node word 0xe4c; the key is at node + 0xc).
void COnlineControl::UpdateIgnoreListToServer() {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(this);
    uint32_t it = RdW(self, 0xe4c);
    while (it != A32(self) + 0xe48) {
        ignore_callee::ChatIgnore(RdW(self, 0), static_cast<const ignore_callee::IgnoreWString*>(P32(it + 0xc)), this);
        ignore_callee::TreeIncrement(&it);
    }
}

// 0x1021c254: the text 0x9d of the UI string table goes to the feedback buffer and to the chat panel; the goto flag is cleared.
void COnlineControl::GotoFailed() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    char16_t* buffer = reinterpret_cast<char16_t*>(self + 0x220);
    const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[19]);   // Mac 0x598
    table->FormatString(buffer, 0x400, 0x9d);
    ignore_callee::IgnoreWString first, second;
    online_callee::WStringCtorFrom(&first, buffer);
    online_callee::WStringCopy(&second, &first, 0, 0xffffffffu);
    ReceiveText(*reinterpret_cast<std::u16string*>(&second), 7);
    online_callee::WStringDtor(&second, -1);
    self[0xe84] = 0;
    online_callee::WStringDtor(&first, -1);
}

// 0x1021c330: Chat::Goto with the name of the stored person (the text pointer at person + 0xc).
void COnlineControl::GotoStoredPerson() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    ignore_callee::IgnoreWString name;
    uint32_t person = RdW(self, 0xe88);
    online_callee::WStringCtorFrom(&name, static_cast<const char16_t*>(P32(RdW(P32(person), 0xc))));
    docommand_callee::ChatGoto(RdW(self, 0), &name, this);
    online_callee::WStringDtor(&name, -1);
}

// 0x1021bd24: std::pair<const nocase_string, BuddyInfo>::~pair
BuddyEntry* BuddyEntryDestruct(BuddyEntry* self, int16_t flag) {
    if (self == nullptr) return self;
    online_callee::NocaseWStringDtor(self, 0);
    if (flag > 0) online_callee::OperatorDelete(self);
    return self;
}

// 0x1021ce74: std::basic_ofstream<wchar_t>::~basic_ofstream. The original tests this + 8, this + 0x24 and this + 0x58 against 0 (never true).
BasicOfstreamWchar* BasicOfstreamWchar::destroy(int16_t flag) {
    if (this == nullptr) return this;
    void* filebuf = &mFilebufVt;   // Mac this + 8
    void* locale = mLocale;        // Mac this + 0x24
    void* ios = &mIosVtable;       // Mac this + 0x58
    const uint8_t* files = gIfsWFiles;

    mVtable = gOfsWVtbl;
    mVbase->mFirst = &gOfsWVtbl[3];
    mVbase->mBaseOffset = reinterpret_cast<const uint8_t*>(ios) - reinterpret_cast<const uint8_t*>(mVbase);
    mFilebufVt = gIfsWVtblD;
    if (mFile != files && mFile != files + 0x54 && mFile != files + 0xa8)
        ignore_callee::FilebufClose(static_cast<ignore_callee::IgnoreFilebuf*>(filebuf));
    mFilebufVt = gIfsWVtblC;
    ifstream_callee::LocaleDtor(locale, -1);

    mVtable = gOfsWVtblB;
    mVbase->mFirst = &gOfsWVtblB[3];
    mVbase->mBaseOffset = reinterpret_cast<const uint8_t*>(filebuf) - reinterpret_cast<const uint8_t*>(mVbase);
    if (flag != 0) {
        mIosVtable = gIfsWVtblE;
        ifstream_callee::IosBaseDtor(ios, 0);
    }
    if (flag > 0) online_callee::OperatorDelete(this);
    return this;
}

// COnlineControl::SaveBuddy 0x1021c75c and COnlineControl::ProcessVoteResponse 0x1021c3cc (online.cpp).
// SaveBuddy writes the buddy map's names, one swprintf'd line each, to a file whose wide path is the online path + a separator + the
// player's name + a suffix. The narrow name built from the argument (and ":buddy.dat") is never used afterwards: the original builds it
// and drops it. Both stream failure checks throw an ios_base::failure when the exception mask asks for it (the fake __throw returns).

extern char gIosFailureDtorVec[];   // 0x102661d0
extern char gIosFailureVtbl[];      // 0x1029d084

namespace {

// The 32-bit words of the Mac's objects hold pointers as offsets from the arena base (Ptr32's convention).
uint8_t* Deref_2(uint32_t offset) { return offset != 0 ? reinterpret_cast<uint8_t*>(tpw::gArenaBase + offset) : nullptr; }
uint32_t* Word_5(uint8_t* address) { return reinterpret_cast<uint32_t*>(address); }

const char16_t kLineFormat[] = u"%ls\n";      // 0x10251810
const char kTypeName[] = "!std::exception!!std::ios_base::failure!!";   // 0x102517e6
const char kFailureMessage[] = "ios_base failure in clear";               // 0x1025148b
const char kBuddyFile[] = ":buddy.dat";                                  // 0x10251480

// The stack object of the original: stream at 0x1f0, filebuf at 0x1f8, ios part at 0x248 (host layout differs, the code never relies on it).
struct BuddyStreamObject {
    ignore_callee::IgnoreStream mStream;
    ignore_callee::IgnoreFilebuf mBuf;
    ignore_callee::IgnoreIos mIos;
};

struct IosFailureObject {
    const void* mVtable;   // 0x00 gIosFailureVtbl
    char* mMessage;        // 0x04 the RefCountedPtr
};

void ThrowFailure() {
    using namespace ignore_callee;
    IosFailureObject object;
    object.mVtable = gIosFailureVtbl;
    uint32_t size = static_cast<uint32_t>(std::strlen(kFailureMessage)) + 1;
    char* block = NewArray(size);
    RefPtrCtor(&object.mMessage, block);
    std::strcpy(object.mMessage, kFailureMessage);
    buddy_callee::Throw(kTypeName, &object, gIosFailureDtorVec);
}

// ios.setstate(failbit) when the open or close failed: the badbit goes on, the failbit too when there is no buffer, and the
// exception mask decides whether the failure is thrown.
void SetFailure(ignore_callee::IgnoreIos* ios) {
    ios->mState |= 4;
    if (ios->mStreamBuf == 0) ios->mState |= 1;
    if ((ios->mState & ios->mExceptions) != 0) ThrowFailure();
}

void Teardown(BuddyStreamObject& o, ignore_callee::IgnorePathString& path, void* name) {
    using namespace ignore_callee;
    PathDtor(&path, -1);
    online_callee::StringDtor(name, -1);
    o.mStream.mSlot = gOfsWVtbl;
    o.mStream.mIos->mVtable = reinterpret_cast<const uint8_t*>(gOfsWVtbl) + 0xc;
    o.mStream.mIos->mBaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(&o.mIos) - reinterpret_cast<intptr_t>(o.mStream.mIos));
    FilebufDtor(&o.mBuf, -1);
    OstreamDtor(&o.mStream, 0);
    IosDtor(&o.mIos, 0);
}

}  // namespace

bool OnlineSaveBuddy(COnlineControl* self, const char* path) {
    using namespace ignore_callee;
    uint8_t* me = reinterpret_cast<uint8_t*>(self);
    BuddyStreamObject o;
    OfstreamCtor(&o.mStream, 1);
    void* name[2];
    online_callee::StringCtor(name);
    if (gUserInfo == nullptr) {
        void* block = online_callee::OperatorNew(0xd4);
        if (block != nullptr) UserInfoCtor(block);
        gUserInfo = static_cast<CFSUserInfo*>(block);
    }
    IgnorePathString temp;
    ReturnOnlinePath(&temp, gUserInfo);
    IgnorePathString full;
    PathCopy(&full, &temp);
    PathDtor(&temp, -1);
    PathAppendString(&full, reinterpret_cast<const uint8_t*>(&gTPGlobals) + 0x44c0);
    uint32_t nameRep = *reinterpret_cast<uint32_t*>(me + 8);
    uint32_t nameText = *Word_5(Deref_2(nameRep) + 0xc);
    PathAppendText(&full, reinterpret_cast<const char16_t*>(Deref_2(nameText)));
    PathAppendText(&full, kPathSuffix);
    CharStringAssign(name, path, static_cast<uint32_t>(std::strlen(path)));
    CharStringAppend(name, kBuddyFile, static_cast<uint32_t>(std::strlen(kBuddyFile)));
    char buffer[260];
    SprintfWide(buffer, "%ls", full.mData);
    if (FilebufOpen(&o.mBuf, buffer, 0x10) == 0) SetFailure(o.mStream.mIos);
    if (o.mBuf.mFile == 0) {
        Teardown(o, full, name);
        return false;
    }
    uint32_t node = *reinterpret_cast<uint32_t*>(me + 0xe40);
    while (node != tpw::ArenaOffset(me + 0xe38)) {
        uint32_t key = *Word_5(Deref_2(node) + 0xc);
        uint32_t text = *Word_5(Deref_2(key) + 0xc);
        char16_t line[32];
        SwprintfWide(line, 0x20, kLineFormat, reinterpret_cast<const char16_t*>(Deref_2(text)));
        OstreamWrite(&o.mStream, line);
        TreeIncrement(&node);
    }
    if (FilebufClose(&o.mBuf) == 0) SetFailure(o.mStream.mIos);
    Teardown(o, full, name);
    return true;
}

// The answer of the vote connection: the string picked by the code is formatted from the UI table into the buffer at 0x220, copied
// into a std::wstring and shown as chat text (colour 7); then the done-voting flag goes on.
void COnlineControl::ProcessVoteResponse(int32_t code) {
    uint32_t id = 0x81;
    if (code == 0) id = 0x80;
    else if (code == -2) id = 0x82;
    else if (code == -0x1e) id = 0x83;
    else if (code == -0x1f) id = 0xb4;
    uint8_t* me = reinterpret_cast<uint8_t*>(this);
    char16_t* buffer = reinterpret_cast<char16_t*>(me + 0x220);
    static_cast<TbITable*>(gText.mResourcePointers[19])->FormatString(buffer, 0x400, id);
    void* first[2];
    online_callee::WStringCtorFrom(first, buffer);
    void* second[2];
    online_callee::WStringCopy(second, first, 0, 0xffffffffu);
    ReceiveText(*reinterpret_cast<std::u16string*>(second), 7);
    online_callee::WStringDtor(second, -1);
    online_callee::WStringDtor(first, -1);
    me[0xe60] = 1;
    SamsUtilities::UDebug("Done voting - setting the mDoneVoting flag\n");
}
void COnlineControl::SaveBuddy(const char* path) { OnlineSaveBuddy(this, path); }

// 0x1021d004
bool COnlineControl::LoadBuddy(const char* path) {
    if (Word(this, 0) == 0) return false;
    IgnoreStream stream{};
    IgnoreFilebuf filebuf{};
    alignas(8) uint8_t iosPart[0x50] = {};
    IgnorePathString filePath{};
    char buffer[260];
    char16_t line[32];
    uint64_t text = 0;

    IfstreamCtor(&stream, 1);
    online_callee::StringCtor(&text);
    BuildPath(this, path, &text, &filePath, buffer);
    if (FilebufOpen(&filebuf, buffer, 8) == 0) SetFailed(stream.mIos);
    for (;;) {
        IgnoreLocale locale;
        IosGetloc(&locale, stream.mIos);
        void* facet = UseFacetCtype(&locale);
        const char16_t delimiter = CtypeWiden(facet, 10);
        LocaleDtor(&locale, -1);
        IgnoreStream* result = IstreamGetline(&stream, line, 0x20, delimiter);
        IgnoreIos* ios = result != nullptr ? result->mIos : nullptr;
        uint32_t more = 0;
        if ((ios->mState & 5) == 0) more = ios->mStreamBuf;
        if (more == 0) break;
        IgnoreWString first, name;
        online_callee::WStringCtorFrom(&first, line);
        online_callee::WStringCopy(&name, &first, 0, 0xffffffffu);
        online_callee::WStringDtor(&first, -1);
        AddBuddy(*reinterpret_cast<std::u16string*>(&name), 0);
        online_callee::WStringDtor(&name, -1);
    }
    if (FilebufClose(&filebuf) == 0) SetFailed(stream.mIos);
    PathDtor(&filePath, -1);
    online_callee::StringDtor(&text, -1);
    stream.mSlot = gIfsWVtblB;
    stream.mIos->mVtable = reinterpret_cast<const uint8_t*>(gIfsWVtblB) + 0xc;
    stream.mIos->mBaseOffset = static_cast<int32_t>(iosPart - reinterpret_cast<uint8_t*>(stream.mIos));
    FilebufDtor(&filebuf, -1);
    IstreamDtor(&stream, 0);
    IosDtor(iosPart, 0);
    return true;
}

// Unit v5235: COnlineControl::SaveIgnore 0x1021d74c and COnlineControl::LoadIgnore 0x1021db98 (online.cpp).
// Both build the file name "<online path><separator><player name><suffix>" in a wide string, print it with sprintf("%ls") and open an
// (o|i)fstream on it. The narrow string built from the argument and ":ignore.dat" is never used (kept so the calls run as on the Mac).
// Save writes one line per name of the ignore tree (this + 0xe48); Load reads lines and ignores each name.
// The iostream failure path (clear() with a mask hit) builds an ios_base::failure object and calls __throw, which returns here.

namespace {

const char kIgnoreDat[] = ":ignore.dat";                                   // 0x102514a5
const char16_t kPathSuffix_2[] = u".ign";                                 // 0x1025181a
const char16_t kFormatEntry[] = u"%ls\n";                                  // 0x10251810

uint32_t Word_6(const void* base, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

// The start both functions share (the compiler emitted it twice): the file name in `buffer`.
void BuildPath_2(COnlineControl* self, const char* path, void* text, IgnorePathString* filePath, char* buffer) {
    if (gUserInfo == nullptr) {
        void* block = online_callee::OperatorNew(0xd4);
        if (block != nullptr) UserInfoCtor(block);
        gUserInfo = static_cast<CFSUserInfo*>(block);
    }
    IgnorePathString online;
    ReturnOnlinePath(&online, gUserInfo);
    PathCopy(filePath, &online);
    PathDtor(&online, -1);
    PathAppendString(filePath, reinterpret_cast<const uint8_t*>(&gTPGlobals) + 0x44c0);
    // this + 8 is the player name (a std::wstring: one word, the representation's data pointer at +0xc, native pointer at +0x10 here)
    const uint8_t* rep = static_cast<const uint8_t*>(online_callee::HandleToPointer(Word_6(self, 8)));
    const char16_t* name;
    std::memcpy(&name, rep + 0x10, sizeof(name));
    PathAppendText(filePath, name);
    PathAppendText(filePath, kPathSuffix_2);
    CharStringAssign(text, path, static_cast<uint32_t>(std::strlen(path)));
    CharStringAppend(text, kIgnoreDat, static_cast<uint32_t>(std::strlen(kIgnoreDat)));
    SprintfWide(buffer, kFormatPath, filePath->mData);
}

}  // namespace

// 0x1021d74c
bool OnlineSaveIgnore(COnlineControl* self, const char* path) {
    IgnoreStream stream{};
    IgnoreFilebuf filebuf{};
    alignas(8) uint8_t iosPart[0x50] = {};
    uint64_t text = 0;   // the narrow std::string (one word on the Mac)
    IgnorePathString filePath{};
    char buffer[260];

    OfstreamCtor(&stream, 1);
    online_callee::StringCtor(&text);
    BuildPath_2(self, path, &text, &filePath, buffer);
    if (FilebufOpen(&filebuf, buffer, 0x10) == 0) SetFailed(stream.mIos);

    bool answer = false;
    if (filebuf.mFile != 0) {
        uint32_t node = Word_6(self, 0xe4c);
        const uint32_t end = online_callee::PointerToHandle(reinterpret_cast<uint8_t*>(self) + 0xe48);
        while (node != end) {
            char16_t line[32];
            const uint8_t* entry = static_cast<const uint8_t*>(online_callee::HandleToPointer(node));
            const uint8_t* rep = static_cast<const uint8_t*>(online_callee::HandleToPointer(Word_6(entry, 0xc)));
            const char16_t* name;
            std::memcpy(&name, rep + 0x10, sizeof(name));
            SwprintfWide(line, 0x20, kFormatEntry, name);
            OstreamWrite(&stream, line);
            TreeIncrement(&node);
        }
        if (FilebufClose(&filebuf) == 0) SetFailed(stream.mIos);
        answer = true;
    }
    PathDtor(&filePath, -1);
    online_callee::StringDtor(&text, -1);
    stream.mSlot = gOfsWVtbl;
    stream.mIos->mVtable = reinterpret_cast<const uint8_t*>(gOfsWVtbl) + 0xc;
    stream.mIos->mBaseOffset = static_cast<int32_t>(iosPart - reinterpret_cast<uint8_t*>(stream.mIos));
    FilebufDtor(&filebuf, -1);
    OstreamDtor(&stream, 0);
    IosDtor(iosPart, 0);
    return answer;
}

void COnlineControl::SaveIgnore(const char* path) { OnlineSaveIgnore(this, path); }

// 0x1021db98
bool COnlineControl::LoadIgnore(const char* path) {
    if (Word_6(this, 0) == 0) return false;
    IgnoreStream stream{};
    IgnoreFilebuf filebuf{};
    alignas(8) uint8_t iosPart[0x50] = {};
    uint64_t text = 0;
    IgnorePathString filePath{};
    char buffer[260];
    char16_t line[32];

    IfstreamCtor(&stream, 1);
    online_callee::StringCtor(&text);
    BuildPath_2(this, path, &text, &filePath, buffer);
    if (FilebufOpen(&filebuf, buffer, 8) == 0) SetFailed(stream.mIos);
    for (;;) {
        IgnoreLocale locale;
        IosGetloc(&locale, stream.mIos);
        void* facet = UseFacetCtype(&locale);
        const char16_t delimiter = CtypeWiden(facet, 10);
        LocaleDtor(&locale, -1);
        IgnoreStream* result = IstreamGetline(&stream, line, 0x20, delimiter);
        IgnoreIos* ios = result != nullptr ? result->mIos : nullptr;
        uint32_t more = 0;
        if ((ios->mState & 5) == 0) more = ios->mStreamBuf;
        if (more == 0) break;
        IgnoreWString first, name, second;
        online_callee::WStringCtorFrom(&first, line);
        online_callee::WStringCopy(&name, &first, 0, 0xffffffffu);
        online_callee::WStringDtor(&first, -1);
        IgnoreOn(*reinterpret_cast<std::u16string*>(&name));
        online_callee::WStringCtorFrom(&second, line);
        ChatIgnore(Word_6(this, 0), &second, this);
        online_callee::WStringDtor(&second, -1);
        online_callee::WStringDtor(&name, -1);
    }
    if (FilebufClose(&filebuf) == 0) SetFailed(stream.mIos);
    PathDtor(&filePath, -1);
    online_callee::StringDtor(&text, -1);
    stream.mSlot = gIfsWVtblB;
    stream.mIos->mVtable = reinterpret_cast<const uint8_t*>(gIfsWVtblB) + 0xc;
    stream.mIos->mBaseOffset = static_cast<int32_t>(iosPart - reinterpret_cast<uint8_t*>(stream.mIos));
    FilebufDtor(&filebuf, -1);
    IstreamDtor(&stream, 0);
    IosDtor(iosPart, 0);
    return true;
}

// Unit v5907: online.cpp's static initialiser (0x1022a06c), CheckShifts (0x1022a3b4) and the deque assign
// std::__cdeque<const void*>::do_assign<__cdeque<vector<Argument>*>::iterator> (0x10229ae4).

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyString(void* object);    // 0x103bcd38, the narrow string destructor glue
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue
alignas(16) uint8_t gOnlineSinitNarrow[18][64];
alignas(16) uint8_t gOnlineSinitWide[2][64];
uint8_t gOnlineSinitNodes[20][16];
int32_t gOnlineSinitZero;
int32_t gOnlineSinitMask;

static_assert(sizeof(TbDynamicStringTemplate<char>) <= 64, "slot too small");
static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= 64, "slot too small");

// 0x1022a06c (720): the language names, the application name and the registry key, then the two wide strings; each is made and its
// destructor registered for the exit.
void __sinit_online_cpp() {
    static const char* const kNames[18] = {"English", "French", "German",  "Spanish",           "Swedish",           "Italian",
                                           "Dutch",   "Polish", "Danish",  "Japanese",          "TraditionalChinese", "SimplifiedChinese",
                                           "Thai",    "Korean", "Hebrew",  "American",          "Theme Park World",  "Language"};
    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyString);
    void* destroyWide = reinterpret_cast<void*>(&UIMain_DestroyWString);
    gOnlineSinitZero = 0;
    gOnlineSinitMask = 0x10000;
    for (int i = 0; i < 18; i++) {
        auto* object = new (gOnlineSinitNarrow[i]) TbDynamicStringTemplate<char>(kNames[i]);
        __register_global_object(object, destroy, gOnlineSinitNodes[i]);
    }
    auto* wideA = new (gOnlineSinitWide[0]) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(wideA, destroyWide, gOnlineSinitNodes[18]);
    auto* wideB = new (gOnlineSinitWide[1]) TbDynamicStringTemplate<char16_t>(u"gms.dat");
    __register_global_object(wideB, destroyWide, gOnlineSinitNodes[19]);
}

// 0x10229ae4 (836): assigns the elements first..last (an iterator of a deque of vector pointers, the pointers read as words) to this
// deque: word0 capacity, word4 size, word8 start, wordC the block. A count above 0x3fffffff throws length_error (the fakes return,
// and the original goes on).
void CdequeDoAssignArgumentIter(std::__cdeque<const void*, std::allocator<const void*>>* self, MslCdequeIter first, MslCdequeIter last) {
    using namespace IME;
    // The words hold 32-bit addresses; the host's heap lies above 4 GB (gMslMacAddressHigh, 0 on the Mac).
    auto block = [](uint32_t word) { return reinterpret_cast<uint32_t*>((gMslMacAddressHigh << 32) | word); };
    uint32_t count = last.mIndex - first.mIndex;
    if (count > 0x3fffffffu) {
        MslExceptionObject object;
        object.mVtbl = gMslExcVtblC;
        object.mVtbl = gMslExcVtblB;
        size_t length = MslStrlen("cdeque::assign length error");
        object.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* counter = static_cast<int32_t*>(MslNew(4));
            if (counter != nullptr) *counter = 1;
            object.mRc.mCount = counter;
        }
        MslStrcpy(object.mRc.mPtr, "cdeque::assign length error");
        object.mVtbl = gMslLengthErrorVtbl;
        MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, gMslLengthErrorDtorTV);
    }
    uint32_t index = first.mIndex;
    uint32_t* slot = first.mPtr;
    auto* ring = first.mDeque;
    if (count <= self->mWord0) {
        uint32_t* destination = block(self->mWordC);
        while (index != last.mIndex) {
            *destination = *slot;
            index++;
            slot++;
            if (slot == block(ring->mWordC) + ring->mWord0) slot = block(ring->mWordC);
            destination++;
        }
        self->mWord8 = 0;
        self->mWord4 = count;
    } else {
        uint32_t oldBlock = self->mWordC;
        self->mWordC = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(MslNew(count * 4)));
        uint32_t* destination = block(self->mWordC);
        while (index != last.mIndex) {
            *destination = *slot;
            index++;
            slot++;
            if (slot == block(ring->mWordC) + ring->mWord0) slot = block(ring->mWordC);
            destination++;
        }
        if (oldBlock != 0) MslDelete(block(oldBlock));
        self->mWord0 = count;
        self->mWord4 = count;
        self->mWord8 = 0;
    }
}

// OnlineChatData constructor 0x1021e068 and destructor 0x10213cac (online.cpp).

OnlineChatData* OnlineChatData::Construct(OnlineChatData* self) {
    online_callee::WStringCtor(&self->mString0);
    online_callee::WStringCtor(&self->mString4);
    chatdata_callee::TreeWStringCtor(&self->mTreeA);
    chatdata_callee::TreeBuddyCtor(&self->mTreeB);
    chatdata_callee::TreeNocaseCtor(&self->mTreeC);
    self->mFlag30 = 0;
    self->mFlag31 = 0;
    chatdata_callee::ListCtor(&self->mList);
    return self;
}

OnlineChatData* OnlineChatData::Destruct(OnlineChatData* self, int16_t deleteFlag) {
    if (self == nullptr) return self;
    chatdata_callee::ListClear(&self->mList);
    if (self->mTreeC[1] != 0) chatdata_callee::TreeNocaseDestroy(&self->mTreeC, self->mTreeC[1]);
    if (self->mTreeB[1] != 0) chatdata_callee::TreeBuddyDestroy(&self->mTreeB, self->mTreeB[1]);
    if (self->mTreeA[1] != 0) chatdata_callee::TreeWStringDestroy(&self->mTreeA, self->mTreeA[1]);
    online_callee::WStringDtor(&self->mString4, -1);
    online_callee::WStringDtor(&self->mString0, -1);
    if (deleteFlag > 0) online_callee::OperatorDelete(self);
    return self;
}

// std::basic_istream<wchar_t> ~basic_istream 0x1021d4a0, getline 0x102202b0 and the constructor 0x102266e4 (unit v5708).
// The compiler's exception handler regions (the setstate/clear catch blocks) are unreachable without a real exception
// and are not translated.

namespace {

using ignore_callee::IgnoreIos;
using ignore_callee::IgnoreStream;

const char kIosFailureText[] = "ios_base failure in clear";                                // 0x1025148b
const char kIosFailureType[] = "!std::exception!!std::ios_base::failure!!";                // 0x102517e6

inline WideIstream* Wide(IgnoreStream* stream) { return reinterpret_cast<WideIstream*>(stream); }

inline IgnoreIos* IosPart(WideIstream* self) {
    return reinterpret_cast<IgnoreIos*>(reinterpret_cast<uint8_t*>(self) + kWideIstreamIosPart);
}

inline MacStreamBuf* StreamBufOf(IgnoreIos* ios) { return Ptr32<MacStreamBuf>{ios->mStreamBuf}.get(); }

inline uint32_t Entry(MacStreamBuf* sb, uint32_t offset) { return *reinterpret_cast<uint32_t*>(sb->mVtable.get() + offset); }

// Builds the ios_base::failure the stream throws once its state meets the exception mask (the fake __throw returns).
void ThrowFailure_2() {
    MslThrowObject object;
    object.mVtbl = gIosFailureVtbl;
    uint32_t length = wostream_callee::Strlen(kIosFailureText);
    object.mMessage = wostream_callee::NewBytes(length + 1);
    object.mCount = nullptr;
    if (object.mMessage != nullptr) {
        int32_t* count = wostream_callee::NewInt();
        if (count != nullptr) *count = 1;
        object.mCount = count;
    }
    wostream_callee::CopyString(object.mMessage, kIosFailureText);
    wostream_callee::Throw(kIosFailureType, &object, gIosFailureDtorVec);
}

// Sets the stream state bits, then raises when the state meets the exception mask.
void SetStateBits(IgnoreIos* ios, uint32_t bits) {
    ios->mState |= bits;
    if (ios->mStreamBuf == 0) ios->mState |= 1;
    if ((ios->mState & ios->mExceptions) != 0) ThrowFailure_2();
}

void Bump(IgnoreIos* ios) {
    MacStreamBuf* sb = StreamBufOf(ios);
    if (sb->mGptr.mOffset < sb->mEgptr.mOffset) sb->mGptr.mOffset += 2;
    else MslStreamBufVirtual(sb, Entry(sb, 0x2c));
}

}  // namespace

namespace ignore_callee {

// The original answers this; the declaration in online.h is void.
void IstreamDtor(IgnoreStream* stream, int16_t flag) {
    WideIstream* self = Wide(stream);
    if (self != nullptr) {
        self->mVtable = gIfsWVtblA;
        IgnoreIos* part = IosPart(self);
        self->mIos->mVtable = reinterpret_cast<const uint8_t*>(gIfsWVtblA) + 0xc;
        self->mIos->mBaseOffset = static_cast<int32_t>(reinterpret_cast<uint8_t*>(part) - reinterpret_cast<uint8_t*>(self->mIos));
        if (flag != 0) {
            // The original also tests this + 0xc != 0, always true for a real object.
            part->mVtable = gIfsWVtblE;
            ifstream_callee::IosBaseDtor(part, 0);
        }
        if (flag > 0) MslOperatorDelete(self);
    }
}

IgnoreStream* IstreamGetline(IgnoreStream* stream, char16_t* buffer, int32_t count, char16_t delimiter) {
    WideIstream* self = Wide(stream);
    bool ok = false;
    self->mGcount = 0;
    if (!wostream_callee::Good(self->mIos)) {
        wostream_callee::Setstate(self->mIos, 4);
    } else {
        if (wostream_callee::Tie(self->mIos) != nullptr) wostream_callee::Flush(wostream_callee::Tie(self->mIos));
        if (wostream_callee::Good(self->mIos)) ok = true;
        else wostream_callee::Setstate(self->mIos, 4);
    }
    if (buffer == nullptr || count <= 0) {
        SetStateBits(self->mIos, 4);
        return stream;
    }
    if (!ok) return stream;
    uint32_t flags = 0;
    for (;;) {
        MacStreamBuf* sb = StreamBufOf(self->mIos);
        uint16_t c;
        if (sb->mGptr.mOffset < sb->mEgptr.mOffset) c = *reinterpret_cast<uint16_t*>(sb->mGptr.get());
        else c = static_cast<uint16_t>(MslStreamBufVirtual(sb, Entry(sb, 0x28)));
        if (c == 0xffff) {
            flags |= 2;
            break;
        }
        if (c == delimiter) {
            Bump(self->mIos);
            self->mGcount = self->mGcount + 1;
            break;
        }
        if (count == 1) {
            flags |= 4;
            break;
        }
        Bump(self->mIos);
        self->mGcount = self->mGcount + 1;
        count--;
        *buffer = c;
        buffer++;
    }
    if (self->mGcount == 0) flags |= 4;
    *buffer = 0;
    SetStateBits(self->mIos, flags);
    return stream;
}

IgnoreStream* IstreamCtor(IgnoreStream* stream, int16_t withVirtualBase, uint32_t streambuf) {
    WideIstream* self = Wide(stream);
    IgnoreIos* part = IosPart(self);
    if (withVirtualBase != 0) {
        self->mIos = part;
        part->mVtable = gIfsWVtblE;   // the original first stores __vt__ios_base, overwritten at once
    }
    self->mVtable = gIfsWVtblA;
    self->mIos->mVtable = reinterpret_cast<const uint8_t*>(gIfsWVtblA) + 0xc;
    self->mIos->mBaseOffset = static_cast<int32_t>(reinterpret_cast<uint8_t*>(part) - reinterpret_cast<uint8_t*>(self->mIos));
    self->mGcount = 0;
    IgnoreIos* ios = self->mIos;
    wistream_callee::IosInit(ios, streambuf);
    std::memset(&ios->mPad3c[0], 0, 4);          // Mac 0x3c
    char16_t fill = wistream_callee::Widen(ios, ' ');
    std::memcpy(&ios->mPad3c[4], &fill, 2);      // Mac 0x40
    return stream;
}

}  // namespace ignore_callee

// Unit v51059: CParser's set helpers and a handful of small library/destructor functions of the online code (main app, addresses in comments).

uint8_t gIPBaseVtableBlock[16];     // 0x102b1f7c
uint8_t gIPSystemVtableBlock[16];   // 0x102b1938

namespace v51059 {

// 0x10214348: both vtable stores happen, the second one stays.
void* IPSystemDtor(void* self, int16_t deleteFlag) {
    if (self) {
        void** word = static_cast<void**>(self);
        *word = &gIPBaseVtableBlock[8];
        *word = &gIPSystemVtableBlock[8];
        if (deleteFlag > 0) cparser_callee::OperatorDelete(self);
    }
    return self;
}

}  // namespace v51059

namespace online_callee {

// 0x1021425c
void NocaseSetDtor(void* self, int16_t flag) {
    if (self) {
        uint32_t* words = static_cast<uint32_t*>(self);
        if (words[1]) chatdata_callee::TreeNocaseDestroy(self, words[1]);
        if (flag > 0) cparser_callee::OperatorDelete(self);
    }
}

// 0x10214148
void BuddyMapDtor(void* self, int16_t flag) {
    if (self) {
        uint32_t* words = static_cast<uint32_t*>(self);
        if (words[1]) chatdata_callee::TreeBuddyDestroy(self, words[1]);
        if (flag > 0) cparser_callee::OperatorDelete(self);
    }
}

// 0x10213fbc
void WStringSetDtor(void* self, int16_t flag) {
    if (self) {
        uint32_t* words = static_cast<uint32_t*>(self);
        if (words[1]) chatdata_callee::TreeWStringDestroy(self, words[1]);
        if (flag > 0) cparser_callee::OperatorDelete(self);
    }
}

}  // namespace online_callee

// Weak definitions of per-unit callees that are calls through the cross-fragment glue into the Mac's C library (MSL in "c_c++ symbols
// shared") and NS_MacDoze. Unit v5L49: without them the launcher's generated stand-ins answered 0 (wcslen 0, nothing written).
// Each test library keeps its own recording fake, which wins over these; tests/test_glue_callees.py runs these against the Mac's code.

namespace {

// MSL's wcstombs (c_c++ 0x100240f4) as __pformatter's %ls uses it: each wide character gives its low byte, up to `room` bytes; the zero
// that ends the text (a character whose low byte is 0) is stored and ends the copy. Answers the bytes before it.
uint32_t MslWcsToMbs(char* out, const char16_t* text, uint32_t room) {
    uint32_t count = 0;
    while (count < room) {
        const char byte = static_cast<char>(text[count] & 0xff);
        out[count] = byte;
        if (byte == 0) break;
        ++count;
    }
    return count;
}

}  // namespace

// wcslen (c_c++ 0x10033ff0), glue 0x102389c8: 16-bit units up to the zero.
__attribute__((weak)) uint32_t online_parser_callee::WcsLen(const char16_t* text) { return wcslen(text); }
__attribute__((weak)) uint32_t wostream_callee::Wcslen(const char16_t* text) { return wcslen(text); }

// NS_MacDoze::_wcsset (macdoze 0x1000392c), glue 0x102397f0: every character up to the zero becomes `c`.
__attribute__((weak)) void textobj_callee::WcsSet(char16_t* text, char16_t c) {
    for (char16_t* p = text; *p != 0; ++p) *p = c;
}

// sprintf (c_c++ 0x10027a78), glue 0x10238578, for the one format the callers give it, "%ls" (kFormatPath 0x1025135d and SaveBuddy's):
// __pformatter's %ls (0x10027058) converts the text with wcstombs into a 0x200-byte buffer of its own (NULL: the empty text) and copies
// that out, so `buffer` gets the low byte of each character up to the first whose low byte is 0. Other characters of the format are
// copied as they are and "%%" gives '%'. The Mac's buffer has no zero after 0x200 characters (strlen then reads on into its stack); this
// one stops there.
__attribute__((weak)) void ignore_callee::SprintfWide(char* buffer, const char* format, const char16_t* text) {
    static const char16_t kEmpty[1] = {0};
    char* out = buffer;
    for (const char* f = format; *f != 0; ++f) {
        if (f[0] == '%' && f[1] == '%') {
            *out++ = '%';
            ++f;
        } else if (f[0] == '%' && f[1] == 'l' && f[2] == 's') {
            char converted[0x201];
            const uint32_t length = MslWcsToMbs(converted, text != nullptr ? text : kEmpty, 0x200);
            for (uint32_t i = 0; i < length; ++i) *out++ = converted[i];
            f += 2;
        } else {
            *out++ = *f;
        }
    }
    *out = 0;
}

// std::__tree<std::pair<const nocase_string, BuddyInfo>, ...> (the buddy map's tree): find 0x1021f8d8, insert_one 0x10222d9c,
// destroy 0x10222564, erase 0x102278fc. Node: left +0, right +4, parent|colour +8, pair +0xc (Mac); the tree: count +0, root +4 (the end
// node's left word), begin +0xc.

namespace {

inline BuddyEntry* PairOf(BuddyTreeNode* node) { return reinterpret_cast<BuddyEntry*>(node->mPair); }

inline BuddyTreeNode* EndOf(BuddyTree* tree) { return reinterpret_cast<BuddyTreeNode*>(&tree->mRoot); }

// The key's string destructor, inlined by the compiler: drops one reference; the last one frees the data and the rep.
void ReleaseKey(BuddyTreeNode* node) {
    BuddyEntry* pair = PairOf(node);
    NocaseRep* rep = reinterpret_cast<NocaseRep*>(pair->mKey);
    if (rep->mRefs == 0) rep->mRefs = 1;
    uint32_t before = rep->mRefs;
    rep->mRefs = before - 1;
    if (before - 1 != 0) rep = nullptr;
    pair->mKey = reinterpret_cast<uintptr_t>(rep);
    if (rep) {
        buddy_tree_callee::Delete(rep->mData);
        buddy_tree_callee::Delete(rep);
    }
}

// A grandchild the compiler unrolled: its subtrees, then the allocator's destroy and deallocate (the node is not freed here).
void DestroyLeaf(BuddyTree* self, BuddyTreeNode* node) {
    if (node->mLeft) BuddyTreeDestroy(self, node->mLeft);
    if (node->mRight) BuddyTreeDestroy(self, node->mRight);
    buddy_tree_callee::AllocDestroy(self, node->mPair);
    buddy_tree_callee::Deallocate(&self->mRoot, node, 1);
}

// A child the compiler unrolled: its two children as leaves, then the pair's destructor and operator delete.
void DestroyChild(BuddyTree* self, BuddyTreeNode* child) {
    BuddyTreeNode* first = child->mLeft;
    if (first) DestroyLeaf(self, first);
    BuddyTreeNode* second = child->mRight;
    if (second) DestroyLeaf(self, second);
    BuddyEntryDestruct(PairOf(child), -1);
    buddy_tree_callee::Delete(child);
}

}  // namespace

// 0x1021f8d8
BuddyTreeNode* BuddyTreeFind(BuddyTree* self, const void* key) {
    BuddyTreeNode* best = EndOf(self);
    BuddyTreeNode* node = self->mRoot;
    while (node) {
        if (!NocaseWStringLess(node->mPair, key)) {
            best = node;
            node = node->mLeft;
        } else {
            node = node->mRight;
        }
    }
    if (best == EndOf(self) || NocaseWStringLess(key, best->mPair)) return EndOf(self);
    return best;
}

// 0x10222d9c. The two constants are the bytes at 0x102b1fb0 (1) and 0x102b1fb1 (0).
void BuddyTreeInsertOne(BuddyTreeInsertResult* result, BuddyTree* self, const void* pair) {
    BuddyTreeNode* parent = EndOf(self);
    BuddyTreeNode* notLess = nullptr;
    bool left = true;
    bool leftmost = true;
    BuddyTreeNode* node = self->mRoot;
    while (node) {
        parent = node;
        if (NocaseWStringLess(pair, node->mPair)) {
            node = node->mLeft;
            left = true;
        } else {
            notLess = node;
            node = node->mRight;
            left = false;
            leftmost = false;
        }
    }
    if (notLess == nullptr || NocaseWStringLess(notLess->mPair, pair)) {
        BuddyTreeNode* made = BuddyTreeInsertNodeAt(self, parent, left, leftmost, pair);
        result->mNode = made;
        result->mInserted = true;
    } else {
        result->mNode = notLess;
        result->mInserted = false;
    }
}

// 0x10222564
void BuddyTreeDestroy(BuddyTree* self, BuddyTreeNode* node) {
    BuddyTreeNode* first = node->mLeft;
    if (first) DestroyChild(self, first);
    BuddyTreeNode* second = node->mRight;
    if (second) DestroyChild(self, second);
    ReleaseKey(node);
    buddy_tree_callee::Delete(node);
}

// 0x102278fc
void BuddyTreeErase(BuddyTree* self, BuddyTreeNode* position) {
    BuddyTreeNode* node = position;
    if (node == self->mLeftmost) {
        BuddyTreeNode* it = node->mRight;
        if (it) {
            while (it->mLeft) it = it->mLeft;
        } else {
            it = node;
            for (;;) {
                BuddyTreeNode* current = it;
                BuddyTreeNode* parent = reinterpret_cast<BuddyTreeNode*>(current->mParentColor & ~static_cast<uintptr_t>(1));
                it = parent;
                if (current == parent->mLeft) break;
            }
        }
        self->mLeftmost = it;
    }
    MslRedBlackTree::balance_erase(reinterpret_cast<MslRedBlackTree::node_base*>(node),
                                   reinterpret_cast<MslRedBlackTree::node_base*>(self->mRoot));
    ReleaseKey(node);
    buddy_tree_callee::Delete(node);
    self->mCount = self->mCount - 1;
}

// Unit v51044: small functions of the online, lobby and 2D map code:
//   0x1021eb7c std::less<std::wstring>::operator()          0x1021ecb8 std::allocator<nocase_string>::destroy
//   0x1021e4dc std::basic_streambuf<wchar_t>::uflow         0x10222c78 std::allocator<pair<const nocase_string, BuddyInfo>>::destroy
//   0x102238d4 std::allocator<std::wstring>::destroy        0x10227840 std::pair<const nocase_string, BuddyInfo>::pair (copy)
//   0x1022b7e8 ShowWorldLobbySats  0x1022b85c HideWorldLobbySats  0x1022ba30 WorldLobbySetZoomButton
//   0x10230d64 CUI2DMapSprite::CUI2DMapSprite

// 0x102318f0: the sprite list's AddTail (translated elsewhere; the test library fakes it).
template <>
void TbTList<CUI2DMapSprite*, CUI2DMapSprite*>::AddTail(CUI2DMapSprite* value);

namespace {

// The three allocators' destroy functions are the same code: the string is one word, a pointer to a rep with the reference count at 0x08
// and the data pointer at 0x0c. The count is first made at least 1 (a count of 0 is taken as 1), then lowered; the string keeps its rep
// only when the count reached 0, and then the data and the rep are freed.
void ReleaseString(void* slot) {
    if (slot == nullptr) return;
    NocaseRep** word = reinterpret_cast<NocaseRep**>(slot);
    NocaseRep* rep = *word;
    if (rep->mRefs == 0) {
        uint32_t refs = rep->mRefs;
        if (refs == 0) refs = 1;
        rep->mRefs = refs;
    }
    uint32_t left = rep->mRefs - 1;
    rep->mRefs = left;
    if (left != 0) rep = nullptr;
    *word = rep;
    if (*word != nullptr) {
        buddy_tree_callee::Delete((*word)->mData);
        buddy_tree_callee::Delete(*word);
    }
}

}  // namespace

// 0x1021eb7c
bool WideStringLess(void* functor, const MslWideString* a, const MslWideString* b) {
    (void)functor;
    return wstring_less_callee::Lt(a, b);
}

namespace nocase_callee {
// 0x1021ecb8
void AllocDestroy(void* allocator, nocase_string* value) {
    (void)allocator;
    ReleaseString(value);
}
}  // namespace nocase_callee

namespace buddy_tree_callee {
// 0x10222c78
void AllocDestroy(void* allocator, void* pair) {
    (void)allocator;
    ReleaseString(pair);
}

// 0x10227840: the key is copied whole, then the two flag bytes (a halfword on the Mac).
void* PairCtor(void* self, const void* src) {
    online_callee::NocaseWStringCopy(self, src, 0, 0xffffffffu);
    BuddyEntry* to = static_cast<BuddyEntry*>(self);
    const BuddyEntry* from = static_cast<const BuddyEntry*>(src);
    to->mDirect = from->mDirect;
    to->mFlag = from->mFlag;
    return self;
}
}  // namespace buddy_tree_callee

namespace wstring_alloc {
// 0x102238d4
void Destroy(void* allocator, void* string) {
    (void)allocator;
    ReleaseString(string);
}
}  // namespace wstring_alloc

// 0x1021e4dc
uint16_t WStreambufUflow(MslWStreambuf* self) {
    uint16_t result = 0xffff;
    if (wstreambuf_callee::Underflow(self) != 0xffff) {
        char16_t* at = self->mGNext;
        self->mGNext = at + 1;
        result = *at;
    }
    return result;
}
