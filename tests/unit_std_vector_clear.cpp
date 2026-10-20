// Test library for the unit v5956 functions (libtpw_unit_std_vector_clear.dylib, tests/test_std_vector_clear.py), main app:
//   src/COnlineParser_ParseGoto_COnlineParser_COnlineParser_std_vector_clear.cpp
//   std::vector<Argument>::clear / tear_down, CToken::CToken (two), CParser::GetChar, COnlinePerson::COnlinePerson,
//   std::list<std::wstring>::insert.
//
// The callees are recording fakes with the same event names and arguments as the PowerPC stubs of the test. The persistent arena holds
// four blocks (A..D, 0x400 bytes each) and eight pool blocks (0x100 bytes) that the fake allocator hands out in order.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "ui_ime.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "HLSprite.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld;
uint16_t gPersonTable[2];
char IME::gMslLengthErrorVtbl[16];
char IME::gMslLengthErrorDtorTV[8];

namespace {

alignas(16) char gArena[0x1800];
uint32_t gPool = 0;

void* NextPool() { return gArena + 0x1000 + 0x100 * (gPool++ & 7); }

uint32_t TextHash(const char* text) {
    uint32_t h = 0, n = 0;
    for (; text[n] != 0; ++n) h = h * 31u + static_cast<uint8_t>(text[n]);
    return h + n * 1000003u;
}

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
void std_destroy::DestroyRangeClear(Argument* first, Argument* last) {
    ev("destroyClear", {lab(first), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<char*>(last) - reinterpret_cast<char*>(first)))});
}
void std_destroy::DestroyRangeTagged(Argument* first, Argument* last) {
    ev("destroyTagged", {lab(first), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<char*>(last) - reinterpret_cast<char*>(first)))});
}
void slc_callee::OperatorDelete(void* p) { ev("delete", {lab(p)}); }
void* online_callee::OperatorNew(uint32_t size) {
    ev("new", {size});
    return NextPool();
}
char* wostream_callee::NewBytes(uint32_t size) {
    ev("newarr", {size});
    return static_cast<char*>(NextPool());
}
int32_t* wostream_callee::NewInt() {
    ev("new", {4});
    return static_cast<int32_t*>(NextPool());
}
uint32_t wostream_callee::Strlen(const char* text) {
    ev("strlen", {TextHash(text)});
    return static_cast<uint32_t>(std::strlen(text));
}
void wostream_callee::CopyString(char* to, const char* from) {
    ev("strcpy", {lab(to), TextHash(from)});
    std::strcpy(to, from);
}
void wostream_callee::Throw(const char* type, void* object, const void* destructors) {
    IME::MslExceptionObject* obj = static_cast<IME::MslExceptionObject*>(object);
    const char* text = obj->mRc.mPtr != nullptr ? obj->mRc.mPtr : "";
    ev("throw", {TextHash(type), lab(obj->mVtbl), lab(obj->mRc.mPtr), lab(obj->mRc.mCount), TextHash(text),
                 obj->mRc.mCount != nullptr ? static_cast<int64_t>(static_cast<uint32_t>(*obj->mRc.mCount)) : 0, lab(destructors)});
}
void online_callee::WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WStringCopy", {lab(self), lab(other), pos, count});
}
char16_t* IME::MslWideString_index(MslWideString* self, uint32_t index) {
    ev("index", {lab(self), index});
    return reinterpret_cast<char16_t*>(gArena + 0xc00) + index;
}
CPeep::CPeep(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model) {
    ev("CPeep", {lab(this), cell.mIndex, static_cast<uint32_t>(model)});
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("thing"));
    return id;
}
void CWorld::SeedRand(uint32_t seed) { ev("SeedRand", {lab(this), seed}); }
int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    ev("ESPRand", {static_cast<uint32_t>(set)});
    return static_cast<int32_t>(ret("rand"));
}
int32_t ESP_GetNumberOfSprites(ESP_Sprites set) {
    ev("ESPNum", {static_cast<uint32_t>(set)});
    return static_cast<int32_t>(ret("num"));
}
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("Spawn", {lab(this), static_cast<uint32_t>(script), static_cast<uint32_t>(set), static_cast<uint32_t>(index)});
    mSpriteScript = static_cast<int32_t>(ret("spawn"));
}
void SCR_SetFloatVariable(int32_t handle, uint32_t index, float value) {
    ev("SCRFloat", {static_cast<uint32_t>(handle), index, CanonBits(value)});
}
void ignore_callee::SwprintfWide(char16_t* buffer, uint32_t count, const char16_t* format, const char16_t* text) {
    int64_t chars[12] = {0};
    for (int i = 0; i < 12 && format[i] != 0; ++i) chars[i] = format[i];
    ev("swprintf", {lab(buffer), count, chars[0], chars[1], chars[2], chars[3], chars[4], chars[5], chars[6], chars[7], chars[8],
                    lab(text)});
}

