// Test library for the small e-mail functions of unit v51057 (libtpw_unit_AddressBook_First.dylib, tests/test_AddressBook_First.py), main app:
//   src/game/online/email.cpp   the 22 functions (namespace v51057, email_callee::MailCbfn)
//
// The callees outside the unit are recording fakes: list remove and clear, operator delete, ios_base::~ios_base, Sock::~Sock, the string
// compare and Email::Callback. Every pointer argument is logged as its offset from the case's block (the test sets the base), so the
// log is the same on both sides. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "std_basic_filebuf.h"
#include "std_list_uint.h"

using namespace calllog;

// ---- globals of other files, defined here
alignas(256) char gOstreamVt1[0x40];
alignas(256) char gIfstreamVtblC[0x40];
alignas(256) uint32_t gEmailBlob;

namespace {

uint64_t gBase;

int64_t Off(const void* p) {
    uint64_t a = reinterpret_cast<uint64_t>(p);
    return a == 0 ? 0 : static_cast<int64_t>(static_cast<uint32_t>(a - gBase));
}

}  // namespace

// ---- the fakes (same event names and arguments as the module's PowerPC stubs)
namespace std_list_uint {
void Remove(List* list, const uint32_t* value) {
    ev("Remove", {Off(list), static_cast<int64_t>(*value)});
}
void Clear(List* list) { ev("ClearUint", {Off(list)}); }
}  // namespace std_list_uint

namespace email_callee {
void ListClear(void* list) { ev("ClearStr", {Off(list)}); }
}  // namespace email_callee

void OstreamIosBaseDtor(MacOstreamIos* ios, int32_t flags) { ev("IosDtor", {Off(ios), static_cast<int64_t>(static_cast<uint32_t>(flags))}); }

void MslOperatorDelete(void* p) { ev("Delete", {Off(p)}); }

namespace csock_callee {
void SockDtor(void* self, int16_t flag) { ev("SockDtor", {Off(self), static_cast<int64_t>(static_cast<uint16_t>(flag))}); }
}  // namespace csock_callee

// The string is the Mac's word that points at a rep; only the first 32 bytes of the text are summed.
namespace msl_char_string {
int32_t compare(const MslCharString* self, uint32_t pos, uint32_t n1, const char* s, uint32_t n2) {
    uint32_t sum = 0;
    uint32_t n = n2 < 32 ? n2 : 32;
    for (uint32_t i = 0; i < n; i++) sum = sum * 31 + static_cast<uint8_t>(s[i]);
    ev("Compare", {Off(self), pos, n1, Off(s), n2, sum});
    return static_cast<int32_t>(ret("Compare"));
}
}  // namespace msl_char_string

void Email::Callback(uint32_t message, int32_t code, uint8_t* data) {
    ev("Callback", {Off(this), message, static_cast<int64_t>(static_cast<uint32_t>(code)), Off(data)});
}

extern "C" {

void tpw_v51057_setbase(uint64_t base) { gBase = base; }

uint64_t tpw_v51057_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"EmailBlob", &gEmailBlob},
        {"OstreamVt1", gOstreamVt1},
        {"IfstreamVtblC", gIfstreamVtblC},
        {"ListStrInit", &gStringListInitWord},
        {"ListUintInit", &gUintListInitWord},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51057_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_v51057_arena() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// Words are arena offsets (0: null); the addresses of the case's block are real pointers.
#define OBJ(x) (static_cast<uint32_t>(x) != 0 ? tpw::gArenaBase + static_cast<uint32_t>(x) : static_cast<uint8_t*>(nullptr))
#define CNOBJ(x) static_cast<const uint8_t*>(OBJ(x))
#define U32(x) static_cast<uint64_t>(static_cast<uint32_t>(x))
static uint64_t PtrWord(const void* p) { return p != nullptr ? static_cast<uint64_t>(tpw::ArenaOffset(p)) : 0; }
#define PTR32(x) PtrWord(x)

uint64_t tpw_v51057_first(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::AddressBookFirst(OBJ(a))); }
uint64_t tpw_v51057_next(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::AddressBookNext(OBJ(a))); }
uint64_t tpw_v51057_unmark_send(uint64_t a, uint64_t, uint64_t, uint64_t) {
    v51057::OutboxUnmarkToSend(OBJ(a));
    return 0;
}
uint64_t tpw_v51057_unmark_delete(uint64_t a, uint64_t, uint64_t, uint64_t) {
    v51057::OutboxUnmarkToDelete(OBJ(a));
    return 0;
}
uint64_t tpw_v51057_exceptions(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::IosExceptions(CNOBJ(a))); }
uint64_t tpw_v51057_or_assign(uint64_t a, uint64_t b, uint64_t, uint64_t) {
    return PTR32(v51057::IosOrAssign(reinterpret_cast<uint32_t*>(OBJ(a)), static_cast<uint32_t>(b)));
}
uint64_t tpw_v51057_state(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::IosState(OBJ(a))); }
uint64_t tpw_v51057_and(uint64_t a, uint64_t b, uint64_t, uint64_t) { return U32(v51057::IosAnd(static_cast<uint32_t>(a), static_cast<uint32_t>(b))); }
uint64_t tpw_v51057_rdstate(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::IosRdstate(CNOBJ(a))); }
uint64_t tpw_v51057_tie(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::BasicIosTie(CNOBJ(a))); }
uint64_t tpw_v51057_good(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::IosGood(CNOBJ(a))); }
uint64_t tpw_v51057_sentry_dtor(uint64_t a, uint64_t b, uint64_t, uint64_t) {
    return PTR32(v51057::IstreamSentryDtor(OBJ(a), static_cast<int16_t>(b)));
}
uint64_t tpw_v51057_istream_dtor(uint64_t a, uint64_t b, uint64_t, uint64_t) {
    return PTR32(v51057::IstreamDtor(OBJ(a), static_cast<int16_t>(b)));
}
uint64_t tpw_v51057_strlist_ctor(uint64_t a, uint64_t, uint64_t, uint64_t) { return PTR32(v51057::StringListCtor(OBJ(a))); }
uint64_t tpw_v51057_uintlist_ctor(uint64_t a, uint64_t, uint64_t, uint64_t) { return PTR32(v51057::UintListCtor(OBJ(a))); }
uint64_t tpw_v51057_uintlist_dtor(uint64_t a, uint64_t b, uint64_t, uint64_t) {
    return PTR32(v51057::UintListDtor(OBJ(a), static_cast<int16_t>(b)));
}
uint64_t tpw_v51057_strlist_dtor(uint64_t a, uint64_t b, uint64_t, uint64_t) {
    return PTR32(v51057::StringListDtor(OBJ(a), static_cast<int16_t>(b)));
}
uint64_t tpw_v51057_csock_dtor(uint64_t a, uint64_t b, uint64_t, uint64_t) {
    return PTR32(v51057::CSockDtor(OBJ(a), static_cast<int16_t>(b)));
}
uint64_t tpw_v51057_less(uint64_t a, uint64_t b, uint64_t, uint64_t) { return U32(v51057::StringLess(CNOBJ(a), CNOBJ(b))); }
uint64_t tpw_v51057_begin(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::StringBegin(CNOBJ(a))); }
uint64_t tpw_v51057_size(uint64_t a, uint64_t, uint64_t, uint64_t) { return U32(v51057::StringSize(CNOBJ(a))); }
uint64_t tpw_v51057_mail_cbfn(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    email_callee::MailCbfn(static_cast<int32_t>(a), static_cast<int32_t>(b), OBJ(c), OBJ(d));
    return 0;
}

}  // extern "C"
