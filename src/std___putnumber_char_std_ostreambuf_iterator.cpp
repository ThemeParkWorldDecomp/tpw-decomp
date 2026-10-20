// std::__putnumber<char, ostreambuf_iterator<char>> 0x1020a140: writes the prefix, suffix and the fill padding by the adjust field.
#include "std_basic_istream.h"

namespace {

// ostreambuf_iterator::operator=(char): store at pptr or call overflow; a failure (-1) nulls the iterator.
inline void Put(MacPutStreamBuf*& sb, uint8_t c) {
    if (sb == nullptr) return;
    int32_t r;
    uint8_t* p = sb->mPptr.get();
    if (p < sb->mEpptr.get()) {
        sb->mPptr = p + 1;
        *p = c;
        r = *p;
    } else {
        r = MslPutOverflow(sb, c);
    }
    if (r == -1) sb = nullptr;
}

}  // namespace

MacPutStreamBuf* PutNumberChar(MacPutStreamBuf* iter, MacPutIos* ios, char fill, const char* prefix, int32_t prefixLen,
                               const char* suffix, int32_t suffixLen) {
    MacPutStreamBuf* sb = iter;
    int32_t pad = 0;
    int32_t total = prefixLen + suffixLen;
    if (ios->mWidth > total) pad = ios->mWidth - total;
    uint32_t adjust = ios->mFlags & 0xb0;
    const uint8_t f = static_cast<uint8_t>(fill);
    if (adjust != 0x20 && adjust != 0x10)
        for (int32_t i = 0; i < pad; i++) Put(sb, f);
    for (int32_t i = 0; i < prefixLen; i++) Put(sb, static_cast<uint8_t>(*prefix++));
    if (adjust == 0x10)
        for (int32_t i = 0; i < pad; i++) Put(sb, f);
    for (int32_t i = 0; i < suffixLen; i++) Put(sb, static_cast<uint8_t>(*suffix++));
    if (adjust == 0x20)
        for (int32_t i = 0; i < pad; i++) Put(sb, f);
    ios->mWidth = 0;
    return sb;
}
