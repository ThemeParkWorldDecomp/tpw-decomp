// test_installshield.h: writers for the Theme Park Inc tests' synthetic fixtures (host_importer.cpp): a small
// InstallShield cabinet set (version 6, the discs' format, or version 5, the Fargus release's), a zip of several
// stored members, an ISO 9660 image of root files, and the files of a tiny fake installed game. Nothing here comes from
// the game: every byte is made up by the test. The formats follow installshield.h's reading of the real cabinets (which
// the corpus check compared with unshield's extraction), not any copied code.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <zlib.h>

#include "../installshield.h"

namespace tpwtest {

inline void Put16(std::string& s, size_t at, uint32_t v) {
    s[at] = char(v);
    s[at + 1] = char(v >> 8);
}
inline void Put32(std::string& s, size_t at, uint32_t v) {
    Put16(s, at, v & 0xffff);
    Put16(s, at + 2, v >> 16);
}
inline void Put64(std::string& s, size_t at, uint64_t v) {
    Put32(s, at, uint32_t(v));
    Put32(s, at + 4, uint32_t(v >> 32));
}
inline void Append32(std::string& s, uint32_t v) {
    s.append(4, '\0');
    Put32(s, s.size() - 4, v);
}

// A file to put in a cabinet: its directory inside the group ("" or "Data\\levels\\arabian", as the tables store it),
// its name, its content, and how it is stored.
struct IsFile {
    std::string dir;
    std::string name;
    std::string data;
    bool compressed = false;
    bool obfuscated = false;
    bool wrongMd5 = false;   // version 6: record a checksum that is not the content's
};
struct IsGroup {
    std::string name;   // "Data", "Program Executable Files"
    std::vector<IsFile> files;
};
struct IsOptions {
    int version = 6;             // 5 or 6
    uint64_t volumeBytes = 0;    // the most stored bytes a volume holds (0: one volume); a file that goes past is split
    bool zlibWrapped = false;    // compressed chunks with a zlib header (older cabinets), instead of raw deflate
};

// The stored form of a file's bytes: deflate chunks (a 16-bit length and a stream of its own, 32 KB of input each) when
// compressed, then obfuscated when asked (XOR 0xd5, rotate right 2, minus index mod 0x47, undone in that order by the
// reader: so here add first, rotate left, then XOR).
inline std::string IsStored(const IsFile& f, bool zlibWrapped) {
    std::string out;
    if (f.compressed) {
        size_t at = 0;
        do {
            const size_t piece = std::min<size_t>(f.data.size() - at, 32 << 10);
            z_stream z{};
            deflateInit2(&z, 6, Z_DEFLATED, zlibWrapped ? 15 : -15, 8, Z_DEFAULT_STRATEGY);
            std::string body(deflateBound(&z, uLong(piece)) + 16, '\0');
            z.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(f.data.data() + at));
            z.avail_in = uInt(piece);
            z.next_out = reinterpret_cast<Bytef*>(&body[0]);
            z.avail_out = uInt(body.size());
            deflate(&z, Z_FINISH);
            body.resize(z.total_out);
            deflateEnd(&z);
            out.push_back(char(body.size() & 0xff));
            out.push_back(char(body.size() >> 8));
            out += body;
            at += piece;
        } while (at < f.data.size());
    } else {
        out = f.data;
    }
    if (f.obfuscated) {
        for (size_t i = 0; i < out.size(); ++i) {
            const uint8_t plain = uint8_t(out[i]);
            const uint8_t added = uint8_t(plain + uint8_t(i % 0x47));
            const uint8_t rotated = uint8_t((added << 2) | (added >> 6));
            out[i] = char(rotated ^ 0xd5);
        }
    }
    return out;
}

