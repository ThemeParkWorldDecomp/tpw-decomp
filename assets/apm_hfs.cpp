// Apple partition map and classic HFS (Inside Macintosh: Files) as a Source. The map is only used to find the volume:
// like devtools/import_originals.py's find_hfs, the first "Apple_HFS" partition whose master directory block fits is
// taken, and an image with no map must be a bare HFS volume (the signature "BD" at byte 1024). The catalog B-tree lists
// every file and folder; a file's data and resource forks are runs of allocation blocks (three extents in the catalog
// record, the rest in the extents overflow B-tree). The two trees are small (a few hundred KB for a CD) and are read
// into memory; file data is streamed from the Blob. Names are Mac Roman converted to UTF-8, a '/' becoming ':'.
// HFS Plus volumes (and HFS wrappers around one) are not supported.
#include <algorithm>
#include <cstring>
#include <map>
#include <tuple>

#include "macroman.h"
#include "range_stream.h"
#include "source.h"

namespace tpw::assets {

namespace {

uint16_t B16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
uint32_t B32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

const uint64_t kMaxTreeBytes = 32u << 20;  // the catalog and extents files are read whole; a CD's are well under 1 MB

struct Extent {
    uint16_t start = 0;
    uint16_t count = 0;
};

struct Volume {
    uint64_t partStart = 0;
    uint64_t partLength = 0;
    uint32_t blockSize = 0;     // allocation block size in bytes
    uint64_t firstBlock = 0;    // byte offset of allocation block 0 within the partition
    uint32_t blockCount = 0;
    uint32_t catalogSize = 0;   // bytes
    uint32_t extentsSize = 0;
    Extent catalogExt[3];
    Extent extentsExt[3];
    bool wrapper = false;       // an HFS wrapper around an HFS Plus volume
};

struct FileRec {
    std::string path;
    uint32_t fileId = 0;
    uint32_t dataLen = 0, rsrcLen = 0;
    Extent dataExt[3];
    Extent rsrcExt[3];
    char type[4] = {}, creator[4] = {};  // the Finder info's fdType and fdCreator
};

// Whether an HFS master directory block sits at start + 1024 and its allocation blocks fit in length bytes.
bool ReadMdb(Blob& blob, uint64_t start, uint64_t length, Volume& v, std::string& err) {
    uint8_t m[162];
    if (start + 1024 + sizeof m > blob.Size()) return false;
    int64_t got = blob.ReadAt(start + 1024, m, sizeof m, err);
    if (got != static_cast<int64_t>(sizeof m)) return false;
    if (B16(m) != 0x4244) return false;  // "BD"
    uint32_t blocks = B16(m + 18);
    uint32_t blkSize = B32(m + 20);
    uint32_t first = B16(m + 28);
    if (blkSize == 0 || blkSize % 512 != 0) return false;
    if (512ull * first + uint64_t(blocks) * blkSize > length) return false;
    if (start + length > blob.Size()) return false;
    v.partStart = start;
    v.partLength = length;
    v.blockSize = blkSize;
    v.firstBlock = 512ull * first;
    v.blockCount = blocks;
    v.extentsSize = B32(m + 130);
    v.catalogSize = B32(m + 146);
    for (int i = 0; i < 3; i++) {
        v.extentsExt[i] = {B16(m + 134 + 4 * i), B16(m + 134 + 4 * i + 2)};
        v.catalogExt[i] = {B16(m + 150 + 4 * i), B16(m + 150 + 4 * i + 2)};
    }
    v.wrapper = B16(m + 124) == 0x482B;  // drEmbedSigWord "H+"
    return true;
}

bool FindVolume(Blob& blob, Volume& v, std::string& err) {
    uint8_t head[512];
    int64_t got = blob.ReadAt(0, head, sizeof head, err);
    if (got < 0) return false;
    if (got == static_cast<int64_t>(sizeof head) && head[0] == 'E' && head[1] == 'R') {
        uint32_t blk = B16(head + 2);
        uint32_t sizes[2] = {blk, 512};
        for (int s = 0; s < 2; s++) {
            uint32_t size = sizes[s];
            if (size == 0 || (s == 1 && size == sizes[0])) continue;
            uint8_t e[512];
            if (blob.ReadAt(size, e, 8, err) != 8 || e[0] != 'P' || e[1] != 'M') continue;
            uint32_t count = B32(e + 4);
            for (uint32_t i = 1; i <= count && i < 256; i++) {
                uint64_t at = uint64_t(i) * size;
                if (blob.ReadAt(at, e, 80, err) != 80 || e[0] != 'P' || e[1] != 'M') break;
                uint32_t pstart = B32(e + 8), plen = B32(e + 12);
                char type[33];
                std::memcpy(type, e + 48, 32);
                type[32] = 0;
                if (std::strcmp(type, "Apple_HFS") == 0 &&
                    ReadMdb(blob, uint64_t(pstart) * size, uint64_t(plen) * size, v, err)) {
                    return true;
                }
            }
        }
        err = "hfs: the image has an Apple partition map but no Apple_HFS partition with a valid HFS volume in it";
        return false;
    }
    if (ReadMdb(blob, 0, blob.Size(), v, err)) return true;
    if (err.empty()) err = "hfs: no HFS volume found (no partition map and no HFS header at byte 1024)";
    return false;
}

class HfsSource : public Source {
public:
    const char* Kind() const override { return "hfs"; }
    const std::vector<Entry>& Entries() const override { return entries_; }

    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        size_t index = static_cast<size_t>(entry.id >> 1);
        if (index >= files_.size()) {
            err = "not an entry of this volume";
            return nullptr;
        }
        const FileRec& f = files_[index];
        bool rsrc = (entry.id & 1) != 0;
        try {
            std::vector<ByteRange> ranges;
            if (!Ranges(f, rsrc, ranges, err)) return nullptr;
            return std::make_unique<RangeStream>(blob_, std::move(ranges), rsrc ? f.rsrcLen : f.dataLen);
        } catch (...) {
            err = "out of memory";
            return nullptr;
        }
    }

