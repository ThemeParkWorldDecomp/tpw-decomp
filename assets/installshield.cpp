// installshield.cpp: the InstallShield cabinet reader (installshield.h).
//
// Layout, all little-endian. A header file starts with a common header (signature "ISc(" 0x28635349, version, a volume
// info word, the offset and size of the cabinet descriptor). The descriptor holds the file table offset, the directory
// and file counts and 71 offsets of file group lists and 71 of component lists. The file table starts with one offset per
// directory name, then (version 6 and later) the file descriptors, 87 bytes each, at the descriptor's second file table
// offset: flags, expanded size, compressed size, data offset (all sizes 64-bit), an MD5, the name offset, the directory
// index, the link fields and the volume the bytes start in. Names are NUL-terminated and offset from the file table's
// start. A group list entry is {name offset, descriptor offset, next entry offset}; a group descriptor names the group
// and gives the indexes of its first and last file. Every volume starts with the same common header followed by its own
// header: the data offset, the first and last file indexes it holds and, for a file that continues from the previous
// volume or into the next, where its part starts and how long it is.
#include "installshield.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>

#include <zlib.h>

namespace tpw::assets {
namespace {

constexpr uint32_t kSignature = 0x28635349;   // "ISc("
constexpr size_t kCommonHeaderSize = 20;
constexpr size_t kGroupLists = 71;
constexpr size_t kFileDescriptorSize = 0x57;
constexpr uint64_t kMaxHeader = 64ull << 20;   // the biggest file table this reader will load whole

constexpr uint16_t kFlagSplit = 1;
constexpr uint16_t kFlagObfuscated = 2;
constexpr uint16_t kFlagCompressed = 4;
constexpr uint16_t kFlagInvalid = 8;
constexpr uint8_t kLinkPrevious = 1;

void Require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

uint16_t Get16(const std::vector<uint8_t>& b, uint64_t o) {
    Require(o <= b.size() && b.size() - o >= 2, "the file table runs past the end of the header");
    return uint16_t(b[o] | (b[o + 1] << 8));
}
uint32_t Get32(const std::vector<uint8_t>& b, uint64_t o) {
    Require(o <= b.size() && b.size() - o >= 4, "the file table runs past the end of the header");
    return uint32_t(b[o]) | (uint32_t(b[o + 1]) << 8) | (uint32_t(b[o + 2]) << 16) | (uint32_t(b[o + 3]) << 24);
}
uint64_t Get64(const std::vector<uint8_t>& b, uint64_t o) {
    return uint64_t(Get32(b, o)) | (uint64_t(Get32(b, o + 4)) << 32);
}
std::string GetString(const std::vector<uint8_t>& b, uint64_t o) {
    Require(o <= b.size(), "a name lies past the end of the header");
    size_t end = o;
    while (end < b.size() && b[end] != 0) ++end;
    Require(end < b.size(), "a name runs past the end of the header");
    return std::string(reinterpret_cast<const char*>(b.data() + o), end - o);
}

// The major version named by the version word: 1.x.y words encode it in bits 12 to 15 (0x0100600c is 6), the later
// 2.x and 4.x words carry a hundredfold number in the low half (0x00000bb8 is 30 for InstallShield 2012).
int MajorVersion(uint32_t version) {
    const uint32_t scheme = version >> 24;
    if (scheme == 1) {
        // InstallShield 5 words (0x01520001 and the like) have no number here.
        const int major = int((version >> 12) & 0xf);
        return major == 0 ? 5 : major;
    }
    if (scheme == 2 || scheme == 4) {
        const uint32_t major = version & 0xffff;
        return major >= 100 ? int(major / 100) : int(major);
    }
    return 0;
}

// The volume header's fields (version 6 and later: every field is 64-bit as two words).
struct VolumeHeader {
    uint64_t dataOffset = 0;
    uint32_t firstFile = 0, lastFile = 0;
    uint64_t firstFileOffset = 0, firstFileExpanded = 0, firstFileCompressed = 0;
    uint64_t lastFileOffset = 0, lastFileExpanded = 0, lastFileCompressed = 0;
};

struct Volume {
    std::shared_ptr<Blob> blob;
    VolumeHeader header;
};

struct FileRecord {
    InstallShieldFile file;
    uint64_t compressedSize = 0;
    uint64_t dataOffset = 0;
    uint32_t linkPrevious = 0;
    uint8_t linkFlags = 0;
    uint8_t md5[16] = {};
    bool checked = false;     // the table's MD5 is of the file's content (InstallShield 6 and later), so a read checks it
};

// MD5 (RFC 1321), for the per-file checksums the cabinet's table carries.
class Md5 {
public:
    void Update(const uint8_t* data, size_t n) {
        total_ += n;
        while (n > 0) {
            const size_t take = std::min(n, size_t(64) - fill_);
            std::memcpy(buf_ + fill_, data, take);
            fill_ += take;
            data += take;
            n -= take;
            if (fill_ == 64) {
                Block(buf_);
                fill_ = 0;
            }
        }
    }
    void Finish(uint8_t out[16]) {
        const uint64_t bits = total_ * 8;
        uint8_t pad = 0x80;
        Update(&pad, 1);
        pad = 0;
        while (fill_ != 56) Update(&pad, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = uint8_t(bits >> (8 * i));
        Update(len, 8);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) out[4 * i + j] = uint8_t(h_[i] >> (8 * j));
        }
    }

private:
    static uint32_t Rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }
    void Block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
            0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
            0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
            0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
            0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
            0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
            0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
            0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
        static const int r[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                  5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                  4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                  6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
        uint32_t w[16];
        for (int i = 0; i < 16; ++i) {
            w[i] = uint32_t(p[4 * i]) | (uint32_t(p[4 * i + 1]) << 8) | (uint32_t(p[4 * i + 2]) << 16) |
                   (uint32_t(p[4 * i + 3]) << 24);
        }
        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
        for (int i = 0; i < 64; ++i) {
            uint32_t f;
            int g;
            if (i < 16) {
                f = (b & c) | (~b & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | ~d);
                g = (7 * i) % 16;
            }
            const uint32_t t = d;
            d = c;
            c = b;
            b = b + Rotl(a + f + k[i] + w[g], r[i]);
            a = t;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
    }
    uint32_t h_[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    uint8_t buf_[64];
    size_t fill_ = 0;
    uint64_t total_ = 0;
};

class Reader;

// Deobfuscation: each stored byte is XORed with 0xd5, rotated right by two bits, and lowered by (its index in the file's
// stored bytes modulo 0x47).
void Deobfuscate(uint8_t* buf, size_t n, uint64_t& seed) {
    for (size_t i = 0; i < n; ++i, ++seed) {
        const uint8_t x = uint8_t(buf[i] ^ 0xd5);
        const uint8_t r = uint8_t((x >> 2) | (x << 6));
        buf[i] = uint8_t(r - uint8_t(seed % 0x47));
    }
}

// The stored bytes of one file, read forward across the volumes it spans and deobfuscated.
class StoredBytes {
public:
    StoredBytes(Reader& reader, const FileRecord& rec, uint32_t index);
    // Fills exactly n bytes or fails (-1 with err); 0 only when the file's stored bytes are used up.
    int64_t ReadExactly(uint8_t* buf, size_t n, std::string& err);
    uint64_t Left() const { return total_ - done_; }

private:
    bool OpenVolume(std::string& err);
    Reader& reader_;
    const FileRecord& rec_;
    uint32_t index_;
    uint64_t total_;          // the stored size of the whole file
    uint64_t done_ = 0;
    uint64_t seed_ = 0;
    int volume_;              // the one open now
    std::shared_ptr<Blob> blob_;
    uint64_t at_ = 0;         // the next byte in the open volume
    uint64_t volumeLeft_ = 0; // the file's bytes left in the open volume
    bool opened_ = false;
    // Read-ahead: the volume is read in 256 KB pieces, not chunk by chunk (a chunk's length is 2 bytes).
    std::vector<uint8_t> ahead_;
    size_t aheadAt_ = 0;
};

class ChunkStream : public Stream {
public:
    ChunkStream(std::shared_ptr<const Reader> hold, Reader& reader, const FileRecord& rec, uint32_t index)
        : hold_(std::move(hold)), stored_(reader, rec, index), rec_(rec), size_(rec.file.size),
          compressed_(rec.file.compressed) {}
    uint64_t Size() const override { return size_; }
    int64_t Read(void* buf, size_t n, std::string& err) override;

private:
    bool NextChunk(std::string& err);
    std::shared_ptr<const Reader> hold_;   // the Source stays alive while a stream reads it
    StoredBytes stored_;
    const FileRecord& rec_;
    uint64_t size_;
    bool compressed_;
    uint64_t produced_ = 0;
    Md5 md5_;
    bool verified_ = false;
    std::vector<uint8_t> in_;
    std::vector<uint8_t> out_;
    size_t outAt_ = 0;
};

class Reader : public InstallShieldSource, public std::enable_shared_from_this<Reader> {
public:
    Reader(InstallShieldVolumes volumes) : volumes_(std::move(volumes)) {}
    const char* Kind() const override { return "installshield"; }
    const std::vector<Entry>& Entries() const override { return entries_; }
    int MajorVersion() const override { return major_; }
    const std::vector<std::string>& Groups() const override { return groups_; }
    const std::vector<InstallShieldFile>& Files() const override { return files_; }
    bool VolumesOf(const std::string& group, std::vector<int>& out, std::string& err) override {
        out.clear();
        int volume = 1;
        for (size_t i = 0; i < records_.size(); ++i) {
            const FileRecord& r = records_[i];
            if (!r.file.valid || r.file.group != group) continue;
            VolumeHeader vh;
            if (major_ <= 5) {
                // as StoredBytes::OpenVolume: the first volume whose last file is not before this one
                if (!VolumeBlob(volume, vh, err)) return false;
                for (int guard = 0; uint32_t(i) > vh.lastFile && guard < 999; ++guard) {
                    ++volume;
                    if (!VolumeBlob(volume, vh, err)) return false;
                }
            } else {
                volume = r.file.volume;
                if (!VolumeBlob(volume, vh, err)) return false;
            }
            out.push_back(volume);
            if (!r.file.split) continue;
            // the piece in each volume, as reading takes it (the header's count, else the rest), until the file is whole
            const uint64_t total = r.file.compressed ? r.compressedSize : r.file.size;
            uint64_t piece = uint32_t(i) == vh.lastFile ? (r.file.compressed ? vh.lastFileCompressed : vh.lastFileExpanded) : 0;
            if (piece == 0 || piece > total) piece = total;
            uint64_t done = piece;
            int next = volume;
            for (int guard = 0; done < total && guard < 999; ++guard) {
                ++next;
                if (!VolumeBlob(next, vh, err)) return false;
                if (vh.firstFile != uint32_t(i)) {
                    err = "data" + std::to_string(next) + ".cab does not hold the rest of " + r.file.path;
                    return false;
                }
                out.push_back(next);
                piece = r.file.compressed ? vh.firstFileCompressed : vh.firstFileExpanded;
                if (piece == 0 || piece > total - done) piece = total - done;
                done += piece;
            }
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return true;
    }
    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id >= records_.size() || !records_[size_t(entry.id)].file.valid) {
            err = "not an entry of this cabinet";
            return nullptr;
        }
        // A file linked to an earlier one shares that one's bytes (the installer wrote the content once).
        uint32_t index = uint32_t(entry.id);
        for (int hops = 0; records_[index].linkFlags & kLinkPrevious; ++hops) {
            const uint32_t prev = records_[index].linkPrevious;
            if (prev >= records_.size() || prev == index || hops > 64) {
                err = "a file link in the cabinet's table leads nowhere";
                return nullptr;
            }
            index = prev;
        }
        try {
            return std::make_unique<ChunkStream>(shared_from_this(), *this, records_[index], index);
        } catch (const std::exception& e) {
            err = e.what();
            return nullptr;
        }
    }

    // Parses the header. Throws runtime_error with the reason.
    void Parse(const std::vector<uint8_t>& h) {
        Require(h.size() >= kCommonHeaderSize && Get32(h, 0) == kSignature, "not an InstallShield cabinet header");
        major_ = tpw::assets::MajorVersion(Get32(h, 4));
        Require(major_ >= 5, "an InstallShield cabinet of an unknown version");
        const bool old = major_ <= 5;
        const uint64_t desc = Get32(h, 12);
        Require(Get32(h, 16) != 0, "the cabinet's file table is not in this file");
        const uint64_t fileTable = desc + Get32(h, desc + 0x0c);
        const uint32_t directories = Get32(h, desc + 0x1c);
        const uint32_t files = Get32(h, desc + 0x28);
        const uint64_t descriptors = fileTable + Get32(h, desc + 0x2c);
        Require(files < (1u << 22) && directories < (1u << 20), "the cabinet's table is absurdly large");
        std::vector<std::string> dirs(directories);
        for (uint32_t i = 0; i < directories; ++i) dirs[i] = GetString(h, fileTable + Get32(h, fileTable + 4ull * i));
        records_.resize(files);
        for (uint32_t i = 0; i < files; ++i) {
            FileRecord& r = records_[i];
            uint16_t flags;
            uint32_t nameOffset, dir;
            if (old) {
                // InstallShield 5: the file table's offsets continue after the directories' with one per file, each
                // leading to a descriptor of 32-bit sizes and offsets (its MD5 field is not a hash of the content).
                const uint64_t q = fileTable + Get32(h, fileTable + 4ull * (uint64_t(directories) + i));
                Require(q + 0x3a <= h.size(), "the file descriptors run past the end of the header");
                nameOffset = Get32(h, q);
                dir = Get32(h, q + 4);
                flags = Get16(h, q + 8);
                r.file.size = Get32(h, q + 10);
                r.compressedSize = Get32(h, q + 14);
                r.dataOffset = Get32(h, q + 0x26);
                r.file.volume = 1;   // the volume headers say which volume holds which files
            } else {
                const uint64_t q = descriptors + uint64_t(i) * kFileDescriptorSize;
                Require(q + kFileDescriptorSize <= h.size(), "the file descriptors run past the end of the header");
                flags = Get16(h, q);
                r.file.size = Get64(h, q + 2);
                r.compressedSize = Get64(h, q + 10);
                r.dataOffset = Get64(h, q + 18);
                std::memcpy(r.md5, h.data() + q + 26, 16);
                nameOffset = Get32(h, q + 58);
                dir = Get16(h, q + 62);
                r.linkPrevious = Get32(h, q + 76);
                r.linkFlags = h[q + 84];
                r.file.volume = Get16(h, q + 85);
                for (uint8_t byte : r.md5) r.checked = r.checked || byte != 0;
            }
            r.file.compressed = (flags & kFlagCompressed) != 0;
            r.file.obfuscated = (flags & kFlagObfuscated) != 0;
            r.file.split = (flags & kFlagSplit) != 0;
            r.file.valid = (flags & kFlagInvalid) == 0;
            if (!r.file.valid) continue;
            Require(dir < directories, "a file names a directory the table does not have");
            r.file.path = JoinPath(dirs[dir], GetString(h, fileTable + nameOffset));
        }
        // The groups, each with its range of file indexes.
        for (size_t slot = 0; slot < kGroupLists; ++slot) {
            uint32_t at = Get32(h, desc + 0x3e + 4 * slot);
            for (int guard = 0; at != 0 && guard < 100000; ++guard) {
                const std::string name = GetString(h, desc + Get32(h, desc + at));
                const uint64_t gd = desc + Get32(h, desc + at + 4);
                const uint64_t range = gd + 4 + (old ? 0x48 : 0x12);
                const uint32_t first = Get32(h, range), last = Get32(h, range + 4);
                groups_.push_back(name);
                if (first <= last && last < files) {
                    for (uint32_t i = first; i <= last; ++i) {
                        if (records_[i].file.group.empty()) records_[i].file.group = name;
                    }
                }
                at = Get32(h, desc + at + 8);
            }
        }
        files_.reserve(files);
        for (uint32_t i = 0; i < files; ++i) {
            files_.push_back(records_[i].file);
            if (!records_[i].file.valid) continue;
            Entry e;
            e.path = records_[i].file.group + "/" + records_[i].file.path;
            e.size = records_[i].file.size;
            e.id = i;
            entries_.push_back(std::move(e));
        }
        std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.path < b.path; });
    }

    // The volume's blob and header, opened once. Null with a message.
    std::shared_ptr<Blob> VolumeBlob(int volume, VolumeHeader& header, std::string& err) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = open_.find(volume);
        if (it == open_.end()) {
            Volume v;
            if (volume < 1 || volume > 999) {
                err = "the cabinet names volume " + std::to_string(volume);
                return nullptr;
            }
            v.blob = volumes_(volume, err);
            if (!v.blob) {
                if (err.empty()) err = "data" + std::to_string(volume) + ".cab is missing";
                return nullptr;
            }
            uint8_t raw[kCommonHeaderSize + 64];
            if (v.blob->ReadAt(0, raw, sizeof raw, err) != int64_t(sizeof raw)) {
                if (err.empty()) err = "data" + std::to_string(volume) + ".cab is too short to be a cabinet volume";
                return nullptr;
            }
            std::vector<uint8_t> b(raw, raw + sizeof raw);
            if (Get32(b, 0) != kSignature) {
                err = "data" + std::to_string(volume) + ".cab is not an InstallShield cabinet volume";
                return nullptr;
            }
            const size_t p = kCommonHeaderSize;
            if (major_ <= 5) {
                v.header.dataOffset = Get32(b, p);
                v.header.firstFile = Get32(b, p + 8);
                v.header.lastFile = Get32(b, p + 12);
                v.header.firstFileOffset = Get32(b, p + 16);
                v.header.firstFileExpanded = Get32(b, p + 20);
                v.header.firstFileCompressed = Get32(b, p + 24);
                v.header.lastFileOffset = Get32(b, p + 28);
                v.header.lastFileExpanded = Get32(b, p + 32);
                v.header.lastFileCompressed = Get32(b, p + 36);
            } else {
                v.header.dataOffset = Get64(b, p);
                v.header.firstFile = Get32(b, p + 8);
                v.header.lastFile = Get32(b, p + 12);
                v.header.firstFileOffset = Get64(b, p + 16);
                v.header.firstFileExpanded = Get64(b, p + 24);
                v.header.firstFileCompressed = Get64(b, p + 32);
                v.header.lastFileOffset = Get64(b, p + 40);
                v.header.lastFileExpanded = Get64(b, p + 48);
                v.header.lastFileCompressed = Get64(b, p + 56);
            }
            it = open_.emplace(volume, std::move(v)).first;
        }
        header = it->second.header;
        return it->second.blob;
    }

