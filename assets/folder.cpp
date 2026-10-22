// A folder as a Source. Paths are relative and '/' separated. The resource fork of a file x comes from the side file
// x.rsrc (the extracted trees keep it there; the .rsrc file is then not listed as a file of its own) or, on macOS, from
// the real fork (x/..namedfork/rsrc) when the file has one.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>

#include "source.h"

#ifdef __APPLE__
#include <sys/xattr.h>
#endif

namespace tpw::assets {

namespace {

namespace fs = std::filesystem;

class FileStream : public Stream {
public:
    FileStream(FILE* f, uint64_t size) : f_(f), size_(size) {}
    ~FileStream() override { std::fclose(f_); }
    uint64_t Size() const override { return size_; }
    int64_t Read(void* buf, size_t n, std::string& err) override {
        if (n == 0) return 0;
        size_t got = std::fread(buf, 1, n, f_);
        if (got == 0 && std::ferror(f_)) {
            err = "read failed in a folder file";
            return -1;
        }
        return static_cast<int64_t>(got);
    }

private:
    FILE* f_;
    uint64_t size_;
};

struct Item {
    std::string disk;  // the path to open
};

class FolderSource : public Source {
public:
    const char* Kind() const override { return "folder"; }
    const std::vector<Entry>& Entries() const override { return entries_; }

    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        if (entry.id >= items_.size()) {
            err = "not an entry of this folder";
            return nullptr;
        }
        const std::string& path = items_[static_cast<size_t>(entry.id)].disk;
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            err = "cannot open " + entry.path;
            return nullptr;
        }
        try {
            return std::make_unique<FileStream>(f, entry.size);
        } catch (...) {
            std::fclose(f);
            err = "out of memory";
            return nullptr;
        }
    }

    bool Scan(const std::string& root, std::string& err) {
        std::error_code ec;
        fs::path base(root);
        if (!fs::is_directory(base, ec)) {
            err = "not a folder: " + root;
            return false;
        }
        struct Found {
            std::string disk;
            uint64_t size;
        };
        std::map<std::string, Found> files;  // relative path -> disk path and size
        fs::recursive_directory_iterator it(base, fs::directory_options::skip_permission_denied, ec), end;
        if (ec) {
            err = "cannot read " + root + ": " + ec.message();
            return false;
        }
        for (; it != end; it.increment(ec)) {
            if (ec) {
                err = "cannot read " + root + ": " + ec.message();
                return false;
            }
            std::error_code e2;
            if (!it->is_regular_file(e2) || it->is_symlink(e2)) continue;
            uint64_t size = it->file_size(e2);
            if (e2) continue;
            std::string rel = it->path().lexically_relative(base).generic_string();
            files[rel] = Found{it->path().string(), size};
        }
        for (const auto& kv : files) {
            const std::string& rel = kv.first;
            if (rel.size() > 5 && rel.compare(rel.size() - 5, 5, ".rsrc") == 0 &&
                files.count(rel.substr(0, rel.size() - 5))) {
                continue;  // listed below as the resource fork of its file
            }
            Add(rel, kv.second.size, EntryKind::Data, kv.second.disk);
#ifdef __APPLE__
            // The Finder info (type and creator are its first 8 of 32 bytes), when the file has some that is not zero.
            uint8_t finfo[32];
            if (getxattr(kv.second.disk.c_str(), "com.apple.FinderInfo", finfo, sizeof finfo, 0, XATTR_NOFOLLOW) >= 8) {
                bool any = false;
                for (int i = 0; i < 8; i++) any = any || finfo[i] != 0;
                if (any) {
                    Entry& e = entries_.back();
                    e.hasFinderInfo = true;
                    std::memcpy(e.finderType, finfo, 4);
                    std::memcpy(e.finderCreator, finfo + 4, 4);
                }
            }
#endif
            std::string side = rel + ".rsrc";
            auto s = files.find(side);
            if (s != files.end()) {
                Add(rel, s->second.size, EntryKind::Rsrc, s->second.disk);
            } else {
#ifdef __APPLE__
                ssize_t fork = getxattr(kv.second.disk.c_str(), "com.apple.ResourceFork", nullptr, 0, 0, XATTR_NOFOLLOW);
                if (fork > 0) Add(rel, static_cast<uint64_t>(fork), EntryKind::Rsrc, kv.second.disk + "/..namedfork/rsrc");
#endif
            }
        }
        return true;
    }

private:
    void Add(const std::string& rel, uint64_t size, EntryKind kind, const std::string& disk) {
        Entry e;
        e.path = rel;
        e.size = size;
        e.kind = kind;
        e.id = items_.size();
        items_.push_back(Item{disk});
        entries_.push_back(std::move(e));
    }
    std::vector<Entry> entries_;
    std::vector<Item> items_;
};

}  // namespace

std::unique_ptr<Source> OpenFolder(const std::string& path, std::string& err) {
    try {
        auto s = std::make_unique<FolderSource>();
        if (!s->Scan(path, err)) return nullptr;
        return s;
    } catch (...) {
        err = "cannot read " + path;
        return nullptr;
    }
}

}  // namespace tpw::assets
