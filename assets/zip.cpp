// A zip archive as a Source (PKWARE APPNOTE): stored and deflate entries, zip64, data descriptors. The central directory
// is the index, so a data descriptor after an entry (general purpose flag bit 3, which leaves the local header's sizes
// zero) needs no reading: sizes and the CRC come from the central directory. Reading an entry checks its CRC-32 when
// the whole entry is read. Deflate is zlib's raw inflate. Encrypted entries and other methods are listed, and Open says
// why it cannot read them. Names are taken as UTF-8 (backslashes become '/'); directory entries and names that climb out
// of the archive ("..", an absolute path) are not listed.
#include <zlib.h>

#include <algorithm>
#include <cstring>

#include "source.h"

namespace tpw::assets {

namespace {

uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t U32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
uint64_t U64(const uint8_t* p) { return uint64_t(U32(p)) | (uint64_t(U32(p + 4)) << 32); }

struct Rec {
    uint16_t flags = 0;
    uint16_t method = 0;
    uint32_t crc = 0;
    uint64_t csize = 0;
    uint64_t usize = 0;
    uint64_t localOffset = 0;
};

class ZipStream : public Stream {
public:
    ZipStream(std::shared_ptr<Blob> blob, uint64_t dataStart, const Rec& rec, std::string name)
        : blob_(std::move(blob)), at_(dataStart), rec_(rec), name_(std::move(name)) {
        crc_ = crc32(0, nullptr, 0);
    }
    ~ZipStream() override {
        if (inflating_) inflateEnd(&z_);
    }
    bool Init(std::string& err) {
        if (rec_.method == 8) {
            std::memset(&z_, 0, sizeof z_);
            if (inflateInit2(&z_, -15) != Z_OK) {
                err = "zlib could not start";
                return false;
            }
            inflating_ = true;
            in_.resize(1 << 16);
        }
        return true;
    }
    uint64_t Size() const override { return rec_.usize; }
    int64_t Read(void* buf, size_t n, std::string& err) override {
        if (n == 0 || out_ >= rec_.usize) return 0;
        if (n > rec_.usize - out_) n = static_cast<size_t>(rec_.usize - out_);
        int64_t got = rec_.method == 0 ? ReadStored(buf, n, err) : ReadDeflate(buf, n, err);
        if (got <= 0) return got;
        crc_ = crc32(crc_, static_cast<const Bytef*>(buf), static_cast<uInt>(got));
        out_ += got;
        if (out_ == rec_.usize && uint32_t(crc_) != rec_.crc) {
            err = "zip: checksum mismatch in " + name_;
            return -1;
        }
        return got;
    }

private:
    int64_t ReadStored(void* buf, size_t n, std::string& err) {
        if (n > rec_.csize - inCount_) n = static_cast<size_t>(rec_.csize - inCount_);
        if (n == 0) {
            err = "zip: " + name_ + " is shorter than its size";
            return -1;
        }
        int64_t got = blob_->ReadAt(at_, buf, n, err);
        if (got == 0) err = "zip: " + name_ + " is cut off";
        if (got <= 0) return -1;
        at_ += got;
        inCount_ += got;
        return got;
    }
    int64_t ReadDeflate(void* buf, size_t n, std::string& err) {
        z_.next_out = static_cast<Bytef*>(buf);
        z_.avail_out = static_cast<uInt>(std::min<size_t>(n, 1u << 30));
        while (z_.avail_out > 0) {
            if (z_.avail_in == 0 && inCount_ < rec_.csize) {
                size_t want = static_cast<size_t>(std::min<uint64_t>(in_.size(), rec_.csize - inCount_));
                int64_t got = blob_->ReadAt(at_, in_.data(), want, err);
                if (got <= 0) {
                    if (got == 0) err = "zip: " + name_ + " is cut off";
                    return -1;
                }
                at_ += got;
                inCount_ += got;
                z_.next_in = in_.data();
                z_.avail_in = static_cast<uInt>(got);
            }
            int rc = inflate(&z_, Z_NO_FLUSH);
            if (rc == Z_STREAM_END) {
                streamEnd_ = true;
                break;
            }
            if (rc == Z_BUF_ERROR && z_.avail_in == 0 && inCount_ >= rec_.csize) {
                err = "zip: " + name_ + " ends inside its data";
                return -1;
            }
            if (rc != Z_OK && rc != Z_BUF_ERROR) {
                err = "zip: bad deflate data in " + name_;
                return -1;
            }
        }
        size_t produced = n - z_.avail_out;
        if (produced == 0 && streamEnd_) {
            err = "zip: " + name_ + " is shorter than its size";
            return -1;
        }
        return static_cast<int64_t>(produced);
    }

