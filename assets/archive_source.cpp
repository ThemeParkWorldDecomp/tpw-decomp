// 7-Zip and RAR archives as a Source (source.h): libarchive's 7-Zip, RAR and RAR 5 readers (third_party/libarchive).
//
// Both formats are solid as a rule (one compressed stream across many files), so an entry cannot be read without
// unpacking everything before it, and the importer reads every entry and then opens the disc images among them with
// random access. So the archive is unpacked once, in order, when it is opened: each regular file goes to a temporary file
// in the folder SniffOptions::spillDir names (assets/.staging), checked against the free space first, through one 1 MB
// buffer, never whole in memory. The folder of those files is deleted when the last thing that reads from it goes (the
// Source, a stream or a Blob of an entry), and on a failure or a cancel before the Source exists; the importer also
// empties assets/.staging every time it starts, so a crash leaves nothing behind for long.
//
// Entries are opened as Blobs of their temporary files (Source::OpenBlob), so an image inside an archive reads with
// random access and needs no second copy. Only regular files are listed: folders, links and devices are not files the
// game has; a hard link (a RAR 5 file copy) is listed with the bytes of the file it names. A password-protected archive,
// a compression method libarchive does not read (BZip2 or Zstandard in a 7-Zip archive, for one) and a damaged archive
// each fail with a message saying so; the importer puts the archive's name in front.
#include "source.h"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <system_error>

namespace tpw::assets {

namespace {

namespace fs = std::filesystem;

// The folder of an archive's unpacked files; removed with everything in it when the last holder goes.
class TempDir {
public:
    explicit TempDir(std::string path) : path_(std::move(path)) {}
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(fs::path(path_), ec);
    }
    const std::string& Path() const { return path_; }

private:
    std::string path_;
};

// A Blob of one unpacked file. The file is closed (inner_ goes first) before the folder is removed, which Windows needs.
class HeldBlob : public Blob {
public:
    HeldBlob(std::shared_ptr<TempDir> dir, std::shared_ptr<Blob> inner) : dir_(std::move(dir)), inner_(std::move(inner)) {}
    uint64_t Size() const override { return inner_->Size(); }
    int64_t ReadAt(uint64_t offset, void* buf, size_t n, std::string& err) override {
        return inner_->ReadAt(offset, buf, n, err);
    }

private:
    std::shared_ptr<TempDir> dir_;
    std::shared_ptr<Blob> inner_;
};

class BlobStream : public Stream {
public:
    explicit BlobStream(std::shared_ptr<Blob> blob) : blob_(std::move(blob)) {}
    uint64_t Size() const override { return blob_->Size(); }
    int64_t Read(void* buf, size_t n, std::string& err) override {
        int64_t got = blob_->ReadAt(pos_, buf, n, err);
        if (got > 0) pos_ += static_cast<uint64_t>(got);
        return got;
    }

private:
    std::shared_ptr<Blob> blob_;
    uint64_t pos_ = 0;
};

class ArchiveSource : public Source {
public:
    ArchiveSource(const char* kind, std::shared_ptr<TempDir> dir) : kind_(kind), dir_(std::move(dir)) {}
    const char* Kind() const override { return kind_; }
    const std::vector<Entry>& Entries() const override { return entries_; }
    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        std::shared_ptr<Blob> blob = OpenBlob(entry, err);
        if (!blob) return nullptr;
        try {
            return std::unique_ptr<Stream>(new BlobStream(std::move(blob)));
        } catch (...) {
            err = "out of memory";
            return nullptr;
        }
    }
    std::shared_ptr<Blob> OpenBlob(const Entry& entry, std::string& err) override {
        if (entry.id >= files_.size()) {
            err = "no such entry in the archive";
            return nullptr;
        }
        std::shared_ptr<Blob> file = OpenFileBlob(files_[entry.id], err);
        if (!file) return nullptr;
        try {
            return std::make_shared<HeldBlob>(dir_, std::move(file));
        } catch (...) {
            err = "out of memory";
            return nullptr;
        }
    }

    const char* kind_;
    std::shared_ptr<TempDir> dir_;
    std::vector<std::string> files_;  // the unpacked file of each entry id
    std::vector<Entry> entries_;
};

// ---- libarchive's client callbacks over Blobs, one per volume of a multi-volume archive

struct Volume {
    std::shared_ptr<Blob> blob;
    uint64_t pos = 0;
    std::vector<uint8_t> buf;
};

la_ssize_t ReadVolume(struct archive* a, void* data, const void** buffer) {
    Volume* v = static_cast<Volume*>(data);
    try {
        if (v->buf.empty()) v->buf.resize(1 << 20);
    } catch (...) {
        archive_set_error(a, ENOMEM, "out of memory");
        return -1;
    }
    std::string err;
    int64_t got = v->blob->ReadAt(v->pos, v->buf.data(), v->buf.size(), err);
    if (got < 0) {
        archive_set_error(a, EIO, "%s", err.c_str());
        return -1;
    }
    v->pos += static_cast<uint64_t>(got);
    *buffer = v->buf.data();
    return static_cast<la_ssize_t>(got);
}

