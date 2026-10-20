// ltms: multibyte character sets and the multibyte to Unicode converter (ltms shared,
// 0x1000063c, 0x10002f3c, 0x10003204..0x10003338). Their loaders are in
// TbResourceFactories.cpp.
#include "TbResource.h"

TbIMBInfo::~TbIMBInfo() {}
TbIMBToUnicode::~TbIMBToUnicode() {}

uint32_t TbIMBInfo::GetIndex(const unsigned char*& text) const {
    uint32_t index = static_cast<uint32_t>(*text++) - 1;
    if (mFirstLead <= (index & 0xffff)) {
        index = ((index & 0xffff) * 0xff + static_cast<uint32_t>(mFirstLead) * static_cast<uint32_t>(-0xfe) +
                 *text++) - 1;
    }
    return index;
}

// The counter is 16 bits and is decremented before it is tested, so running out of room
// leaves it at 0xffff and the "too small" answer comes only when the text ends with the
// counter exactly at zero (the last character is then overwritten by the terminator).
TbError TbIMBToUnicode::ConvertString(char16_t* out, const unsigned char* source, uint16_t size) {
    uint16_t remaining = size;
    while (*source != 0) {
        const bool exhausted = remaining == 0;
        --remaining;
        if (exhausted) break;
        uint16_t index = static_cast<uint16_t>(*source++ - 1);
        if (mFirstLead <= index)
            index = static_cast<uint16_t>(index * 0xff + mFirstLead * -0xfe + *source++ - 1);
        *out++ = index < mCount ? mTable[index] : 0;
    }
    if (remaining == 0) {
        out[-1] = 0;
        return TbError(-1);
    }
    *out = 0;
    return TbError(0);
}
