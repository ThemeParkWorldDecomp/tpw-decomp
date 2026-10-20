// Test library for COnlineControl::IgnoreOff, IgnoreOn, BuddyOff, SendText, RemoveContact, RemoveBuddy, UpdateUserList (unit v5897;
// libtpw_unit_COnlineControl_IgnoreOff.dylib, tests/test_COnlineControl_IgnoreOff.py), main app:
//   src/COnlineControl_..._COnlineControl_IgnoreOff.cpp
//
// The callees are fakes that record what they read. Strings the original keeps as one word are opaque buffers: a fake that builds one
// stores the 32-bit address of a std::u16string it allocates (a "pseudo" string, registered in gPseudo); a source that is not registered is
// a real std::u16string (the name argument, a contact node's value). Text is logged as a hash of its UTF-16 units.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <set>
#include <string>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "UIInterface.h"
#include "MslSubscriptionTree.h"
#include "MslWStringTree.h"

using namespace calllog;

UIInterface gUIInterface;   // 0x10396440

namespace {

uint8_t* gSelf = nullptr;
std::set<const void*> gPseudo;
std::vector<void*> gNodes;
const void* gParser = nullptr;
void* gVec = nullptr;
void* gOutput = nullptr;

const char16_t* const kParseTexts[3] = {u"", u"hi there", u"Zed"};

uint32_t Hash(const char16_t* text, size_t n) {
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < n; i++) h = (h ^ static_cast<uint32_t>(text[i])) * 0x01000193u;
    return h ^ (static_cast<uint32_t>(n) * 0x9e3779b1u);
}
int64_t HashS(const std::u16string& s) { return Hash(s.data(), s.size()); }
int64_t HashC(const char16_t* text) {
    size_t n = 0;
    while (text[n] != 0) n++;
    return Hash(text, n);
}

// The text of a string object: a pseudo string holds the address of its std::u16string, anything else is a std::u16string.
const std::u16string& Txt(const void* object) {
    if (gPseudo.count(object)) {
        uint32_t offset;
        std::memcpy(&offset, object, 4);
        return *reinterpret_cast<const std::u16string*>(tpw::ArenaBase() + offset);
    }
    return *static_cast<const std::u16string*>(object);
}

void MakePseudo(void* self, const std::u16string& value) {
    auto* made = new (tpw::ArenaAllocZeroed(sizeof(std::u16string))) std::u16string(value);
    uint32_t offset = tpw::ArenaOffset(made);
    std::memcpy(self, &offset, 4);
    gPseudo.insert(self);
}

int64_t Off(const void* p) { return static_cast<int64_t>(static_cast<const uint8_t*>(p) - gSelf); }

void Begin(void* self) {
    gSelf = static_cast<uint8_t*>(self);
    gPseudo.clear();
}

uint32_t Word(const void* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

void SubCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    const std::u16string& source = Txt(other);
    MakePseudo(self, source.substr(pos, count == 0xffffffffu ? std::u16string::npos : count));
}

}  // namespace

// ---- the callees (online_callee and others), same event names and values as the module's PowerPC stubs
namespace online_callee {
void StringCtor(void* self) {
    uint32_t zero = 0;
    std::memcpy(self, &zero, 4);
    ev("StringCtor", {});
}
void StringDtor(void*, int16_t flag) { ev("StringDtor", {flag}); }
void WStringCtor(void* self) {
    MakePseudo(self, std::u16string());
    ev("WStringCtor", {});
}
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {flag, HashS(Txt(self))}); }
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WStringCopy", {pos, count, HashS(Txt(other))});
    SubCopy(self, other, pos, count);
}
void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("NocaseCopy", {pos, count, HashS(Txt(other))});
    SubCopy(self, other, pos, count);
}
void NocaseWStringDtor(void* self, int16_t flag) { ev("NocaseDtor", {flag, HashS(Txt(self))}); }
void* NocaseSetFind(void* tree, const void* key) {
    ev("NocaseSetFind", {Off(tree), HashS(Txt(key))});
    return static_cast<uint8_t*>(tree) + (ret("find") ? 0x40 : 4);
}
void* BuddyTreeFind(void* tree, const void* key) {
    ev("BuddyFind", {Off(tree), HashS(Txt(key))});
    return static_cast<uint8_t*>(tree) + (ret("bfind") ? 0x40 : 4);
}
void BuddyTreeEraseOne(void* tree, const void* key) { ev("BuddyErase", {Off(tree), HashS(Txt(key))}); }
void ArgumentCtor(void* self, const void* text) {
    ev("ArgumentCtor", {HashS(Txt(text))});
    std::memset(self, 0, 12);
    std::memcpy(static_cast<uint8_t*>(self) + 4, text, 4);
    gPseudo.insert(static_cast<uint8_t*>(self) + 4);
}
void ArgumentVectorPushBack(void*, const void* argument) {
    const uint8_t* a = static_cast<const uint8_t*>(argument);
    ev("PushBack", {Word(a), Word(a + 8), HashS(Txt(a + 4))});
}
void ArgumentVectorTearDown(void* vec) { ev("TearDown", {Word(static_cast<uint8_t*>(vec) + 4)}); }
void UIOnlineAddContact(void* ui, const char16_t* text, int32_t flag) { ev("UIAddContact", {lab(ui), HashC(text), flag}); }
void UIOnlineUpdateContact(void* ui, const char16_t* text, int32_t flag) { ev("UIUpdateContact", {lab(ui), HashC(text), flag}); }
void UIOnlineRemoveContact(void* ui, const char16_t* text) { ev("UIRemoveContact", {lab(ui), HashC(text)}); }
void UIOnlineRemoveBuddy(void* ui, const char16_t* text) { ev("UIRemoveBuddy", {lab(ui), HashC(text)}); }
}  // namespace online_callee

