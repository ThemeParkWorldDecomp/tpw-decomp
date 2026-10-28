// An ISO 9660 image as a Source (ECMA-119; Joliet names when there is a Joliet supplementary volume). Cooked images have
// 2048-byte sectors; raw images (ECMA-130) have 2352-byte sectors that start with the 12-byte sync, and the mode byte at
// offset 15 says where the 2048 bytes of user data start: offset 16 for MODE1, offset 24 for MODE2 form 1. Raw images
// saved with the sub-channel (some .mdf files) have 2448-byte sectors: the same 2352 bytes, then 96 sub-channel bytes,
// which are skipped. Files are read straight from the Blob in chunks of a few sectors; no image is ever loaded whole.
// Version suffixes (";1") are removed from file names. Multi-extent files are joined.
#include <algorithm>
#include <cstring>
#include <mutex>
#include <set>

#include "source.h"

namespace tpw::assets {

namespace {

constexpr uint32_t kUser = 2048;
constexpr uint32_t kRaw = 2352;
constexpr uint32_t kRawSub = 2448;  // a raw sector followed by 96 bytes of sub-channel data
constexpr uint32_t kChunkSectors = 64;
const uint8_t kSync[12] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0};

uint32_t LE32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
uint16_t LE16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }

struct Extent {
    uint64_t sector;  // first sector of user data
    uint64_t bytes;
};

struct File {
    std::vector<Extent> extents;
};

// Reads sectors of user data from the image, whichever way they are stored.
class Sectors {
public:
    bool Init(std::shared_ptr<Blob> blob, std::string& err) {
        blob_ = std::move(blob);
        // Volume descriptors start at sector 16; a raw image has the sync there, at its own stride.
        uint8_t head[32];
        uint32_t rawStride = 0;
        for (uint32_t stride : {kRaw, kRawSub}) {
            int64_t got = blob_->ReadAt(uint64_t(16) * stride, head, sizeof head, err);
            if (got < 0) return false;
            if (got == int64_t(sizeof head) && std::memcmp(head, kSync, 12) == 0) {
                rawStride = stride;
                break;
            }
        }
        if (rawStride != 0) {
            uint8_t mode = head[15];
            if (mode != 1 && mode != 2) {
                err = "iso: raw sectors of mode " + std::to_string(mode) + " are not supported";
                return false;
            }
            raw_ = true;
            offset_ = mode == 1 ? 16 : 24;
            stride_ = rawStride;
        } else {
            raw_ = false;
            offset_ = 0;
            stride_ = kUser;
        }
        total_ = blob_->Size() / stride_;
        uint8_t d[8];
        if (!Read(16, 1, d, 8, err)) return false;
        if (std::memcmp(d + 1, "CD001", 5) != 0) {
            err = "iso: no volume descriptor at sector 16";
            return false;
        }
        return true;
    }

    // Reads up to `limit` bytes of the user data of `count` sectors starting at `first` into out.
    bool Read(uint64_t first, uint32_t count, uint8_t* out, size_t limit, std::string& err) {
        if (count == 0) return true;
        if (first + count > total_) {
            err = "iso: sector " + std::to_string(first + count - 1) + " is past the end of the image";
            return false;
        }
        if (!raw_) {
            size_t want = std::min<size_t>(limit, size_t(count) * kUser);
            int64_t got = blob_->ReadAt(first * kUser, out, want, err);
            if (got < 0) return false;
            if (size_t(got) != want) {
                err = "iso: the image is cut off";
                return false;
            }
            return true;
        }
        tmp_.resize(size_t(count) * stride_);
        int64_t got = blob_->ReadAt(first * stride_, tmp_.data(), tmp_.size(), err);
        if (got < 0) return false;
        if (size_t(got) != tmp_.size()) {
            err = "iso: the image is cut off";
            return false;
        }
        for (uint32_t i = 0; i < count && size_t(i) * kUser < limit; i++) {
            const uint8_t* s = &tmp_[size_t(i) * stride_];
            if (std::memcmp(s, kSync, 12) != 0) {
                err = "iso: sector " + std::to_string(first + i) + " has no sync";
                return false;
            }
            size_t take = std::min<size_t>(kUser, limit - size_t(i) * kUser);
            std::memcpy(out + size_t(i) * kUser, s + offset_, take);
        }
        return true;
    }

    uint64_t Total() const { return total_; }
    bool Raw() const { return raw_; }

private:
    std::shared_ptr<Blob> blob_;
    bool raw_ = false;
    uint32_t offset_ = 0;
    uint32_t stride_ = kUser;
    uint64_t total_ = 0;
    std::vector<uint8_t> tmp_;
};

