// icon.cpp: readers for the places the player's copy of the game keeps its application icon (icon.h), and the icon.png they
// are kept as. Every reader is defensive: offsets and lengths are checked against the bytes given, and a picture that does
// not add up is skipped, never an error (the icon is a nicety; the import's result does not depend on it).
#include "icon.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <zlib.h>

namespace tpw::assets {

namespace {

struct Bytes {
    const uint8_t* p;
    size_t n;
    bool has(uint64_t at, uint64_t len) const { return at <= n && len <= n - at; }
    uint32_t be16(size_t at) const { return (uint32_t(p[at]) << 8) | p[at + 1]; }
    uint32_t be32(size_t at) const { return (be16(at) << 16) | be16(at + 2); }
    uint32_t le16(size_t at) const { return uint32_t(p[at]) | (uint32_t(p[at + 1]) << 8); }
    uint32_t le32(size_t at) const { return le16(at) | (le16(at + 2) << 16); }
};

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
    return s;
}

std::string Basename(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool Consider(IconImage& best, IconImage candidate) {
    if (candidate.Empty() || !BetterIcon(candidate, best)) return false;
    best = std::move(candidate);
    return true;
}

// ---- the classic Mac icon families (Inside Macintosh, "Icon Utilities")

// The System 7 256-colour table: a 6 x 6 x 6 cube of FF, CC, 99, 66, 33, 00 (red outermost, without its final black),
// ramps of EE, DD, BB, AA, 88, 77, 55, 44, 22, 11 for red, green, blue and grey, and black last.
void Clut8(uint8_t (&clut)[256][3]) {
    static const uint8_t level[6] = {0xff, 0xcc, 0x99, 0x66, 0x33, 0x00};
    static const uint8_t ramp[10] = {0xee, 0xdd, 0xbb, 0xaa, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11};
    int i = 0;
    for (uint8_t r : level)
        for (uint8_t g : level)
            for (uint8_t b : level) {
                if (i == 215) break;
                clut[i][0] = r, clut[i][1] = g, clut[i][2] = b;
                ++i;
            }
    for (int c = 0; c < 4; ++c)
        for (uint8_t v : ramp) {
            clut[i][0] = (c == 0 || c == 3) ? v : 0;
            clut[i][1] = (c == 1 || c == 3) ? v : 0;
            clut[i][2] = (c == 2 || c == 3) ? v : 0;
            ++i;
        }
    clut[255][0] = clut[255][1] = clut[255][2] = 0;
}

// The 16-colour table: white, yellow, orange, red, magenta, purple, blue, cyan, green, dark green, brown, tan, light
// grey, medium grey, dark grey, black.
const uint8_t kClut4[16][3] = {{0xff, 0xff, 0xff}, {0xfc, 0xf3, 0x05}, {0xff, 0x64, 0x02}, {0xdd, 0x08, 0x06},
                               {0xf2, 0x08, 0x84}, {0x46, 0x00, 0xa5}, {0x00, 0x00, 0xd4}, {0x02, 0xab, 0xea},
                               {0x1f, 0xb7, 0x14}, {0x00, 0x64, 0x11}, {0x56, 0x2c, 0x05}, {0x90, 0x71, 0x3a},
                               {0xc0, 0xc0, 0xc0}, {0x80, 0x80, 0x80}, {0x40, 0x40, 0x40}, {0x00, 0x00, 0x00}};

constexpr uint32_t FourCC(const char (&s)[5]) {
    return (uint32_t(uint8_t(s[0])) << 24) | (uint32_t(uint8_t(s[1])) << 16) | (uint32_t(uint8_t(s[2])) << 8) | uint8_t(s[3]);
}

// An indexed or 1-bit n x n picture with a 1-bit mask (`mask`: n*n/8 bytes, 1 = opaque; null: all opaque).
IconImage IndexedIcon(const uint8_t* data, int n, int depth, const uint8_t* mask) {
    IconImage out;
    out.width = out.height = n;
    out.bitDepth = depth;
    out.rgba.resize(static_cast<size_t>(n) * n * 4);
    uint8_t clut8[256][3] = {};
    if (depth == 8) Clut8(clut8);
    for (int i = 0; i < n * n; ++i) {
        uint8_t r = 0, g = 0, b = 0;
        if (depth == 8) {
            r = clut8[data[i]][0], g = clut8[data[i]][1], b = clut8[data[i]][2];
        } else if (depth == 4) {
            const int index = (data[i / 2] >> ((i % 2 == 0) ? 4 : 0)) & 15;
            r = kClut4[index][0], g = kClut4[index][1], b = kClut4[index][2];
        } else {
            const bool black = (data[i / 8] >> (7 - i % 8)) & 1;
            r = g = b = black ? 0 : 255;
        }
        const bool opaque = mask == nullptr || ((mask[i / 8] >> (7 - i % 8)) & 1);
        uint8_t* px = &out.rgba[static_cast<size_t>(i) * 4];
        px[0] = r, px[1] = g, px[2] = b, px[3] = opaque ? 255 : 0;
    }
    return out;
}

// The 32-bit icon families' channel packing (count < 128: the next count + 1 bytes as they are; else the next byte
// count - 125 times). False when the data does not fill `count` bytes.
bool UnpackRle(const uint8_t* d, size_t n, size_t count, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(count);
    size_t i = 0;
    while (out.size() < count && i < n) {
        const unsigned c = d[i++];
        if (c < 128) {
            const size_t take = std::min<size_t>(c + 1, n - i);
            out.insert(out.end(), d + i, d + i + take);
            i += take;
        } else {
            if (i >= n) break;
            out.insert(out.end(), c - 125, d[i++]);
        }
    }
    if (out.size() < count) return false;
    out.resize(count);
    return true;
}

// An il32/ih32/it32 picture (RLE channels, or raw ARGB or RGB) with its 8-bit mask (l8mk/h8mk/t8mk; null: from the
// 1-bit `mask1`, or opaque).
IconImage Icon32(const uint8_t* data, size_t n, int size, const uint8_t* mask8, size_t mask8Size, const uint8_t* mask1) {
    IconImage out;
    const size_t pixels = static_cast<size_t>(size) * size;
    if (size == 128 && n >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0) data += 4, n -= 4;
    std::vector<uint8_t> channels;
    const uint8_t* r = nullptr;
    const uint8_t* g = nullptr;
    const uint8_t* b = nullptr;
    size_t step = 1;
    if (n == pixels * 4) {
        r = data + 1, g = data + 2, b = data + 3, step = 4;
    } else if (n == pixels * 3) {
        r = data, g = data + 1, b = data + 2, step = 3;
    } else {
        if (!UnpackRle(data, n, pixels * 3, channels)) return out;
        r = channels.data(), g = r + pixels, b = g + pixels;
    }
    if (mask8 != nullptr && mask8Size < pixels) mask8 = nullptr;
    out.width = out.height = size;
    out.bitDepth = 32;
    out.rgba.resize(pixels * 4);
    for (size_t i = 0; i < pixels; ++i) {
        uint8_t* px = &out.rgba[i * 4];
        px[0] = r[i * step], px[1] = g[i * step], px[2] = b[i * step];
        if (mask8 != nullptr) px[3] = mask8[i];
        else if (mask1 != nullptr) px[3] = ((mask1[i / 8] >> (7 - i % 8)) & 1) ? 255 : 0;
        else px[3] = 255;
    }
    return out;
}

struct IconElement {
    uint32_t type;
    const uint8_t* data;
    size_t size;
};

const IconElement* Find(const std::vector<IconElement>& elements, uint32_t type, size_t atLeast) {
    for (const IconElement& e : elements)
        if (e.type == type && e.size >= atLeast) return &e;
    return nullptr;
}

// The best picture of one icon family (one resource id, or one 'icns' container): the elements of a classic suite and
// of the 32-bit families, each with its mask.
IconImage BestOfFamily(const std::vector<IconElement>& elements) {
    IconImage best;
    struct Classic {
        uint32_t type, maskType;
        int size, depth;
    };
    static const Classic kClassic[] = {
        {FourCC("ich8"), FourCC("ich#"), 48, 8}, {FourCC("ich4"), FourCC("ich#"), 48, 4}, {FourCC("ich#"), FourCC("ich#"), 48, 1},
        {FourCC("icl8"), FourCC("ICN#"), 32, 8}, {FourCC("icl4"), FourCC("ICN#"), 32, 4}, {FourCC("ICN#"), FourCC("ICN#"), 32, 1},
        {FourCC("ics8"), FourCC("ics#"), 16, 8}, {FourCC("ics4"), FourCC("ics#"), 16, 4}, {FourCC("ics#"), FourCC("ics#"), 16, 1},
    };
    for (const Classic& c : kClassic) {
        const size_t bytes = static_cast<size_t>(c.size) * c.size * c.depth / 8;
        const IconElement* pic = Find(elements, c.type, bytes);
        if (pic == nullptr) continue;
        const size_t maskBytes = static_cast<size_t>(c.size) * c.size / 8;
        const IconElement* mask = Find(elements, c.maskType, maskBytes * 2);
        Consider(best, IndexedIcon(pic->data, c.size, c.depth, mask != nullptr ? mask->data + maskBytes : nullptr));
    }
    struct Deep {
        uint32_t type, maskType, mask1Type;
        int size;
    };
    static const Deep kDeep[] = {
        {FourCC("it32"), FourCC("t8mk"), 0, 128},
        {FourCC("ih32"), FourCC("h8mk"), FourCC("ich#"), 48},
        {FourCC("il32"), FourCC("l8mk"), FourCC("ICN#"), 32},
        {FourCC("is32"), FourCC("s8mk"), FourCC("ics#"), 16},
    };
    for (const Deep& d : kDeep) {
        const IconElement* pic = Find(elements, d.type, 1);
        if (pic == nullptr) continue;
        const size_t pixels = static_cast<size_t>(d.size) * d.size;
        const IconElement* mask8 = Find(elements, d.maskType, pixels);
        const IconElement* mask1 = d.mask1Type != 0 ? Find(elements, d.mask1Type, pixels / 8 * 2) : nullptr;
        Consider(best, Icon32(pic->data, pic->size, d.size, mask8 != nullptr ? mask8->data : nullptr,
                              mask8 != nullptr ? mask8->size : 0, mask1 != nullptr ? mask1->data + pixels / 8 : nullptr));
    }
    return best;
}

// An 'icns' container ("icns", total length, then elements of type, length, data) as its elements.
void IcnsElements(const uint8_t* data, size_t size, std::vector<IconElement>& out) {
    const Bytes b{data, size};
    if (!b.has(0, 8) || b.be32(0) != FourCC("icns")) return;
    size_t at = 8;
    const size_t end = std::min<size_t>(size, b.be32(4));
    while (at + 8 <= end) {
        const uint32_t type = b.be32(at);
        const uint32_t length = b.be32(at + 4);
        if (length < 8 || length > end - at) break;
        out.push_back({type, data + at + 8, length - 8});
        at += length;
    }
}

// A classic resource fork (header, data, map): the icon resources grouped by id, the application's own (the lowest id
// at a tie) winning.
bool DecodeResourceFork(const uint8_t* fork, size_t size, IconImage& out) {
    const Bytes b{fork, size};
    if (!b.has(0, 16)) return false;
    const uint64_t dataOffset = b.be32(0), mapOffset = b.be32(4), dataLength = b.be32(8), mapLength = b.be32(12);
    if (!b.has(dataOffset, dataLength) || !b.has(mapOffset, mapLength) || mapLength < 30) return false;
    const uint64_t mapEnd = mapOffset + mapLength;
    const uint64_t typeList = mapOffset + b.be16(static_cast<size_t>(mapOffset) + 24);
    if (typeList + 2 > mapEnd) return false;
    const uint32_t typeCount = (b.be16(static_cast<size_t>(typeList)) + 1) & 0xffff;
    if (typeList + 2 + uint64_t(typeCount) * 8 > mapEnd) return false;
    struct Found {
        int id;
        IconElement element;
    };
    std::vector<Found> found;
    for (uint32_t t = 0; t < typeCount; ++t) {
        const size_t entry = static_cast<size_t>(typeList) + 2 + t * 8;
        const uint32_t type = b.be32(entry);
        const uint32_t count = b.be16(entry + 4) + 1;
        const uint64_t references = typeList + b.be16(entry + 6);
        if (references + uint64_t(count) * 12 > mapEnd) return false;
        // 'icns' and the classic families ('ICN#', 'icl8', 'icl4', 'ich8', 'ich#', 'ics8', ...): "ic..." or "ICN."
        const bool wanted = (type >> 16) == ((uint32_t('i') << 8) | 'c') || (type >> 8) == (FourCC("ICN#") >> 8);
        if (!wanted) continue;
        for (uint32_t r = 0; r < count; ++r) {
            const size_t reference = static_cast<size_t>(references) + r * 12;
            const int id = static_cast<int16_t>(b.be16(reference));
            const uint64_t offset = b.be32(reference + 4) & 0xffffff;
            if (offset + 4 > dataLength) continue;
            const uint64_t length = b.be32(static_cast<size_t>(dataOffset + offset));
            if (offset + 4 + length > dataLength) continue;
            found.push_back({id, {type, fork + dataOffset + offset + 4, static_cast<size_t>(length)}});
        }
    }
    if (found.empty()) return false;
    std::stable_sort(found.begin(), found.end(), [](const Found& x, const Found& y) { return x.id < y.id; });
    IconImage best;
    for (size_t i = 0; i < found.size();) {
        const int id = found[i].id;
        std::vector<IconElement> family;
        for (; i < found.size() && found[i].id == id; ++i) {
            if (found[i].element.type == FourCC("icns")) IcnsElements(found[i].element.data, found[i].element.size, family);
            else family.push_back(found[i].element);
        }
        Consider(best, BestOfFamily(family));   // a later (higher) id must be strictly better to win
    }
    if (best.Empty()) return false;
    out = std::move(best);
    return true;
}

// ---- Windows icons

// A device-independent bitmap as .ico files and RT_ICON resources hold one: BITMAPINFOHEADER with the height doubled,
// a BGRX palette for 8 bits and fewer, bottom-up colour rows, then the 1-bit AND mask (1: see-through).
IconImage DecodeDib(const uint8_t* data, size_t size) {
    IconImage out;
    const Bytes b{data, size};
    if (!b.has(0, 40) || b.le32(0) < 40) return out;
    const size_t header = b.le32(0);
    const int32_t width = static_cast<int32_t>(b.le32(4));
    const int32_t height2 = static_cast<int32_t>(b.le32(8));
    const unsigned bpp = b.le16(14);
    const uint32_t compression = b.le32(16);
    uint32_t colours = b.le32(32);
    if (width <= 0 || width > 1024 || height2 <= 0 || height2 % 2 != 0 || height2 / 2 > 1024 || compression != 0) return out;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) return out;
    const int height = height2 / 2;
    if (bpp <= 8) {
        if (colours == 0 || colours > (1u << bpp)) colours = 1u << bpp;
    } else {
        colours = 0;
    }
    const size_t palette = header;
    const size_t xorAt = palette + size_t(colours) * 4;
    const size_t xorStride = ((size_t(width) * bpp + 31) / 32) * 4;
    const size_t andStride = ((size_t(width) + 31) / 32) * 4;
    const size_t andAt = xorAt + xorStride * height;
    if (!b.has(xorAt, xorStride * height)) return out;
    const bool hasMask = b.has(andAt, andStride * height);
    out.width = width;
    out.height = height;
    out.bitDepth = static_cast<int>(bpp);
    out.rgba.resize(size_t(width) * height * 4);
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = data + xorAt + xorStride * (height - 1 - y);
        const uint8_t* mask = hasMask ? data + andAt + andStride * (height - 1 - y) : nullptr;
        for (int x = 0; x < width; ++x) {
            uint8_t r = 0, g = 0, bl = 0, a = 255;
            if (bpp <= 8) {
                unsigned index = 0;
                if (bpp == 8) index = row[x];
                else if (bpp == 4) index = (row[x / 2] >> ((x % 2 == 0) ? 4 : 0)) & 15;
                else index = (row[x / 8] >> (7 - x % 8)) & 1;
                if (index >= colours) index = 0;
                const uint8_t* pal = data + palette + index * 4;
                bl = pal[0], g = pal[1], r = pal[2];
            } else if (bpp == 24) {
                bl = row[x * 3], g = row[x * 3 + 1], r = row[x * 3 + 2];
            } else {
                bl = row[x * 4], g = row[x * 4 + 1], r = row[x * 4 + 2], a = row[x * 4 + 3];
            }
            if (bpp != 32 && mask != nullptr && ((mask[x / 8] >> (7 - x % 8)) & 1)) a = 0;
            uint8_t* px = &out.rgba[(size_t(y) * width + x) * 4];
            px[0] = r, px[1] = g, px[2] = bl, px[3] = a;
        }
    }
    return out;
}