// The cabinet set: [0] is data1.hdr, [1..] are data1.cab, data2.cab, ... (a version 5 set's header is data1.hdr too).
inline std::vector<std::string> IsCabinetOf(const std::vector<IsGroup>& groups, const IsOptions& opt = IsOptions()) {
    const bool old = opt.version <= 5;
    // flat list of files with group ranges
    struct Rec {
        const IsFile* file;
        size_t group;
        std::string stored;
        uint16_t volume = 1;
        uint64_t dataOffset = 0;
        bool split = false;
    };
    std::vector<Rec> recs;
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    std::vector<std::string> dirs;
    std::vector<uint32_t> dirOf;
    for (size_t g = 0; g < groups.size(); ++g) {
        const uint32_t first = uint32_t(recs.size());
        for (const IsFile& f : groups[g].files) {
            size_t d = 0;
            while (d < dirs.size() && dirs[d] != f.dir) ++d;
            if (d == dirs.size()) dirs.push_back(f.dir);
            dirOf.push_back(uint32_t(d));
            recs.push_back(Rec{&f, g, IsStored(f, opt.zlibWrapped)});
        }
        ranges.emplace_back(first, uint32_t(recs.empty() ? 0 : recs.size() - 1));
    }
    // volumes: files are laid out in order from each volume's data offset; a file that would run past volumeBytes is
    // split: the rest goes on at the next volume's data offset (and on, if it is longer than a volume)
    const uint64_t dataStart = 0x100;
    struct Vol {
        std::string bytes;
        uint32_t firstFile = 0, lastFile = 0;
        uint64_t firstOffset = 0, firstExpanded = 0, firstCompressed = 0;
        uint64_t lastOffset = 0, lastExpanded = 0, lastCompressed = 0;
        bool any = false;
    };
    std::vector<Vol> vols(1);
    vols[0].bytes.assign(size_t(dataStart), '\0');
    for (size_t i = 0; i < recs.size(); ++i) {
        Rec& r = recs[i];
        Vol* v = &vols.back();
        if (opt.volumeBytes && v->any && v->bytes.size() - dataStart + r.stored.size() > opt.volumeBytes &&
            v->bytes.size() > dataStart) {
            // start the file on a new volume when the current one already holds something: keeps it simple
            if (r.stored.size() <= opt.volumeBytes) {
                vols.emplace_back();
                vols.back().bytes.assign(size_t(dataStart), '\0');
                v = &vols.back();
            }
        }
        r.volume = uint16_t(vols.size());
        r.dataOffset = v->bytes.size();
        if (!v->any) {
            v->firstFile = uint32_t(i);
            v->firstOffset = r.dataOffset;
            v->firstExpanded = r.file->data.size();
            v->firstCompressed = r.stored.size();
            v->any = true;
        }
        uint64_t room = opt.volumeBytes ? opt.volumeBytes - (v->bytes.size() - dataStart) : r.stored.size();
        if (room >= r.stored.size()) {
            v->bytes += r.stored;
            v->lastFile = uint32_t(i);
            v->lastOffset = r.dataOffset;
            v->lastExpanded = r.file->data.size();
            v->lastCompressed = r.stored.size();
            continue;
        }
        // split across volumes
        r.split = true;
        size_t at = 0;
        for (;;) {
            const size_t piece = size_t(std::min<uint64_t>(room, r.stored.size() - at));
            if (at > 0) v->firstCompressed = piece;   // the part in this volume, as the reader takes it
            v->bytes.append(r.stored, at, piece);
            v->lastFile = uint32_t(i);
            v->lastOffset = at == 0 ? r.dataOffset : dataStart;
            v->lastExpanded = r.file->data.size();
            v->lastCompressed = piece;
            at += piece;
            if (at >= r.stored.size()) break;
            vols.emplace_back();
            vols.back().bytes.assign(size_t(dataStart), '\0');
            v = &vols.back();
            v->any = true;
            v->firstFile = uint32_t(i);
            v->firstOffset = dataStart;
            v->firstExpanded = r.file->data.size();
            v->firstCompressed = r.stored.size() - at;
            room = opt.volumeBytes;
        }
    }
    // the header: common header, descriptor, group lists and descriptors, file table (names, directories, descriptors)
    std::string h(20, '\0');
    Put32(h, 0, 0x28635349);
    Put32(h, 4, old ? 0x01520001u : 0x0100600cu);
    const size_t desc = 0x200;
    h.resize(desc, '\0');
    Put32(h, 12, uint32_t(desc));
    Put32(h, 16, 0x200);
    const size_t descSize = 0x3e + 71 * 4 + 71 * 4;   // through the component list offsets
    h.resize(desc + descSize, '\0');
    Put32(h, desc + 0x1c, uint32_t(dirs.size()));
    Put32(h, desc + 0x28, uint32_t(recs.size()));
    // one group list: each entry {name offset, descriptor offset, next}, offsets relative to the descriptor
    uint32_t prevAt = 0;
    for (size_t g = 0; g < groups.size(); ++g) {
        const uint32_t nameAt = uint32_t(h.size() - desc);
        h += groups[g].name;
        h.push_back('\0');
        while (h.size() % 4) h.push_back('\0');
        const uint32_t gdAt = uint32_t(h.size() - desc);
        h.append(old ? 0x60 : 0x30, '\0');
        Put32(h, desc + gdAt + 4 + (old ? 0x48 : 0x12), ranges[g].first);
        Put32(h, desc + gdAt + 4 + (old ? 0x4c : 0x16), ranges[g].second);
        const uint32_t entryAt = uint32_t(h.size() - desc);
        h.append(12, '\0');
        Put32(h, desc + entryAt, nameAt);
        Put32(h, desc + entryAt + 4, gdAt);
        if (g == 0) Put32(h, desc + 0x3e, entryAt);
        else Put32(h, desc + prevAt + 8, entryAt);
        prevAt = entryAt;
    }
    while (h.size() % 16) h.push_back('\0');
    const size_t fileTable = h.size();
    Put32(h, desc + 0x0c, uint32_t(fileTable - desc));
    // offsets: directories, then (version 5) one per file
    const size_t offsets = dirs.size() + (old ? recs.size() : 0);
    h.append(4 * offsets, '\0');
    std::vector<uint32_t> nameAt(recs.size());
    for (size_t d = 0; d < dirs.size(); ++d) {
        Put32(h, fileTable + 4 * d, uint32_t(h.size() - fileTable));
        h += dirs[d];
        h.push_back('\0');
    }
    for (size_t i = 0; i < recs.size(); ++i) {
        nameAt[i] = uint32_t(h.size() - fileTable);
        h += recs[i].file->name;
        h.push_back('\0');
    }
    while (h.size() % 4) h.push_back('\0');
    const size_t descriptors = h.size();
    Put32(h, desc + 0x2c, uint32_t(descriptors - fileTable));
    for (size_t i = 0; i < recs.size(); ++i) {
        const Rec& r = recs[i];
        uint16_t flags = 0;
        if (r.split) flags |= 1;
        if (r.file->obfuscated) flags |= 2;
        if (r.file->compressed) flags |= 4;
        if (old) {
            const size_t q = h.size();
            Put32(h, fileTable + 4 * (dirs.size() + i), uint32_t(q - fileTable));
            h.append(0x3a, '\0');
            Put32(h, q, nameAt[i]);
            Put32(h, q + 4, dirOf[i]);
            Put16(h, q + 8, flags);
            Put32(h, q + 10, uint32_t(r.file->data.size()));
            Put32(h, q + 14, uint32_t(r.stored.size()));
            Put32(h, q + 0x26, uint32_t(r.dataOffset));
        } else {
            const size_t q = h.size();
            h.append(0x57, '\0');
            Put16(h, q, flags);
            Put64(h, q + 2, r.file->data.size());
            Put64(h, q + 10, r.stored.size());
            Put64(h, q + 18, r.dataOffset);
            uint8_t md5[16];
            tpw::assets::InstallShieldMd5(reinterpret_cast<const uint8_t*>(r.file->data.data()), r.file->data.size(), md5);
            if (r.file->wrongMd5) md5[3] ^= 0x5a;
            std::memcpy(&h[q + 26], md5, 16);
            Put32(h, q + 58, nameAt[i]);
            Put16(h, q + 62, dirOf[i]);
            Put16(h, q + 85, r.volume);
        }
    }
    std::vector<std::string> out;
    out.push_back(h);
    for (const Vol& v : vols) {
        std::string b = v.bytes;
        Put32(b, 0, 0x28635349);
        Put32(b, 4, old ? 0x01520001u : 0x0100600cu);
        if (old) {
            Put32(b, 20, uint32_t(dataStart));
            Put32(b, 28, v.firstFile);
            Put32(b, 32, v.lastFile);
            Put32(b, 36, uint32_t(v.firstOffset));
            Put32(b, 40, uint32_t(v.firstExpanded));
            Put32(b, 44, uint32_t(v.firstCompressed));
            Put32(b, 48, uint32_t(v.lastOffset));
            Put32(b, 52, uint32_t(v.lastExpanded));
            Put32(b, 56, uint32_t(v.lastCompressed));
        } else {
            Put64(b, 20, dataStart);
            Put32(b, 28, v.firstFile);
            Put32(b, 32, v.lastFile);
            Put64(b, 36, v.firstOffset);
            Put64(b, 44, v.firstExpanded);
            Put64(b, 52, v.firstCompressed);
            Put64(b, 60, v.lastOffset);
            Put64(b, 68, v.lastExpanded);
            Put64(b, 76, v.lastCompressed);
        }
        out.push_back(b);
    }
    return out;
}

