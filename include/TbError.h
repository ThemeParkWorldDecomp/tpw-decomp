// TbError.h: the bullfrog library's result class (one long). Functions return it in a
// hidden r3 buffer on PowerPC, as CodeWarrior returns every class.
//
// Out-of-line copies: 0x10001564 TbError(long) 8 B, 0x100014b8 TbError(const TbError&)
// 12 B, 0x100014f0 operator long() 8 B (bullfrog, LbSCBack.cpp range),
// counted in src/LbFile_more.cpp (unit w221).
// Values seen so far: 0 success, -1 failure.
#pragma once
#include <cstdint>

class TbError {
public:
    TbError() {}   // Unit v51113: 0x1000eb1c (4): the Mac's default constructor is a bare blr, it leaves the word untouched
    TbError(int32_t value) : mValue(value) {}
    TbError(const TbError& other) : mValue(other.mValue) {}
    operator int32_t() const { return mValue; }

    // The library compares results with a constant TbError, and every such comparison
    // compiles to (result >= 0) == (constant == 0), the constant being a zero word that
    // the compiler keeps in data for that one comparison (LbLog has two, at 0x10072680 and
    // 0x10072684). Nothing writes the constants, so the test is result >= 0. The
    // original's operator is not known; this name is invented.
    bool Succeeded() const { return mValue >= 0; }

    int32_t mValue;   // 0x00
};