// A .ico file: a directory of pictures (a PNG picture is skipped; the 1999 icons have none).
bool DecodeIco(const uint8_t* data, size_t size, IconImage& out) {
    const Bytes b{data, size};
    if (!b.has(0, 6) || b.le16(0) != 0 || b.le16(2) != 1) return false;
    const unsigned count = b.le16(4);
    if (count == 0 || !b.has(6, size_t(count) * 16)) return false;
    IconImage best;
    for (unsigned i = 0; i < count; ++i) {
        const size_t entry = 6 + i * 16;
        const uint32_t bytes = b.le32(entry + 8), offset = b.le32(entry + 12);
        if (!b.has(offset, bytes)) continue;
        Consider(best, DecodeDib(data + offset, bytes));
    }
    if (best.Empty()) return false;
    out = std::move(best);
    return true;
}

// A Windows program (PE): the pictures of its RT_ICON resources (type 3), the best of them.
bool DecodeExe(const uint8_t* data, size_t size, IconImage& out) {
    const Bytes b{data, size};
    if (!b.has(0, 64) || b.le16(0) != 0x5a4d) return false;
    const uint32_t pe = b.le32(60);
    if (!b.has(pe, 24) || b.le32(pe) != 0x4550) return false;
    const unsigned sections = b.le16(pe + 6);
    const unsigned optionalSize = b.le16(pe + 20);
    const size_t optional = pe + 24;
    if (!b.has(optional, optionalSize) || optionalSize < 96 + 8) return false;
    const unsigned magic = b.le16(optional);
    const size_t directories = magic == 0x20b ? optional + 112 : optional + 96;
    if (!b.has(directories, 3 * 8)) return false;
    const uint32_t resourceRva = b.le32(directories + 2 * 8);
    const uint32_t resourceSize = b.le32(directories + 2 * 8 + 4);
    if (resourceRva == 0 || resourceSize == 0) return false;
    struct Section {
        uint32_t va, vsize, raw, rawSize;
    };
    std::vector<Section> table;
    const size_t sectionAt = optional + optionalSize;
    if (!b.has(sectionAt, size_t(sections) * 40)) return false;
    for (unsigned s = 0; s < sections; ++s) {
        const size_t h = sectionAt + s * 40;
        table.push_back({b.le32(h + 12), b.le32(h + 8), b.le32(h + 20), b.le32(h + 16)});
    }
    auto fileOffset = [&](uint32_t rva, uint32_t length, size_t& at) {
        for (const Section& s : table) {
            const uint32_t extent = std::max(s.vsize, s.rawSize);
            if (rva >= s.va && rva - s.va < extent && length <= extent - (rva - s.va)) {
                at = size_t(s.raw) + (rva - s.va);
                return b.has(at, length);
            }
        }
        return false;
    };
    size_t root = 0;
    if (!fileOffset(resourceRva, resourceSize, root)) return false;
    const size_t end = root + resourceSize;
    auto entries = [&](size_t dir, std::vector<std::pair<uint32_t, uint32_t>>& list) {
        list.clear();
        if (dir + 16 > end) return false;
        const unsigned count = b.le16(dir + 12) + b.le16(dir + 14);
        if (dir + 16 + size_t(count) * 8 > end) return false;
        for (unsigned i = 0; i < count; ++i) list.push_back({b.le32(dir + 16 + i * 8), b.le32(dir + 16 + i * 8 + 4)});
        return true;
    };
    IconImage best;
    std::vector<std::pair<uint32_t, uint32_t>> types, names, languages;
    if (!entries(root, types)) return false;
    for (const auto& type : types) {
        if (type.first != 3 || !(type.second & 0x80000000u)) continue;   // RT_ICON, a subdirectory
        if (!entries(root + (type.second & 0x7fffffffu), names)) continue;
        for (const auto& name : names) {
            if (!(name.second & 0x80000000u)) continue;
            if (!entries(root + (name.second & 0x7fffffffu), languages)) continue;
            for (const auto& language : languages) {
                if (language.second & 0x80000000u) continue;
                const size_t dataEntry = root + language.second;
                if (dataEntry + 16 > end) continue;
                size_t at = 0;
                const uint32_t rva = b.le32(dataEntry), length = b.le32(dataEntry + 4);
                if (length == 0 || length > (64u << 20) || !fileOffset(rva, length, at)) continue;
                Consider(best, DecodeDib(data + at, length));
            }
        }
    }
    if (best.Empty()) return false;
    out = std::move(best);
    return true;
}