// A zip of stored members (no compression), names with '/' between folders.
struct ZipMember {
    std::string name;
    std::string data;
};
inline std::string ZipArchiveOf(const std::vector<ZipMember>& members) {
    std::string out, central;
    for (const ZipMember& m : members) {
        const uint32_t crc = uint32_t(crc32(0, reinterpret_cast<const Bytef*>(m.data.data()), uInt(m.data.size())));
        const uint32_t at = uint32_t(out.size());
        std::string local(30, '\0');
        Put32(local, 0, 0x04034b50);
        Put16(local, 4, 20);
        Put32(local, 14, crc);
        Put32(local, 18, uint32_t(m.data.size()));
        Put32(local, 22, uint32_t(m.data.size()));
        Put16(local, 26, uint32_t(m.name.size()));
        out += local + m.name + m.data;
        std::string c(46, '\0');
        Put32(c, 0, 0x02014b50);
        Put16(c, 4, 20);
        Put16(c, 6, 20);
        Put32(c, 16, crc);
        Put32(c, 20, uint32_t(m.data.size()));
        Put32(c, 24, uint32_t(m.data.size()));
        Put16(c, 28, uint32_t(m.name.size()));
        Put32(c, 42, at);
        central += c + m.name;
    }
    std::string end(22, '\0');
    Put32(end, 0, 0x06054b50);
    Put16(end, 8, uint32_t(members.size()));
    Put16(end, 10, uint32_t(members.size()));
    Put32(end, 12, uint32_t(central.size()));
    Put32(end, 16, uint32_t(out.size()));
    return out + central + end;
}

