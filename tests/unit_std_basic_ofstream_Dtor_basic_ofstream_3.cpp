// Test library for the buddy / ignore list helpers, GotoFailed, GotoStoredPerson, pair::~pair and basic_ofstream::~basic_ofstream
// (unit v5903; libtpw_unit_std_basic_ofstream_Dtor_basic_ofstream_3.dylib, tests/test_std_basic_ofstream_Dtor_basic_ofstream_3.py), main app:
//   src/COnlineControl_..._ProcessQueue_std_basic_ofstream_Dtor_basic_ofstream_3.cpp
//
// Every callee is a recording fake with the PowerPC stub's event name and arguments. A string object is one word that holds the text pointer.
// Pointers are logged as small ids (0 the object, 1 filebuf, 2 locale, 3 ios) and texts as a hash of their characters.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "Ptr32.h"
#include "std_basic_ifstream.h"
#include "Text.h"
#include "TbITable.h"
#include "UIInterface.h"

using namespace calllog;

// ---- globals of other files (the program's definitions are in files this library does not compile)
uint32_t gIfsWVtblC[4];
uint32_t gIfsWVtblD[4];
uint32_t gIfsWVtblE[4];
uint32_t gOfsWVtbl[4];
uint8_t gIfsWFiles[0xfc];
UIInterface gUIInterface;
// gText is raw storage under the symbol of the program's CText (its constructor would drag in the font classes); only the pointer at
// mResourcePointers[19] is read.
alignas(16) uint8_t gTextStorage[sizeof(CText) + 64] __asm__("_gText");

namespace {

uint64_t gObj = 0;

int64_t Th(const char16_t* p) {
    if (p == nullptr) return -1;
    uint32_t n = 0;
    while (p[n] != 0 && n < 0x420) n++;
    uint32_t h = n;
    for (uint32_t i = 0; i < n; i++) h = (h * 31u + p[i]) & 0x7fffffffu;
    return h;
}

int64_t Oid(const void* p) {
    uint64_t a = reinterpret_cast<uintptr_t>(p);
    if (a == gObj) return 0;
    if (a == gObj + 0x10) return 1;
    if (a == gObj + 0x18) return 2;
    if (a == gObj + 0x38) return 3;
    return -1;
}

int64_t IsObj(const void* p) { return reinterpret_cast<uintptr_t>(p) == gObj ? 1 : 0; }

const char16_t* StrOf(const void* object) { return *reinterpret_cast<const char16_t* const*>(object); }

const uint8_t* Res(uint32_t offset) {
    Ptr32<uint8_t> p;
    p.mOffset = offset;
    return p.get();
}

int64_t KeyHash(const void* addr) {
    uint32_t rep = *reinterpret_cast<const uint32_t*>(addr);
    uint32_t data = *reinterpret_cast<const uint32_t*>(Res(rep) + 0xc);
    return Th(reinterpret_cast<const char16_t*>(Res(data)));
}

}  // namespace

// ---- the fakes
namespace online_callee {
void WStringCtorFrom(void* self, const char16_t* text) {
    ev("WStringCtorFrom", {Th(text)});
    *reinterpret_cast<const char16_t**>(self) = text;
}
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WStringCopy", {Th(StrOf(other)), static_cast<int64_t>(pos), static_cast<int64_t>(static_cast<int32_t>(count))});
    *reinterpret_cast<const char16_t**>(self) = StrOf(other);
}
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {Th(StrOf(self)), static_cast<int64_t>(flag)}); }
void OperatorDelete(void* p) { ev("Delete", {Oid(p)}); }
void NocaseWStringDtor(void* self, int16_t flag) { ev("NocaseDtor", {Oid(self), static_cast<int64_t>(flag)}); }
void UIOnlineAddBuddy(void* ui, const char16_t* text, int32_t data) {
    ev("OnlineAddBuddy", {ui == &gUIInterface ? 1 : 0, Th(text), static_cast<int64_t>(data)});
}
}  // namespace online_callee

namespace docommand_callee {
void ChatGoto(uint32_t chat, const void* name, void* context) {
    ev("ChatGoto", {static_cast<int64_t>(chat), Th(StrOf(name)), IsObj(context)});
}
void ChatAddBuddy(uint32_t chat, const void* name, void* context) {
    ev("ChatAddBuddy", {static_cast<int64_t>(chat), KeyHash(name), IsObj(context)});
}
}  // namespace docommand_callee

