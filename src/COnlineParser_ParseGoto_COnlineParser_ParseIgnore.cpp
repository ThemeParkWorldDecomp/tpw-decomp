#include <cstdint>
#include <string>
#include "online.h"

namespace {
const uint32_t kNposIgnore = 0xffffffffu;

// Builds the temporary Argument (string, wstring, command) and pushes it on the vector, then destroys it.
void PushCommand(void* vector, uint32_t command) {
    uint32_t arg[3];
    online_callee::StringCtor(&arg[0]);
    online_callee::WStringCtor(&arg[1]);
    arg[2] = command;
    online_callee::ArgumentVectorPushBack(vector, arg);
    online_callee::WStringDtor(&arg[1], -1);
    online_callee::StringDtor(&arg[0], -1);
}

// Copies the wide string, wraps it in an Argument, pushes it, destroys the temporaries.
void PushText(void* vector, const void* text) {
    uint32_t ws[1];
    online_callee::WStringCopy(ws, text, 0, kNposIgnore);
    uint32_t arg[3];
    online_callee::ArgumentCtor(arg, ws);
    online_callee::ArgumentVectorPushBack(vector, arg);
    online_callee::WStringDtor(&arg[1], -1);
    online_callee::StringDtor(&arg[0], -1);
    online_callee::WStringDtor(ws, -1);
}
}  // namespace

// 0x1020fbd4
bool COnlineParser::ParseIgnore() {
    PushCommand(mArguments, 0xd);
    uint32_t name[1];
    online_callee::WStringCtor(name);
    if (!online_callee::ParserGetString(this, name)) {
        online_callee::WStringDtor(name, -1);
        return false;
    }
    PushText(mArguments, name);
    bool result = online_callee::ParserIgnoreRemainingString(this);
    online_callee::WStringDtor(name, -1);
    return result;
}

// 0x1020e608: the std::wstring parameter is passed by address (CodeWarrior by-value struct).
void COnlineParser::DoParse(const void* text) {
    uint8_t props[3] = {0, 1, 1};
    {
        uint32_t copy[1];
        online_callee::WStringCopy(copy, text, 0, kNposIgnore);
        online_parse_callee::ParserSetup(this, copy, &mF00[0x30], props);
        online_callee::WStringDtor(copy, -1);
    }
    online_parse_callee::ParserSkipWhitespace(this);
    char16_t ch;
    if (online_parse_callee::ParserGetChar(this, &ch) && ch == 0x2f) {
        if (online_parse_callee::ParserParse(this) == 0) PushCommand(mArguments, 0x18);
    } else {
        online_parse_callee::WStringAssignRange(mText, text, 0, kNposIgnore);
        PushCommand(mArguments, 0);
        PushText(mArguments, text);
    }
}
