// Helpers of the archive tests (host_assets.cpp and host_importer.cpp): the fixtures in tests/archives are uuencoded text
// (tests/archives/README.md says where each comes from), and RAR archives are written here, stored rather than compressed,
// since no program on the build machines makes one.
#pragma once

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace tpw::assets::test {

// The bytes of a uuencoded file ("begin MODE NAME", lines of a length character and groups of four, "end"); empty when the
// file is missing or has no "end" line.
inline std::string UuDecode(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string line, out;
    bool begun = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!begun) {
            begun = line.rfind("begin ", 0) == 0;
            continue;
        }
        if (line == "end") return out;
        if (line.empty()) continue;
        auto six = [&](size_t i) { return i < line.size() ? uint32_t((line[i] - 0x20) & 0x3f) : 0u; };
        size_t n = six(0), start = out.size();
        for (size_t i = 1; out.size() - start < n; i += 4) {
            uint32_t v = six(i) << 18 | six(i + 1) << 12 | six(i + 2) << 6 | six(i + 3);
            out.push_back(char(v >> 16));
            out.push_back(char(v >> 8));
            out.push_back(char(v));
        }
        out.resize(start + n);
    }
    return std::string();
}

struct RarMember {
    std::string name;   // with '\\' between folders, as RAR stores them
    std::string data;
};

// A RAR 4 archive of stored members (method 0x30). With volumes > 1 the members' data is cut into that many nearly equal
// pieces, one per volume, each volume a whole archive (marker, main header, its pieces, end); a member cut in two has a
// header in both volumes, flagged "continued in the next" and "continued from the previous", the last piece's header
// carrying the CRC of the whole member, as RAR writes them.
inline std::vector<std::string> RarOf(const std::vector<RarMember>& members, size_t volumes = 1) {
    auto put16 = [](std::string& s, uint32_t v) { s += char(v); s += char(v >> 8); };
    auto put32 = [&](std::string& s, uint32_t v) { put16(s, v & 0xffff); put16(s, v >> 16); };
    auto crc = [](const std::string& s, uLong start = 0) {
        return uint32_t(crc32(start, reinterpret_cast<const Bytef*>(s.data()), uInt(s.size())));
    };
    // A block: its CRC (the low 16 bits of the CRC-32 of everything after it), then type, flags, size and the rest.
    auto block = [&](uint8_t type, uint16_t flags, const std::string& rest) {
        std::string b;
        b += char(type);
        put16(b, flags);
        put16(b, uint32_t(rest.size() + 7));
        b += rest;
        std::string out;
        put16(out, crc(b) & 0xffff);
        return out + b;
    };
    size_t total = 0;
    for (const RarMember& m : members) total += m.data.size();
    volumes = std::max<size_t>(volumes, 1);
    const size_t room = (total + volumes - 1) / volumes;
    std::vector<std::string> out(volumes);
    for (size_t v = 0; v < volumes; ++v) {
        std::string main(6, '\0');   // reserved
        uint16_t flags = volumes > 1 ? 0x0001 | 0x0010 | (v == 0 ? 0x0100 : 0) : 0;   // volume, new numbering, first
        out[v] = std::string("Rar!\x1a\x07\x00", 7) + block(0x73, flags, main);
    }
    size_t v = 0, used = 0;
    for (const RarMember& m : members) {
        size_t at = 0;
        do {
            if (used == room && v + 1 < volumes) { ++v; used = 0; }
            size_t take = std::min(m.data.size() - at, v + 1 < volumes ? room - used : m.data.size() - at);
            std::string piece = m.data.substr(at, take);
            bool before = at > 0, after = at + take < m.data.size();
            std::string h;
            put32(h, uint32_t(piece.size()));      // packed size
            put32(h, uint32_t(m.data.size()));     // unpacked size
            h += char(2);                          // host: Windows
            put32(h, after ? crc(piece) : crc(m.data));
            put32(h, 0x5a210000);                  // 2025-01-01, DOS time
            h += char(20);                         // version 2.0 to unpack
            h += char(0x30);                       // stored
            put16(h, uint32_t(m.name.size()));
            put32(h, 0x20);                        // archive attribute
            h += m.name;
            out[v] += block(0x74, uint16_t(0x8000 | (before ? 0x01 : 0) | (after ? 0x02 : 0)), h) + piece;
            at += take;
            used += take;
        } while (at < m.data.size());
    }
    for (size_t i = 0; i < volumes; ++i) out[i] += block(0x7b, uint16_t(0x4000 | (i + 1 < volumes ? 0x0001 : 0)), "");
    return out;
}

}  // namespace tpw::assets::test
