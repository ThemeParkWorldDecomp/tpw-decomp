// COnlineParser (OnlinePerson.cpp): InitToken 0x1020ed70, InitTokenList 0x1020e898, ParseSay 0x1020ef70, ParseTell 0x1020f0cc.
#include <cstdint>
#include <cstring>

#include "TbITable.h"
#include "TbResource.h"
#include "Text.h"
#include "online.h"

namespace {
// The Mac's stack objects are opaque words here: strings are one word, an Argument is three, a CToken four.
const uint32_t kNpos = 0xffffffffu;
}

// 0x1020ed70: formats string `id` of the UI table (no NULL check on the table), converts it to UTF-16 when the multibyte converter
// exists (otherwise the wide buffer is read uninitialised, as in the original), and inserts a CToken built from it before the node
// held in the word at this+0x38.
void COnlineParser::InitToken(uint32_t id, ParserHandler handler) {
    unsigned char narrow[256];
    char16_t wide[134] = {};   // the original leaves it uninitialised (zero in the emulator); zeroed so the host is deterministic
    uint32_t s68[1];
    uint32_t s50[1];
    uint32_t token[4];

    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[19]);
    table->FormatString(narrow, 0x80, id);
    TbIMBToUnicode* converter = static_cast<TbIMBToUnicode*>(gText.mResourcePointers[0]);
    if (converter != nullptr) converter->ConvertString(wide, narrow, 0x80);
    online_callee::WStringCtorFrom(s68, wide);
    online_callee::WStringCopy(s50, s68, 0, kNpos);
    token_callee::CTokenCtor(token, s50, handler.mWords[0], handler.mWords[1], handler.mWords[2]);
    uint32_t position;
    std::memcpy(&position, reinterpret_cast<uint8_t*>(this) + 0x38, 4);
    token_list::TokenListNode* result;
    token_list::TokenList_insert(&result, reinterpret_cast<token_list::TokenList*>(reinterpret_cast<uint8_t*>(this) + 0x30),
                                 reinterpret_cast<token_list::TokenListNode*>(static_cast<uintptr_t>(position)),
                                 reinterpret_cast<const CToken*>(token));
    online_callee::WStringDtor(token, -1);
    online_callee::WStringDtor(s50, -1);
    online_callee::WStringDtor(s68, -1);
}

// 0x1020e898
void COnlineParser::InitTokenList() {
    static const uint8_t ids[36] = {0x00, 0x2b, 0x01, 0x2c, 0x02, 0x2d, 0x03, 0x2e, 0x04, 0x2f, 0x05, 0x30,
                                    0x06, 0x31, 0x0c, 0x37, 0x0d, 0x38, 0x0e, 0x39, 0x0f, 0x3a, 0x10, 0x3b,
                                    0x12, 0x3d, 0x14, 0x3f, 0x15, 0x40, 0x13, 0x3e, 0x16, 0x41, 0x17, 0x42};
    for (int i = 0; i < 36; ++i) InitToken(ids[i], gOnlineTokenHandlers[i]);
}

// 0x1020ef70
bool COnlineParser::ParseSay() {
    uint32_t arg[3];
    online_callee::StringCtor(&arg[0]);
    online_callee::WStringCtor(&arg[1]);
    arg[2] = 0;
    online_callee::ArgumentVectorPushBack(mArguments, arg);
    online_callee::WStringDtor(&arg[1], -1);
    online_callee::StringDtor(&arg[0], -1);
    if (!token_callee::ParserGetRemainingString(this, mText, nullptr)) return false;
    uint32_t ws[1];
    online_callee::WStringCopy(ws, mText, 0, kNpos);
    uint32_t arg2[3];
    online_callee::ArgumentCtor(arg2, ws);
    online_callee::ArgumentVectorPushBack(mArguments, arg2);
    online_callee::WStringDtor(&arg2[1], -1);
    online_callee::StringDtor(&arg2[0], -1);
    online_callee::WStringDtor(ws, -1);
    return true;
}

// 0x1020f0cc: a failed GetString answers true (the original does).
bool COnlineParser::ParseTell() {
    uint32_t arg[3];
    online_callee::StringCtor(&arg[0]);
    online_callee::WStringCtor(&arg[1]);
    arg[2] = 1;
    online_callee::ArgumentVectorPushBack(mArguments, arg);
    online_callee::WStringDtor(&arg[1], -1);
    online_callee::StringDtor(&arg[0], -1);
    uint32_t name[1];
    online_callee::WStringCtor(name);
    if (!online_callee::ParserGetString(this, name)) {
        online_callee::WStringDtor(name, -1);
        return true;
    }
    uint32_t ws[1];
    online_callee::WStringCopy(ws, name, 0, kNpos);
    uint32_t arg2[3];
    online_callee::ArgumentCtor(arg2, ws);
    online_callee::ArgumentVectorPushBack(mArguments, arg2);
    online_callee::WStringDtor(&arg2[1], -1);
    online_callee::StringDtor(&arg2[0], -1);
    online_callee::WStringDtor(ws, -1);
    if (!token_callee::ParserGetRemainingString(this, mText, nullptr)) {
        online_callee::WStringDtor(name, -1);
        return false;
    }
    uint32_t ws2[1];
    online_callee::WStringCopy(ws2, mText, 0, kNpos);
    uint32_t arg3[3];
    online_callee::ArgumentCtor(arg3, ws2);
    online_callee::ArgumentVectorPushBack(mArguments, arg3);
    online_callee::WStringDtor(&arg3[1], -1);
    online_callee::StringDtor(&arg3[0], -1);
    online_callee::WStringDtor(ws2, -1);
    online_callee::WStringDtor(name, -1);
    return true;
}