void COnlineControl::DoCommand(void* arguments) {
    ev("DoCommand", {Off(this), Word(static_cast<uint8_t*>(arguments) + 4)});
}
uint32_t COnlineControl::GetDataForUser(const nocase_string& key) const {
    ev("GetDataForUser", {Off(this), HashS(Txt(&key))});
    return static_cast<uint32_t>(ret("gdfu"));
}

COnlineParser* COnlineParser::Construct(COnlineParser* self, void* arguments, void* text) {
    gParser = self;
    gVec = arguments;
    gOutput = text;
    ev("ParserCtor", {HashS(Txt(text))});
    return self;
}
void COnlineParser::DoParse(const void* text) {
    ev("DoParse", {HashS(Txt(text))});
    uint32_t pushed = ret("parsepush") ? 1 : 0;
    if (pushed) std::memcpy(static_cast<uint8_t*>(gVec) + 4, &pushed, 4);
    MakePseudo(gOutput, std::u16string(kParseTexts[ret("parsetext") % 3]));
}
void CTokenListClear(CTokenList* list) { ev("ListClear", {static_cast<const uint8_t*>(static_cast<const void*>(list)) - static_cast<const uint8_t*>(gParser)}); }
namespace online_parser_callee {
void ParserDtor(void* parser, int16_t flag) {
    ev("ParserDtor", {parser == gParser ? 1 : 0, flag, Word(parser)});
}
}  // namespace online_parser_callee

bool WStringTree::erase_one(const MslWideString& key) {
    ev("ContactErase", {Off(this), HashS(*reinterpret_cast<const std::u16string*>(&key))});
    return false;
}

void MslRedBlackTree::increment(node_base*& iterator) {
    ev("Increment", {});
    size_t i = 0;
    while (i < gNodes.size() && gNodes[i] != iterator) i++;
    iterator = (i + 1 < gNodes.size()) ? static_cast<node_base*>(gNodes[i + 1]) : reinterpret_cast<node_base*>(gSelf + 0xe2c);
}

extern "C" {

uint64_t tpw_cocig_addr(const char* name) {
    if (std::strcmp(name, "ui") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gUIInterface));
    return 0;
}

uint64_t tpw_cocig_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A real std::u16string in the arena; its address.
uint64_t tpw_cocig_u16(const uint16_t* data, uint32_t n) {
    auto* made = new (tpw::ArenaAllocZeroed(sizeof(std::u16string))) std::u16string(reinterpret_cast<const char16_t*>(data), n);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(made));
}

// A contact node (host layout: node base, then the std::u16string) at `node`.
void tpw_cocig_node(uint64_t node, const uint16_t* data, uint32_t n) {
    auto* made = new (reinterpret_cast<void*>(static_cast<uintptr_t>(node) + sizeof(MslRedBlackTree::node_base)))
        std::u16string(reinterpret_cast<const char16_t*>(data), n);
    (void)made;
}
uint32_t tpw_cocig_offset(uint64_t address) { return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }
void tpw_cocig_clear_nodes(){ gNodes.clear(); }
void tpw_cocig_push_node(uint64_t node) { gNodes.push_back(reinterpret_cast<void*>(static_cast<uintptr_t>(node))); }

int32_t tpw_cocig_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(COnlineControl));
    out[n++] = static_cast<int64_t>(sizeof(MslRedBlackTree::node_base));
    return n;
}

#define SELF_NAME_FN(NAME)                                                                                        \
    void tpw_cocig_##NAME(uint64_t self, uint64_t name) {                                                         \
        Begin(reinterpret_cast<void*>(static_cast<uintptr_t>(self)));                                             \
        reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(self))                                           \
            ->NAME(*reinterpret_cast<std::u16string*>(static_cast<uintptr_t>(name)));                             \
    }
SELF_NAME_FN(IgnoreOff)
SELF_NAME_FN(IgnoreOn)
SELF_NAME_FN(BuddyOff)
SELF_NAME_FN(RemoveContact)
SELF_NAME_FN(RemoveBuddy)

void tpw_cocig_SendText(uint64_t self, uint64_t name) {
    Begin(reinterpret_cast<void*>(static_cast<uintptr_t>(self)));
    reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(self))
        ->SendText(*reinterpret_cast<std::u16string*>(static_cast<uintptr_t>(name)));
}

void tpw_cocig_UpdateUserList(uint64_t self) {
    Begin(reinterpret_cast<void*>(static_cast<uintptr_t>(self)));
    reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(self))->UpdateUserList();
}

}  // extern "C"
