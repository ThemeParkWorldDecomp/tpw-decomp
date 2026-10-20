// std::__putnumber<wchar_t, ostreambuf_iterator<wchar_t>> 0x10226358, basic_ios<wchar_t>::widen 0x10226828 and
// std::deque<std::vector<Argument>>::const_iterator::operator++ 0x1022571c.
#include <cstdint>
#include <cstring>

#include "Ptr32.h"
#include "online.h"
#include "std_basic_istream.h"

extern uint64_t gMslMacAddressHigh;

namespace {

// ostreambuf_iterator<wchar_t>::operator= : store at pptr while pptr < epptr, else overflow() through the vtable (0x38).
// A stored or returned 0xffff (eof as 16 bits) makes the iterator fail (NULL).
void Put(MacPutStreamBuf*& sb, uint32_t c) {
    if (sb == nullptr) return;
    uint32_t p = sb->mPptr.mOffset;
    uint16_t r;
    if (p < sb->mEpptr.mOffset) {
        sb->mPptr.mOffset = p + 2;
        uint16_t v = static_cast<uint16_t>(c);
        std::memcpy(sb->mPptr.get() - 2, &v, 2);
        r = v;
    } else {
        r = static_cast<uint16_t>(MslPutOverflow(sb, c));
    }
    if (r == 0xffff) sb = nullptr;
}

uint32_t* W(uint32_t word) {
    return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word));
}

}  // namespace

namespace wostream_callee {
void* PutNumber(void* iter, void* iosPtr, char16_t fill, const char16_t* prefix, int32_t prefixLen, const char16_t* suffix,
                int32_t suffixLen) {
    MacPutStreamBuf* sb = static_cast<MacPutStreamBuf*>(iter);
    MacPutIos* ios = static_cast<MacPutIos*>(iosPtr);
    int32_t pad = 0;
    int32_t total = prefixLen + suffixLen;
    if (ios->mWidth > total) pad = ios->mWidth - total;
    uint32_t adjust = ios->mFlags & 0xb0;
    if (adjust != 0x20 && adjust != 0x10)
        for (int32_t i = 0; i < pad; i++) Put(sb, fill);
    for (int32_t i = 0; i < prefixLen; i++) {
        uint16_t c = *prefix++;   // read even when the iterator has failed
        Put(sb, c);
    }
    if (adjust == 0x10)
        for (int32_t i = 0; i < pad; i++) Put(sb, fill);
    for (int32_t i = 0; i < suffixLen; i++) {
        uint16_t c = *suffix++;
        Put(sb, c);
    }
    if (adjust == 0x20)
        for (int32_t i = 0; i < pad; i++) Put(sb, fill);
    ios->mWidth = 0;
    return sb;
}
}  // namespace wostream_callee

namespace wistream_callee {
char16_t Widen(void* ios, char c) {
    ignore_callee::IgnoreLocale locale;
    ignore_callee::IosGetloc(&locale, static_cast<ignore_callee::IgnoreIos*>(ios));
    void* facet = ignore_callee::UseFacetCtype(&locale);
    char16_t r = ignore_callee::CtypeWiden(facet, c);
    ignore_callee::LocaleDtor(&locale, -1);
    return r;
}
}  // namespace wistream_callee

namespace wostream_u {
char16_t Widen(void* ios, char c) { return wistream_callee::Widen(ios, c); }
}  // namespace wostream_u

namespace argdeque_callee {
void IterInc(MslArgumentDequeIter* it) {
    it->mIndex = it->mIndex + 1;
    uint32_t element = it->mElement;
    it->mElement = element + 0xc;
    uint32_t blockEnd = it->mBlockEnd;
    if (element + 0xc != blockEnd) return;
    uint32_t blockStart = *W(it->mCursor.mSlot);
    it->mCursor.mIndex = it->mCursor.mIndex + 1;
    int32_t diff = static_cast<int32_t>(blockEnd - blockStart);
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * diff) >> 32) >> 1;
    uint32_t slot = it->mCursor.mSlot;
    it->mCursor.mSlot = slot + 4;
    q = q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
    uint32_t* cdeque = W(it->mCursor.mCdeque);
    uint32_t count = cdeque[0];
    uint32_t table = cdeque[3];
    if (slot + 4 == table + count * 4) it->mCursor.mSlot = table;
    uint32_t block = *W(it->mCursor.mSlot);
    it->mElement = block;
    it->mBlockEnd = block + static_cast<uint32_t>(q) * 0xc;
}
}  // namespace argdeque_callee
