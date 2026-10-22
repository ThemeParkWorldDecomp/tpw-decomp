#include "sniff.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace tpw::assets {

namespace {

// Sector 16 of a raw 2352-byte image is at 37632 and its "CD001" at +16+1 (MODE1) or +24+1 (MODE2 form 1).
const size_t kHead = 16 * 2352 + 32;

// A temporary copy of an entry; the file goes away with the Blob.
class SpillBlob : public Blob {
public:
    SpillBlob(std::shared_ptr<Blob> inner, std::string path) : inner_(std::move(inner)), path_(std::move(path)) {}
    ~SpillBlob() override {
        inner_.reset();
        std::remove(path_.c_str());
    }
    uint64_t Size() const override { return inner_->Size(); }
    int64_t ReadAt(uint64_t offset, void* buf, size_t n, std::string& err) override {
        return inner_->ReadAt(offset, buf, n, err);
    }

private:
    std::shared_ptr<Blob> inner_;
    std::string path_;
};

std::shared_ptr<Blob> Spill(Source& parent, const Entry& entry, const std::string& dir, std::string& err) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::space_info space = fs::space(dir, ec);
    if (ec) {
        err = "cannot check the free space in " + dir;
        return nullptr;
    }
    const uint64_t margin = 64ull << 20;
    if (space.available < entry.size + margin) {
        err = "not enough free space in " + dir + ": " + std::to_string((entry.size + margin) >> 20) +
              " MB are needed to unpack " + entry.path;
        return nullptr;
    }
    auto in = parent.Open(entry, err);
    if (!in) return nullptr;
    static std::atomic<unsigned> counter{0};
    std::string path = dir + "/tpw-spill-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                       std::to_string(counter++) + ".tmp";
    FILE* f = std::fopen(path.c_str(), "wbx");
    if (!f) {
        err = "cannot create " + path;
        return nullptr;
    }
    try {
        std::vector<uint8_t> buf(1 << 20);
        for (;;) {
            int64_t got = in->Read(buf.data(), buf.size(), err);
            if (got < 0) break;
            if (got == 0) {
                if (std::fclose(f) != 0) {
                    err = "cannot write " + path;
                    std::remove(path.c_str());
                    return nullptr;
                }
                auto blob = OpenFileBlob(path, err);
                if (!blob) {
                    std::remove(path.c_str());
                    return nullptr;
                }
                return std::make_shared<SpillBlob>(std::move(blob), path);
            }
            if (std::fwrite(buf.data(), 1, static_cast<size_t>(got), f) != static_cast<size_t>(got)) {
                err = "cannot write " + path + " (disk full?)";
                break;
            }
        }
    } catch (...) {
        err = "out of memory";
    }
    std::fclose(f);
    std::remove(path.c_str());
    return nullptr;
}

bool Has(const uint8_t* head, size_t n, size_t at, const char* s) {
    size_t len = std::strlen(s);
    return at + len <= n && std::memcmp(head + at, s, len) == 0;
}

bool RawSync(const uint8_t* head, size_t n) {
    static const uint8_t sync[12] = {0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0};
    return n >= 12 && std::memcmp(head, sync, 12) == 0;
}

// The candidates in the order they are tried.
std::vector<Container> Classify(Blob& blob, const SniffOptions& options, uint64_t& cabOffset, std::string& err) {
    std::vector<Container> out;
    std::vector<uint8_t> head(kHead);
    int64_t got = blob.ReadAt(0, head.data(), head.size(), err);
    if (got < 0) return out;
    size_t n = static_cast<size_t>(got);
    if (Has(head.data(), n, 0, "PK\x03\x04") || Has(head.data(), n, 0, "PK\x05\x06")) out.push_back(Container::Zip);
    if (Has(head.data(), n, 0, "MZ")) {
        int64_t at = FindCabinet(blob, options.exeScan, err);
        if (at >= 0) {
            cabOffset = static_cast<uint64_t>(at);
            out.push_back(Container::Cab);
        }
    } else if (Has(head.data(), n, 0, "MSCF")) {
        cabOffset = 0;
        out.push_back(Container::Cab);
    }
    bool hfs = Has(head.data(), n, 0, "ER") || Has(head.data(), n, 1024, "BD");
    bool iso = Has(head.data(), n, 16 * 2048 + 1, "CD001") ||
               (RawSync(head.data(), n) && (Has(head.data(), n, 16 * 2352 + 17, "CD001") ||
                                            Has(head.data(), n, 16 * 2352 + 25, "CD001")));
    if (hfs) out.push_back(Container::Hfs);
    if (iso) out.push_back(Container::Iso9660);
    // MacBinary has no signature; its header is validated when it is opened, and it is the last resort.
    if (out.empty() && n >= 128 && head[0] == 0 && head[74] == 0 && head[82] == 0) out.push_back(Container::MacBinary);
    return out;
}

