// COnlineControl::ProcessResponse 0x10215560 (8376 bytes, online.cpp; unit v5638): the chat server's responses. The first Argument's
// command number minus 0xb picks a case of the jump table at 0x102b1c44 (72 entries); the gaps and anything past 0x52 return at once.
// Every wide string is one word on the Mac (MSL's rep pointer, the text pointer at rep+0xc); here they are std::u16string objects the
// callees fill. The fields are reached by Mac offset (see online.h).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include "online.h"
#include "UI_pump.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "platform/samsutils.h"
#include "email.h"
#include "FS_Online.h"
#include "Text.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "UIInterface.h"
#include "Debug.h"
#include "std_basic_filebuf.h"
#include "InterfaceWindow.h"

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

uint32_t Word(const void* block, size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const uint8_t*>(block) + offset, 4);
    return word;
}

void* PointerAt(const void* object, size_t offset) { return online_callee::HandleToPointer(Word(object, offset)); }

// A signed halfword at a Mac offset (lha): the high byte first.
int32_t HalfAt(const void* block, size_t offset) {
    const uint8_t* p = static_cast<const uint8_t*>(block) + offset;
    return static_cast<int16_t>(static_cast<uint16_t>((p[0] << 8) | p[1]));
}

int32_t Halfword(MapPos pos) { return static_cast<int16_t>(static_cast<uint16_t>((pos.mCell << 8) | pos.mOffset)); }

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

CFSOnlineSystem* System() {
    if (gOnlineSystem == nullptr) {
        void* memory = online_callee::OperatorNew(0x404);
        CFSOnlineSystem* created = nullptr;
        if (memory != nullptr) created = new (memory) CFSOnlineSystem();
        gOnlineSystem = created;
    }
    return gOnlineSystem;
}

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
    if (person == nullptr || person == PointerAt(self, 4)) {
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
        CTRACE(3, kTraceTeleport, Halfword(x), Halfword(y));
    } else if (mode == 2) {
        peep->HideMySprite();
    } else if (mode == 3) {
        CTRACE(3, kTraceBeforeUnhide, HalfAt(person, 4), HalfAt(person, 6));
        peep->DrawMySpriteAt(0.0f, 0.0f, 0.0f, 0, static_cast<CDelayedScript::ScriptIdx>(3));
        SCR_SetFloatVariable(static_cast<int32_t>(Word(person, 0xc)), 8, 1.5f);
        self->Teleport(thing->GetThingId(), x, y);
        CTRACE(3, kTraceUnhide, Halfword(x), Halfword(y));
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
    if (park != static_cast<int32_t>(Word(info, 8))) {
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
