// std::basic_ifstream<char>::~basic_ifstream, the copy at 0x1020448c (unit v5642).
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <list>
#include <string>
#include "email.h"
#include "Ptr32.h"
#include "platform/macdoze.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "platform/arena.h"

extern char gIfstreamVtbl[];   // 0x102b1778 (also declared in FS_MasterSave.h)

namespace {
// A Mac address as a 32-bit word: an arena offset for blocks, the low 32 bits for the program's own globals.
uint32_t A(const void* p) {
    return tpw::InArena(p) ? tpw::ArenaOffset(p) : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}
void* P(uint32_t a) { return tpw::gArenaBase + a; }
uint32_t& W(uint32_t base, uint32_t off) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + (base + off)); }
}  // namespace

void* BasicIfstreamDtorEmail(void* selfp, int16_t flags) {
    if (selfp == nullptr) return selfp;
    uint32_t self = A(selfp);
    uint32_t files = A(gIfsWFiles);
    W(self, 8) = A(gIfstreamVtbl);
    W(W(self, 0), 0) = A(gIfstreamVtbl) + 0xc;
    W(W(self, 0), 0x44) = (self + 0x4c) - W(self, 0);
    if (self + 0xc != 0) {
        W(self, 0xc) = A(gFilebufCharVtblB);
        uint32_t file = W(self, 0x30);
        if (file != files && file != files + 0x54 && file != files + 0xa8) OfstreamClose(P(self + 0xc));
        if (self + 0xc != 0) {
            W(self, 0xc) = A(gFilebufCharVtblA);
            if (self + 0x28 != 0) OfstreamLocaleDtor(P(self + 0x28), -1);
        }
    }
    W(self, 8) = A(gIfstreamVtblC);
    W(W(self, 0), 0) = A(gIfstreamVtblC) + 0xc;
    W(W(self, 0), 0x44) = (self + 0xc) - W(self, 0);
    if (flags != 0 && self + 0x4c != 0) {
        W(self, 0x4c) = A(gOstreamVt1);
        OstreamIosBaseDtor(static_cast<MacOstreamIos*>(P(self + 0x4c)), 0);
    }
    if (flags > 0) OstreamDelete(selfp);
    return selfp;
}