class IsoStream : public Stream {
public:
    IsoStream(std::shared_ptr<Sectors> sectors, std::shared_ptr<std::mutex> lock, std::vector<Extent> extents, uint64_t size)
        : sectors_(std::move(sectors)), lock_(std::move(lock)), extents_(std::move(extents)), size_(size) {}
    uint64_t Size() const override { return size_; }
    int64_t Read(void* buf, size_t n, std::string& err) override {
        if (n == 0 || done_ >= size_) return 0;
        if (have_ == used_) {
            if (!Fill(err)) return -1;
        }
        size_t take = std::min(n, have_ - used_);
        std::memcpy(buf, buffer_.data() + used_, take);
        used_ += take;
        done_ += take;
        return static_cast<int64_t>(take);
    }

private:
    bool Fill(std::string& err) {
        while (extent_ < extents_.size() && inExtent_ >= extents_[extent_].bytes) {
            extent_++;
            inExtent_ = 0;
        }
        if (extent_ >= extents_.size()) {
            err = "iso: the file ended early";
            return false;
        }
        const Extent& e = extents_[extent_];
        uint64_t left = e.bytes - inExtent_;
        uint32_t count = uint32_t(std::min<uint64_t>(kChunkSectors, (left + kUser - 1) / kUser));
        size_t bytes = size_t(std::min<uint64_t>(left, uint64_t(count) * kUser));
        buffer_.resize(size_t(count) * kUser);
        {
            std::lock_guard<std::mutex> g(*lock_);
            if (!sectors_->Read(e.sector + inExtent_ / kUser, count, buffer_.data(), bytes, err)) return false;
        }
        have_ = bytes;
        used_ = 0;
        inExtent_ += bytes;
        return true;
    }
    std::shared_ptr<Sectors> sectors_;
    std::shared_ptr<std::mutex> lock_;
    std::vector<Extent> extents_;
    uint64_t size_;
    uint64_t done_ = 0;
    size_t extent_ = 0;
    uint64_t inExtent_ = 0;
    std::vector<uint8_t> buffer_;
    size_t have_ = 0;
    size_t used_ = 0;
};

void AppendUtf8(std::string& s, uint32_t c) {
    if (c < 0x80) {
        s += char(c);
    } else if (c < 0x800) {
        s += char(0xC0 | (c >> 6));
        s += char(0x80 | (c & 63));
    } else if (c < 0x10000) {
        s += char(0xE0 | (c >> 12));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    } else {
        s += char(0xF0 | (c >> 18));
        s += char(0x80 | ((c >> 12) & 63));
        s += char(0x80 | ((c >> 6) & 63));
        s += char(0x80 | (c & 63));
    }
}

std::string Utf16BeToUtf8(const uint8_t* p, size_t bytes) {
    std::string out;
    for (size_t i = 0; i + 1 < bytes; i += 2) {
        uint32_t c = (uint32_t(p[i]) << 8) | p[i + 1];
        if (c >= 0xD800 && c < 0xDC00 && i + 3 < bytes) {
            uint32_t lo = (uint32_t(p[i + 2]) << 8) | p[i + 3];
            if (lo >= 0xDC00 && lo < 0xE000) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        AppendUtf8(out, c);
    }
    return out;
}

struct Record {
    uint32_t extent = 0;
    uint32_t size = 0;
    uint8_t flags = 0;
    uint8_t xattr = 0;
    std::string name;
    bool special = false;  // "." and ".."
};

class IsoSource : public Source {
public:
    const char* Kind() const override { return "iso9660"; }
    const std::vector<Entry>& Entries() const override { return entries_; }

    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id >= files_.size()) {
            err = "not an entry of this image";
            return nullptr;
        }
        try {
            return std::make_unique<IsoStream>(sectors_, lock_, files_[size_t(entry.id)].extents, entry.size);
        } catch (...) {
            err = "out of memory";
            return nullptr;
        }
    }

    bool Load(std::shared_ptr<Blob> blob, std::string& err) {
        sectors_ = std::make_shared<Sectors>();
        lock_ = std::make_shared<std::mutex>();
        if (!sectors_->Init(std::move(blob), err)) return false;
        bool havePrimary = false, haveJoliet = false;
        Record primary, joliet;
        for (uint64_t s = 16; s < 16 + 64; s++) {
            uint8_t d[2048];
            if (!sectors_->Read(s, 1, d, sizeof d, err)) return false;
            if (std::memcmp(d + 1, "CD001", 5) != 0 || d[0] == 255) break;
            if (d[0] == 1 && !havePrimary) {
                if (LE16(d + 128) != kUser) {
                    err = "iso: logical block size is not 2048";
                    return false;
                }
                primary = ParseRecord(d + 156, 34, false);
                havePrimary = true;
            } else if (d[0] == 2 && d[88] == '%' && d[89] == '/' && !haveJoliet) {
                joliet = ParseRecord(d + 156, 34, true);
                haveJoliet = true;
            }
        }
        if (!havePrimary) {
            err = "iso: no primary volume descriptor";
            return false;
        }
        unicode_ = haveJoliet;
        Record root = haveJoliet ? joliet : primary;
        std::set<uint32_t> seen;
        if (!Walk(root, "", 0, seen, err)) return false;
        std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.path < b.path; });
        return true;
    }