private:
    static std::string JoinPath(const std::string& dir, const std::string& name) {
        std::string p = dir.empty() ? name : dir + "\\" + name;
        for (char& c : p) {
            if (c == '\\') c = '/';
        }
        // a hostile table cannot climb out of its folder or hide a file behind an empty name
        std::string out;
        size_t at = 0;
        while (at <= p.size()) {
            size_t end = p.find('/', at);
            if (end == std::string::npos) end = p.size();
            std::string part = p.substr(at, end - at);
            if (part.empty() || part == ".") part = "_";
            if (part == "..") part = "_";
            if (!out.empty()) out += '/';
            out += part;
            at = end + 1;
        }
        return out;
    }

    InstallShieldVolumes volumes_;
    int major_ = 0;
    std::vector<std::string> groups_;
    std::vector<FileRecord> records_;
    std::vector<InstallShieldFile> files_;
    std::vector<Entry> entries_;
    std::mutex mutex_;
    std::map<int, Volume> open_;
};

StoredBytes::StoredBytes(Reader& reader, const FileRecord& rec, uint32_t index)
    : reader_(reader), rec_(rec), index_(index), total_(rec.file.compressed ? rec.compressedSize : rec.file.size),
      volume_(rec.file.volume) {}

bool StoredBytes::OpenVolume(std::string& err) {
    VolumeHeader vh;
    blob_ = reader_.VolumeBlob(volume_, vh, err);
    if (!blob_) return false;
    if (reader_.MajorVersion() <= 5 && done_ == 0) {
        // An InstallShield 5 table does not say which volume holds a file: the first volume whose last file is not
        // before this one does.
        for (int guard = 0; index_ > vh.lastFile && guard < 999; ++guard) {
            ++volume_;
            blob_ = reader_.VolumeBlob(volume_, vh, err);
            if (!blob_) return false;
        }
    }
    at_ = rec_.dataOffset;
    volumeLeft_ = total_;
    ahead_.clear();
    aheadAt_ = 0;
    if (rec_.file.split) {
        // The part of this file in this volume: the volume header says where it starts and how long it is, as the
        // volume's last file (it goes on in the next volume) or its first (it came from the previous one).
        const bool compressed = rec_.file.compressed;
        if (index_ == vh.lastFile) {
            at_ = vh.lastFileOffset;
            volumeLeft_ = compressed ? vh.lastFileCompressed : vh.lastFileExpanded;
        } else if (index_ == vh.firstFile) {
            at_ = vh.firstFileOffset;
            volumeLeft_ = compressed ? vh.firstFileCompressed : vh.firstFileExpanded;
        } else {
            err = "data" + std::to_string(volume_) + ".cab does not hold the rest of " + rec_.file.path;
            return false;
        }
        if (volumeLeft_ == 0 || volumeLeft_ > total_ - done_) volumeLeft_ = total_ - done_;
    }
    opened_ = true;
    return true;
}