// ---- PNG

void Put32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(uint8_t(v >> 24)), out.push_back(uint8_t(v >> 16)), out.push_back(uint8_t(v >> 8)), out.push_back(uint8_t(v));
}

void Chunk(std::vector<uint8_t>& out, const char (&type)[5], const std::vector<uint8_t>& data) {
    Put32(out, static_cast<uint32_t>(data.size()));
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    Put32(out, static_cast<uint32_t>(crc32(0, out.data() + start, static_cast<uInt>(out.size() - start))));
}

int Paeth(int a, int b, int c) {
    const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

}  // namespace

IconFile IconFileKind(const std::string& path, bool resourceFork) {
    const std::string name = Lower(Basename(path));
    if (name.empty()) return IconFile::None;
    const bool themePark = name.find("theme park") != std::string::npos || name.find("themepark") != std::string::npos;
    if (resourceFork) return themePark ? IconFile::ResourceFork : IconFile::None;
    const size_t dot = name.find_last_of('.');
    const std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
    const std::string ext = dot == std::string::npos ? std::string() : name.substr(dot + 1);
    if (ext == "ico") return (stem == "tp" || stem == "tpw" || themePark) ? IconFile::Ico : IconFile::None;
    if (ext == "exe") {
        static const char* const kPrograms[] = {"tp", "tpw", "tpworld", "themeparkworld", "simthemepark", "theme park world",
                                                "sim theme park", "simtheme park"};
        for (const char* p : kPrograms)
            if (stem == p) return IconFile::Exe;
    }
    return IconFile::None;
}

