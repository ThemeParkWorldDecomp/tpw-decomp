// Test library for COnlineControl::DoCommand (unit v5636; libtpw_unit_COnlineControl_DoCommand.dylib, tests/test_COnlineControl_DoCommand.py),
// main app:
//   src/game/online/online.cpp   DoCommand 0x10219bec
//   and gVoteSendData
//
// The callees outside the unit are fakes that record what the real ones read (strings as length and hash, pointers as lab(p)) and answer
// from the test's queues or the blocks the test set (tpw_dc_set). A std::wstring is one 32-bit word, the arena offset of an
// IME::MslWideStringRep; the string fakes share reps (BStrVal, assign and the nocase copy store the source's rep). The globals of other
// files the unit reads (gText, gOnlineSystem, gMasterSave, gOnlineConfig) are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "online.h"
#include "ui_ime.h"
#include "TbVTag.h"
#include "TbITable.h"
#include "Text.h"
#include "FS_Online.h"
#include "TPMain.h"
#include "FS_MasterSave.h"
#include "ui_ErrorDisplay.h"
#include "platform/samsutils.h"
#include "email.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
// gText as zeroed storage under its symbol (CText's members' constructors live in files this library does not compile; only
// mResourcePointers[19] is read, set by tpw_dc_set).
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
CFSOnlineSystem* gOnlineSystem;
CFSGlobalMasterSave* gMasterSave;
OnlineConfig::OnlineConfig() {}
OnlineConfig gOnlineConfig;

namespace {

void* gPark;       // GetFTPDownloadParkInfo's answer
void* gCity;       // ReturnCityInfo's answer
void* gPassword;   // GetLoginPassword's answer
void* gFound;      // the tree finds' node when the queue says found

uint32_t Word(const void* block, size_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(block) + offset, 4);
    return value;
}

void* Ptr(uint32_t word) { return word != 0 ? static_cast<void*>(tpw::gArenaBase + word) : nullptr; }

uint32_t Addr32(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }

int64_t Fnv(const char16_t* text, uint32_t n) {
    uint32_t h = 0x811c9dc5u;
    for (uint32_t i = 0; i < n; ++i) {
        h ^= static_cast<uint16_t>(text[i]);
        h *= 16777619u;
    }
    return h & 0x7fffffff;
}

int64_t FnvBytes(const char* text, uint32_t n) {
    uint32_t h = 0x811c9dc5u;
    for (uint32_t i = 0; i < n; ++i) {
        h ^= static_cast<uint8_t>(text[i]);
        h *= 16777619u;
    }
    return h & 0x7fffffff;
}

const IME::MslWideStringRep* RepOf(const void* slot) { return static_cast<const IME::MslWideStringRep*>(Ptr(Word(slot, 0))); }

int64_t SigLen(const void* slot) { return RepOf(slot)->mLength; }

int64_t SigHash(const void* slot) {
    const IME::MslWideStringRep* rep = RepOf(slot);
    return Fnv(rep->mData, rep->mLength < 32 ? rep->mLength : 32);
}

uint32_t WLen(const char16_t* text) {
    uint32_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}

int64_t CLen(const char* text) { return static_cast<int64_t>(std::strlen(text)); }

int64_t CHash(const char* text) {
    uint32_t n = static_cast<uint32_t>(std::strlen(text));
    return FnvBytes(text, n < 32 ? n : 32);
}

void SetSlot(void* slot, uint32_t word) { std::memcpy(slot, &word, 4); }

int64_t Chat(uint32_t chat) { return lab(Ptr(chat)); }

}  // namespace

// ---- the fakes
void* online_callee::OperatorNew(uint32_t size) {
    if (size == 0xc && ret("VoteNull") != 0) {
        ev("new", {size, 0});
        return nullptr;
    }
    void* p = tpw::ArenaAllocZeroed(size);
    ev("new", {size, newlab(p)});
    return p;
}

