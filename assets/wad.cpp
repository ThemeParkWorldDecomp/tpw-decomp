// Bullfrog DWFB wads (the PC disc's data/fonts.wad): a 0x58-byte header ("DWFB", version 2; at 0x48 the entry count,
// the name block's offset and length) and 40-byte entries from 0x58 (seven little-endian u32: name hash, name offset,
// name length, data offset, stored size, flags, unpacked size). Flag 4 with stored bytes starting 10 FB is a RefPack
// (EA's LZ77) stream, unpacked here in memory; anything else is served as it is stored. Only the PC disc's FONTS.WAD
// is read this way (its 17 TrueType fonts); the game reads its other wads itself.
#include <algorithm>
#include <cstring>

#include "range_stream.h"
#include "source.h"

namespace tpw::assets {

namespace {

constexpr uint32_t kWadHeader = 0x58;
constexpr uint32_t kWadEntry = 40;
constexpr uint32_t kWadMaxEntries = 65536;
constexpr uint64_t kWadMaxUnpacked = 64ull << 20;   // one entry, unpacked in memory (the fonts are 50 to 150 KB)

uint32_t L32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

class MemoryStream : public Stream {
public:
    explicit MemoryStream(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {}
    uint64_t Size() const override { return bytes_.size(); }
    int64_t Read(void* buf, size_t n, std::string&) override {
        if (n > bytes_.size() - pos_) n = bytes_.size() - pos_;
        if (n) std::memcpy(buf, bytes_.data() + pos_, n);
        pos_ += n;
        return static_cast<int64_t>(n);
    }

private:
    std::vector<uint8_t> bytes_;
    size_t pos_ = 0;
};

class WadSource : public Source {
public:
    const char* Kind() const override { return "wad"; }
    const std::vector<Entry>& Entries() const override { return entries_; }
    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id >= items_.size()) {
            err = "not an entry of this wad";
            return nullptr;
        }
        const Item& item = items_[entry.id];
        try {
            if (!item.packed) return std::make_unique<RangeStream>(blob_, std::vector<ByteRange>{{item.offset, item.stored}}, item.stored);
            std::vector<uint8_t> stored(item.stored);
            int64_t got = blob_->ReadAt(item.offset, stored.data(), stored.size(), err);
            if (got < 0) return nullptr;
            if (static_cast<uint64_t>(got) != item.stored) {
                err = "wad: " + entry.path + " is cut off";
                return nullptr;
            }
            std::vector<uint8_t> out;
            if (!RefPackDecode(stored.data(), stored.size(), kWadMaxUnpacked, out, err)) {
                err = "wad: " + entry.path + ": " + err;
                return nullptr;
            }
            if (out.size() != entry.size) {
                err = "wad: " + entry.path + " unpacks to another size than its entry gives";
                return nullptr;
            }
            return std::make_unique<MemoryStream>(std::move(out));
        } catch (...) {
            err = "out of memory";
            return nullptr;
        }
    }

    bool Load(std::shared_ptr<Blob> blob, std::string& err) {
        blob_ = std::move(blob);
        const uint64_t size = blob_->Size();
        uint8_t h[kWadHeader];
        int64_t got = blob_->ReadAt(0, h, sizeof h, err);
        if (got < 0) return false;
        if (got < static_cast<int64_t>(kWadHeader) || std::memcmp(h, "DWFB", 4) != 0) {
            err = "wad: not a DWFB wad";
            return false;
        }
        const uint32_t count = L32(h + 0x48);
        if (count > kWadMaxEntries || kWadHeader + uint64_t(count) * kWadEntry > size) {
            err = "wad: the entry table runs past the end of the file";
            return false;
        }
        std::vector<uint8_t> table(size_t(count) * kWadEntry);
        if (count && blob_->ReadAt(kWadHeader, table.data(), table.size(), err) != static_cast<int64_t>(table.size())) {
            if (err.empty()) err = "wad: the entry table is cut off";
            return false;
        }
        for (uint32_t k = 0; k < count; ++k) {
            const uint8_t* p = table.data() + size_t(k) * kWadEntry;
            const uint32_t nameOffset = L32(p + 4), nameLength = L32(p + 8);
            Item item;
            item.offset = L32(p + 12);
            item.stored = L32(p + 16);
            const uint32_t flags = L32(p + 20), unpacked = L32(p + 24);
            if (nameLength == 0 || nameLength > 1024 || uint64_t(nameOffset) + nameLength > size ||
                item.offset + item.stored > size) {
                err = "wad: an entry runs past the end of the file";
                return false;
            }
            std::string name(nameLength, '\0');
            if (blob_->ReadAt(nameOffset, &name[0], nameLength, err) != static_cast<int64_t>(nameLength)) {
                if (err.empty()) err = "wad: a name is cut off";
                return false;
            }
            name.resize(std::strlen(name.c_str()));
            for (char& c : name)
                if (c == '\\') c = '/';
            if (name.empty() || name[0] == '/' || name.find("..") != std::string::npos) {
                err = "wad: an entry has no usable name";
                return false;
            }
            uint8_t magic[2] = {};
            if (item.stored >= 2 && blob_->ReadAt(item.offset, magic, 2, err) < 0) return false;
            item.packed = (flags & 4) != 0 && magic[0] == 0x10 && magic[1] == 0xFB;
            Entry e;
            e.path = name;
            e.size = item.packed ? unpacked : item.stored;
            e.id = items_.size();
            items_.push_back(item);
            entries_.push_back(e);
        }
        std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.path < b.path; });
        return true;
    }