bool DecodeIcon(IconFile kind, const uint8_t* data, size_t size, IconImage& out) {
    if (data == nullptr || size == 0) return false;
    switch (kind) {
        case IconFile::ResourceFork: return DecodeResourceFork(data, size, out);
        case IconFile::Ico: return DecodeIco(data, size, out);
        case IconFile::Exe: return DecodeExe(data, size, out);
        case IconFile::None: break;
    }
    return false;
}

bool BetterIcon(const IconImage& candidate, const IconImage& current) {
    if (candidate.Empty()) return false;
    if (current.Empty()) return true;
    const long long a = 1ll * candidate.width * candidate.height, b = 1ll * current.width * current.height;
    if (a != b) return a > b;
    return candidate.bitDepth > current.bitDepth;
}

std::vector<uint8_t> EncodeIconPng(const IconImage& icon, const std::string& source) {
    std::vector<uint8_t> out;
    if (icon.Empty() || icon.rgba.size() < size_t(icon.width) * icon.height * 4) return out;
    static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    out.insert(out.end(), kSignature, kSignature + 8);
    std::vector<uint8_t> ihdr;
    Put32(ihdr, static_cast<uint32_t>(icon.width));
    Put32(ihdr, static_cast<uint32_t>(icon.height));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});   // 8 bits, RGBA, deflate, no filter method but per-row, not interlaced
    Chunk(out, "IHDR", ihdr);
    char text[160];
    std::snprintf(text, sizeof text, "%dx%d, %d-bit", icon.width, icon.height, icon.bitDepth);
    std::vector<uint8_t> source_;
    const char* keyword = "Source";
    source_.insert(source_.end(), keyword, keyword + 7);   // with its terminating zero
    std::string said = source.empty() ? std::string(text) : source + ", " + text;
    for (char& c : said)
        if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) c = '?';   // tEXt is Latin-1; keep it plain
    source_.insert(source_.end(), said.begin(), said.end());
    Chunk(out, "tEXt", source_);
    const size_t stride = size_t(icon.width) * 4;
    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * icon.height);
    for (int y = 0; y < icon.height; ++y) {
        raw.push_back(0);   // filter: none
        raw.insert(raw.end(), icon.rgba.begin() + y * stride, icon.rgba.begin() + (y + 1) * stride);
    }
    uLongf packed = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> idat(packed);
    if (compress2(idat.data(), &packed, raw.data(), static_cast<uLong>(raw.size()), 9) != Z_OK) return {};
    idat.resize(packed);
    Chunk(out, "IDAT", idat);
    Chunk(out, "IEND", {});
    return out;
}