int64_t StoredBytes::ReadExactly(uint8_t* buf, size_t n, std::string& err) {
    if (n == 0) return 0;
    if (done_ >= total_) return 0;
    if (n > total_ - done_) {
        err = rec_.file.path + " in the cabinet ends early";
        return -1;
    }
    size_t got = 0;
    while (got < n) {
        if (!opened_ && !OpenVolume(err)) return -1;
        if (aheadAt_ >= ahead_.size()) {
            if (volumeLeft_ == 0) {
                // on to the next volume
                if (!rec_.file.split) {
                    err = rec_.file.path + " in the cabinet ends early";
                    return -1;
                }
                ++volume_;
                opened_ = false;
                continue;
            }
            const size_t piece = size_t(std::min<uint64_t>(volumeLeft_, 256 * 1024));
            ahead_.resize(piece);
            aheadAt_ = 0;
            const int64_t r = blob_->ReadAt(at_, ahead_.data(), piece, err);
            if (r <= 0) {
                if (err.empty()) err = "data" + std::to_string(volume_) + ".cab ends before " + rec_.file.path + " does";
                return -1;
            }
            ahead_.resize(size_t(r));
            at_ += uint64_t(r);
            volumeLeft_ -= uint64_t(r);
        }
        size_t want = n - got;
        if (want > ahead_.size() - aheadAt_) want = ahead_.size() - aheadAt_;
        std::memcpy(buf + got, ahead_.data() + aheadAt_, want);
        aheadAt_ += want;
        got += want;
    }
    if (rec_.file.obfuscated) Deobfuscate(buf, n, seed_);
    done_ += n;
    return int64_t(n);
}