    std::shared_ptr<Blob> blob_;
    uint64_t at_;
    Rec rec_;
    std::string name_;
    uint64_t inCount_ = 0;  // compressed bytes taken from the blob
    uint64_t out_ = 0;      // bytes returned
    uLong crc_;
    z_stream z_{};
    bool inflating_ = false;
    bool streamEnd_ = false;
    std::vector<uint8_t> in_;
};

class ZipSource : public Source {
public:
    const char* Kind() const override { return "zip"; }
    const std::vector<Entry>& Entries() const override { return entries_; }

    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id >= recs_.size()) {
            err = "not an entry of this zip";
            return nullptr;
        }
        const Rec& r = recs_[static_cast<size_t>(entry.id)];
        if (r.flags & 1) {
            err = "zip: " + entry.path + " is encrypted";
            return nullptr;
        }
        if (r.method != 0 && r.method != 8) {
            err = "zip: " + entry.path + " uses compression method " + std::to_string(r.method);
            return nullptr;
        }
        uint8_t h[30];
        int64_t got = blob_->ReadAt(r.localOffset, h, sizeof h, err);
        if (got < 0) return nullptr;
        if (got < 30 || U32(h) != 0x04034b50) {
            err = "zip: bad local header for " + entry.path;
            return nullptr;
        }
        uint64_t dataStart = r.localOffset + 30 + U16(h + 26) + U16(h + 28);
        if (dataStart + r.csize > blob_->Size()) {
            err = "zip: " + entry.path + " runs past the end of the archive";
            return nullptr;
        }
        try {
            auto s = std::make_unique<ZipStream>(blob_, dataStart, r, entry.path);
            if (!s->Init(err)) return nullptr;
            return s;
        } catch (...) {
            err = "out of memory";
            return nullptr;
        }
    }