la_int64_t SeekVolume(struct archive*, void* data, la_int64_t offset, int whence) {
    Volume* v = static_cast<Volume*>(data);
    int64_t size = static_cast<int64_t>(v->blob->Size());
    int64_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? static_cast<int64_t>(v->pos) : size;
    int64_t at = base + offset;
    if (at < 0) return ARCHIVE_FATAL;
    if (at > size) at = size;
    v->pos = static_cast<uint64_t>(at);
    return at;
}

la_int64_t SkipVolume(struct archive*, void* data, la_int64_t request) {
    Volume* v = static_cast<Volume*>(data);
    uint64_t left = v->blob->Size() - std::min(v->pos, v->blob->Size());
    uint64_t n = request <= 0 ? 0 : std::min<uint64_t>(static_cast<uint64_t>(request), left);
    v->pos += n;
    return static_cast<la_int64_t>(n);
}

// The next volume is read from its start.
int SwitchVolume(struct archive*, void*, void* next) {
    if (next) static_cast<Volume*>(next)->pos = 0;
    return ARCHIVE_OK;
}

struct ArchiveHandle {
    struct archive* a = nullptr;
    ~ArchiveHandle() {
        if (a) archive_read_free(a);
    }
};

bool Contains(const std::string& text, const char* word) {
    std::string lower = text;
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return lower.find(word) != std::string::npos;
}

// The player's message for libarchive's: what is wrong and what to do instead.
std::string Explain(const std::string& what, bool encrypted) {
    std::string detail = what.empty() ? std::string() : " (" + what + ")";
    if (encrypted || Contains(what, "encrypt") || Contains(what, "passphrase") || Contains(what, "password"))
        return "this archive is protected by a password, which this program cannot open" + detail +
               ". Unpack it with its password and drop the files instead.";
    if (Contains(what, "unsupported") || Contains(what, "not supported") || Contains(what, "no support"))
        return "this archive uses a compression method this program cannot read" + detail +
               ". Unpack it with 7-Zip or WinRAR and drop the files instead.";
    return "this archive is damaged or incomplete" + detail +
           ". Download it again, or unpack it and drop the files instead.";
}

std::string LibError(struct archive* a) {
    const char* s = archive_error_string(a);
    return s ? s : "";
}

// An archive's path as an entry path: '/' separated, no leading slash, no empty, "." or ".." parts.
std::string EntryPath(const char* raw) {
    std::string in = raw ? raw : "", out, part;
    std::replace(in.begin(), in.end(), '\\', '/');
    size_t at = 0;
    while (at <= in.size()) {
        size_t slash = in.find('/', at);
        if (slash == std::string::npos) slash = in.size();
        part = in.substr(at, slash - at);
        if (!part.empty() && part != "." && part != "..") out += (out.empty() ? "" : "/") + part;
        at = slash + 1;
    }
    return out;
}

const char* EntryName(struct archive_entry* e) {
    const char* s = archive_entry_pathname_utf8(e);
    return s ? s : archive_entry_pathname(e);
}

bool Cancelled(const ArchiveOptions& options) { return options.cancel && options.cancel->load(); }

}  // namespace

