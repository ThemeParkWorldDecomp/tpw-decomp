// COnlineControl::DoCommand 0x10219bec (online.cpp, main app): runs one chat command, the vector of Argument the parser built (the first
// Argument's command number picks a case of the jump table at 0x102b1d68). Fields (Mac offsets; 32-bit words are arena offsets, Ptr32):
// 0x00 Chat*, 0x08 the player's name, 0x220 the feedback buffer (0x400 wide chars), 0xe20 the last tell target, 0xe24 the last teller,
// 0xe28 the contact set (first node word at 0xe30, end node this + 0xe2c), 0xe34 the buddy map (end this + 0xe38), 0xe44 the ignore set
// (end this + 0xe48), 0xe50 earmuffs byte, 0xe51 AFK byte, 0xe64 Vote*, 0xe84 goto-in-progress byte, 0xe88 the goto target.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "online.h"
#include "UI_pump.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "platform/samsutils.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "ui_ime.h"
#include "TbVTag.h"
#include "TbITable.h"
#include "Text.h"
#include "FS_Online.h"
#include "TPMain.h"
#include "FS_MasterSave.h"
#include "ui_ErrorDisplay.h"
#include "email.h"

// 0x102b1b58: the 16 bytes DoCommand copies to the stack and sends with Vote::SendVote.
uint32_t gVoteSendData[4] = {0x5c200a41u, 0x298111d3u, 0xadf400e0u, 0x18904fd5u};

namespace {

using Slot = uint64_t;   // a std::wstring (or nocase string) on the stack: one 32-bit word, 8 bytes kept

uint32_t Word(const void* block, size_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(block) + offset, 4);
    return value;
}

void SetWord(void* block, size_t offset, uint32_t value) { std::memcpy(static_cast<uint8_t*>(block) + offset, &value, 4); }

void* Ptr(uint32_t word) { return word != 0 ? static_cast<void*>(tpw::gArenaBase + word) : nullptr; }

uint32_t Addr32(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }

const IME::MslWideStringRep* Rep(const void* slot) { return static_cast<const IME::MslWideStringRep*>(Ptr(Word(slot, 0))); }

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

TbITable* Table() { return static_cast<TbITable*>(gText.mResourcePointers[19]); }   // gText + 0x598

}  // namespace

void COnlineControl::DoCommand(void* arguments) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    ArgumentVector* vector = static_cast<ArgumentVector*>(arguments);
    char16_t* buffer = reinterpret_cast<char16_t*>(self + 0x220);

    auto count = [&]() { return vector->mSize; };
    auto arg = [&](uint32_t i) -> const void* { return &vector->mData[i]; };
    auto chat = [&]() { return Word(self, 0); };
    // The buffer as a string, handed to Feedback.
    auto feed = [&]() {
        Slot text = 0;
        online_callee::WStringCtorFrom(&text, buffer);
        Feedback(AsString(text));
        online_callee::WStringDtor(&text, -1);
    };
    auto report = [&](uint32_t id) {
        Table()->FormatString(buffer, 0x400, id);
        feed();
    };
    auto reportWith = [&](uint32_t id, const char16_t* text) {
        TbUniStringVTag tag(0, text);
        Table()->FormatString(buffer, 0x400, id, &tag);
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
        online_callee::ChatSetHearingRange(Ptr(chat()), static_cast<const Argument*>(arg(1))->mCommand, this);
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
            Table()->FormatString(buffer, 0x400, 0x7c);
            online_callee::WStringCtorFrom(&text, buffer);
            docommand_callee::ChatSay(chat(), &text, false, this);
        } else {
            Table()->FormatString(buffer, 0x400, 0x7b);
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
            uint32_t node = Word(self, 0xe30);
            while (node != Addr32(self + 0xe2c)) {
                Slot other = 0;
                online_callee::NocaseWStringCopy(&other, static_cast<uint8_t*>(Ptr(node)) + 0xc, 0, 0xffffffffu);
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
                Table()->FormatString(buffer, 0x400, 0x7f, &tag);
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
        void* city = init_callee::ReturnCityInfo(Word(system, 0xe0), static_cast<int32_t>(Word(info, 0x48)));
        if (Word(city, 0x60) != 0) return;
        if (Word(self, 0xe64) != 0) {
            SamsUtilities::UDebug(kVoteBusy);
            return;
        }
        uint32_t data[4];
        std::memcpy(data, gVoteSendData, sizeof data);
        void* vote = online_callee::OperatorNew(0xc);
        if (vote != nullptr) docommand_callee::VoteCtor(vote);
        SetWord(self, 0xe64, Addr32(vote));
        if (Word(self, 0xe64) == 0) return;
        SamsUtilities::UDebug(kVoteCreated);
        online_callee::DisableVoteButton(true);
        int32_t connected = docommand_callee::VoteConnect(Ptr(Word(self, 0xe64)), const_cast<char*>(gOnlineConfig.mVoteServer.c_str()), nullptr,
                                                          static_cast<uint16_t>(gOnlineConfig.mVotePort), VoteNotifyCallback);
        if (connected != 1) return;
        SamsUtilities::UDebug(kVoteConnected);
        EnsureSystem();
        CFSOnlineSystem* forPlayer = EnsureSystem();
        CFSOnlineSystem* forCustomer = EnsureSystem();
        const char16_t* password = online_callee::LoginPassword(gOnlineSystem);
        int32_t customer = online_callee::LoginCustomerID(forCustomer);
        int32_t player = online_callee::LoginPlayerID(forPlayer);
        docommand_callee::VoteSetSessionInfo(Ptr(Word(self, 0xe64)), 9, player, customer, password);
        EnsureSystem();
        info = gOnlineSystem->GetFTPDownloadParkInfo();
        docommand_callee::VoteSendVote(Ptr(Word(self, 0xe64)), static_cast<int32_t>(Word(info, 8)), data, 1, nullptr);
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
