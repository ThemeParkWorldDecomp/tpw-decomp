// Unit v5956: std::vector<Argument>::clear / tear_down, CToken copy and (string, member pointer) constructors, CParser::GetChar,
// COnlinePerson::COnlinePerson and std::list<std::wstring>::insert.
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include "online.h"
#include "ui_ime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "HLSprite.h"

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

// 0x10201310: destroy the elements and free the storage when there is any
void online_callee::ArgumentVectorTearDown(void* vec) {
    ArgumentVector* v = static_cast<ArgumentVector*>(vec);
    char* first = reinterpret_cast<char*>(v->mData);
    if (first != nullptr) {
        std_destroy::DestroyRangeTagged(v->mData, reinterpret_cast<Argument*>(first + v->mSize * 12u));
        slc_callee::OperatorDelete(v->mData);
    }
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
