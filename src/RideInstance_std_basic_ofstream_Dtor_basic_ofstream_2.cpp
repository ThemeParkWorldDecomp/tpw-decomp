// std::basic_ofstream<char>: constructor 0x10071e34 and destructor 0x1006fdc0 (unit v5527).
// The object keeps the Mac's 32-bit words: +0 the pointer to the virtual ios part (an arena offset here), +4 the vtable word,
// +8 the filebuf, +0x48 the ios part. Addresses of globals (vtables, __files) are stored truncated to 32 bits.
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "Debug.h"
#include "FS_System.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "platform/samsutils.h"
#include "platform/arena.h"
#include "email.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

char gOfstreamVt[0x20];   // 0x1029d1a8 (initial word 0x1029d1a0): the ofstream's own vtable block

namespace {
inline uint32_t& W(void* base, uint32_t off) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(base) + off); }
inline uint32_t A(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }   // a global's address
inline uint32_t AO(const void* p) { return tpw::ArenaOffset(p); }                                    // an object's address
inline uint8_t* PO(uint32_t w) { return tpw::gArenaBase + w; }
}  // namespace

// 0x10071e34 (180)
void* BasicOfstreamCtor(void* self, int16_t withVirtualBase) {
    uint8_t* obj = static_cast<uint8_t*>(self);
    if (withVirtualBase != 0) {
        uint8_t* ios = obj + 0x48;
        W(obj, 0) = AO(ios);
        W(ios, 0) = A(gIosBaseVt);
        W(ios, 0) = A(gOstreamVt1);
    }
    BasicOstreamCtor(reinterpret_cast<MacOstream*>(obj), 0, AO(obj + 8));
    W(obj, 4) = A(gOfstreamVt);
    uint8_t* q = PO(W(obj, 0));
    W(q, 0) = A(gOfstreamVt) + 0xc;
    q = PO(W(obj, 0));
    W(q, 0x44) = AO(obj + 0x48) - AO(q);
    reinterpret_cast<BasicFilebufChar*>(obj + 8)->construct(nullptr);
    return self;
}

// 0x1006fdc0 (324): the deleting destructor, `flags` is the hidden short after this.
void* BasicOfstreamDtor(void* self, int16_t flags) {
    if (self == nullptr) return self;
    uint8_t* obj = static_cast<uint8_t*>(self);
    W(obj, 4) = A(gOfstreamVt);
    uint8_t* q = PO(W(obj, 0));
    W(q, 0) = A(gOfstreamVt) + 0xc;
    q = PO(W(obj, 0));
    W(q, 0x44) = AO(obj + 0x48) - AO(q);
    // the filebuf part
    W(obj, 8) = A(gFilebufCharVtblB);
    uint32_t file = W(obj, 0x2c);
    uint32_t files = A(gIfsWFiles);
    if (file != files && file != files + 0x54 && file != files + 0xa8) OfstreamClose(obj + 8);
    W(obj, 8) = A(gFilebufCharVtblA);
    OfstreamLocaleDtor(obj + 0x24, -1);
    // the ostream part
    W(obj, 4) = A(gOstreamVt2);
    q = PO(W(obj, 0));
    W(q, 0) = A(gOstreamVt2) + 0xc;
    q = PO(W(obj, 0));
    W(q, 0x44) = AO(obj + 8) - AO(q);
    if (flags != 0) {
        W(obj, 0x48) = A(gOstreamVt1);
        OstreamIosBaseDtor(reinterpret_cast<MacOstreamIos*>(obj + 0x48), 0);
    }
    if (flags > 0) OstreamDelete(self);
    return self;
}
