// OnlineParser.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of OnlineParser code, named after it.

#define TPW_V51039_STRONG
#include "analyser.h"
#include "FS_MasterSave.h"
#include "FS_OnlineList.h"
#include "helpers/InterfaceFontHelpers.h"
#include "HLSprite.h"
#include "InterfaceFontWrapper.h"
#include "online.h"
#include "platform/arena.h"
#include "Serialise.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <new>
#include <string>

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

// COnlineParser (OnlinePerson.cpp): InitToken 0x1020ed70, InitTokenList 0x1020e898, ParseSay 0x1020ef70, ParseTell 0x1020f0cc.

namespace {
// The Mac's stack objects are opaque words here: strings are one word, an Argument is three, a CToken four.
const uint32_t kNpos = 0xffffffffu;
}

// 0x1020e898
void COnlineParser::InitTokenList() {
    static const uint8_t ids[36] = {0x00, 0x2b, 0x01, 0x2c, 0x02, 0x2d, 0x03, 0x2e, 0x04, 0x2f, 0x05, 0x30,
                                    0x06, 0x31, 0x0c, 0x37, 0x0d, 0x38, 0x0e, 0x39, 0x0f, 0x3a, 0x10, 0x3b,
                                    0x12, 0x3d, 0x14, 0x3f, 0x15, 0x40, 0x13, 0x3e, 0x16, 0x41, 0x17, 0x42};
    for (int i = 0; i < 36; ++i) InitToken(ids[i], gOnlineTokenHandlers[i]);
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

namespace {

// Pushes an Argument that carries only the command number (the common start of every command).
void PushCommand_2(void* vec, uint32_t command) {
    uint32_t arg[3];
    online_callee::StringCtor(&arg[0]);
    online_callee::WStringCtor(&arg[1]);
    arg[2] = command;
    online_callee::ArgumentVectorPushBack(vec, arg);
    online_callee::WStringDtor(&arg[1], -1);
    online_callee::StringDtor(&arg[0], -1);
}

// Pushes an Argument built from a copy of the whole wide string `source`.
void PushText_2(void* vec, const void* source) {
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
    PushCommand_2(vec, command);
    if (!token_callee::ParserGetRemainingString(self, text, nullptr)) return false;
    PushText_2(vec, text);
    return true;
}

}  // namespace

// 0x1020f2f0: command 2; the rest is pushed only when the count GetRemainingString reports is not 0 (true either way).
bool COnlineParser::ParseReply() {
    PushCommand_2(mArguments, 2);
    int32_t count;
    if (!token_callee::ParserGetRemainingString(this, mText, &count)) return false;
    if (count == 0) return true;
    PushText_2(mArguments, mText);
    return true;
}

// 0x1020f460: as ParseReply with command 3.
bool COnlineParser::ParseLast() {
    PushCommand_2(mArguments, 3);
    int32_t count;
    if (!token_callee::ParserGetRemainingString(this, mText, &count)) return false;
    if (count == 0) return true;
    PushText_2(mArguments, mText);
    return true;
}

// 0x1020f5d0: command 4, then the rest of the line.
bool COnlineParser::ParseShout() { return ParseRest(this, mArguments, mText, 4); }

// 0x1020f72c: command 5, then the rest of the line.
bool COnlineParser::ParseEmote() { return ParseRest(this, mArguments, mText, 5); }

// 0x1020f888: command 6, one word (or the initial text when GetString fails), then the rest of the line.
bool COnlineParser::ParseEmoteTell() {
    PushCommand_2(mArguments, 6);
    uint32_t name[1];
    online_callee::WStringCtor(name);
    if (online_callee::ParserGetString(this, name)) {
        PushText_2(mArguments, name);
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
    PushText_2(mArguments, mText);
    online_callee::WStringDtor(name, -1);
    return true;
}

// COnlineParser: constructor, destructor and the four argument-less commands (unit v5131).

// 0x10250a28; the Mac word there starts 0x00002173 (a zero first character).
char16_t gOnlineParserInitialText[16] = {0, 0x2173};

namespace {

constexpr uint32_t kVtableWord = 0x102b1b2c;   // TOC -0x5edc: the address stored at offset 0

// Pushes an Argument (empty string, empty wide string, command) and ignores the rest of the line.
void PushCommand_3(COnlineParser* self, uint32_t command) {
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

void COnlineParser::ParseEarmuffs() { PushCommand_3(this, 0xc); }    // 0x1020faf8

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

// COnlineParser::ParseBlackMark, ParseBuddy, ParsePage, ParseGoto, ParseLocate (main app, OnlinePerson.cpp).

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

void COnlineParser::ParseAfk() { PushCommand_3(this, 0x12); }        // 0x1021023c

bool COnlineParser::ParseGoto() { return ParseCommand(this, 0x13); }       // 0x10210314
bool COnlineParser::ParseLocate() { return ParseCommand(this, 0x14); }     // 0x102104ac

void COnlineParser::ParseVote() { PushCommand_3(this, 0x15); }       // 0x10210648
void COnlineParser::ParseFilter() { PushCommand_3(this, 0x16); }     // 0x10210720

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

// Unit v5956: std::vector<Argument>::clear / tear_down, CToken copy and (string, member pointer) constructors, CParser::GetChar,
// COnlinePerson::COnlinePerson and std::list<std::wstring>::insert.

namespace {
// 0x10250c90 (TOC -0x3a24): the wide format of the person's name; the first word of the original is 0x0025006c.
const char16_t kPersonNameFormat[] = u"%ls";
// 0x10250784 and 0x10250870 (reached through the TOC bases 0x10250722 and 0x10250846 with +0x62 and +0x2a)
const char kInsertMessage[] = "list::insert length error";
const char kLogicLengthType[] = "!std::exception!!std::logic_error!!std::length_error!!";
}  // namespace

// 0x102108a4: destroy every element, then forget them (the storage stays)
void online_parser_callee::ArgumentVectorClear(void* vec) {
    ArgumentVector* v = static_cast<ArgumentVector*>(vec);
    char* first = reinterpret_cast<char*>(v->mData);
    std_destroy::DestroyRangeClear(v->mData, reinterpret_cast<Argument*>(first + v->mSize * 12u));
    v->mSize = 0;
}

// 0x10210d4c: copy the string, then the three words
CToken::CToken(const CToken& other) {
    online_callee::WStringCopy(this, &other, 0, 0xffffffffu);
    mWords[1] = other.mWords[1];
    mWords[2] = other.mWords[2];
    mWords[3] = other.mWords[3];
}

// 0x10211f88: copy the string, then store the pointer to member function's three words
void token_callee::CTokenCtor(void* self, const void* name, uint32_t w0, uint32_t w1, uint32_t w2) {
    CToken* token = static_cast<CToken*>(self);
    online_callee::WStringCopy(self, name, 0, 0xffffffffu);
    token->mWords[1] = w0;
    token->mWords[2] = w1;
    token->mWords[3] = w2;
}

// 0x10211ef8
bool online_parse_callee::ParserGetChar(void* parser, char16_t* ch) {
    CParser* p = static_cast<CParser*>(parser);
    uint32_t pos = p->mPos;
    uint32_t length = *static_cast<const uint32_t*>(p->mString);
    if (!(pos < length)) return false;
    p->mPos = pos + 1;
    const char16_t* c = IME::MslWideString_index(reinterpret_cast<IME::MslWideString*>(&p->mString), pos);
    *ch = *c;
    return true;
}

// 0x10212d98: the object is a block of Mac offsets (+0x0c CDelayedScript, +0x20 sprite, +0x24 set, +0x188, +0x19c name, +0xc2, +0x3a0, +0x3a4)
void online_callee::COnlinePersonCtor(void* self, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, uint32_t avatar, void* name,
                                      bool flag) {
    uint8_t* bytes = static_cast<uint8_t*>(self);
    auto word = [bytes](uint32_t off) -> int32_t& { return *reinterpret_cast<int32_t*>(bytes + off); };
    new (self) CPeep(cell, static_cast<CThing::ThingModel>(0x12));
    bytes[0x3a0] = flag;
    word(0x3a4) = 0;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = static_cast<CThing*>(self)->GetThingId();
    gWorld->SeedRand(id.mIndex);
    word(0x24) = 0;
    word(0x20) = ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(word(0x24)));
    int32_t count = ESP_GetNumberOfSprites(static_cast<ESP_Sprites>(0));
    uint32_t limit = static_cast<uint32_t>(count) - 1u;
    uint32_t pick = avatar <= limit ? avatar : limit;
    word(0x20) = static_cast<int32_t>(pick);
    CDelayedScript* script = reinterpret_cast<CDelayedScript*>(bytes + 0xc);
    script->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(3), word(0x24), word(0x20));
    SCR_SetFloatVariable(script->mSpriteScript, 8, 2.0f);
    IME::MslWideStringRep* rep = *static_cast<IME::MslWideStringRep**>(name);
    ignore_callee::SwprintfWide(reinterpret_cast<char16_t*>(bytes + 0x19c), 0x100, kPersonNameFormat, rep->mData);
    uint16_t sprite = gPersonTable[1];
    std::memcpy(bytes + 0xc2, &sprite, sizeof sprite);
    word(0x188) = 1;
}

// 0x1020deec: std::list<std::wstring>::insert(iterator position, const std::wstring&): the new node goes before position
void wstring_list::WStringList_insert(WStringNode** result, WStringList* list, WStringNode* position, const void* text) {
    if (list->mSize > 0x7ffffffeu) {
        IME::MslExceptionObject obj;
        uint32_t len = wostream_callee::Strlen(kInsertMessage);
        char* copy = wostream_callee::NewBytes(len + 1);
        int32_t* counter = nullptr;
        if (copy != nullptr) {
            counter = wostream_callee::NewInt();
            if (counter != nullptr) *counter = 1;
        }
        obj.mRc.mPtr = copy;
        obj.mRc.mCount = counter;
        wostream_callee::CopyString(copy, kInsertMessage);
        obj.mVtbl = IME::gMslLengthErrorVtbl;
        wostream_callee::Throw(kLogicLengthType, &obj, IME::gMslLengthErrorDtorTV);
    }
    WStringNode* node = static_cast<WStringNode*>(online_callee::OperatorNew(12));   // 12 bytes on the Mac
    if (node != nullptr) online_callee::WStringCopy(&node->mString, text, 0, 0xffffffffu);
    position->mPrev->mNext = node;
    node->mPrev = position->mPrev;
    position->mPrev = node;
    node->mNext = position;
    list->mSize = list->mSize + 1;
    *result = node;
}

// std::list<CToken>::clear 0x102109fc (unit v5132).

void CTokenListClear(CTokenList* list) {
    if (list->mSize == 0) return;
    CTokenListNode* sentinel = reinterpret_cast<CTokenListNode*>(&list->mPrev);
    CTokenListNode* node = list->mNext.get();
    while (node != sentinel) {
        Ptr32<CTokenHandle>* slot = &node->mData;
        CTokenHandle* h = slot->get();
        if (h->mCount == 0) h->mCount = 1;
        h->mCount = h->mCount - 1;
        if (h->mCount != 0) h = nullptr;
        slot->set(h);
        if (slot->get() != nullptr) {
            slc_callee::OperatorDelete(slot->get()->mBlock.get());
            slc_callee::OperatorDelete(slot->get());
        }
        CTokenListNode* next = node->mNext.get();
        slc_callee::OperatorDelete(node);
        node = next;
    }
    list->mSize = 0;
    list->mNext.set(sentinel);
    list->mPrev.set(sentinel);
}

// std::list<CToken>::insert (0x10210b44): a length check (the throw sequence is built inline), then one node linked in before the position.

namespace token_list {

void TokenList_insert(TokenListNode** result, TokenList* list, TokenListNode* position, const CToken* token) {   // 0x10210b44
    if (list->mSize > 0x7ffffffeu) {
        // list::insert length error: the exception object is built on the stack and thrown (the fakes of __throw return)
        static const char kMessage[] = "list::insert length error";
        IME::MslExceptionObject exception;
        exception.mVtbl = IME::gMslExcVtblB;
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kMessage)) + 1));
        IME::MslRefCountedCharsCtor(&exception.mRc, text);
        IME::MslStrcpy(exception.mRc.mPtr, kMessage);
        exception.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &exception, IME::gMslLengthErrorDtorTV);
    }
    TokenListNode* node = static_cast<TokenListNode*>(IME::MslNew(0x18));
    ::new (static_cast<void*>(&node->mValue)) CToken(*token);
    position->mPrev->mNext = node;
    node->mPrev = position->mPrev;
    position->mPrev = node;
    node->mNext = position;
    list->mSize = list->mSize + 1;
    *result = node;
}

}  // namespace token_list

