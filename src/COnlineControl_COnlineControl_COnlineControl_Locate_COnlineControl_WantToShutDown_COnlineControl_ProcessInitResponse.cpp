// COnlineControl::ProcessInitResponse (0x10214e80): the chat client's answer to the init request. The first Argument's command number picks
// a case of the jump table at 0x102b1b68 (55 entries; a number past 0x36 and the entries that point at 0x10215480 take the default).
#include <cstdint>
#include <cstdio>
#include <new>

#include "FS_Online.h"
#include "Text.h"
#include "UI_pump.h"
#include "World.h"
#include "online.h"
#include "platform/samsutils.h"

namespace {

// Strings of the data section at 0x10250e7f (the TOC slot -0x395c) and their offsets.
const char kEmptyPath[] = "";           // +0x1a
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

inline uint32_t Word(const void* base, uint32_t offset) {
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

COnlineControl* Control() {
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
    COnlineControl* control = Control();
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
    const uint32_t command = Word(data, 8);
    switch (CaseOf(command)) {
    case 1: {
        SamsUtilities::UDebug(kMsgB2);
        uint8_t* info = reinterpret_cast<uint8_t*>(System()->GetFTPDownloadParkInfo());
        char buffer[16];
        std::sprintf(buffer, kFormatD, static_cast<int>(Word(info, 8)));
        uint64_t text;
        init_callee::StringFromChars(&text, buffer);
        init_callee::ChatSetPlayerData(Word(this, 0), static_cast<int32_t>(online_callee::AvatarID(System())), this);
        info = reinterpret_cast<uint8_t*>(System()->GetFTPDownloadParkInfo());
        uint64_t name;
        online_callee::WStringCtorFrom(&name, WideAt(info, 0x14));
        uint64_t topic;
        online_callee::WStringCtorFrom(&topic, kRandomName);
        CFSOnlineSystem* system = System();
        System();
        info = reinterpret_cast<uint8_t*>(System()->GetFTPDownloadParkInfo());
        uint8_t* city = static_cast<uint8_t*>(init_callee::ReturnCityInfo(Word(system, 0xe0), static_cast<int32_t>(Word(info, 0x48))));
        uint64_t cityName;
        online_callee::WStringCtorFrom(&cityName, WideAt(city, 0x14));
        init_callee::ChatCreateRoom(Word(this, 0), &name, &topic, &cityName, &text, 0x100, this);
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