extern "C" {

uint64_t tpw_pv_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"arena", gArena},           {"gWorld", &gWorld},          {"gPersonTable", gPersonTable},
        {"vtbl", IME::gMslLengthErrorVtbl}, {"dtortv", IME::gMslLengthErrorDtorTV},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

void tpw_pv_reset() { gPool = 0; }

int32_t tpw_pv_layout(int64_t* out) {
    alignas(16) static char scratch[256];
    int32_t n = 0;
    ArgumentVector* vec = reinterpret_cast<ArgumentVector*>(scratch);
    CParser* parser = reinterpret_cast<CParser*>(scratch);
    IME::MslWideStringRep* rep = reinterpret_cast<IME::MslWideStringRep*>(scratch);
    wstring_list::WStringNode* node = reinterpret_cast<wstring_list::WStringNode*>(scratch);
    out[n++] = reinterpret_cast<char*>(&vec->mSize) - scratch;
    out[n++] = reinterpret_cast<char*>(&vec->mData) - scratch;
    out[n++] = static_cast<int64_t>(sizeof(CToken));
    out[n++] = reinterpret_cast<char*>(&parser->mString) - scratch;
    out[n++] = reinterpret_cast<char*>(&parser->mPos) - scratch;
    out[n++] = reinterpret_cast<char*>(&rep->mData) - scratch;
    out[n++] = reinterpret_cast<char*>(&node->mNext) - scratch;
    out[n++] = reinterpret_cast<char*>(&node->mString) - scratch;
    return n;
}

void tpw_pv_clear(uint64_t vec) { online_parser_callee::ArgumentVectorClear(reinterpret_cast<void*>(vec)); }
void tpw_pv_teardown(uint64_t vec) { online_callee::ArgumentVectorTearDown(reinterpret_cast<void*>(vec)); }
void tpw_pv_token_copy(uint64_t self, uint64_t other) {
    new (reinterpret_cast<void*>(self)) CToken(*reinterpret_cast<const CToken*>(other));
}
void tpw_pv_token_ctor(uint64_t self, uint64_t name, uint32_t w0, uint32_t w1, uint32_t w2) {
    token_callee::CTokenCtor(reinterpret_cast<void*>(self), reinterpret_cast<const void*>(name), w0, w1, w2);
}
uint32_t tpw_pv_getchar(uint64_t parser, uint64_t ch) {
    return online_parse_callee::ParserGetChar(reinterpret_cast<void*>(parser), reinterpret_cast<char16_t*>(ch)) ? 1u : 0u;
}
void tpw_pv_person(uint64_t self, uint32_t cell, uint32_t avatar, uint64_t name, uint32_t flag) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(cell);
    online_callee::COnlinePersonCtor(reinterpret_cast<void*>(self), tag, avatar, reinterpret_cast<void*>(name), flag != 0);
}
void tpw_pv_insert(uint64_t result, uint64_t list, uint64_t pos, uint64_t text) {
    wstring_list::WStringList_insert(reinterpret_cast<wstring_list::WStringNode**>(result),
                                     reinterpret_cast<wstring_list::WStringList*>(list),
                                     reinterpret_cast<wstring_list::WStringNode*>(pos), reinterpret_cast<const void*>(text));
}

}  // extern "C"