void docommand_callee::ArgumentBStrVal(void* result, const void* argument) {
    ev("BStrVal", {lab(argument)});
    SetSlot(result, Word(argument, 4));
}

void online_callee::WStringCtorFrom(void* self, const char16_t* text) {
    uint32_t n = WLen(text);
    ev("WCtor", {lab(text), n, Fnv(text, n < 32 ? n : 32)});
    auto* rep = static_cast<IME::MslWideStringRep*>(tpw::ArenaAllocZeroed(sizeof(IME::MslWideStringRep)));
    rep->mLength = n;
    rep->mCapacity = n;
    rep->mRefs = 1;
    rep->mData = const_cast<char16_t*>(text);
    SetSlot(self, Addr32(rep));
}

void online_callee::WStringDtor(void* self, int16_t flag) { ev("WDtor", {SigLen(self), SigHash(self), flag}); }

void online_parse_callee::WStringAssignRange(void* self, const void* src, uint32_t pos, uint32_t count) {
    ev("Assign", {lab(self), SigLen(src), SigHash(src), pos, static_cast<int32_t>(count)});
    SetSlot(self, Word(src, 0));
}

void online_callee::NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("NCopy", {lab(other), SigLen(other), SigHash(other), pos, static_cast<int32_t>(count)});
    SetSlot(self, Word(other, 0));
}

void online_callee::NocaseWStringDtor(void* self, int16_t flag) { ev("NDtor", {SigLen(self), SigHash(self), flag}); }

bool online_callee::NocaseWStringEqual(const void* a, const void* b) {
    ev("Equal", {SigLen(a), SigHash(a), SigLen(b), SigHash(b)});
    return ret("Equal") != 0;
}

void* online_callee::NocaseSetFind(void* tree, const void* key) {
    ev("SetFind", {lab(tree), SigLen(key), SigHash(key)});
    return ret("SetFind") != 0 ? gFound : static_cast<uint8_t*>(tree) + 4;
}

void* online_callee::BuddyTreeFind(void* tree, const void* key) {
    ev("BuddyFind", {lab(tree), SigLen(key), SigHash(key)});
    return ret("BuddyFind") != 0 ? gFound : static_cast<uint8_t*>(tree) + 4;
}

void ignore_callee::TreeIncrement(uint32_t* node) {
    void* n = Ptr(*node);
    ev("Inc", {lab(n)});
    *node = Word(n, 8);
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("Format", {lab(this), lab(out), size, id});
    if (id == 0x56 || id == 0x57 || id == 0x7f) {
        va_list ap;
        va_start(ap, id);
        TbVTag* tag = va_arg(ap, TbVTag*);
        va_end(ap);
        TbUniStringVTag* text = dynamic_cast<TbUniStringVTag*>(tag);
        const char16_t* t = static_cast<TbUniStringVTag*>(tag)->mText;
        uint32_t n = WLen(t);
        ev("VTag", {tag->mMagic, text != nullptr ? 1 : 0, tag->mId, n, Fnv(t, n < 32 ? n : 32)});
    }
    out[0] = static_cast<char16_t>(0x41 + id % 26);
    out[1] = static_cast<char16_t>(0x61 + (id >> 3) % 26);
    out[2] = 0;
    return 0;
}

int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

void COnlineControl::Feedback(std::u16string& text) { ev("Feedback", {lab(this), SigLen(&text), SigHash(&text)}); }

void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {CLen(format), CHash(format)}); }

InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton b1, dlgcallback c1, dlgbutton b2, dlgcallback c2, dlgbutton b3, dlgcallback c3,
                              bool flag) {
    ev("Dialog", {lab(params.mText), params.mRed, params.mGreen, params.mBlue, params.mValue8, params.mValueC});
    ev("DialogB", {static_cast<int32_t>(b1), lab(reinterpret_cast<const void*>(c1)), static_cast<int32_t>(b2),
                   lab(reinterpret_cast<const void*>(c2)), static_cast<int32_t>(b3), lab(reinterpret_cast<const void*>(c3)), flag ? 1 : 0});
    return nullptr;
}

