// Test library for the lobby's chat satellite (unit v5226; libtpw_unit_ProcessGetChatSatellitePlayers.dylib,
// tests/test_ProcessGetChatSatellitePlayers.py), main app:
//   src/ProcessGetChatSatellitePlayers.cpp   ChatServerCallbackFn, ProcessGetChatSatellitePlayers, WorldLobbyControllerMsg and the
//                                            six chat satellite globals
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The virtual calls
// on the lobby screens go through a hand-made vtable (Itanium slots: Show 8, Hide 9, Slot50 11). The globals of other files the
// unit reads are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "email.h"
#include "lobby.h"
#include "online.h"
#include "platform/samsutils.h"
#include "std_basic_filebuf.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

#define I(x) static_cast<int64_t>(x)

// ---- globals of other files
CFSOnlineSystem* gOnlineSystem;
CLBScreenSystem* gLobbyScreens;
InterfaceWindow* gUISatelliteParent;
OnlineConfig gOnlineConfig;
OnlineConfig::OnlineConfig() {}

namespace {

uint32_t Crc(const char* text) {
    uint32_t crc = 0xffffffffu;
    for (; *text != 0; text++) {
        crc ^= static_cast<uint8_t>(*text);
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc & 0x7fffffffu;
}

typedef CFSOnlineSystemList<TbPtrList<CFSOnlinePark>, ccParkList> ParkListBase;
typedef TbTList<CFSOnlinePark*, CFSOnlinePark*> ParkTable;

MslCharRep* MakeRep(const char* text) {
    MslCharRep* rep = static_cast<MslCharRep*>(tpw::ArenaAllocZeroed(sizeof(MslCharRep)));
    size_t n = std::strlen(text);
    char* data = static_cast<char*>(tpw::ArenaAllocZeroed(n + 1));
    std::memcpy(data, text, n);
    rep->data = data;
    return rep;
}

// The virtual calls of the lobby screens.
void FakeShow(void* self) { ev("Show", {I(lab(self))}); }
void FakeHide(void* self) { ev("Hide", {I(lab(self))}); }
void FakeSlot50(void* self) { ev("Slot50", {I(lab(self))}); }
void* gVtable[16];

}  // namespace

// ---- the fakes
void* online_callee::OperatorNew(uint32_t size) {
    ev("OperatorNew", {I(size)});
    if (size == 0xc && ret("newnull") != 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}

void outbox_callee::OnlineSystemCtor(void* system) {
    ev("SystemCtor", {I(newlab(system))});
    CFSOnlineSystem* sys = static_cast<CFSOnlineSystem*>(system);
    const char* keys[4] = {"c_e0", "c_f4", "c_f8", "c_fc"};
    for (int k = 0; k < 4; k++) {
        char* block = static_cast<char*>(tpw::ArenaAllocZeroed(0x100));
        *reinterpret_cast<int32_t*>(block + offsetof(ParkListBase, mF28)) = static_cast<int32_t>(ret(keys[k]));
        if (k == 1) {
            char* table = static_cast<char*>(tpw::ArenaAllocZeroed(0x40));
            *reinterpret_cast<int32_t*>(table + offsetof(ParkTable, mCount)) = static_cast<int32_t>(ret("c_cnt"));
            *reinterpret_cast<void**>(block + offsetof(ParkListBase, mList)) = table;
        }
        if (k == 0) sys->mCityList = reinterpret_cast<CFSOnlineSystemCityList*>(block);
        if (k == 1) sys->mParksInCity2 = reinterpret_cast<CFSOnlineSystemParkList_InCity*>(block);
        if (k == 2) sys->mThemeList = reinterpret_cast<CFSOnlineSystemThemeList*>(block);
        if (k == 3) sys->mServerList = reinterpret_cast<CFSOnlineSystemServerList*>(block);
    }
}

void online_callee::ChatCtor(void* chat) { ev("ChatCtor", {I(newlab(chat))}); }
void online_callee::ChatRelease(void* chat) { ev("ChatRelease", {I(lab(chat))}); }
void online_callee::StringDtor(void* self, int16_t flag) {
    (void)self;
    ev("StringDtor", {I(flag)});
}

void outbox_callee::StringCtor(void* str, const char* text) {
    ev("StringCtor", {I(Crc(text))});
    static_cast<MslCharString*>(str)->mRep = MakeRep(text);
}

namespace chat_satellite_callee {
void ArgumentStrVal(MslCharString* result, const Argument* argument) {
    ev("StrVal", {I(argument->mCommand)});
    char text[16];
    std::snprintf(text, sizeof text, "%d", static_cast<int>(ret("strval")));
    result->mRep = MakeRep(text);
}

int32_t ChatConnect(void* chat, const char* server, uint16_t port, ChatCallback callback, char* extra) {
    ev("ChatConnect", {I(lab(chat)), I(Crc(server)), I(port), I(lab(reinterpret_cast<const void*>(callback))),
                       I(reinterpret_cast<uintptr_t>(extra))});
    return static_cast<int32_t>(ret("connect"));
}

void ChatRoomInfo(void* chat, const MslCharString& room, void* context) {
    ev("RoomInfo", {I(lab(chat)), I(Crc(room.mRep->data)), I(reinterpret_cast<uintptr_t>(context))});
}
}  // namespace chat_satellite_callee

namespace msl_char_string {
MslCharString* construct(MslCharString* self, const MslCharString* src, uint32_t pos, uint32_t n) {
    ev("StrSub", {I(static_cast<int32_t>(pos)), I(static_cast<int32_t>(n))});
    self->mRep = src->mRep;
    return self;
}
}  // namespace msl_char_string

void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {I(Crc(format))}); }

