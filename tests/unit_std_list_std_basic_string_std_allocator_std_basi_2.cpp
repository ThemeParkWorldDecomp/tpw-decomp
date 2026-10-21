// Test library for std::list<std::string>::unique (unit v5259; libtpw_unit_std_list_std_basic_string_std_allocator_std_basi_2.dylib), main app:
//   src/game/MslSupport.cpp   StringListUnique
//
// The callees are fakes: the string compare answers from the queue "compare", the list erase records its arguments (the host takes the
// last iterator as the answer, which the real erase gives). Strings are told by the id word at rep + 0x10. Pointer words are arena offsets.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "std_basic_filebuf.h"

using namespace calllog;

namespace {
inline uint32_t Wd(uint32_t off) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + off); }
// The id of the string whose rep word is at offset a.
int64_t IdOfStringWord(uint32_t a) { return Wd(Wd(a) + 0x10); }
uint32_t Off(const void* p) { return static_cast<uint32_t>(tpw::ArenaOffset(p)); }
}  // namespace

namespace msl_char_string {
int32_t compare(const MslCharString* self, uint32_t pos, uint32_t n1, const char* s, uint32_t n2) {
    ev("compare", {IdOfStringWord(Off(self)), pos, n1, static_cast<int64_t>(Wd(Off(s))), n2});
    return static_cast<int32_t>(ret("compare"));
}
}  // namespace msl_char_string

namespace email_callee {
void ListErase(void* list, uint32_t first, void* last) {
    uint32_t l = Off(list);
    uint32_t e = Off(last);
    int64_t idf = first == l + 4 ? -1 : IdOfStringWord(first + 8);
    int64_t idl = e == l + 4 ? -1 : IdOfStringWord(e + 8);
    ev("erase", {idf, idl});
}
}  // namespace email_callee

extern "C" {

uint64_t tpw_sluq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_sluq_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

int32_t tpw_sluq_layout(int64_t*) { return 0; }

void tpw_sluq_unique(uint64_t list) { StringListUnique(reinterpret_cast<void*>(static_cast<uintptr_t>(list))); }

}  // extern "C"
