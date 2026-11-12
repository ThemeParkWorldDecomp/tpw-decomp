// tpi_fsh.cpp: SHPI .fsh to TGA (tpi_fsh.h). The SHPI file: "SHPI", the file's length, the entry count, a four-letter
// directory id, then (name, offset) pairs of 4 bytes each from 16. An entry: a code byte (bit 7: the pixels are RefPack
// compressed), a 3-byte block length, width and height (16 bits each), 8 bytes of centre and position, then the pixels
// (code & 0x7f == 0x7b: one byte per pixel, a palette index). The blocks that follow the entry are its attachments:
// 0x24 a 24-bit palette (R, G, B per entry), 0x2a a 32-bit palette (B, G, R, A), 0x2d a 16-bit palette (1555), 0x70 a
// text; each attachment has the same code and 3-byte length and its entry count at +4; a block length of 0 ends the list.
#include "tpi_fsh.h"

#include <cstring>

#include "source.h"

namespace tpw::assets {

namespace {

uint32_t Get16(const std::vector<uint8_t>& d, size_t at) { return d[at] | (d[at + 1] << 8); }
uint32_t Get24(const std::vector<uint8_t>& d, size_t at) { return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16); }
uint32_t Get32(const std::vector<uint8_t>& d, size_t at) { return Get24(d, at) | (static_cast<uint32_t>(d[at + 3]) << 24); }

}  // namespace

bool IsTpiFsh(const std::vector<uint8_t>& bytes) { return bytes.size() >= 16 && std::memcmp(bytes.data(), "SHPI", 4) == 0; }

bool DecodeTpiFsh(const std::vector<uint8_t>& d, TpiImage& image, std::string& err) {
    if (!IsTpiFsh(d)) {
        err = "not a SHPI texture";
        return false;
    }
    const uint32_t count = Get32(d, 8);
    if (count < 1 || d.size() < 24) {
        err = "SHPI texture with no entry";
        return false;
    }
    const uint32_t off = Get32(d, 20);
    if (off + 16 > d.size()) {
        err = "SHPI entry offset past the end";
        return false;
    }
    const uint8_t code = d[off];
    const uint32_t block = Get24(d, off + 1);
    const uint32_t width = Get16(d, off + 4);
    const uint32_t height = Get16(d, off + 8 - 2);
    if ((code & 0x7f) != 0x7b) {
        err = "SHPI entry code 0x" + std::to_string(code) + " (only 8-bit paletted images are known)";
        return false;
    }
    if (width == 0 || height == 0 || width > 4096 || height > 4096) {
        err = "SHPI entry with an impossible size";
        return false;
    }
    const uint64_t pixels = static_cast<uint64_t>(width) * height;
    // The pixel block ends at the next attachment (block length) or at the file's end when the length is 0.
    size_t bodyEnd = block != 0 ? off + block : d.size();
    if (bodyEnd > d.size() || bodyEnd < off + 16) {
        err = "SHPI entry block past the end";
        return false;
    }
    std::vector<uint8_t> indices;
    if (code & 0x80) {
        // The packed pixels are padded to a multiple of 16 bytes (a 250 by 250 image unpacks to 62512 bytes).
        if (!RefPackDecode(d.data() + off + 16, bodyEnd - off - 16, (pixels + 15) / 16 * 16, indices, err)) return false;
    } else {
        indices.assign(d.begin() + static_cast<ptrdiff_t>(off + 16), d.begin() + static_cast<ptrdiff_t>(bodyEnd));
    }
    if (indices.size() < pixels) {
        err = "SHPI entry has fewer pixels than its size says";
        return false;
    }
    // The palette: the first attachment with a palette code.
    std::vector<uint8_t> palette;   // BGRA per entry
    size_t at = block != 0 ? off + block : d.size();
    while (at + 16 <= d.size()) {
        const uint8_t c = d[at];
        const uint32_t length = Get24(d, at + 1);
        const uint32_t entries = Get16(d, at + 4);
        const size_t data = at + 16;
        if (c == 0x24 || c == 0x2a || c == 0x2d) {
            const size_t per = c == 0x24 ? 3 : c == 0x2a ? 4 : 2;
            if (data + static_cast<size_t>(entries) * per > d.size()) {
                err = "SHPI palette past the end";
                return false;
            }
            palette.resize(static_cast<size_t>(entries) * 4);
            for (uint32_t i = 0; i < entries; ++i) {
                const size_t src = data + static_cast<size_t>(i) * per;
                uint8_t* dst = palette.data() + static_cast<size_t>(i) * 4;
                if (c == 0x24) {
                    dst[0] = d[src + 2];   // stored R, G, B
                    dst[1] = d[src + 1];
                    dst[2] = d[src];
                    dst[3] = 0xff;
                } else if (c == 0x2a) {
                    dst[0] = d[src];
                    dst[1] = d[src + 1];
                    dst[2] = d[src + 2];
                    dst[3] = d[src + 3];
                } else {
                    const uint32_t v = Get16(d, src);   // 1555: A at bit 15, R 10-14, G 5-9, B 0-4
                    const uint32_t b5 = v & 0x1f, g5 = (v >> 5) & 0x1f, r5 = (v >> 10) & 0x1f;
                    dst[0] = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
                    dst[1] = static_cast<uint8_t>((g5 << 3) | (g5 >> 2));
                    dst[2] = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
                    dst[3] = (v & 0x8000) ? 0xff : 0;
                }
            }
            break;
        }
        if (length == 0) break;
        at += length;
    }
    if (palette.empty()) {
        err = "SHPI image without a palette";
        return false;
    }
    const uint32_t paletteEntries = static_cast<uint32_t>(palette.size() / 4);
    image.width = width;
    image.height = height;
    image.bgra.resize(static_cast<size_t>(pixels) * 4);
    for (uint64_t i = 0; i < pixels; ++i) {
        uint32_t index = indices[static_cast<size_t>(i)];
        if (index >= paletteEntries) index = paletteEntries - 1;
        std::memcpy(image.bgra.data() + static_cast<size_t>(i) * 4, palette.data() + static_cast<size_t>(index) * 4, 4);
    }
    return true;
}

std::vector<uint8_t> EncodeTpiTga32(const TpiImage& image) {
    std::vector<uint8_t> out(18 + static_cast<size_t>(image.width) * image.height * 4, 0);
    out[2] = 2;   // uncompressed true colour
    out[12] = static_cast<uint8_t>(image.width);
    out[13] = static_cast<uint8_t>(image.width >> 8);
    out[14] = static_cast<uint8_t>(image.height);
    out[15] = static_cast<uint8_t>(image.height >> 8);
    out[16] = 32;
    out[17] = 8;   // 8 alpha bits, rows bottom-up
    const size_t rowBytes = static_cast<size_t>(image.width) * 4;
    for (uint32_t y = 0; y < image.height; ++y) {
        const uint8_t* src = image.bgra.data() + static_cast<size_t>(y) * rowBytes;
        uint8_t* dst = out.data() + 18 + static_cast<size_t>(image.height - 1 - y) * rowBytes;
        std::memcpy(dst, src, rowBytes);
    }
    return out;
}

bool TpiFshToTga(const std::vector<uint8_t>& fsh, std::vector<uint8_t>& tga, std::string& err) {
    TpiImage image;
    if (!DecodeTpiFsh(fsh, image, err)) return false;
    tga = EncodeTpiTga32(image);
    return true;
}

}  // namespace tpw::assets
