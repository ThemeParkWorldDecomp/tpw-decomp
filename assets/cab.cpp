// A Microsoft Cabinet ([MS-CAB]) as a Source: the header, the folders (a folder is one compressed stream of CFDATA
// blocks), the files (a folder, an offset and a size in that stream). Folders stored or compressed with MSZIP are
// readable; MSZIP ([MS-MCI]) is raw deflate per block, each block starting with "CK", and each block uses the previous
// block's 32 KB of output as its dictionary (zlib's inflateSetDictionary on a raw stream). Quantum and LZX folders are
// listed, and Open says why it cannot read them. Names lose the leading backslash installers write. A cabinet that continues in another file is refused. The offsets in the
// cabinet are relative to its own first byte, so one can sit inside an installer .exe (FindCabinet).
//
// Reading a file decodes its folder from the start, because each block depends on the one before it. Every stream owns
// its decoder and holds one 32 KB block and its predecessor, so memory stays small. The blocks' checksums are verified.
#include <zlib.h>

#include <algorithm>
#include <cstring>

#include "source.h"

namespace tpw::assets {

namespace {

uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t U32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

const size_t kHeaderSize = 36;

// The cabinet checksum: XOR of little-endian 32-bit words. The one to three bytes left over form a last word the way
// Microsoft's and libmspack's code build it, the first leftover byte in the highest place.
uint32_t CabChecksum(const uint8_t* p, size_t n, uint32_t seed) {
    uint32_t sum = seed;
    while (n >= 4) {
        sum ^= U32(p);
        p += 4;
        n -= 4;
    }
    uint32_t last = 0;
    switch (n) {
        case 3: last |= uint32_t(*p++) << 16; [[fallthrough]];
        case 2: last |= uint32_t(*p++) << 8; [[fallthrough]];
        case 1: last |= uint32_t(*p); break;
        default: break;
    }
    return sum ^ last;
}

struct Folder {
    uint32_t dataOffset = 0;  // relative to the cabinet start
    uint16_t blocks = 0;
    uint16_t type = 0;  // low nibble: 0 stored, 1 MSZIP, 2 Quantum, 3 LZX
};

struct FileRec {
    uint32_t size = 0;
    uint32_t folderOffset = 0;
    uint16_t folder = 0;
};

// Decodes one folder's blocks in order, giving the uncompressed bytes.
class FolderDecoder {
public:
    FolderDecoder(std::shared_ptr<Blob> blob, uint64_t start, const Folder& folder, uint8_t perBlockReserve)
        : blob_(std::move(blob)), nextAt_(start + folder.dataOffset), blocksLeft_(folder.blocks),
          mszip_((folder.type & 0xF) == 1), reserve_(perBlockReserve) {}