void UpdateSatellite(int32_t which, int32_t value) { ev("UpdateSatellite", {I(which), I(value)}); }

CFSOnlineSystem* CFSOnlineSystem::Instance() {
    ev("Instance", {});
    return gOnlineSystem;
}

int32_t CFSOnlineSystem::AreWeProcessing() {
    ev("AreWeProcessing", {I(lab(this))});
    return static_cast<int32_t>(ret("processing"));
}

CFSOnlinePark* CFSOnlineSystemParkList_InCity::ReturnParkInfo(int32_t index) {
    ev("ReturnParkInfo", {I(lab(this)), I(index)});
    char* block = static_cast<char*>(tpw::ArenaAllocZeroed(0x100));
    *reinterpret_cast<int32_t*>(block + offsetof(CFSOnlinePark, mParkID)) = static_cast<int32_t>(ret("park", index, 0));
    return reinterpret_cast<CFSOnlinePark*>(block);
}

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("GetExtraParam", {I(lab(this)), I(index)});
    return reinterpret_cast<void*>(static_cast<intptr_t>(ret("extra")));
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {I(lab(this)), I(message), I(a), I(b)});
    return static_cast<int32_t>(ret("default"));
}

void CLBScreenComponentOnlineGlobe::GlobeLeft() { ev("GlobeLeft", {I(lab(this))}); }
void CLBScreenComponentOnlineGlobe::GlobeRight() { ev("GlobeRight", {I(lab(this))}); }
void CLBScreenComponentOnlineGlobe::GlobeUp() { ev("GlobeUp", {I(lab(this))}); }
void CLBScreenComponentOnlineGlobe::GlobeDown() { ev("GlobeDown", {I(lab(this))}); }
void CLBScreenComponentOnlineGlobe::GlobeZoomIn() { ev("GlobeZoomIn", {I(lab(this))}); }
void CLBScreenComponentOnlineGlobe::GlobeZoomOut() { ev("GlobeZoomOut", {I(lab(this))}); }

void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) {
    ev("AdvisorAdd", {I(lab(this)), I(response.mId), I(clearFirst)});
}

void FindParkOnlineParkInfoPanelCancelCallback() {}
void CreateOnlineParkFindPanel(void (*onCancel)()) {
    ev("CreateParkFind", {I(lab(reinterpret_cast<const void*>(onCancel)))});
}

// The text object the Mac builds on the stack: the constructor, Format (it writes the number into a buffer) and the destructor are
// fakes; the other virtual members are only here for the vtable.
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
    ev("DynCtor", {});
}
template<> void TbDynamicStringTemplate<char>::Format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int value = va_arg(args, int);
    va_end(args);
    ev("Format", {I(Crc(format)), I(value)});
    char* buffer = static_cast<char*>(tpw::ArenaAllocZeroed(16));
    std::snprintf(buffer, 16, "%d", value);
    mData = buffer;
}
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() { ev("DynDtor", {}); }
template<> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>&) {
    return *this;
}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

extern "C" {

uint64_t tpw_cs_addr(const char* name) {
    gVtable[8] = reinterpret_cast<void*>(&FakeShow);
    gVtable[9] = reinterpret_cast<void*>(&FakeHide);
    gVtable[11] = reinterpret_cast<void*>(&FakeSlot50);
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"wanted", &gChatSatelliteWanted},
        {"chat", &gChatSatelliteChat},
        {"finished", &gChatSatelliteFinished},
        {"answers", &gChatSatelliteAnswers},
        {"values", gChatSatelliteValues},
        {"ids", gChatSatelliteParkIDs},
        {"sysptr", &gOnlineSystem},
        {"screens", &gLobbyScreens},
        {"satparent", &gUISatelliteParent},
        {"vtable", gVtable},
        {"findcb", reinterpret_cast<const void*>(&FindParkOnlineParkInfoPanelCancelCallback)},
        {"chatcb", reinterpret_cast<const void*>(&ChatServerCallbackFn)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_cs_setcfg(const char* text, int32_t port) {
    gOnlineConfig.mChatServer = text;
    gOnlineConfig.mChatPort = static_cast<int16_t>(port);
}

int32_t tpw_cs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = I(offsetof(CFSOnlineSystem, mCityList));
    out[n++] = I(offsetof(CFSOnlineSystem, mParksInCity2));
    out[n++] = I(offsetof(CFSOnlineSystem, mThemeList));
    out[n++] = I(offsetof(CFSOnlineSystem, mServerList));
    out[n++] = I(offsetof(ParkListBase, mF28));
    out[n++] = I(offsetof(ParkListBase, mList));
    out[n++] = I(offsetof(ParkTable, mCount));
    out[n++] = I(offsetof(CFSOnlinePark, mParkID));
    out[n++] = I(offsetof(CLBScreenSystem, mThemeIslands));
    out[n++] = I(offsetof(CLBScreenSystem, mDownloadGlobe));
    out[n++] = I(offsetof(CLBScreenSystem, mPublishGlobe));
    out[n++] = I(offsetof(CLBScreenSystem, mAdvisor));
    out[n++] = I(offsetof(ArgumentVector, mSize));
    out[n++] = I(offsetof(ArgumentVector, mData));
    out[n++] = I(sizeof(Argument));
    return n;
}

void tpw_cs_proc() { ProcessGetChatSatellitePlayers(); }
void tpw_cs_cb(int32_t id, uint64_t vector) {
    ChatServerCallbackFn(id, nullptr, *reinterpret_cast<ArgumentVector*>(static_cast<uintptr_t>(vector)));
}
int32_t tpw_cs_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return WorldLobbyControllerMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
