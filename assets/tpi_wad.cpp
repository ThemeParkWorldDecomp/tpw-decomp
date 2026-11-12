// tpi_wad.cpp: the DWFB wad reader and writer (tpi_wad.h).
#include "tpi_wad.h"

#include <algorithm>
#include <cstring>

#include "source.h"

namespace tpw::assets {

namespace {

constexpr uint32_t kHeader = 0x58;
constexpr uint32_t kEntry = 40;

void Put32(std::vector<uint8_t>& out, size_t at, uint32_t v) {
    out[at] = static_cast<uint8_t>(v);
    out[at + 1] = static_cast<uint8_t>(v >> 8);
    out[at + 2] = static_cast<uint8_t>(v >> 16);
    out[at + 3] = static_cast<uint8_t>(v >> 24);
}

uint8_t Upper(uint8_t c) { return (c >= 'a' && c <= 'z') ? static_cast<uint8_t>(c - 'a' + 'A') : c; }

std::string FolderOf(const std::string& name) {
    const size_t split = name.find_last_of("\\/");
    return split == std::string::npos ? std::string() : name.substr(0, split);
}

std::string FileOf(const std::string& name) {
    const size_t split = name.find_last_of("\\/");
    return split == std::string::npos ? name : name.substr(split + 1);
}

}  // namespace

uint32_t TpiWadNameHash(const std::string& path) {
    const auto* p = reinterpret_cast<const uint8_t*>(path.data());
    size_t count = path.size();
    uint32_t hash = 0;
    for (; count >= 4; count -= 4, p += 4) {
        for (int i = 0; i < 4; ++i) hash += static_cast<uint32_t>(Upper(p[i])) << i;
    }
    for (size_t i = 0; i < count; ++i) hash += Upper(p[i]);
    return hash;
}

std::vector<uint8_t> BuildTpiWad(const std::vector<TpiWadMember>& given) {
    // The name block cannot say "back to the root" (a name without a folder goes into the folder of the entry before
    // it), so the root's members come first, then each folder's members together, as the original wads are laid out.
    std::vector<const TpiWadMember*> order;
    order.reserve(given.size());
    for (const TpiWadMember& m : given) order.push_back(&m);
    std::stable_sort(order.begin(), order.end(), [](const TpiWadMember* a, const TpiWadMember* b) {
        const std::string fa = FolderOf(a->name), fb = FolderOf(b->name);
        if (fa.empty() != fb.empty()) return fa.empty();
        return fa < fb;
    });
    std::vector<TpiWadMember> members;
    members.reserve(given.size());
    for (const TpiWadMember* m : order) members.push_back(*m);
    const uint32_t count = static_cast<uint32_t>(members.size());
    // The names as the name block holds them: the folder prefix on the first member of each run of one folder.
    std::vector<std::string> names;
    names.reserve(count);
    std::string folder;
    bool first = true;
    for (const TpiWadMember& m : members) {
        std::string path = m.name;
        for (char& c : path) {
            if (c == '/') c = '\\';
        }
        const std::string thisFolder = FolderOf(path);
        if (first || thisFolder != folder) {
            names.push_back(path);
            folder = thisFolder;
            first = false;
        } else {
            names.push_back(FileOf(path));
        }
    }
    uint64_t dataBytes = 0;
    uint64_t nameBytes = 0;
    for (uint32_t i = 0; i < count; ++i) {
        dataBytes += members[i].data.size();
        nameBytes += names[i].size() + 1;
    }
    const uint64_t dataStart = kHeader + static_cast<uint64_t>(kEntry) * count;
    const uint64_t nameStart = dataStart + dataBytes;
    std::vector<uint8_t> out(static_cast<size_t>(nameStart + nameBytes), 0);
    std::memcpy(out.data(), "DWFB", 4);
    Put32(out, 4, 2);
    Put32(out, 0x48, count);
    Put32(out, 0x4c, static_cast<uint32_t>(nameStart));
    Put32(out, 0x50, static_cast<uint32_t>(nameBytes));
    uint64_t dataAt = dataStart;
    uint64_t nameAt = nameStart;
    for (uint32_t i = 0; i < count; ++i) {
        const size_t e = kHeader + static_cast<size_t>(kEntry) * i;
        std::string full = members[i].name;
        for (char& c : full) {
            if (c == '/') c = '\\';
        }
        Put32(out, e, TpiWadNameHash(full));
        Put32(out, e + 4, static_cast<uint32_t>(nameAt));
        Put32(out, e + 8, static_cast<uint32_t>(names[i].size()));
        Put32(out, e + 12, static_cast<uint32_t>(dataAt));
        Put32(out, e + 16, static_cast<uint32_t>(members[i].data.size()));
        Put32(out, e + 20, 0);
        Put32(out, e + 24, 0);
        if (!members[i].data.empty()) std::memcpy(out.data() + dataAt, members[i].data.data(), members[i].data.size());
        std::memcpy(out.data() + nameAt, names[i].c_str(), names[i].size() + 1);
        dataAt += members[i].data.size();
        nameAt += names[i].size() + 1;
    }
    return out;
}

bool IsTpiWad(const std::vector<uint8_t>& b) { return b.size() >= kHeader && std::memcmp(b.data(), "DWFB", 4) == 0; }

namespace {
uint32_t Get32(const std::vector<uint8_t>& d, size_t at) {
    return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | (static_cast<uint32_t>(d[at + 3]) << 24);
}
}  // namespace

bool ReadTpiWad(const std::vector<uint8_t>& wad, std::vector<TpiWadMember>& members, std::string& err) {
    members.clear();
    if (!IsTpiWad(wad)) {
        err = "not a wad";
        return false;
    }
    const uint32_t count = Get32(wad, 0x48);
    if (static_cast<uint64_t>(kHeader) + static_cast<uint64_t>(kEntry) * count > wad.size()) {
        err = "wad: the entry table runs past the end";
        return false;
    }
    std::string folder;
    for (uint32_t i = 0; i < count; ++i) {
        const size_t e = kHeader + static_cast<size_t>(kEntry) * i;
        const uint32_t nameOff = Get32(wad, e + 4), nameLen = Get32(wad, e + 8), dataOff = Get32(wad, e + 12);
        const uint32_t stored = Get32(wad, e + 16), flags = Get32(wad, e + 20), unpacked = Get32(wad, e + 24);
        if (static_cast<uint64_t>(nameOff) + nameLen > wad.size() || static_cast<uint64_t>(dataOff) + stored > wad.size()) {
            err = "wad: an entry runs past the end";
            return false;
        }
        std::string name(reinterpret_cast<const char*>(wad.data() + nameOff), nameLen);
        name.resize(std::strlen(name.c_str()));
        for (char& c : name) {
            if (c == '\\') c = '/';
        }
        if (name.empty() || name[0] == '/' || name.find("..") != std::string::npos) {
            err = "wad: an entry has no usable name";
            return false;
        }
        const size_t split = name.find_last_of('/');
        if (split != std::string::npos) {
            folder = name.substr(0, split);
        } else if (!folder.empty()) {
            name = folder + "/" + name;
        }
        TpiWadMember m;
        m.name = name;
        const bool packed = (flags & 4) != 0 && stored >= 2 && wad[dataOff] == 0x10 && wad[dataOff + 1] == 0xfb;
        if (packed) {
            if (!RefPackDecode(wad.data() + dataOff, stored, unpacked ? unpacked : (1u << 30), m.data, err)) {
                err = "wad member " + name + ": " + err;
                return false;
            }
        } else {
            m.data.assign(wad.begin() + dataOff, wad.begin() + dataOff + stored);
        }
        members.push_back(std::move(m));
    }
    return true;
}

}  // namespace tpw::assets