namespace ignore_callee {
void ChatIgnore(uint32_t chat, const IgnoreWString* name, COnlineControl* context) {
    ev("ChatIgnore", {static_cast<int64_t>(chat), KeyHash(name), IsObj(context)});
}
void TreeIncrement(uint32_t* node) {
    ev("TreeIncrement", {0});
    *node = *reinterpret_cast<const uint32_t*>(Res(*node));
}
uint32_t FilebufClose(IgnoreFilebuf* filebuf) {
    ev("close", {Oid(filebuf)});
    return 1;
}
}  // namespace ignore_callee

namespace ifstream_callee {
void LocaleDtor(void* locale, int32_t flag) { ev("LocaleDtor", {Oid(locale), static_cast<int64_t>(flag)}); }
void IosBaseDtor(void* ios, int32_t flag) { ev("IosBaseDtor", {Oid(ios), static_cast<int64_t>(flag)}); }
}  // namespace ifstream_callee

uint32_t COnlineControl::GetDataForUser(const nocase_string& key) const {
    ev("GetDataForUser", {IsObj(this), KeyHash(&key)});
    return static_cast<uint32_t>(ret("GetDataForUser"));
}

void COnlineControl::ReceiveText(std::u16string& text, int32_t colour) {
    ev("ReceiveText", {IsObj(this), Th(StrOf(&text)), static_cast<int64_t>(colour)});
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this)), static_cast<int64_t>(static_cast<int32_t>(size)),
                        static_cast<int64_t>(id), reinterpret_cast<uintptr_t>(out) == gObj + 0x220 ? 1 : 0});
    int64_t k = ret("FormatString");
    int n = static_cast<int>(k % 5) + 1;
    for (int i = 0; i < n; i++) out[i] = static_cast<char16_t>(0x41 + ((k + i * 3) % 26));
    out[n] = 0;
    return 0;
}

extern "C" {

uint64_t tpw_ofs3_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ui", &gUIInterface},     {"files", gIfsWFiles},   {"textres", gTextStorage + offsetof(CText, mResourcePointers) + 19 * sizeof(void*)},
        {"vtOfs", gOfsWVtbl},      {"vtOfsB", gOfsWVtblB},  {"vtC", gIfsWVtblC},
        {"vtD", gIfsWVtblD},       {"vtE", gIfsWVtblE},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ofs3_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ofs3_setobj(uint64_t object) { gObj = object; }

// The arena offset of a host address (what the object's pointer words hold).
uint32_t tpw_ofs3_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_ofs3_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(BasicOfstreamWchar, mVtable));
    out[n++] = static_cast<int64_t>(offsetof(BasicOfstreamWchar, mFilebufVt));
    out[n++] = static_cast<int64_t>(offsetof(BasicOfstreamWchar, mLocale));
    out[n++] = static_cast<int64_t>(offsetof(BasicOfstreamWchar, mFile));
    out[n++] = static_cast<int64_t>(offsetof(BasicOfstreamWchar, mIosVtable));
    out[n++] = static_cast<int64_t>(offsetof(IfstreamWVbTable, mBaseOffset));
    out[n++] = static_cast<int64_t>(sizeof(IfstreamWVbTable));
    return n;
}

void tpw_ofs3_goto_stored(uint64_t c) { reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(c))->GotoStoredPerson(); }
void tpw_ofs3_goto_failed(uint64_t c) { reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(c))->GotoFailed(); }
void tpw_ofs3_upd_ignore(uint64_t c) { reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(c))->UpdateIgnoreListToServer(); }
void tpw_ofs3_upd_buddy_srv(uint64_t c) { reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(c))->UpdateBuddyListToServer(); }
void tpw_ofs3_upd_buddy(uint64_t c) { reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(c))->UpdateBuddyList(); }
uint64_t tpw_ofs3_pair(uint64_t p, int32_t flag) {
    return reinterpret_cast<uintptr_t>(BuddyEntryDestruct(reinterpret_cast<BuddyEntry*>(static_cast<uintptr_t>(p)), static_cast<int16_t>(flag)));
}
uint64_t tpw_ofs3_ofs(uint64_t p, int32_t flag) {
    return reinterpret_cast<uintptr_t>(
        reinterpret_cast<BasicOfstreamWchar*>(static_cast<uintptr_t>(p))->destroy(static_cast<int16_t>(flag)));
}

}  // extern "C"
