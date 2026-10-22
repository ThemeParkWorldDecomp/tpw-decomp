#include "source.h"

#include <cstdio>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#define TPW_FSEEK _fseeki64
#define TPW_FTELL _ftelli64
#else
#define TPW_FSEEK fseeko
#define TPW_FTELL ftello
#endif

namespace tpw::assets {

namespace {

// ---- A file as a Blob. One FILE with a lock: the readers above read sequentially in large chunks.
class FileBlob : public Blob {
public:
    FileBlob(FILE* f, uint64_t size) : f_(f), size_(size) {}
    ~FileBlob() override { std::fclose(f_); }
    uint64_t Size() const override { return size_; }
    int64_t ReadAt(uint64_t offset, void* buf, size_t n, std::string& err) override {
        if (offset >= size_ || n == 0) return 0;
        if (n > size_ - offset) n = size_ - offset;
        std::lock_guard<std::mutex> lock(mutex_);
        if (TPW_FSEEK(f_, static_cast<long long>(offset), SEEK_SET) != 0) {
            err = "seek failed in the file";
            return -1;
        }
        size_t got = std::fread(buf, 1, n, f_);
        if (got < n && std::ferror(f_)) {
            err = "read failed in the file";
            return -1;
        }
        return static_cast<int64_t>(got);
    }

private:
    FILE* f_;
    uint64_t size_;
    std::mutex mutex_;
};

class MemoryBlob : public Blob {
public:
    explicit MemoryBlob(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {}
    uint64_t Size() const override { return bytes_.size(); }
    int64_t ReadAt(uint64_t offset, void* buf, size_t n, std::string&) override {
        if (offset >= bytes_.size()) return 0;
        if (n > bytes_.size() - offset) n = bytes_.size() - offset;
        std::memcpy(buf, bytes_.data() + offset, n);
        return static_cast<int64_t>(n);
    }

private:
    std::vector<uint8_t> bytes_;
};

// An entry of another source as a Blob: reads forward through one open stream; a read behind it reopens the entry.
class EntryBlob : public Blob {
public:
    EntryBlob(std::shared_ptr<Source> source, const Entry& entry) : source_(std::move(source)), entry_(entry) {}
    uint64_t Size() const override { return entry_.size; }
    int64_t ReadAt(uint64_t offset, void* buf, size_t n, std::string& err) override {
        if (offset >= entry_.size || n == 0) return 0;
        if (n > entry_.size - offset) n = entry_.size - offset;
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stream_ || offset < pos_) {
            stream_ = source_->Open(entry_, err);
            pos_ = 0;
            if (!stream_) return -1;
        }
        if (offset > pos_) {
            if (scratch_.empty()) scratch_.resize(1 << 16);
            while (pos_ < offset) {
                size_t want = scratch_.size();
                if (want > offset - pos_) want = static_cast<size_t>(offset - pos_);
                int64_t got = stream_->Read(scratch_.data(), want, err);
                if (got <= 0) return Fail(got, err);
                pos_ += got;
            }
        }
        size_t done = 0;
        while (done < n) {
            int64_t got = stream_->Read(static_cast<uint8_t*>(buf) + done, n - done, err);
            if (got < 0) return Fail(got, err);
            if (got == 0) break;
            done += static_cast<size_t>(got);
            pos_ += got;
        }
        return static_cast<int64_t>(done);
    }

private:
    int64_t Fail(int64_t got, std::string& err) {
        stream_.reset();
        if (got == 0) err = "the entry ended early";
        return -1;
    }
    std::shared_ptr<Source> source_;
    Entry entry_;
    std::mutex mutex_;
    std::unique_ptr<Stream> stream_;
    uint64_t pos_ = 0;
    std::vector<uint8_t> scratch_;
};

}  // namespace

std::shared_ptr<Blob> OpenFileBlob(const std::string& path, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        err = "cannot open " + path;
        return nullptr;
    }
    if (TPW_FSEEK(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        err = "cannot size " + path;
        return nullptr;
    }
    long long size = static_cast<long long>(TPW_FTELL(f));
    if (size < 0) {
        std::fclose(f);
        err = "cannot size " + path;
        return nullptr;
    }
    try {
        return std::make_shared<FileBlob>(f, static_cast<uint64_t>(size));
    } catch (...) {
        std::fclose(f);
        err = "out of memory";
        return nullptr;
    }
}

std::shared_ptr<Blob> MakeMemoryBlob(std::vector<uint8_t> bytes) {
    try {
        return std::make_shared<MemoryBlob>(std::move(bytes));
    } catch (...) {
        return nullptr;
    }
}

std::shared_ptr<Blob> OpenEntryBlob(std::shared_ptr<Source> source, const Entry& entry, std::string& err) {
    if (!source) {
        err = "no source";
        return nullptr;
    }
    try {
        return std::make_shared<EntryBlob>(std::move(source), entry);
    } catch (...) {
        err = "out of memory";
        return nullptr;
    }
}

bool HashStream(Stream& stream, Sha256Digest& out, std::string& err) {
    try {
        std::vector<uint8_t> buf(1 << 20);
        Sha256 sha;
        for (;;) {
            int64_t got = stream.Read(buf.data(), buf.size(), err);
            if (got < 0) return false;
            if (got == 0) break;
            sha.Update(buf.data(), static_cast<size_t>(got));
        }
        out = sha.Final();
        return true;
    } catch (...) {
        err = "out of memory";
        return false;
    }
}

bool ReadAll(Stream& stream, uint64_t maxBytes, std::vector<uint8_t>& out, std::string& err) {
    try {
        out.clear();
        if (stream.Size() > maxBytes) {
            err = "the entry is larger than the limit";
            return false;
        }
        out.resize(static_cast<size_t>(stream.Size()));
        size_t done = 0;
        while (done < out.size()) {
            int64_t got = stream.Read(out.data() + done, out.size() - done, err);
            if (got < 0) return false;
            if (got == 0) {
                err = "the entry ended early";
                return false;
            }
            done += static_cast<size_t>(got);
        }
        return true;
    } catch (...) {
        err = "out of memory";
        return false;
    }
}

}  // namespace tpw::assets
