// COnlineParser: constructor, destructor and the four argument-less commands (unit v5131).
#include <cstdint>
#include <string>

#include "online.h"

// 0x10250a28; the Mac word there starts 0x00002173 (a zero first character).
char16_t gOnlineParserInitialText[16] = {0, 0x2173};

namespace {

constexpr uint32_t kVtableWord = 0x102b1b2c;   // TOC -0x5edc: the address stored at offset 0

// Pushes an Argument (empty string, empty wide string, command) and ignores the rest of the line.
void PushCommand(COnlineParser* self, uint32_t command) {
    uint32_t argument[3];   // 0 char string, 4 wide string, 8 command
    online_callee::StringCtor(&argument[0]);
    online_callee::WStringCtor(&argument[1]);
    argument[2] = command;
    online_callee::ArgumentVectorPushBack(self->mArguments, argument);
    online_callee::WStringDtor(&argument[1], -1);
    online_callee::StringDtor(&argument[0], -1);
    online_callee::ParserIgnoreRemainingString(self);
}

}  // namespace

void COnlineParser::ParseEarmuffs() { PushCommand(this, 0xc); }    // 0x1020faf8
void COnlineParser::ParseAfk() { PushCommand(this, 0x12); }        // 0x1021023c
void COnlineParser::ParseVote() { PushCommand(this, 0x15); }       // 0x10210648
void COnlineParser::ParseFilter() { PushCommand(this, 0x16); }     // 0x10210720

// 0x1020e400
COnlineParser* COnlineParser::Construct(COnlineParser* self, void* arguments, void* text) {
    uint8_t* bytes = reinterpret_cast<uint8_t*>(self);
    online_parser_callee::ParserCtor(self);
    *reinterpret_cast<uint32_t*>(bytes) = kVtableWord;
    online_parser_callee::ListCtor(bytes + 0x30);
    self->mArguments = arguments;
    self->mText = text;
    online_parser_callee::ArgumentVectorClear(self->mArguments);
    void* target = self->mText;
    uint32_t length = online_parser_callee::WcsLen(gOnlineParserInitialText);
    online_callee::WStringAssign(target, gOnlineParserInitialText, length);
    online_parser_callee::InitTokenList(self);
    return self;
}

// 0x102107fc
COnlineParser* COnlineParser::Destruct(COnlineParser* self, int16_t deleteFlag) {
    if (self != nullptr) {
        uint8_t* bytes = reinterpret_cast<uint8_t*>(self);
        *reinterpret_cast<uint32_t*>(bytes) = kVtableWord;
        online_parser_callee::ListClear(bytes + 0x30);
        online_parser_callee::ParserDtor(self, 0);
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
    return self;
}