std::unique_ptr<Source> OpenArchive(const std::vector<std::shared_ptr<Blob>>& volumes, const ArchiveOptions& options,
                                    std::string& err) {
    if (volumes.empty() || !volumes.front()) {
        err = "no archive";
        return nullptr;
    }
    if (options.tempDir.empty()) {
        err = "there is no folder to unpack the archive into";
        return nullptr;
    }
    try {
        static std::atomic<unsigned> counter{0};
        std::string dirPath = options.tempDir + "/tpw-archive-" +
                              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                              std::to_string(counter++);
        std::error_code ec;
        if (!fs::create_directories(fs::path(dirPath), ec) || ec) {
            err = "cannot create " + dirPath + (ec ? ": " + ec.message() : std::string());
            return nullptr;
        }
        auto dir = std::make_shared<TempDir>(dirPath);

        std::vector<Volume> vols(volumes.size());
        for (size_t i = 0; i < volumes.size(); ++i) vols[i].blob = volumes[i];
        ArchiveHandle h;
        h.a = archive_read_new();
        if (!h.a) {
            err = "out of memory";
            return nullptr;
        }
        // The three readers and nothing else (never archive_read_support_format_all): a zip, a tar or an ISO is one of
        // this program's own readers' or not a game file at all.
        archive_read_support_format_7zip(h.a);
        archive_read_support_format_rar(h.a);
        archive_read_support_format_rar5(h.a);
        archive_read_set_read_callback(h.a, ReadVolume);
        archive_read_set_seek_callback(h.a, SeekVolume);
        archive_read_set_skip_callback(h.a, SkipVolume);
        archive_read_set_switch_callback(h.a, SwitchVolume);
        archive_read_set_callback_data(h.a, &vols[0]);
        for (size_t i = 1; i < vols.size(); ++i) archive_read_append_callback_data(h.a, &vols[i]);
        if (archive_read_open1(h.a) != ARCHIVE_OK) {
            err = Explain(LibError(h.a), false);
            return nullptr;
        }

        std::unique_ptr<ArchiveSource> source;
        std::map<std::string, uint64_t> byPath;  // entry path -> file id; a later entry of the same path wins
        std::vector<uint8_t> buf(1 << 20);
        for (;;) {
            if (Cancelled(options)) {
                err = "cancelled";
                return nullptr;
            }
            struct archive_entry* ae = nullptr;
            int r = archive_read_next_header(h.a, &ae);
            if (r == ARCHIVE_EOF) break;
            if (r < ARCHIVE_WARN) {
                err = Explain(LibError(h.a), archive_read_has_encrypted_entries(h.a) > 0);
                return nullptr;
            }
            if (!source) {
                int format = archive_format(h.a) & ARCHIVE_FORMAT_BASE_MASK;
                source.reset(new ArchiveSource(format == ARCHIVE_FORMAT_7ZIP ? "7z" : "rar", dir));
            }
            const std::string path = EntryPath(EntryName(ae));
            const char* linkRaw = archive_entry_hardlink_utf8(ae);
            if (!linkRaw) linkRaw = archive_entry_hardlink(ae);
            if (linkRaw) {
                auto target = byPath.find(EntryPath(linkRaw));
                if (!path.empty() && target != byPath.end()) byPath[path] = target->second;
                continue;
            }
            if (archive_entry_filetype(ae) != AE_IFREG || path.empty()) continue;
            if (archive_entry_is_encrypted(ae)) {
                err = Explain("", true);
                return nullptr;
            }
            const uint64_t declared = archive_entry_size_is_set(ae) && archive_entry_size(ae) > 0
                                          ? static_cast<uint64_t>(archive_entry_size(ae))
                                          : 0;
            const uint64_t margin = 64ull << 20;
            fs::space_info space = fs::space(fs::path(dirPath), ec);
            if (ec) {
                err = "cannot check the free space in " + options.tempDir;
                return nullptr;
            }
            if (space.available < declared + margin) {
                err = "not enough free space in " + options.tempDir + ": " + std::to_string((declared + margin) >> 20) +
                      " MB are needed to unpack " + path;
                return nullptr;
            }
            const uint64_t id = source->files_.size();
            const std::string file = dirPath + "/" + std::to_string(id) + ".tmp";
            FILE* f = std::fopen(file.c_str(), "wbx");
            if (!f) {
                err = "cannot create " + file;
                return nullptr;
            }
            source->files_.push_back(file);
            if (options.progress) options.progress("unpacking " + path);
            uint64_t written = 0, reported = 0;
            for (;;) {
                if (Cancelled(options)) {
                    std::fclose(f);
                    err = "cancelled";
                    return nullptr;
                }
                la_ssize_t got = archive_read_data(h.a, buf.data(), buf.size());
                if (got == 0) break;
                if (got < 0) {
                    std::fclose(f);
                    err = Explain(LibError(h.a), archive_entry_is_encrypted(ae) != 0);
                    return nullptr;
                }
                if (std::fwrite(buf.data(), 1, static_cast<size_t>(got), f) != static_cast<size_t>(got)) {
                    std::fclose(f);
                    err = "cannot write " + file + " (disk full?)";
                    return nullptr;
                }
                written += static_cast<uint64_t>(got);
                if (options.progress && written - reported >= (8ull << 20)) {
                    reported = written;
                    std::string of = declared ? " of " + std::to_string(declared >> 20) : std::string();
                    options.progress("unpacking " + path + " (" + std::to_string(written >> 20) + of + " MB)");
                }
            }
            if (std::fclose(f) != 0) {
                err = "cannot write " + file + " (disk full?)";
                return nullptr;
            }
            byPath[path] = id;
        }
        if (!source) source.reset(new ArchiveSource("7z", dir));   // an empty archive: no entries, nothing wrong
        for (const auto& [path, id] : byPath) {
            Entry e;
            e.path = path;
            e.id = id;
            e.size = fs::file_size(fs::path(source->files_[id]), ec);
            if (ec) {
                err = "cannot read back " + source->files_[id];
                return nullptr;
            }
            source->entries_.push_back(std::move(e));
        }
        return source;
    } catch (...) {
        err = "out of memory";
        return nullptr;
    }
}

}  // namespace tpw::assets