    // Reads up to n bytes of the folder's stream from the current position; 0 at the end of the folder.
    int64_t Read(uint8_t* buf, size_t n, std::string& err) {
        while (curPos_ >= cur_.size()) {
            if (blocksLeft_ == 0) return 0;
            if (!NextBlock(err)) return -1;
        }
        size_t take = std::min(n, cur_.size() - curPos_);
        std::memcpy(buf, cur_.data() + curPos_, take);
        curPos_ += take;
        return static_cast<int64_t>(take);
    }

private:
    bool NextBlock(std::string& err) {
        uint8_t h[8];
        int64_t got = blob_->ReadAt(nextAt_, h, sizeof h, err);
        if (got < 0) return false;
        if (got < 8) {
            err = "cab: the cabinet is cut off";
            return false;
        }
        uint32_t csum = U32(h);
        size_t cbData = U16(h + 4), cbUncomp = U16(h + 6);
        if (cbUncomp == 0 || cbData == 0 || cbUncomp > 32768) {
            err = "cab: a data block is empty, continued in another cabinet or too large";
            return false;
        }
        uint64_t dataAt = nextAt_ + 8 + reserve_;
        comp_.resize(cbData);
        got = blob_->ReadAt(dataAt, comp_.data(), cbData, err);
        if (got < 0) return false;
        if (static_cast<size_t>(got) < cbData) {
            err = "cab: the cabinet is cut off";
            return false;
        }
        if (csum != 0) {
            uint32_t sum = CabChecksum(h + 4, 4, 0);
            if (CabChecksum(comp_.data(), cbData, sum) != csum) {
                err = "cab: a data block fails its checksum";
                return false;
            }
        }
        std::swap(prev_, cur_);  // the old block is the dictionary of the new one
        cur_.assign(cbUncomp, 0);
        curPos_ = 0;
        nextAt_ = dataAt + cbData;
        blocksLeft_--;
        if (!mszip_) {
            if (cbData != cbUncomp) {
                err = "cab: a stored block has two different sizes";
                return false;
            }
            std::memcpy(cur_.data(), comp_.data(), cbData);
            return true;
        }
        if (cbData < 2 || comp_[0] != 'C' || comp_[1] != 'K') {
            err = "cab: an MSZIP block lacks its CK header";
            return false;
        }
        z_stream z;
        std::memset(&z, 0, sizeof z);
        if (inflateInit2(&z, -15) != Z_OK) {
            err = "cab: zlib could not start";
            return false;
        }
        bool ok = true;
        if (!prev_.empty()) ok = inflateSetDictionary(&z, prev_.data(), static_cast<uInt>(prev_.size())) == Z_OK;
        if (ok) {
            z.next_in = comp_.data() + 2;
            z.avail_in = static_cast<uInt>(cbData - 2);
            z.next_out = cur_.data();
            z.avail_out = static_cast<uInt>(cbUncomp);
            int rc = inflate(&z, Z_SYNC_FLUSH);
            ok = (rc == Z_STREAM_END || rc == Z_OK) && z.avail_out == 0;
        }
        inflateEnd(&z);
        if (!ok) {
            err = "cab: bad MSZIP data";
            return false;
        }
        return true;
    }

    std::shared_ptr<Blob> blob_;
    uint64_t nextAt_;
    uint32_t blocksLeft_;
    bool mszip_;
    uint8_t reserve_;
    std::vector<uint8_t> comp_, cur_, prev_;
    size_t curPos_ = 0;
};

class CabStream : public Stream {
public:
    CabStream(std::unique_ptr<FolderDecoder> dec, uint32_t skip, uint32_t size)
        : dec_(std::move(dec)), skip_(skip), size_(size), left_(size) {}
    uint64_t Size() const override { return size_; }
    int64_t Read(void* buf, size_t n, std::string& err) override {
        if (n == 0 || left_ == 0) return 0;
        if (skip_ > 0) {
            if (scratch_.empty()) scratch_.resize(1 << 16);
            while (skip_ > 0) {
                int64_t got = dec_->Read(scratch_.data(), std::min<size_t>(scratch_.size(), skip_), err);
                if (got < 0) return -1;
                if (got == 0) {
                    err = "cab: the folder ends before the file starts";
                    return -1;
                }
                skip_ -= static_cast<uint32_t>(got);
            }
        }
        if (n > left_) n = left_;
        int64_t got = dec_->Read(static_cast<uint8_t*>(buf), n, err);
        if (got < 0) return -1;
        if (got == 0) {
            err = "cab: the folder ends inside the file";
            return -1;
        }
        left_ -= static_cast<uint32_t>(got);
        return got;
    }

private:
    std::unique_ptr<FolderDecoder> dec_;
    uint32_t skip_;
    uint32_t size_;
    uint32_t left_;
    std::vector<uint8_t> scratch_;
};

bool KeepName(const std::string& p) {
    if (p.empty() || p[0] == '/') return false;
    for (size_t s = 0, e; s <= p.size(); s = e + 1) {
        e = p.find('/', s);
        if (e == std::string::npos) e = p.size();
        if (p.compare(s, e - s, "..") == 0 && e - s == 2) return false;
    }
    return true;
}

class CabSource : public Source {
public:
    const char* Kind() const override { return "cab"; }
    const std::vector<Entry>& Entries() const override { return entries_; }

    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id >= files_.size()) {
            err = "not an entry of this cabinet";
            return nullptr;
        }
        const FileRec& f = files_[static_cast<size_t>(entry.id)];
        const Folder& fo = folders_[f.folder];
        uint16_t method = fo.type & 0xF;
        if (method != 0 && method != 1) {
            err = "cab: " + entry.path + " uses compression type " + std::to_string(method) + " (only stored and MSZIP are read)";
            return nullptr;
        }
        try {
            auto dec = std::make_unique<FolderDecoder>(blob_, start_, fo, reserveData_);
            return std::make_unique<CabStream>(std::move(dec), f.folderOffset, f.size);
        } catch (...) {
            err = "out of memory";
            return nullptr;
        }
    }

