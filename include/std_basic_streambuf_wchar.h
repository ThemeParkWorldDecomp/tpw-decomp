// std::basic_streambuf<wchar_t, std::char_traits<wchar_t>> (wchar_t is 16 bits: char16_t).
// Mac offsets in comments; the host object uses native pointers (so host offsets are twice the Mac's).
#pragma once
#include <cstddef>
#include <cstdint>

// Unit v5185
// The Mac's wmemcpy (c/c++_shared glue 0x102389e0) on 16-bit characters; defined by the runtime or a test fake.
char16_t* wmemcpy(char16_t* dst, const char16_t* src, size_t n);

class MslWStreambuf {
public:
    // Mac vtable (offset 0 of the object): slot 11 (0x2c) is uflow, slot 14 (0x38) is overflow(c); the others are placeholders.
    virtual void vf0() {}
    virtual void vf1() {}
    virtual void vf2() {}
    virtual void vf3() {}
    virtual void vf4() {}
    virtual void vf5() {}
    virtual void vf6() {}
    virtual void vf7() {}
    virtual void vf8() {}
    virtual void vf9() {}
    virtual void vf10() {}
    virtual uint32_t uflow() { return 0xffff; }
    virtual void vf12() {}
    virtual void vf13() {}
    virtual uint32_t overflow(uint32_t c) { return c; }

    int32_t xsputn(const char16_t* s, int32_t n);  // 0x1021feac (292)
    int32_t xsgetn(char16_t* s, int32_t n);        // 0x10220024 (288)

    void* mUnk04 = nullptr;      // 0x04
    char16_t* mGNext = nullptr;  // 0x08
    char16_t* mGEnd = nullptr;   // 0x0c
    char16_t* mPBeg = nullptr;   // 0x10
    char16_t* mPNext = nullptr;  // 0x14
    char16_t* mPEnd = nullptr;   // 0x18
};

// Unit v51044 (src/game/online/online.cpp)
namespace wstreambuf_callee {
// The virtual call through slot 0x28 (underflow, a TVector via __ptr_glue 0x1023baa8); the test library fakes it.
uint16_t Underflow(MslWStreambuf* self);
}
// 0x1021e4dc std::basic_streambuf<wchar_t>::uflow(): 0xffff when underflow answers 0xffff, else the character at the get pointer, which moves on.
uint16_t WStreambufUflow(MslWStreambuf* self);