bool ChunkStream::NextChunk(std::string& err) {
    outAt_ = 0;
    out_.clear();
    if (produced_ >= size_) return true;
    if (!compressed_) {
        size_t want = size_t(std::min<uint64_t>(size_ - produced_, 1 << 16));
        out_.resize(want);
        if (stored_.ReadExactly(out_.data(), want, err) < 0) return false;
        return true;
    }
    uint8_t len[2];
    if (stored_.ReadExactly(len, 2, err) < 0) return false;
    const size_t n = size_t(len[0] | (len[1] << 8));
    if (n == 0) {
        err = "an empty chunk in the cabinet";
        return false;
    }
    in_.resize(n);
    if (stored_.ReadExactly(in_.data(), n, err) < 0) return false;
    // Each chunk is a raw deflate stream of its own (older cabinets wrapped it in a zlib header); the expanded size is
    // not written, so inflate until it ends.
    const uint64_t cap = size_ - produced_;
    auto inflateChunk = [&](int windowBits) -> int {
        z_stream z{};
        if (inflateInit2(&z, windowBits) != Z_OK) {
            err = "zlib failed to start";
            return -1;
        }
        z.next_in = in_.data();
        z.avail_in = uInt(n);
        out_.resize(size_t(std::min<uint64_t>(cap, 64u << 10)));
        size_t have = 0;
        for (;;) {
            z.next_out = out_.data() + have;
            z.avail_out = uInt(out_.size() - have);
            const int rc = inflate(&z, Z_NO_FLUSH);
            have = out_.size() - z.avail_out;
            if (rc == Z_STREAM_END) break;
            if (rc != Z_OK && rc != Z_BUF_ERROR) {
                inflateEnd(&z);
                err = "a damaged chunk in the cabinet";
                return rc == Z_DATA_ERROR && have == 0 ? 0 : -1;
            }
            if (z.avail_out == 0) {
                if (have >= cap) {
                    inflateEnd(&z);
                    err = "the cabinet holds more bytes than the file's size";
                    return -1;
                }
                out_.resize(size_t(std::min<uint64_t>(cap, uint64_t(out_.size()) * 2)));
                continue;
            }
            if (z.avail_in == 0) {   // the stream ended without its final block
                inflateEnd(&z);
                err = "a chunk in the cabinet ends early";
                return -1;
            }
        }
        inflateEnd(&z);
        out_.resize(have);
        return 1;
    };
    int rc = inflateChunk(-15);
    if (rc == 0) rc = inflateChunk(15);
    if (rc <= 0) {
        if (rc == 0) err = "a damaged chunk in the cabinet";
        return false;
    }
    err.clear();
    return true;
}