// An ISO 9660 image (2048-byte sectors) holding files at its root: the volume descriptor at sector 16, the terminator
// at 17, the root directory at 20, the files from 21 on.
struct IsoFile {
    std::string name;   // as recorded, "DATA1.HDR"
    std::string data;
};
inline std::vector<uint8_t> IsoImageOf(const std::vector<IsoFile>& files, const std::string& volume = "THEME_PARK") {
    std::vector<uint32_t> sectors;
    uint32_t sector = 21;
    for (const IsoFile& f : files) {
        sectors.push_back(sector);
        sector += uint32_t((f.data.size() + 2047) / 2048);
    }
    std::vector<uint8_t> image(size_t(sector) * 2048);
    auto put = [&](size_t p, uint32_t v, size_t bytes = 4) {
        for (size_t i = 0; i < bytes; ++i) image[p + i] = uint8_t(v >> (8 * i));
    };
    auto both = [&](size_t p, uint32_t v) {   // little-endian then big-endian
        put(p, v);
        for (size_t i = 0; i < 4; ++i) image[p + 4 + i] = uint8_t(v >> (8 * (3 - i)));
    };
    const size_t pvd = 16 * 2048;
    image[pvd] = 1;
    std::memcpy(&image[pvd + 1], "CD001", 5);
    image[pvd + 6] = 1;
    std::fill(image.begin() + pvd + 8, image.begin() + pvd + 72, ' ');
    std::memcpy(&image[pvd + 40], volume.data(), std::min<size_t>(volume.size(), 32));
    both(pvd + 80, sector);
    put(pvd + 120, 1, 2); put(pvd + 122, 1, 2);   // set size (both byte orders are the same for 1)
    image[pvd + 121] = 1; image[pvd + 123] = 1;
    put(pvd + 128, 2048, 2); image[pvd + 131] = 8;   // logical block size 2048 (little, big)
    const size_t term = 17 * 2048;
    image[term] = 255;
    std::memcpy(&image[term + 1], "CD001", 5);
    image[term + 6] = 1;
    auto record = [&](size_t p, const std::string& name, uint32_t at, uint32_t bytes, uint8_t flags) {
        const size_t n = 33 + name.size() + (name.size() % 2 == 0);
        image[p] = uint8_t(n);
        both(p + 2, at);
        both(p + 10, bytes);
        image[p + 25] = flags;
        image[p + 28] = 1; image[p + 31] = 1;
        image[p + 32] = uint8_t(name.size());
        std::memcpy(&image[p + 33], name.data(), name.size());
        return n;
    };
    record(pvd + 156, std::string(1, '\0'), 20, 2048, 2);
    size_t pos = 20 * 2048;
    pos += record(pos, std::string(1, '\0'), 20, 2048, 2);
    pos += record(pos, std::string(1, '\1'), 20, 2048, 2);
    for (size_t i = 0; i < files.size(); ++i) {
        pos += record(pos, files[i].name + ";1", sectors[i], uint32_t(files[i].data.size()), 0);
        std::copy(files[i].data.begin(), files[i].data.end(), image.begin() + size_t(sectors[i]) * 2048);
    }
    return image;
}