    bool Load(std::shared_ptr<Blob> blob, std::string& err) {
        blob_ = std::move(blob);
        if (!FindVolume(*blob_, vol_, err)) return false;
        if (vol_.wrapper) {
            err = "hfs: this is an HFS Plus volume, which is not supported";
            return false;
        }
        std::vector<uint8_t> extents, catalog;
        // The extents file has no overflow of its own: its three extents are all it can have.
        if (!ReadFile(vol_.extentsExt, vol_.extentsSize, extents, err)) return false;
        if (!LoadOverflow(extents, err)) return false;
        // The catalog file (id 4) may continue in the overflow records.
        std::vector<ByteRange> all;
        if (!Ranges4(vol_.catalogExt, 0x00, 4, vol_.catalogSize, all, err)) return false;
        if (!ReadRanges(all, vol_.catalogSize, catalog, err)) return false;
        return LoadCatalog(catalog, err);
    }

private:
    // The bytes of allocation block run [start, start + count) as a byte range of the blob.
    ByteRange BlockRange(uint32_t start, uint32_t count) const {
        return {vol_.partStart + vol_.firstBlock + uint64_t(start) * vol_.blockSize, uint64_t(count) * vol_.blockSize};
    }

    bool ReadRanges(const std::vector<ByteRange>& ranges, uint64_t size, std::vector<uint8_t>& out, std::string& err) {
        if (size > kMaxTreeBytes) {
            err = "hfs: a B-tree file is implausibly large";
            return false;
        }
        out.assign(static_cast<size_t>(size), 0);
        uint64_t done = 0;
        for (const ByteRange& r : ranges) {
            uint64_t take = std::min<uint64_t>(r.length, size - done);
            uint64_t got = 0;
            while (got < take) {
                int64_t n = blob_->ReadAt(r.offset + got, out.data() + done + got, static_cast<size_t>(take - got), err);
                if (n < 0) return false;
                if (n == 0) {
                    err = "hfs: the image is cut off";
                    return false;
                }
                got += n;
            }
            done += take;
            if (done == size) break;
        }
        if (done < size) {
            err = "hfs: a B-tree file is shorter than its size";
            return false;
        }
        return true;
    }

    bool ReadFile(const Extent ext[3], uint64_t size, std::vector<uint8_t>& out, std::string& err) {
        std::vector<ByteRange> ranges;
        for (int i = 0; i < 3; i++) {
            if (ext[i].count == 0) continue;
            if (uint64_t(ext[i].start) + ext[i].count > vol_.blockCount) {
                err = "hfs: an extent lies outside the volume";
                return false;
            }
            ranges.push_back(BlockRange(ext[i].start, ext[i].count));
        }
        return ReadRanges(ranges, size, out, err);
    }

