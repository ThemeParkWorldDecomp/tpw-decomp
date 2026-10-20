// ltms: the Unicode to multibyte converter (ltms shared, 0x100055fc..0x100059ac). Its
// loader is in TbResourceFactories.cpp.
#include "TbResource.h"

TbIUnicodeToMB::~TbIUnicodeToMB() {}

uint16_t TbIUnicodeToMB::LocateValue(char16_t value) {
    const auto* node = mData;
    for (;;) {
        if (value < node[0]) {
            if (node[1] == 0) return 0;
            node = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(node) + node[1]);
        } else if (value > node[2]) {
            if (node[3] == 0) return 0;
            node = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(node) + node[3]);
        } else {
            return node[4 + (uint16_t(value) - node[0])];
        }
    }
}

TbError TbIUnicodeToMB::ConvertString(unsigned char* out, const char16_t* source, uint16_t size) {
    uint16_t remaining = static_cast<uint16_t>(size - 2);
    while (*source != 0) {
        const auto before = remaining--;
        if (before == 0) break;
        const auto value = LocateValue(*source++);
        *out++ = static_cast<unsigned char>(value);
        if ((value & 0xff00) != 0 && remaining != 0) {
            *out++ = static_cast<unsigned char>(value >> 8);
            --remaining;
        }
    }
    *out = 0;
    return TbError(remaining == 0 && *source != 0 ? -1 : 0);
}