bool DecodeIconPng(const uint8_t* data, size_t size, IconImage& out, std::string* source) {
    const Bytes b{data, size};
    static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (!b.has(0, 8) || std::memcmp(data, kSignature, 8) != 0) return false;
    size_t at = 8;
    int width = 0, height = 0;
    std::vector<uint8_t> idat;
    std::string text;
    bool sawHeader = false;
    while (b.has(at, 12)) {
        const uint32_t length = b.be32(at);
        if (!b.has(at + 8, uint64_t(length) + 4)) return false;
        const uint8_t* chunk = data + at + 8;
        if (std::memcmp(data + at + 4, "IHDR", 4) == 0) {
            if (length != 13) return false;
            width = static_cast<int>(b.be32(at + 8));
            height = static_cast<int>(b.be32(at + 12));
            if (chunk[8] != 8 || chunk[9] != 6 || chunk[10] != 0 || chunk[11] != 0 || chunk[12] != 0) return false;
            if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
            sawHeader = true;
        } else if (std::memcmp(data + at + 4, "IDAT", 4) == 0) {
            idat.insert(idat.end(), chunk, chunk + length);
        } else if (std::memcmp(data + at + 4, "tEXt", 4) == 0) {
            const std::string whole(reinterpret_cast<const char*>(chunk), length);
            const size_t zero = whole.find('\0');
            if (zero != std::string::npos && whole.substr(0, zero) == "Source") text = whole.substr(zero + 1);
        } else if (std::memcmp(data + at + 4, "IEND", 4) == 0) {
            break;
        }
        at += 12 + size_t(length);
    }
    if (!sawHeader || idat.empty()) return false;
    const size_t stride = size_t(width) * 4;
    std::vector<uint8_t> raw((stride + 1) * height);
    uLongf rawSize = static_cast<uLongf>(raw.size());
    if (uncompress(raw.data(), &rawSize, idat.data(), static_cast<uLong>(idat.size())) != Z_OK || rawSize != raw.size()) return false;
    IconImage image;
    image.width = width;
    image.height = height;
    image.rgba.resize(stride * height);
    std::vector<uint8_t> previous(stride, 0);
    for (int y = 0; y < height; ++y) {
        const uint8_t filter = raw[(stride + 1) * y];
        const uint8_t* row = &raw[(stride + 1) * y + 1];
        uint8_t* line = &image.rgba[stride * y];
        for (size_t i = 0; i < stride; ++i) {
            const int a = i >= 4 ? line[i - 4] : 0, up = previous[i], c = i >= 4 ? previous[i - 4] : 0;
            int v = row[i];
            switch (filter) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += up; break;
                case 3: v += (a + up) / 2; break;
                case 4: v += Paeth(a, up, c); break;
                default: return false;
            }
            line[i] = static_cast<uint8_t>(v);
        }
        std::memcpy(previous.data(), line, stride);
    }
    image.bitDepth = 32;
    // "..., 64x64, 8-bit": the depth from the end of the text
    const size_t comma = text.find_last_of(',');
    int depth = 0;
    if (comma != std::string::npos && std::sscanf(text.c_str() + comma + 1, " %d-bit", &depth) == 1 && depth > 0 && depth <= 32)
        image.bitDepth = depth;
    if (source != nullptr) *source = text;
    out = std::move(image);
    return true;
}

