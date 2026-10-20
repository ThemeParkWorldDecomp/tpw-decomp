// COnlineControl: IgnoreOff, IgnoreOn, BuddyOff, SendText, RemoveContact, RemoveBuddy, UpdateUserList (unit v5897).
// Strings the Mac keeps as one word are opaque buffers here; the callees (library code) are the online_callee functions.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include "online.h"
#include "UI_pump.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "platform/samsutils.h"
#include "UIInterface.h"
#include "platform/arena.h"
#include "MslSubscriptionTree.h"
#include "MslWStringTree.h"

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

// 0x1021ba28
void COnlineControl::RemoveContact(std::u16string& name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    online_callee::UIOnlineRemoveContact(&gUIInterface, name.c_str());
    reinterpret_cast<WStringTree*>(self + 0xe28)->erase_one(reinterpret_cast<const MslWideString&>(name));
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
