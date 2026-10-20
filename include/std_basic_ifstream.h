// std::basic_ifstream<wchar_t> (unit v5201). Mac offsets in comments; the host object uses native pointers
// (arena memory is not 32-bit addressable), so its offsets differ from the Mac's.
#pragma once
#include <cstdint>

// Globals whose addresses the destructor stores (TOC slots). Only their addresses matter.
extern uint32_t gIfsWVtblA[4];   // 0x102b1e04 (TOC -0x5ec4): vtable of the complete object
extern uint32_t gIfsWVtblB[4];   // 0x102b1e40 (TOC -0x5ec0): vtable of the ifstream part
extern uint32_t gIfsWVtblC[4];   // 0x102b1e60 (TOC -0x5ebc): vtable of the streambuf base
extern uint32_t gIfsWVtblD[4];   // 0x102b1eb0 (TOC -0x5eb8): vtable of the filebuf
extern uint32_t gIfsWVtblE[4];   // 0x102b1f20 (TOC -0x5eb0): vtable of the ios_base
extern uint8_t gIfsWFiles[0xfc]; // 0x103bd0bc (import __files): three FILE entries of 0x54 bytes

// The virtual-base table the object points to (Mac 0x48 bytes).
struct IfstreamWVbTable {
    const void* mFirst;      // Mac 0x00
    uint8_t mPad[0x38];      // Mac 0x04..0x43 (host 0x08..0x3f)
    intptr_t mBaseOffset;    // Mac 0x44 (host 0x40): offset of a base subobject from the table pointer
};

struct BasicIfstreamWchar {
    IfstreamWVbTable* mVbase;   // Mac 0x00
    const void* mVtable;        // Mac 0x08 (host 0x08)
    const void* mFilebufVt;     // Mac 0x0c filebuf vtable (host 0x10)
    uint8_t mLocale[16];        // Mac 0x28 locale in the filebuf (host 0x18)
    const void* mFile;          // Mac 0x30 filebuf FILE* (host 0x28)
    uint8_t mPad[8];            // host 0x30
    const void* mIosVtable;     // Mac 0x5c ios_base vtable (host 0x38)
    uint8_t mRest[0x40];        // Mac 0x60.. (host 0x40)

    BasicIfstreamWchar* destroy(int16_t flag);   // 0x1021d5bc (324), ~basic_ifstream with the delete flag
    BasicIfstreamWchar* construct(int16_t flag); // 0x10221e20 (180), basic_ifstream() with the virtual-base flag
};

namespace ifstream_callee {
void IstreamCtor(void* self, int32_t flag, void* streambuf);   // 0x102266e4 basic_istream(streambuf*)
void FilebufCtor(void* self, void* file);                      // 0x10226ef4 basic_filebuf(FILE*)
void LocaleDtor(void* locale, int32_t flag);   // 0x10071f34
void IosBaseDtor(void* ios, int32_t flag);     // 0x10238950
void Delete(void* p);                          // 0x10238218
}  // namespace ifstream_callee

// Unit v5903: std::basic_ofstream<wchar_t> destructor. Mac offsets: 0x00 vbase table, 0x04 vtable slot, 0x08 filebuf (vtable), 0x24 locale,
// 0x2c FILE*, 0x58 ios_base (vtable). Host offsets in comments (same shape as BasicIfstreamWchar).
struct BasicOfstreamWchar {
    IfstreamWVbTable* mVbase;   // Mac 0x00
    const void* mVtable;        // Mac 0x04 (host 0x08)
    const void* mFilebufVt;     // Mac 0x08 (host 0x10)
    uint8_t mLocale[16];        // Mac 0x24 (host 0x18)
    const void* mFile;          // Mac 0x2c (host 0x28)
    uint8_t mPad[8];            // host 0x30
    const void* mIosVtable;     // Mac 0x58 (host 0x38)
    uint8_t mRest[0x40];

    BasicOfstreamWchar* destroy(int16_t flag);   // 0x1021ce74 (324)
};
extern uint32_t gOfsWVtblB[4];   // 0x102b1f08 (TOC -0x5eb4): the vtable stored last (initial word 0x102b1f00)
