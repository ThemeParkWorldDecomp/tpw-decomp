#include "bfl_Refpack.h"

namespace bfl::compress {
CCompressionBase::~CCompressionBase() {}
CRefpack::~CRefpack() {}
int32_t CRefpack::CompressWorkspaceSize() const { return 0xc0000; }
int32_t CRefpack::DecompressWorkspaceSize() const { return 0; }
int32_t CRefpack::DecompressedSize(uint8_t* source) const {
    const uint8_t* length = source + (source[0] == 0x10 && source[1] == 0xfb ? 2 : 5);
    return (length[0] << 16) | (length[1] << 8) | length[2];
}

int32_t CRefpack::MemDecompress(uint8_t* dest, uint8_t* source, uint8_t*) const {
    if (source == nullptr) return 0;
    const uint8_t* input = source + ((source[0] & 1) != 0 ? 5 : 2);
    const int32_t decodedSize = (input[0] << 16) | (input[1] << 8) | input[2];
    input += 3;
    for (;;) {
        const uint32_t command = *input++;
        uint32_t literal, length, distance;
        if ((command & 0x80) == 0) {
            const uint32_t second = *input++;
            literal = command & 3;
            length = ((command >> 2) & 7) + 3;
            distance = ((command & 0x60) << 3) + second + 1;
        } else if ((command & 0x40) == 0) {
            const uint32_t second = *input++;
            const uint32_t third = *input++;
            literal = second >> 6;
            length = (command & 0x3f) + 4;
            distance = ((second & 0x3f) << 8) + third + 1;
        } else if ((command & 0x20) == 0) {
            const uint32_t second = *input++;
            const uint32_t third = *input++;
            const uint32_t fourth = *input++;
            literal = command & 3;
            length = ((command & 0xc) << 6) + fourth + 5;
            distance = ((command & 0x10) << 12) + (second << 8) + third + 1;
        } else {
            literal = ((command & 0x1f) << 2) + 4;
            const bool final = literal > 0x70;
            if (final) literal = command & 3;
            for (uint32_t i = 0; i < literal; ++i) *dest++ = *input++;
            if (final) return decodedSize;
            continue;
        }
        for (uint32_t i = 0; i < literal; ++i) *dest++ = *input++;
        const uint8_t* match = dest - distance;
        for (uint32_t i = 0; i < length; ++i) *dest++ = *match++;
    }
}
}