bool ReadIconPng(const std::string& path, IconImage& out, std::string* source) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    std::vector<uint8_t> bytes;
    uint8_t buffer[1 << 16];
    size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) {
        bytes.insert(bytes.end(), buffer, buffer + got);
        if (bytes.size() > (8u << 20)) break;
    }
    std::fclose(f);
    return DecodeIconPng(bytes.data(), bytes.size(), out, source);
}

namespace {

// `bytes` as the file `path`, through `path`.part and a rename, so that no reader sees half a file.
bool WriteWhole(const std::string& path, const std::vector<uint8_t>& bytes, std::string& err) {
    if (bytes.empty()) {
        err = "no picture to write";
        return false;
    }
    const std::string part = path + ".part";
    FILE* f = std::fopen(part.c_str(), "wb");
    if (f == nullptr) {
        err = "cannot write " + part;
        return false;
    }
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (std::fclose(f) != 0 || !ok) {
        std::remove(part.c_str());
        err = "cannot write " + part;
        return false;
    }
    if (std::rename(part.c_str(), path.c_str()) != 0) {
        std::remove(part.c_str());
        err = "cannot replace " + path;
        return false;
    }
    return true;
}

}  // namespace

bool WriteIconPng(const std::string& path, const IconImage& icon, const std::string& source, std::string& err) {
    return WriteWhole(path, EncodeIconPng(icon, source), err);
}