int32_t GotoPark_OK(void*) { return 0; }
void VoteNotifyCallback(int32_t, int32_t, void*, void*) {}

void docommand_callee::ChatNotAFK(uint32_t chat, void* context) { ev("NotAFK", {Chat(chat), lab(context)}); }
void docommand_callee::ChatSay(uint32_t chat, const void* text, bool flag, void* context) {
    ev("Say", {Chat(chat), SigLen(text), SigHash(text), flag ? 1 : 0, lab(context)});
}
void docommand_callee::ChatTell(uint32_t chat, const void* name, const void* text, bool flag, void* context) {
    ev("Tell", {Chat(chat), lab(name), SigLen(name), SigHash(name), SigLen(text), SigHash(text), flag ? 1 : 0, lab(context)});
}
void docommand_callee::ChatRoomShout(uint32_t chat, const void* text, bool flag, void* context) {
    ev("Shout", {Chat(chat), SigLen(text), SigHash(text), flag ? 1 : 0, lab(context)});
}
void online_callee::ChatSetHearingRange(void* chat, uint32_t range, void* context) { ev("Range", {lab(chat), range, lab(context)}); }
void docommand_callee::ChatEarmuffsOff(uint32_t chat, void* context) { ev("EarOff", {Chat(chat), lab(context)}); }
void docommand_callee::ChatEarmuffsOn(uint32_t chat, void* context) { ev("EarOn", {Chat(chat), lab(context)}); }
void docommand_callee::ChatAFK(uint32_t chat, void* context) { ev("AFK", {Chat(chat), lab(context)}); }
void docommand_callee::ChatUnIgnore(uint32_t chat, const void* name, void* context) {
    ev("UnIgnore", {Chat(chat), SigLen(name), SigHash(name), lab(context)});
}
void ignore_callee::ChatIgnore(uint32_t chat, const IgnoreWString* name, COnlineControl* context) {
    ev("Ignore", {Chat(chat), SigLen(name), SigHash(name), lab(context)});
}
void docommand_callee::ChatBlackmark(uint32_t chat, const void* name, void* context) {
    ev("Blackmark", {Chat(chat), SigLen(name), SigHash(name), lab(context)});
}
void docommand_callee::ChatRemoveBuddy(uint32_t chat, const void* name, void* context) {
    ev("RemoveBuddy", {Chat(chat), SigLen(name), SigHash(name), lab(context)});
}
void docommand_callee::ChatAddBuddy(uint32_t chat, const void* name, void* context) {
    ev("AddBuddy", {Chat(chat), SigLen(name), SigHash(name), lab(context)});
}
void docommand_callee::ChatPage(uint32_t chat, const void* name, void* context) {
    ev("Page", {Chat(chat), SigLen(name), SigHash(name), lab(context)});
}
void docommand_callee::ChatGoto(uint32_t chat, const void* name, void* context) {
    ev("Goto", {Chat(chat), SigLen(name), SigHash(name), lab(context)});
}
void docommand_callee::ChatLocate(uint32_t chat, const void* name, void* context) {
    ev("Locate", {Chat(chat), SigLen(name), SigHash(name), lab(context)});
}