// Unit v51039 (main app): std::list<std::wstring>::clear and the destructors of the swear filter and of the theme map, the font
// wrapper's GetCharWidth, the park list's StoreReceivedList and four small UI helpers. The Mac's strings, heap and virtual calls are
// the callees of the headers' *_callee namespaces (weak definitions in the _callees file; the test library fakes them).

// ---- the weak definitions of the callees the host cannot call as the Mac does (the Mac's strings and heap, new plus constructor,
// virtual calls through __ptr_glue); the unit's test library defines them again as recording fakes.
#define TPW_WEAK __attribute__((weak))
namespace masterlist_callee {
// The Mac's list nodes hold Mac strings; the host never has such a list (no-op).
TPW_WEAK void WStringDtor(void*, int16_t) {}
TPW_WEAK void OperatorDelete(void* block) { ::operator delete(block); }
}  // namespace masterlist_callee

void WStringListClear(MacWStringList* list);

// 0x101697bc (120). The original also tests `this + 0xc` against 0 (this = 0xfffffff4); not reproducible on the host.
SwearFilter* SwearFilter::Destruct(SwearFilter* self, int16_t deleteFlag) {
    if (self != nullptr) {
        uint8_t* base = reinterpret_cast<uint8_t*>(self);
        WStringListClear(reinterpret_cast<MacWStringList*>(base + 0xc));
        WStringListClear(reinterpret_cast<MacWStringList*>(base));
        if (deleteFlag > 0) masterlist_callee::OperatorDelete(self);
    }
    return self;
}