// ---- .ico

namespace {

void PutLe16(std::vector<uint8_t>& out, unsigned v) { out.push_back(uint8_t(v)), out.push_back(uint8_t(v >> 8)); }
void PutLe32(std::vector<uint8_t>& out, uint32_t v) { PutLe16(out, v & 0xffff), PutLe16(out, v >> 16); }

// `src` scaled to `n` x `n`: each pixel the area average of the source pixels under it (a box filter, exact for any
// ratio), the colours weighted by their alpha so that see-through pixels lend no colour to the edge.
IconImage Downscale(const IconImage& src, int n) {
    IconImage out;
    out.width = out.height = n;
    out.bitDepth = 32;
    out.rgba.assign(size_t(n) * n * 4, 0);
    const double sx = double(src.width) / n, sy = double(src.height) / n;
    for (int dy = 0; dy < n; ++dy) {
        const double y0 = dy * sy, y1 = (dy + 1) * sy;
        for (int dx = 0; dx < n; ++dx) {
            const double x0 = dx * sx, x1 = (dx + 1) * sx;
            double r = 0, g = 0, b = 0, a = 0, area = 0;
            for (int y = int(y0); y < src.height && y < y1; ++y) {
                const double wy = std::min(y1, double(y + 1)) - std::max(y0, double(y));
                if (wy <= 0) continue;
                for (int x = int(x0); x < src.width && x < x1; ++x) {
                    const double wx = std::min(x1, double(x + 1)) - std::max(x0, double(x));
                    if (wx <= 0) continue;
                    const double w = wx * wy;
                    const uint8_t* p = &src.rgba[(size_t(y) * src.width + x) * 4];
                    const double wa = w * p[3];
                    r += wa * p[0], g += wa * p[1], b += wa * p[2], a += wa, area += w;
                }
            }
            uint8_t* o = &out.rgba[(size_t(dy) * n + dx) * 4];
            if (a > 0) {
                o[0] = uint8_t(r / a + 0.5), o[1] = uint8_t(g / a + 0.5), o[2] = uint8_t(b / a + 0.5);
                o[3] = uint8_t(std::min(255.0, a / area + 0.5));
            }
        }
    }
    return out;
}

// One picture of a .ico as a 32-bit device-independent bitmap: BITMAPINFOHEADER with the height doubled, bottom-up BGRA
// rows, then the 1-bit AND mask (1 where the pixel is see-through), as DecodeDib reads it.
std::vector<uint8_t> IcoDib(const IconImage& img) {
    std::vector<uint8_t> out;
    const size_t andStride = ((size_t(img.width) + 31) / 32) * 4;
    PutLe32(out, 40), PutLe32(out, uint32_t(img.width)), PutLe32(out, uint32_t(img.height) * 2);
    PutLe16(out, 1), PutLe16(out, 32), PutLe32(out, 0);
    PutLe32(out, uint32_t(size_t(img.width) * img.height * 4 + andStride * img.height));
    PutLe32(out, 0), PutLe32(out, 0), PutLe32(out, 0), PutLe32(out, 0);
    for (int y = img.height - 1; y >= 0; --y) {
        const uint8_t* row = &img.rgba[size_t(y) * img.width * 4];
        for (int x = 0; x < img.width; ++x) {
            const uint8_t* p = row + x * 4;
            out.push_back(p[2]), out.push_back(p[1]), out.push_back(p[0]), out.push_back(p[3]);
        }
    }
    for (int y = img.height - 1; y >= 0; --y) {
        const uint8_t* row = &img.rgba[size_t(y) * img.width * 4];
        std::vector<uint8_t> mask(andStride, 0);
        for (int x = 0; x < img.width; ++x)
            if (row[x * 4 + 3] == 0) mask[x / 8] |= uint8_t(0x80 >> (x % 8));
        out.insert(out.end(), mask.begin(), mask.end());
    }
    return out;
}

}  // namespace

