#include <cstdint>
#include <string>
#include "online.h"

namespace {

// Pushes an Argument that carries only the command number (the common start of every command).
void PushCommand(void* vec, uint32_t command) {
    uint32_t arg[3];
    online_callee::StringCtor(&arg[0]);
    online_callee::WStringCtor(&arg[1]);
    arg[2] = command;
    online_callee::ArgumentVectorPushBack(vec, arg);
    online_callee::WStringDtor(&arg[1], -1);
    online_callee::StringDtor(&arg[0], -1);
}

// Pushes an Argument built from a copy of the whole wide string `source`.
void PushText(void* vec, const void* source) {
    uint32_t ws[1];
    online_callee::WStringCopy(ws, source, 0, 0xffffffffu);
    uint32_t arg[3];
    online_callee::ArgumentCtor(arg, ws);
    online_callee::ArgumentVectorPushBack(vec, arg);
    online_callee::WStringDtor(&arg[1], -1);
    online_callee::StringDtor(&arg[0], -1);
    online_callee::WStringDtor(ws, -1);
}

// The command, then the rest of the line as a second Argument.
bool ParseRest(COnlineParser* self, void* vec, void* text, uint32_t command) {
    PushCommand(vec, command);
    if (!token_callee::ParserGetRemainingString(self, text, nullptr)) return false;
    PushText(vec, text);
    return true;
}

}  // namespace

// 0x1020f2f0: command 2; the rest is pushed only when the count GetRemainingString reports is not 0 (true either way).
bool COnlineParser::ParseReply() {
    PushCommand(mArguments, 2);
    int32_t count;
    if (!token_callee::ParserGetRemainingString(this, mText, &count)) return false;
    if (count == 0) return true;
    PushText(mArguments, mText);
    return true;
}

// 0x1020f460: as ParseReply with command 3.
bool COnlineParser::ParseLast() {
    PushCommand(mArguments, 3);
    int32_t count;
    if (!token_callee::ParserGetRemainingString(this, mText, &count)) return false;
    if (count == 0) return true;
    PushText(mArguments, mText);
    return true;
}

// 0x1020f5d0: command 4, then the rest of the line.
bool COnlineParser::ParseShout() { return ParseRest(this, mArguments, mText, 4); }

// 0x1020f72c: command 5, then the rest of the line.
bool COnlineParser::ParseEmote() { return ParseRest(this, mArguments, mText, 5); }

// 0x1020f888: command 6, one word (or the initial text when GetString fails), then the rest of the line.
bool COnlineParser::ParseEmoteTell() {
    PushCommand(mArguments, 6);
    uint32_t name[1];
    online_callee::WStringCtor(name);
    if (online_callee::ParserGetString(this, name)) {
        PushText(mArguments, name);
    } else {
        uint32_t ws[1];
        online_callee::WStringCtorFrom(ws, gOnlineParserInitialText);
        uint32_t arg[3];
        online_callee::ArgumentCtor(arg, ws);
        online_callee::ArgumentVectorPushBack(mArguments, arg);
        online_callee::WStringDtor(&arg[1], -1);
        online_callee::StringDtor(&arg[0], -1);
        online_callee::WStringDtor(ws, -1);
    }
    if (!token_callee::ParserGetRemainingString(this, mText, nullptr)) {
        online_callee::WStringDtor(name, -1);
        return false;
    }
    PushText(mArguments, mText);
    online_callee::WStringDtor(name, -1);
    return true;
}