    bool Load(std::shared_ptr<Blob> blob, uint64_t offset, std::string& err) {
        blob_ = std::move(blob);
        start_ = offset;
        uint8_t h[kHeaderSize + 4];
        int64_t got = blob_->ReadAt(offset, h, sizeof h, err);
        if (got < 0) return false;
        if (got < static_cast<int64_t>(kHeaderSize) || std::memcmp(h, "MSCF", 4) != 0) {
            err = "cab: no cabinet header";
            return false;
        }
        uint64_t available = blob_->Size() - offset;
        uint32_t cbCabinet = U32(h + 8), coffFiles = U32(h + 16);
        uint16_t nFolders = U16(h + 26), nFiles = U16(h + 28), flags = U16(h + 30);
        if (h[25] != 1 || h[24] > 3 || cbCabinet > available || coffFiles < kHeaderSize || coffFiles >= cbCabinet ||
            nFolders == 0) {
            err = "cab: the cabinet header is not consistent";
            return false;
        }
        if (flags & 3) {
            err = "cab: this cabinet continues in another cabinet, which is not supported";
            return false;
        }
        uint64_t pos = kHeaderSize;
        uint8_t reserveFolder = 0;
        if (flags & 4) {  // reserve fields present
            uint8_t r[4];
            if (blob_->ReadAt(offset + pos, r, 4, err) != 4) {
                if (err.empty()) err = "cab: the cabinet is cut off";
                return false;
            }
            reserveFolder = r[2];
            reserveData_ = r[3];
            pos += 4 + U16(r);
        }
        // Folders, then (at coffFiles) the files, then the data: read the headers in one piece.
        uint64_t firstData = cbCabinet;
        std::vector<uint8_t> folderBytes(size_t(nFolders) * (8 + reserveFolder));
        if (offset + pos + folderBytes.size() > offset + cbCabinet ||
            blob_->ReadAt(offset + pos, folderBytes.data(), folderBytes.size(), err) !=
                static_cast<int64_t>(folderBytes.size())) {
            if (err.empty()) err = "cab: the cabinet is cut off";
            return false;
        }
        for (size_t i = 0; i < nFolders; i++) {
            const uint8_t* p = folderBytes.data() + i * (8 + reserveFolder);
            Folder f;
            f.dataOffset = U32(p);
            f.blocks = U16(p + 4);
            f.type = U16(p + 6);
            if (f.dataOffset >= cbCabinet) {
                err = "cab: a folder starts outside the cabinet";
                return false;
            }
            firstData = std::min<uint64_t>(firstData, f.dataOffset);
            folders_.push_back(f);
        }
        if (firstData <= coffFiles || firstData - coffFiles > (16u << 20)) {
            err = "cab: the file table is not consistent";
            return false;
        }
        std::vector<uint8_t> table(static_cast<size_t>(firstData - coffFiles));
        if (blob_->ReadAt(offset + coffFiles, table.data(), table.size(), err) != static_cast<int64_t>(table.size())) {
            if (err.empty()) err = "cab: the cabinet is cut off";
            return false;
        }
        size_t at = 0;
        std::vector<std::pair<std::string, size_t>> names;
        for (uint32_t i = 0; i < nFiles; i++) {
            if (at + 16 > table.size()) {
                err = "cab: the file table is cut off";
                return false;
            }
            const uint8_t* p = table.data() + at;
            FileRec f;
            f.size = U32(p);
            f.folderOffset = U32(p + 4);
            uint16_t iFolder = U16(p + 8);
            uint16_t attribs = U16(p + 14);
            size_t nameEnd = at + 16;
            while (nameEnd < table.size() && table[nameEnd] != 0) nameEnd++;
            if (nameEnd >= table.size()) {
                err = "cab: a file name is not terminated";
                return false;
            }
            std::string raw(reinterpret_cast<const char*>(table.data() + at + 16), nameEnd - (at + 16));
            at = nameEnd + 1;
            if (iFolder >= 0xFFFD || iFolder >= nFolders) continue;  // continued from or into another cabinet
            f.folder = iFolder;
            std::string name;
            for (char c : raw) {
                unsigned char u = static_cast<unsigned char>(c);
                if (c == '\\') name += '/';
                else if (u < 0x80 || (attribs & 0x80)) name += c;
                else {  // the DOS code page is unknown: Latin-1
                    name += static_cast<char>(0xC0 | (u >> 6));
                    name += static_cast<char>(0x80 | (u & 63));
                }
            }
            // Installers store paths from the cabinet root with a leading backslash.
            name.erase(0, name.find_first_not_of('/') == std::string::npos ? name.size() : name.find_first_not_of('/'));
            if (!KeepName(name)) continue;
            names.emplace_back(std::move(name), files_.size());
            files_.push_back(f);
        }
        std::sort(names.begin(), names.end());
        for (auto& n : names) {
            Entry e;
            e.path = n.first;
            e.size = files_[n.second].size;
            e.kind = EntryKind::Data;
            e.id = n.second;
            entries_.push_back(std::move(e));
        }
        return true;
    }

private:
    std::shared_ptr<Blob> blob_;
    uint64_t start_ = 0;
    uint8_t reserveData_ = 0;
    std::vector<Folder> folders_;
    std::vector<FileRec> files_;
    std::vector<Entry> entries_;
};

}  // namespace