    // The extents of a fork: the three in its record, then the overflow records, until `size` bytes are covered.
    bool Ranges4(const Extent ext[3], uint8_t fork, uint32_t fileId, uint64_t size, std::vector<ByteRange>& out,
                 std::string& err) {
        uint32_t need = static_cast<uint32_t>((size + vol_.blockSize - 1) / vol_.blockSize);
        uint32_t covered = 0;
        auto add = [&](const Extent* e) -> bool {
            for (int i = 0; i < 3 && covered < need; i++) {
                if (e[i].count == 0) continue;
                if (uint64_t(e[i].start) + e[i].count > vol_.blockCount) {
                    err = "hfs: an extent lies outside the volume";
                    return false;
                }
                out.push_back(BlockRange(e[i].start, e[i].count));
                covered += e[i].count;
            }
            return true;
        };
        if (!add(ext)) return false;
        while (covered < need) {
            auto it = overflow_.find(std::make_tuple(fileId, fork, covered));
            if (it == overflow_.end()) {
                err = "hfs: a fork's extents are missing from the overflow file";
                return false;
            }
            uint32_t before = covered;
            if (!add(it->second.e)) return false;
            if (covered == before) {
                err = "hfs: an overflow record has no extents";
                return false;
            }
        }
        return true;
    }

    bool Ranges(const FileRec& f, bool rsrc, std::vector<ByteRange>& out, std::string& err) {
        return Ranges4(rsrc ? f.rsrcExt : f.dataExt, rsrc ? 0xFF : 0x00, f.fileId, rsrc ? f.rsrcLen : f.dataLen, out, err);
    }

    struct Rec3 {
        Extent e[3];
    };

    // Calls fn(record, length) for every leaf record of a B-tree, following the leaf chain.
    template <typename Fn>
    bool WalkLeaves(const std::vector<uint8_t>& t, std::string& err, Fn fn) {
        if (t.size() < 512) {
            err = "hfs: a B-tree is too small";
            return false;
        }
        const uint8_t* h = t.data() + 14;  // header record
        uint32_t nodeSize = B16(h + 18);
        uint32_t node = B32(h + 10);  // first leaf
        if (nodeSize < 512 || (nodeSize & (nodeSize - 1)) != 0 || t.size() < nodeSize) {
            err = "hfs: bad B-tree node size";
            return false;
        }
        size_t nodes = t.size() / nodeSize;
        for (size_t visits = 0; node != 0 && visits <= nodes; visits++) {
            if (node >= nodes) {
                err = "hfs: a B-tree link points outside the tree";
                return false;
            }
            const uint8_t* n = t.data() + size_t(node) * nodeSize;
            if (static_cast<int8_t>(n[8]) != -1) {
                err = "hfs: a leaf chain reaches a node that is not a leaf";
                return false;
            }
            uint32_t recs = B16(n + 10);
            for (uint32_t i = 0; i < recs; i++) {
                size_t slot = nodeSize - 2 * (i + 1);
                uint32_t off = B16(n + slot);
                uint32_t next = B16(n + slot - 2);  // the next record starts where this one ends (free space follows)
                if (off < 14 || off >= slot || next < off || next > slot) {
                    err = "hfs: a B-tree record offset is bad";
                    return false;
                }
                if (!fn(n + off, next - off, err)) return false;
            }
            node = B32(n);  // fLink
            if (node == 0) return true;
        }
        if (node != 0) {
            err = "hfs: the B-tree leaf chain loops";
            return false;
        }
        return true;
    }

    bool LoadOverflow(const std::vector<uint8_t>& t, std::string& err) {
        if (t.empty()) return true;
        return WalkLeaves(t, err, [&](const uint8_t* r, size_t len, std::string&) -> bool {
            if (len < 8 + 12 || r[0] != 7) return true;
            uint8_t fork = r[1];
            uint32_t fileId = B32(r + 2);
            uint32_t start = B16(r + 6);
            Rec3 rec;
            for (int i = 0; i < 3; i++) rec.e[i] = {B16(r + 8 + 4 * i), B16(r + 8 + 4 * i + 2)};
            overflow_[std::make_tuple(fileId, fork, start)] = rec;
            return true;
        });
    }

    struct Dir {
        uint32_t parent;
        std::string name;
    };
    struct Pending {
        uint32_t parent;
        std::string name;
        FileRec rec;
    };