private:
    Record ParseRecord(const uint8_t* r, size_t len, bool unicode) const {
        Record rec;
        if (len < 34) return rec;
        rec.xattr = r[1];
        rec.extent = LE32(r + 2);
        rec.size = LE32(r + 10);
        rec.flags = r[25];
        size_t n = r[32];
        if (33 + n > len) n = len > 33 ? len - 33 : 0;
        if (n == 1 && (r[33] == 0 || r[33] == 1)) {
            rec.special = true;
            return rec;
        }
        if (unicode) {
            rec.name = Utf16BeToUtf8(r + 33, n);
        } else {
            rec.name.assign(reinterpret_cast<const char*>(r + 33), n);
        }
        return rec;
    }

    bool Walk(const Record& dir, const std::string& prefix, int depth, std::set<uint32_t>& seen, std::string& err) {
        if (depth > 32) {
            err = "iso: folders nest too deeply";
            return false;
        }
        if (!seen.insert(dir.extent).second) return true;  // a loop
        if (dir.size > (16u << 20)) {
            err = "iso: a folder is too large";
            return false;
        }
        uint32_t sectorCount = (dir.size + kUser - 1) / kUser;
        std::vector<uint8_t> data(size_t(sectorCount) * kUser);
        if (!sectors_->Read(uint64_t(dir.extent) + dir.xattr, sectorCount, data.data(), data.size(), err)) return false;
        File pending;
        std::string pendingName;
        uint64_t pendingSize = 0;
        bool havePending = false;
        auto flush = [&]() {
            if (!havePending) return;
            Entry e;
            e.path = prefix + pendingName;
            e.size = pendingSize;
            e.id = files_.size();
            files_.push_back(std::move(pending));
            entries_.push_back(std::move(e));
            pending = File();
            havePending = false;
        };
        size_t pos = 0;
        while (pos < dir.size) {
            size_t n = data[pos];
            if (n == 0) {
                pos = (pos / kUser + 1) * kUser;
                continue;
            }
            if (n < 34 || pos + n > dir.size) break;
            Record r = ParseRecord(&data[pos], n, unicode_);
            pos += n;
            if (r.special || r.name.empty()) continue;
            if (r.flags & 2) {
                flush();
                if (!Walk(r, prefix + r.name + "/", depth + 1, seen, err)) return false;
                continue;
            }
            std::string name = r.name;
            size_t semi = name.rfind(';');
            if (semi != std::string::npos) name.resize(semi);
            if (!unicode_ && name.size() > 1 && name.back() == '.') name.pop_back();
            if (name.empty()) continue;
            if (havePending && name != pendingName) flush();
            if (!havePending) {
                havePending = true;
                pendingName = name;
                pendingSize = 0;
            }
            pending.extents.push_back(Extent{uint64_t(r.extent) + r.xattr, r.size});
            pendingSize += r.size;
            if (!(r.flags & 0x80)) flush();
        }
        flush();
        return true;
    }

    std::shared_ptr<Sectors> sectors_;
    std::shared_ptr<std::mutex> lock_;
    bool unicode_ = false;
    std::vector<File> files_;
    std::vector<Entry> entries_;
};

}  // namespace

std::unique_ptr<Source> OpenIso9660(std::shared_ptr<Blob> blob, std::string& err) {
    if (!blob) {
        err = "iso: no input";
        return nullptr;
    }
    try {
        auto s = std::make_unique<IsoSource>();
        if (!s->Load(std::move(blob), err)) return nullptr;
        return s;
    } catch (...) {
        err = "iso: out of memory";
        return nullptr;
    }
}

}  // namespace tpw::assets