// The files of a tiny fake Theme Park Inc game, in installed form (paths below the game folder, '/' separated), with
// the languages asked for ("English", "American", ...). Every content is made up and small; the worlds listed get a
// Standard.sam and a terrain.wad each.
struct FakeFile {
    std::string path;
    std::string data;
};
// Bytes that do not compress well (a linear congruential sequence), so the fake wads have a size when stored.
inline std::string Noise(uint32_t seed, size_t n) {
    std::string out(n, '\0');
    for (size_t i = 0; i < n; ++i) {
        seed = seed * 1664525u + 1013904223u;
        out[i] = char(seed >> 24);
    }
    return out;
}
inline std::vector<FakeFile> FakeTpiFiles(const std::vector<std::string>& languages,
                                          const std::vector<std::string>& worlds = {"arabian", "water", "science"},
                                          const std::string& themeNames = "Arabian Nights\r\nWater World\r\n") {
    std::vector<FakeFile> files;
    files.push_back({"Game.exe", std::string("MZ fake game ") + std::string(300, 'x')});
    files.push_back({"Data/Online.sam", "Theme Park Inc Online Settings\r\nServer = none\r\n"});
    files.push_back({"Data/global/rides.wad", std::string("WAD-fake-global-") + Noise(1, 9000)});
    for (const std::string& w : worlds) {
        files.push_back({"Data/levels/" + w + "/Standard.sam", "// " + w + " standard settings\r\nStartMoney = 100000\r\n"});
        files.push_back({"Data/levels/" + w + "/terrain.wad", "WAD-fake-" + w + "-" + Noise(uint32_t(w[0]), 4000)});
        files.push_back({"Data/levels/" + w + "/rides/coaster.wad", "WAD-ride-" + w + "-" + Noise(uint32_t(w[1]) + 7, 2500)});
    }
    for (const std::string& l : languages) {
        files.push_back({"Data/Language/" + l + "/THEMENAMES.str", themeNames});
        files.push_back({"Data/Language/" + l + "/ERRORMSG.str", "Cannot find the disc\r\n"});
    }
    return files;
}

// The same files as a cabinet set's Data group plus a program group, as the discs lay them out.
inline std::vector<IsGroup> FakeTpiGroups(const std::vector<FakeFile>& files, bool compressed = true,
                                          const std::string& extraGroup = std::string()) {
    IsGroup program{"Program Executable Files", {}}, data{"Data", {}}, extra{extraGroup, {}};
    for (const FakeFile& f : files) {
        IsFile is;
        const size_t slash = f.path.find_last_of('/');
        is.dir = slash == std::string::npos ? std::string() : f.path.substr(0, slash);
        for (char& c : is.dir) {
            if (c == '/') c = '\\';
        }
        is.name = slash == std::string::npos ? f.path : f.path.substr(slash + 1);
        is.data = f.data;
        is.compressed = compressed && f.data.size() > 64;
        is.obfuscated = compressed && is.name.size() % 2 == 0;   // some of each kind
        if (f.path.compare(0, 5, "Data/") == 0) data.files.push_back(is);
        else program.files.push_back(is);
    }
    std::vector<IsGroup> groups{program, data};
    if (!extraGroup.empty()) {
        extra.files.push_back({"", "Game.exe", "MZ russian build", false, false});
        groups.push_back(extra);
    }
    return groups;
}

}  // namespace tpwtest