std::unique_ptr<Source> OpenCab(std::shared_ptr<Blob> blob, uint64_t offset, std::string& err) {
    if (!blob) {
        err = "no blob";
        return nullptr;
    }
    try {
        auto s = std::make_unique<CabSource>();
        if (!s->Load(std::move(blob), offset, err)) return nullptr;
        return s;
    } catch (...) {
        err = "out of memory";
        return nullptr;
    }
}

int64_t FindCabinet(Blob& blob, uint64_t maxScan, std::string& err) {
    try {
        const size_t kChunk = 1 << 20;
        uint64_t size = blob.Size();
        uint64_t limit = std::min(size, maxScan);
        std::vector<uint8_t> buf(kChunk + 3);
        for (uint64_t base = 0; base + kHeaderSize <= size && base < limit; base += kChunk) {
            size_t want = static_cast<size_t>(std::min<uint64_t>(kChunk + 3, size - base));
            int64_t got = blob.ReadAt(base, buf.data(), want, err);
            if (got < 0) return -1;
            for (size_t i = 0; i + 4 <= static_cast<size_t>(got) && i < kChunk; i++) {
                if (buf[i] != 'M' || buf[i + 1] != 'S' || buf[i + 2] != 'C' || buf[i + 3] != 'F') continue;
                uint64_t at = base + i;
                uint8_t h[kHeaderSize];
                std::string ignore;
                if (blob.ReadAt(at, h, sizeof h, ignore) != static_cast<int64_t>(sizeof h)) continue;
                uint32_t cb = U32(h + 8), coff = U32(h + 16);
                if (U32(h + 4) == 0 && U32(h + 12) == 0 && U32(h + 20) == 0 && h[25] == 1 && h[24] <= 3 &&
                    cb >= kHeaderSize && cb <= size - at && coff >= kHeaderSize && coff < cb && U16(h + 26) > 0) {
                    return static_cast<int64_t>(at);
                }
            }
        }
        return -1;
    } catch (...) {
        err = "out of memory";
        return -1;
    }
}

}  // namespace tpw::assets