    bool Load(std::shared_ptr<Blob> blob, std::string& err) {
        blob_ = std::move(blob);
        uint64_t size = blob_->Size();
        if (size < 22) {
            err = "zip: too small";
            return false;
        }
        // The end-of-central-directory record is in the last 22 + 65535 bytes.
        size_t tailLen = static_cast<size_t>(std::min<uint64_t>(size, 22 + 65535));
        std::vector<uint8_t> tail(tailLen);
        uint64_t tailStart = size - tailLen;
        int64_t got = blob_->ReadAt(tailStart, tail.data(), tailLen, err);
        if (got < 0) return false;
        if (static_cast<size_t>(got) != tailLen) {
            err = "zip: short read";
            return false;
        }
        size_t pos = tailLen - 22 + 1;
        bool found = false;
        while (pos-- > 0) {
            if (U32(&tail[pos]) == 0x06054b50 && pos + 22 + U16(&tail[pos + 20]) <= tailLen) {
                found = true;
                break;
            }
        }
        if (!found) {
            err = "zip: no end-of-central-directory record";
            return false;
        }
        const uint8_t* e = &tail[pos];
        uint64_t eocdAt = tailStart + pos;
        uint64_t count = U16(e + 10), cdSize = U32(e + 12), cdOffset = U32(e + 16);
        uint64_t cdEnd = eocdAt;
        if (U16(e + 4) != 0 || U16(e + 6) != 0) {
            if (U16(e + 4) != 0xFFFF) {
                err = "zip: split archives are not supported";
                return false;
            }
        }
        if (count == 0xFFFF || cdSize == 0xFFFFFFFFu || cdOffset == 0xFFFFFFFFu) {
            uint8_t loc[20];
            if (eocdAt >= 20 && blob_->ReadAt(eocdAt - 20, loc, 20, err) == 20 && U32(loc) == 0x07064b50) {
                uint64_t z64At = U64(loc + 8);
                uint8_t z[56];
                if (z64At + 56 > size || blob_->ReadAt(z64At, z, 56, err) != 56 || U32(z) != 0x06064b50) {
                    if (err.empty()) err = "zip: bad zip64 end record";
                    return false;
                }
                count = U64(z + 32);
                cdSize = U64(z + 40);
                cdOffset = U64(z + 48);
                cdEnd = z64At;
            }
        }
        if (cdSize > (64u << 20)) {
            err = "zip: central directory is too large";
            return false;
        }
        // Data in front of the archive (a self-extractor) moves every offset.
        uint64_t shift = 0;
        if (cdOffset + cdSize < cdEnd) shift = cdEnd - (cdOffset + cdSize);
        if (cdOffset + shift + cdSize > size) {
            err = "zip: central directory is past the end";
            return false;
        }
        std::vector<uint8_t> cd(static_cast<size_t>(cdSize));
        got = blob_->ReadAt(cdOffset + shift, cd.data(), cd.size(), err);
        if (got < 0) return false;
        if (static_cast<size_t>(got) != cd.size()) {
            err = "zip: central directory is cut off";
            return false;
        }
        size_t p = 0;
        for (uint64_t i = 0; i < count; i++) {
            if (p + 46 > cd.size() || U32(&cd[p]) != 0x02014b50) {
                err = "zip: bad central directory";
                return false;
            }
            const uint8_t* c = &cd[p];
            size_t nameLen = U16(c + 28), extraLen = U16(c + 30), commentLen = U16(c + 32);
            if (p + 46 + nameLen + extraLen + commentLen > cd.size()) {
                err = "zip: bad central directory";
                return false;
            }
            Rec r;
            r.flags = U16(c + 8);
            r.method = U16(c + 10);
            r.crc = U32(c + 16);
            r.csize = U32(c + 20);
            r.usize = U32(c + 24);
            r.localOffset = U32(c + 42);
            uint32_t disk = U16(c + 34);
            const uint8_t* x = c + 46 + nameLen;
            size_t xl = extraLen;
            while (xl >= 4) {
                uint16_t id = U16(x), len = U16(x + 2);
                if (size_t(len) + 4 > xl) break;
                if (id == 0x0001) {
                    const uint8_t* v = x + 4;
                    size_t left = len;
                    if (r.usize == 0xFFFFFFFFu && left >= 8) { r.usize = U64(v); v += 8; left -= 8; }
                    if (r.csize == 0xFFFFFFFFu && left >= 8) { r.csize = U64(v); v += 8; left -= 8; }
                    if (r.localOffset == 0xFFFFFFFFu && left >= 8) { r.localOffset = U64(v); v += 8; left -= 8; }
                    if (disk == 0xFFFF && left >= 4) { disk = U32(v); }
                }
                x += 4 + len;
                xl -= 4 + len;
            }
            std::string name(reinterpret_cast<const char*>(c + 46), nameLen);
            p += 46 + nameLen + extraLen + commentLen;
            if (disk != 0) {
                err = "zip: split archives are not supported";
                return false;
            }
            std::replace(name.begin(), name.end(), '\\', '/');
            if (name.empty() || name.back() == '/' || name[0] == '/' || Climbs(name)) continue;
            r.localOffset += shift;
            Entry en;
            en.path = name;
            en.size = r.usize;
            en.id = recs_.size();
            recs_.push_back(r);
            entries_.push_back(std::move(en));
        }
        std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.path < b.path; });
        return true;
    }

private:
    static bool Climbs(const std::string& name) {
        size_t start = 0;
        while (start <= name.size()) {
            size_t slash = name.find('/', start);
            if (slash == std::string::npos) slash = name.size();
            if (name.compare(start, slash - start, "..") == 0) return true;
            start = slash + 1;
        }
        return false;
    }
    std::shared_ptr<Blob> blob_;
    std::vector<Rec> recs_;
    std::vector<Entry> entries_;
};

}  // namespace

std::unique_ptr<Source> OpenZip(std::shared_ptr<Blob> blob, std::string& err) {
    if (!blob) {
        err = "zip: no input";
        return nullptr;
    }
    try {
        auto s = std::make_unique<ZipSource>();
        if (!s->Load(std::move(blob), err)) return nullptr;
        return s;
    } catch (...) {
        err = "zip: out of memory";
        return nullptr;
    }
}

}  // namespace tpw::assets