int64_t ChunkStream::Read(void* buf, size_t n, std::string& err) {
    if (n == 0) return 0;
    size_t done = 0;
    uint8_t* out = static_cast<uint8_t*>(buf);
    while (done < n) {
        if (outAt_ >= out_.size()) {
            if (produced_ >= size_) break;
            if (!NextChunk(err)) return -1;
            if (out_.empty()) {
                err = "the cabinet holds fewer bytes than the file's size";
                return -1;
            }
        }
        size_t take = std::min(n - done, out_.size() - outAt_);
        std::memcpy(out + done, out_.data() + outAt_, take);
        if (rec_.checked) md5_.Update(out_.data() + outAt_, take);
        outAt_ += take;
        produced_ += take;
        done += take;
    }
    if (rec_.checked && !verified_ && produced_ >= size_) {
        // The whole file has been read: it must match the checksum the installer wrote for it.
        verified_ = true;
        uint8_t got[16];
        md5_.Finish(got);
        if (std::memcmp(got, rec_.md5, 16) != 0) {
            err = rec_.file.path + " in the cabinet is damaged (its checksum does not match)";
            return -1;
        }
    }
    return int64_t(done);
}

}  // namespace

void InstallShieldMd5(const uint8_t* data, size_t n, uint8_t out[16]) {
    Md5 md5;
    md5.Update(data, n);
    md5.Finish(out);
}

