// LbString.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "bfl_Strings.h"
#include "DrawRenderClasses.h"
#include "LbString.h"
#include "platform/macdoze.h"
#include "TbRaConvert.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

// 0x10066200 (152 B): copies src into dst, at most size - 1 bytes plus a NUL.
// Returns 1 when the whole string fit (strlen < size - 1, unsigned compare), else 0.
// size == 0 wraps to a huge limit: the whole string is copied (original's behaviour).
int32_t LbString_strnzcpy(char* dst, const char* src, uint32_t size) {
    uint32_t length = static_cast<uint32_t>(std::strlen(src));
    uint32_t limit = size - 1;
    if (length < limit) {
        std::memcpy(dst, src, length + 1);
        return 1;
    }
    std::memcpy(dst, src, limit);
    dst[size - 1] = 0;
    return 0;
}

// Bullfrog WinScrn.cpp, the four conversion overloads, 0x100662c8..0x10066660.

namespace bfl::Strings {
TbError Convert(char16_t* dest, uint32_t size, const char* source) { // 0x100662c8
    return TbError(NS_MacDoze::MultiByteToWideChar(0, 0, source, -1, dest, size) == 0 ? -1 : 0);
}

TbError Convert(TbDynamicStringTemplate<char16_t>& dest, const char* source) { // 0x10066360
    if (*source == 0) {
        dest = u"";
        return TbError(0);
    }
    int32_t count = NS_MacDoze::MultiByteToWideChar(0, 0, source, -1, nullptr, 0);
    char16_t* buffer = dest.LockBuffer(count);
    int32_t done = NS_MacDoze::MultiByteToWideChar(0, 0, source, -1, buffer, count);
    dest.ReleaseBuffer(static_cast<uint32_t>(done) - 1);
    return TbError(count == done ? 0 : -1);
}

TbError Convert(char* dest, uint32_t size, const char16_t* source) { // 0x100664ac
    return TbError(NS_MacDoze::WideCharToMultiByte(0, 0, source, -1, dest, size, nullptr, nullptr) == 0 ? -1 : 0);
}

TbError Convert(TbDynamicStringTemplate<char>& dest, const char16_t* source) { // 0x1006654c
    if (*source == 0) {
        dest = "";
        return TbError(0);
    }
    int32_t count = NS_MacDoze::WideCharToMultiByte(0, 0, source, -1, nullptr, 0, nullptr, nullptr);
    char* buffer = dest.LockBuffer(count);
    int32_t done = NS_MacDoze::WideCharToMultiByte(0, 0, source, -1, buffer, count, nullptr, nullptr);
    // The char overload releases the terminator-inclusive count; the wide one subtracts one.
    dest.ReleaseBuffer(done);
    return TbError(count == done ? 0 : -1);
}
}