private:
    struct Item {
        uint64_t offset = 0, stored = 0;
        bool packed = false;
    };
    std::shared_ptr<Blob> blob_;
    std::vector<Item> items_;
    std::vector<Entry> entries_;
};

}  // namespace

bool RefPackDecode(const uint8_t* in, size_t n, uint64_t maxOut, std::vector<uint8_t>& out, std::string& err) {
    out.clear();
    // The header: flags (0x10, with 0x80 for 4-byte sizes and 0x01 when the packed size comes first), FB, the sizes.
    if (n < 2 || (in[0] & 0x3E) != 0x10 || in[1] != 0xFB) {
        err = "not a RefPack stream";
        return false;
    }
    const size_t width = (in[0] & 0x80) ? 4 : 3;
    size_t p = 2 + ((in[0] & 0x01) ? width : 0);
    if (p + width > n) {
        err = "the RefPack header is cut off";
        return false;
    }
    uint64_t size = 0;
    for (size_t i = 0; i < width; ++i) size = (size << 8) | in[p + i];
    p += width;
    if (size > maxOut) {
        err = "the RefPack stream unpacks to more than the limit";
        return false;
    }
    out.reserve(static_cast<size_t>(size));
    for (;;) {
        if (p >= n) {
            err = "the RefPack stream ends without its stop code";
            return false;
        }
        const uint8_t c0 = in[p];
        size_t literal = 0, copy = 0, back = 0, length = 1;
        bool stop = false;
        if (c0 < 0x80) {
            length = 2;
        } else if (c0 < 0xC0) {
            length = 3;
        } else if (c0 < 0xE0) {
            length = 4;
        }
        if (p + length > n) {
            err = "the RefPack stream is cut off";
            return false;
        }
        const uint8_t* c = in + p;
        if (c0 < 0x80) {
            literal = c0 & 3;
            copy = ((c0 >> 2) & 7) + 3;
            back = ((c0 & 0x60) << 3) + c[1] + 1;
        } else if (c0 < 0xC0) {
            literal = c[1] >> 6;
            copy = (c0 & 0x3F) + 4;
            back = ((c[1] & 0x3F) << 8) + c[2] + 1;
        } else if (c0 < 0xE0) {
            literal = c0 & 3;
            copy = ((c0 & 0x0C) << 6) + c[3] + 5;
            back = ((c0 & 0x10) << 12) + (c[1] << 8) + c[2] + 1;
        } else if (c0 < 0xFC) {
            literal = ((c0 & 0x1F) << 2) + 4;
        } else {
            literal = c0 & 3;
            stop = true;
        }
        p += length;
        if (p + literal > n || out.size() + literal + copy > size) {
            err = "the RefPack stream runs past its end or its size";
            return false;
        }
        out.insert(out.end(), in + p, in + p + literal);
        p += literal;
        if (copy) {
            if (back > out.size()) {
                err = "the RefPack stream refers back before its start";
                return false;
            }
            const size_t from = out.size() - back;   // the copy may overlap what it writes (a run)
            for (size_t i = 0; i < copy; ++i) out.push_back(out[from + i]);
        }
        if (stop) break;
    }
    if (out.size() != size) {
        err = "the RefPack stream unpacks to another size than its header gives";
        return false;
    }
    return true;
}

std::unique_ptr<Source> OpenWad(std::shared_ptr<Blob> blob, std::string& err) {
    if (!blob) {
        err = "no blob";
        return nullptr;
    }
    try {
        auto s = std::make_unique<WadSource>();
        if (!s->Load(std::move(blob), err)) return nullptr;
        return s;
    } catch (...) {
        err = "out of memory";
        return nullptr;
    }
}

}  // namespace tpw::assets