    bool LoadCatalog(const std::vector<uint8_t>& t, std::string& err) {
        std::map<uint32_t, Dir> dirs;
        std::vector<Pending> pending;
        bool ok = WalkLeaves(t, err, [&](const uint8_t* r, size_t len, std::string&) -> bool {
            size_t keyLen = r[0];
            if (keyLen < 6 || keyLen + 1 > len) return true;
            uint32_t parent = B32(r + 2);
            size_t nameLen = r[6];
            if (nameLen + 7 > keyLen + 1 || nameLen > 31) return true;
            std::string name = MacRomanToUtf8(reinterpret_cast<const char*>(r + 7), nameLen, true);
            size_t d = (keyLen + 1 + 1) & ~size_t(1);  // the data starts at an even offset
            if (d + 2 > len) return true;
            const uint8_t* p = r + d;
            size_t left = len - d;
            if (p[0] == 1 && left >= 10) {  // folder record
                dirs[B32(p + 6)] = Dir{parent, name};
            } else if (p[0] == 2 && left >= 98) {  // file record
                Pending f;
                f.parent = parent;
                f.name = name;
                f.rec.fileId = B32(p + 20);
                f.rec.dataLen = B32(p + 26);
                f.rec.rsrcLen = B32(p + 36);
                std::memcpy(f.rec.type, p + 4, 4);
                std::memcpy(f.rec.creator, p + 8, 4);
                for (int i = 0; i < 3; i++) {
                    f.rec.dataExt[i] = {B16(p + 74 + 4 * i), B16(p + 74 + 4 * i + 2)};
                    f.rec.rsrcExt[i] = {B16(p + 86 + 4 * i), B16(p + 86 + 4 * i + 2)};
                }
                pending.push_back(std::move(f));
            }
            return true;
        });
        if (!ok) return false;
        for (Pending& f : pending) {
            // Walk up to the root (folder id 2, whose parent is 1); the volume name is not part of the path.
            std::string path = f.name.empty() ? "_" : f.name;
            uint32_t at = f.parent;
            bool bad = false;
            for (int depth = 0; at != 1 && at != 2; depth++) {
                auto it = dirs.find(at);
                if (it == dirs.end() || depth > 100) {
                    err = "hfs: the catalog has a file outside the folder tree";
                    return false;
                }
                path = (it->second.name.empty() ? "_" : it->second.name) + "/" + path;
                at = it->second.parent;
            }
            // A component of "." or ".." cannot be a file name here: such files are not listed.
            for (size_t s = 0, e; s <= path.size() && !bad; s = e + 1) {
                e = path.find('/', s);
                if (e == std::string::npos) e = path.size();
                std::string c = path.substr(s, e - s);
                if (c == "." || c == "..") bad = true;
            }
            if (bad) continue;
            f.rec.path = std::move(path);
            files_.push_back(std::move(f.rec));
        }
        std::vector<size_t> order(files_.size());
        for (size_t i = 0; i < order.size(); i++) order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return files_[a].path < files_[b].path; });
        std::vector<FileRec> sorted;
        sorted.reserve(files_.size());
        for (size_t i : order) sorted.push_back(std::move(files_[i]));
        files_ = std::move(sorted);
        for (size_t i = 0; i < files_.size(); i++) {
            Entry e;
            e.path = files_[i].path;
            e.size = files_[i].dataLen;
            e.kind = EntryKind::Data;
            e.id = uint64_t(i) << 1;
            e.hasFinderInfo = true;
            std::memcpy(e.finderType, files_[i].type, 4);
            std::memcpy(e.finderCreator, files_[i].creator, 4);
            entries_.push_back(e);
            if (files_[i].rsrcLen > 0) {
                e.size = files_[i].rsrcLen;
                e.kind = EntryKind::Rsrc;
                e.hasFinderInfo = false;
                e.id = (uint64_t(i) << 1) | 1;
                entries_.push_back(e);
            }
        }
        return true;
    }

    std::shared_ptr<Blob> blob_;
    Volume vol_;
    std::map<std::tuple<uint32_t, uint8_t, uint32_t>, Rec3> overflow_;  // (file id, fork, first block) -> extents
    std::vector<FileRec> files_;
    std::vector<Entry> entries_;
};

}  // namespace

std::unique_ptr<Source> OpenHfs(std::shared_ptr<Blob> blob, std::string& err) {
    if (!blob) {
        err = "no blob";
        return nullptr;
    }
    try {
        auto s = std::make_unique<HfsSource>();
        if (!s->Load(std::move(blob), err)) return nullptr;
        return s;
    } catch (...) {
        err = "out of memory";
        return nullptr;
    }
}

}  // namespace tpw::assets