bool SniffInstallShield(Blob& blob, int& majorVersion, bool& header) {
    majorVersion = 0;
    header = false;
    uint8_t raw[kCommonHeaderSize];
    std::string err;
    if (blob.ReadAt(0, raw, sizeof raw, err) != int64_t(sizeof raw)) return false;
    std::vector<uint8_t> b(raw, raw + sizeof raw);
    if (Get32(b, 0) != kSignature) return false;
    majorVersion = MajorVersion(Get32(b, 4));
    header = Get32(b, 16) != 0;
    return true;
}

std::shared_ptr<InstallShieldSource> OpenInstallShield(std::shared_ptr<Blob> header, InstallShieldVolumes volumes,
                                                       std::string& err) {
    if (!header || !volumes) {
        err = "no header";
        return nullptr;
    }
    try {
        const uint64_t size = header->Size();
        if (size < kCommonHeaderSize) {
            err = "not an InstallShield cabinet header";
            return nullptr;
        }
        if (size > kMaxHeader) {
            err = "the cabinet header is too large";
            return nullptr;
        }
        std::vector<uint8_t> h(static_cast<size_t>(size));
        uint64_t at = 0;
        while (at < size) {
            const int64_t n = header->ReadAt(at, h.data() + at, size_t(size - at), err);
            if (n <= 0) {
                if (err.empty()) err = "the cabinet header ends early";
                return nullptr;
            }
            at += uint64_t(n);
        }
        auto reader = std::make_shared<Reader>(std::move(volumes));
        reader->Parse(h);
        return reader;
    } catch (const std::exception& e) {
        err = e.what();
        return nullptr;
    }
}

}  // namespace tpw::assets
