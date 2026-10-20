// std::list<std::string>::unique (0x1020833c, 212 bytes), MSL, main app. The list is kept as Mac words at Mac offsets:
// +0 size, +4 the sentinel (prev +4, next +8), a node is prev +0, next +4, string +8 (one word: the rep pointer; the rep holds
// the length at +0 and the text pointer at +0xc). Every pointer word is a Ptr32 (an offset from the arena base).
#include <cstdint>

#include "email.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"

namespace {
inline uint32_t& W(uint32_t off) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + off); }
inline void* P(uint32_t off) { return reinterpret_cast<void*>(tpw::gArenaBase + off); }
}

// Removes every element equal to the one before it (the original compares each element with the next one; a run of equal
// ones is erased from the second to the last of the run).
void StringListUnique(void* self) {
    uint32_t list = static_cast<uint32_t>(tpw::ArenaOffset(self));
    uint32_t end = list + 4;
    uint32_t a = W(list + 8);
    while (a != end) {
        uint32_t b = W(a + 4);
        while (b != end) {
            uint32_t rep1 = W(a + 8);
            uint32_t rep2 = W(b + 8);
            if (msl_char_string::compare(static_cast<const MslCharString*>(P(a + 8)), 0, W(rep1),
                                         static_cast<const char*>(P(W(rep2 + 12))), W(rep2)) != 0)
                break;
            b = W(b + 4);
        }
        a = W(a + 4);
        if (a != b) {
            email_callee::ListErase(self, a, P(b));
            a = b;   // the erase answers its last iterator
        }
    }
}