std::shared_ptr<Source> OpenCandidates(const std::shared_ptr<Blob>& blob, const std::vector<Container>& cands,
                                       uint64_t cabOffset, std::string& err) {
    std::string last;
    for (Container c : cands) {
        std::string e;
        std::unique_ptr<Source> s;
        switch (c) {
            case Container::Zip: s = OpenZip(blob, e); break;
            case Container::Iso9660: s = OpenIso9660(blob, e); break;
            case Container::Hfs: s = OpenHfs(blob, e); break;
            case Container::MacBinary: s = OpenMacBinary(blob, e); break;
            case Container::Cab: s = OpenCab(blob, cabOffset, e); break;
            case Container::None: break;
        }
        if (s) return std::shared_ptr<Source>(std::move(s));
        last = e;
    }
    err = cands.empty() ? "not a folder, archive, disc image or installer this program can read" : last;
    return nullptr;
}

}  // namespace

const char* ContainerName(Container c) {
    switch (c) {
        case Container::Zip: return "zip";
        case Container::Iso9660: return "iso9660";
        case Container::Hfs: return "hfs";
        case Container::MacBinary: return "macbinary";
        case Container::Cab: return "cab";
        case Container::None: break;
    }
    return "none";
}

Container SniffContainer(Blob& blob, const SniffOptions& options, uint64_t& cabOffset, std::string& err) {
    try {
        std::vector<Container> c = Classify(blob, options, cabOffset, err);
        return c.empty() ? Container::None : c.front();
    } catch (...) {
        err = "out of memory";
        return Container::None;
    }
}

std::shared_ptr<Source> OpenSniffed(std::shared_ptr<Blob> blob, const SniffOptions& options, int depth, std::string& err) {
    if (!blob) {
        err = "no blob";
        return nullptr;
    }
    if (depth >= kMaxNesting) {
        err = "containers are nested too deeply";
        return nullptr;
    }
    try {
        uint64_t cabOffset = 0;
        std::string readErr;
        std::vector<Container> cands = Classify(*blob, options, cabOffset, readErr);
        if (!readErr.empty()) {
            err = readErr;
            return nullptr;
        }
        return OpenCandidates(blob, cands, cabOffset, err);
    } catch (...) {
        err = "out of memory";
        return nullptr;
    }
}

std::shared_ptr<Source> OpenEntrySniffed(std::shared_ptr<Source> parent, const Entry& entry, const SniffOptions& options,
                                         int depth, std::string& err) {
    if (!parent) {
        err = "no source";
        return nullptr;
    }
    if (depth >= kMaxNesting) {
        err = "containers are nested too deeply";
        return nullptr;
    }
    try {
        std::shared_ptr<Blob> blob = OpenEntryBlob(parent, entry, err);
        if (!blob) return nullptr;
        uint64_t cabOffset = 0;
        std::string readErr;
        std::vector<Container> cands = Classify(*blob, options, cabOffset, readErr);
        if (!readErr.empty()) {
            err = readErr;
            return nullptr;
        }
        bool needsRandomAccess = false;
        for (Container c : cands) needsRandomAccess = needsRandomAccess || c != Container::MacBinary;
        bool forwardOnly = std::strcmp(parent->Kind(), "zip") == 0 || std::strcmp(parent->Kind(), "cab") == 0;
        if (needsRandomAccess && forwardOnly && !options.spillDir.empty() && entry.size > options.spillOver) {
            blob = Spill(*parent, entry, options.spillDir, err);
            if (!blob) return nullptr;
        }
        return OpenCandidates(blob, cands, cabOffset, err);
    } catch (...) {
        err = "out of memory";
        return nullptr;
    }
}

std::shared_ptr<Source> OpenPathSniffed(const std::string& path, const SniffOptions& options, std::string& err) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::unique_ptr<Source> s = OpenFolder(path, err);
        return std::shared_ptr<Source>(std::move(s));
    }
    auto blob = OpenFileBlob(path, err);
    if (!blob) return nullptr;
    return OpenSniffed(std::move(blob), options, 0, err);
}

}  // namespace tpw::assets
