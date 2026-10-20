// COnlineParser::ParseBlackMark, ParseBuddy, ParsePage, ParseGoto, ParseLocate (main app, OnlinePerson.cpp).
#include <cstdint>
#include <string>

#include "online.h"

namespace {

// The five functions differ only in the command number stored in the first Argument.
bool ParseCommand(COnlineParser* self, uint32_t command) {
    uint32_t argument[3];    // Argument: char string, wide string, command
    uint32_t text;           // the wide string read from the line
    uint32_t copy;
    void* vector = self->mArguments;
    online_callee::StringCtor(&argument[0]);
    online_callee::WStringCtor(&argument[1]);
    argument[2] = command;
    online_callee::ArgumentVectorPushBack(vector, argument);
    online_callee::WStringDtor(&argument[1], -1);
    online_callee::StringDtor(&argument[0], -1);
    online_callee::WStringCtor(&text);
    if (!online_callee::ParserGetString(self, &text)) {
        online_callee::WStringDtor(&text, -1);
        return false;
    }
    online_callee::WStringCopy(&copy, &text, 0, 0xffffffffu);
    online_callee::ArgumentCtor(argument, &copy);
    online_callee::ArgumentVectorPushBack(vector, argument);
    online_callee::WStringDtor(&argument[1], -1);
    online_callee::StringDtor(&argument[0], -1);
    online_callee::WStringDtor(&copy, -1);
    const bool result = online_callee::ParserIgnoreRemainingString(self);
    online_callee::WStringDtor(&text, -1);
    return result;
}

}  // namespace

bool COnlineParser::ParseBlackMark() { return ParseCommand(this, 0xe); }    // 0x1020fd70
bool COnlineParser::ParseBuddy() { return ParseCommand(this, 0xf); }       // 0x1020ff0c
bool COnlineParser::ParsePage() { return ParseCommand(this, 0x11); }       // 0x102100a4
bool COnlineParser::ParseGoto() { return ParseCommand(this, 0x13); }       // 0x10210314
bool COnlineParser::ParseLocate() { return ParseCommand(this, 0x14); }     // 0x102104ac