void docommand_callee::OnlineSystemCtor(void* self) { ev("SysCtor", {lab(self)}); }
CFSFTPParkInfo* CFSOnlineSystem::GetFTPDownloadParkInfo() {
    ev("GetFTP", {lab(this)});
    return static_cast<CFSFTPParkInfo*>(gPark);
}
void* init_callee::ReturnCityInfo(uint32_t cityList, int32_t id) {
    ev("CityInfo", {cityList, id});
    return gCity;
}
const char16_t* online_callee::LoginPassword(CFSOnlineSystem* sys) {
    ev("Password", {lab(sys)});
    return static_cast<const char16_t*>(gPassword);
}
int32_t online_callee::LoginCustomerID(CFSOnlineSystem* sys) {
    ev("CustomerID", {lab(sys)});
    return static_cast<int32_t>(ret("CustomerID"));
}
int32_t online_callee::LoginPlayerID(CFSOnlineSystem* sys) {
    ev("PlayerID", {lab(sys)});
    return static_cast<int32_t>(ret("PlayerID"));
}
void docommand_callee::VoteCtor(void* self) { ev("VoteCtor", {lab(self)}); }
void online_callee::DisableVoteButton(bool flag) { ev("DisableVote", {flag ? 1 : 0}); }
int32_t docommand_callee::VoteConnect(void* vote, char* server, char* extra, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*)) {
    ev("Connect", {lab(vote), CLen(server), CHash(server), lab(extra), port, lab(reinterpret_cast<const void*>(callback))});
    return static_cast<int32_t>(ret("Connect"));
}
void docommand_callee::VoteSetSessionInfo(void* vote, int16_t kind, int32_t playerId, int32_t customerId, const char16_t* password) {
    ev("Session", {lab(vote), kind, playerId, customerId, lab(password)});
}
void docommand_callee::VoteSendVote(void* vote, int32_t id, void* data, int32_t count, void* extra) {
    ev("SendVote", {lab(vote), id, Word(data, 0), Word(data, 4), Word(data, 8), Word(data, 12), count, lab(extra)});
}
void online_callee::MasterSaveCtor(void* self) { ev("MSCtor", {lab(self)}); }
bool CFSGlobalMasterSave::IsSwearFilterOn() const {
    ev("IsSwear", {lab(this)});
    return (ret("Swear") & 0xff) != 0;
}
void CFSGlobalMasterSave::SetSwearFilter(bool on) { ev("SetSwear", {lab(this), on ? 1 : 0}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_dc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem},
        {"gMasterSave", &gMasterSave},
        {"gVoteSendData", gVoteSendData},
        {"GotoPark_OK", reinterpret_cast<const void*>(&GotoPark_OK)},
        {"VoteNotifyCallback", reinterpret_cast<const void*>(&VoteNotifyCallback)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (arena memory: 32-bit words are offsets from the arena base).
uint64_t tpw_dc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_dc_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

// The blocks the fakes answer with, and gText's string table.
void tpw_dc_set(const char* name, uint64_t value) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(value));
    if (std::strcmp(name, "park") == 0) gPark = p;
    if (std::strcmp(name, "city") == 0) gCity = p;
    if (std::strcmp(name, "password") == 0) gPassword = p;
    if (std::strcmp(name, "found") == 0) gFound = p;
    if (std::strcmp(name, "table") == 0) gText.mResourcePointers[19] = p;
}

void tpw_dc_set_server(const char* server, int32_t port) {
    gOnlineConfig.mVoteServer = server;
    gOnlineConfig.mVotePort = static_cast<int16_t>(port);
}

int64_t tpw_dc_lab(uint64_t p) { return lab(reinterpret_cast<const void*>(static_cast<uintptr_t>(p))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_dc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IME::MslWideStringRep, mData));
    out[n++] = static_cast<int64_t>(sizeof(IME::MslWideStringRep));
    out[n++] = static_cast<int64_t>(offsetof(ArgumentVector, mSize));
    out[n++] = static_cast<int64_t>(offsetof(ArgumentVector, mData));
    out[n++] = static_cast<int64_t>(sizeof(Argument));
    out[n++] = static_cast<int64_t>(offsetof(Argument, mWString));
    out[n++] = static_cast<int64_t>(offsetof(Argument, mCommand));
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    return n;
}

void tpw_dc_call(uint64_t self, uint64_t vec) {
    reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(self))->DoCommand(reinterpret_cast<void*>(static_cast<uintptr_t>(vec)));
}

}  // extern "C"