std::vector<uint8_t> EncodeIco(const IconImage& icon) {
    std::vector<uint8_t> out;
    if (icon.Empty() || icon.rgba.size() < size_t(icon.width) * icon.height * 4 || icon.width > 256 || icon.height > 256) return out;
    const int own = std::max(icon.width, icon.height);
    std::vector<int> sizes;
    for (int n : {16, 24, 32, 48, 64, 128, 256})
        if (n < own) sizes.push_back(n);
    sizes.push_back(own);
    std::vector<std::vector<uint8_t>> pictures;
    for (int n : sizes) {
        const IconImage img = n == own && icon.width == icon.height ? icon : Downscale(icon, n);
        pictures.push_back(n > 128 ? EncodeIconPng(img, "") : IcoDib(img));
    }
    PutLe16(out, 0), PutLe16(out, 1), PutLe16(out, unsigned(sizes.size()));
    size_t offset = 6 + 16 * sizes.size();
    for (size_t i = 0; i < sizes.size(); ++i) {
        const unsigned n = unsigned(sizes[i]);
        out.push_back(uint8_t(n == 256 ? 0 : n)), out.push_back(uint8_t(n == 256 ? 0 : n)), out.push_back(0), out.push_back(0);
        PutLe16(out, 1), PutLe16(out, 32), PutLe32(out, uint32_t(pictures[i].size())), PutLe32(out, uint32_t(offset));
        offset += pictures[i].size();
    }
    for (const std::vector<uint8_t>& p : pictures) out.insert(out.end(), p.begin(), p.end());
    return out;
}

bool WriteIconIco(const std::string& path, const IconImage& icon, std::string& err) {
    return WriteWhole(path, EncodeIco(icon), err);
}

}  // namespace tpw::assets
