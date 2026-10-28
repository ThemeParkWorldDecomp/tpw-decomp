// The asset launcher's container readers share one interface: a Source lists entries (path, size, kind) and opens an
// entry as a stream of bytes. Folders, zip archives and ISO 9660 images implement it (folder.cpp, zip.cpp,
// iso9660.cpp). Nothing here throws across the interface: failures come back as a null result or a negative count with a
// message in the err argument. Nothing loads a whole image; every reader streams.
//
// Nesting: a Source reads from a Blob (random access bytes). A file is a Blob (OpenFileBlob), and so is an entry of
// another Source (OpenEntryBlob), so a zip can hold an image: OpenIso9660(OpenEntryBlob(zip, entry)). Reads of an entry
// that is deflated go forward only, so a read behind the last one restarts the entry; images inside a zip are read
// mostly in order and the small ones (a 7 MB disc) cost nothing, while a 600 MB image inside a zip needs the caller to
// spill it to a file first (the nested-source sniffer, a later unit).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "sha256.h"

namespace tpw::assets {

enum class EntryKind {
    Data,  // the file's bytes (the data fork of a Macintosh file)
    Rsrc,  // the resource fork of the file at the same path
};

struct Entry {
    std::string path;  // relative, '/' separated, UTF-8, no leading slash; ISO ";1" versions are removed
    uint64_t size = 0;
    EntryKind kind = EntryKind::Data;
    uint64_t id = 0;  // private to the Source that listed it
    // The Macintosh Finder type and creator of a Data entry (HFS, MacBinary, and on macOS a folder file that has them),
    // which the importer writes as a ".finfo" side file; hasFinderInfo is false when the source does not know them.
    bool hasFinderInfo = false;
    char finderType[4] = {};
    char finderCreator[4] = {};
};

// Random access to immutable bytes. Safe to call from several threads.
class Blob {
public:
    virtual ~Blob() {}
    virtual uint64_t Size() const = 0;
    // Reads up to n bytes at offset. Returns the count (short only at the end), or -1 with a message in err.
    virtual int64_t ReadAt(uint64_t offset, void* buf, size_t n, std::string& err) = 0;
};

// Forward bytes of one entry.
class Stream {
public:
    virtual ~Stream() {}
    virtual uint64_t Size() const = 0;
    // Returns the count read (at least 1 unless n is 0), 0 at the end, or -1 with a message in err. A stream that has
    // a checksum (zip) reports a mismatch as -1 at the end.
    virtual int64_t Read(void* buf, size_t n, std::string& err) = 0;
};

class Source {
public:
    virtual ~Source() {}
    // "folder", "zip" or "iso9660".
    virtual const char* Kind() const = 0;
    // Every file of the source, sorted by path (a path with both kinds lists Data first).
    virtual const std::vector<Entry>& Entries() const = 0;
    // Null with a message in err when the entry cannot be read (unsupported compression, missing file, ...).
    virtual std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) = 0;
};

// ---- Blobs (source.cpp)
std::shared_ptr<Blob> OpenFileBlob(const std::string& path, std::string& err);
// The bytes are copied. For small inputs and tests.
std::shared_ptr<Blob> MakeMemoryBlob(std::vector<uint8_t> bytes);
// The entry of a source as a Blob. The source must stay alive as long as the Blob does (the Blob holds a reference).
std::shared_ptr<Blob> OpenEntryBlob(std::shared_ptr<Source> source, const Entry& entry, std::string& err);

// ---- Sources
// A folder; resource forks are the x.rsrc side files of x (listed as the Rsrc entry of x) or, on macOS, the real fork.
std::unique_ptr<Source> OpenFolder(const std::string& path, std::string& err);
// A zip archive (stored and deflate entries, zip64, data descriptors).
std::unique_ptr<Source> OpenZip(std::shared_ptr<Blob> blob, std::string& err);
// An ISO 9660 image (Joliet names when there is a Joliet volume): cooked 2048-byte sectors or raw 2352-byte sectors,
// MODE1 or MODE2 form 1, also with 96 sub-channel bytes after each raw sector (2448 bytes, as some .mdf files have).
std::unique_ptr<Source> OpenIso9660(std::shared_ptr<Blob> blob, std::string& err);
// A classic Macintosh HFS volume (apm_hfs.cpp): the first Apple_HFS partition of an Apple partition map (a .toast or
// .iso Mac disc image), or a bare HFS volume. Kind() is "hfs". Both forks are listed: the data fork as a Data entry
// (also for an empty one) and the resource fork as an Rsrc entry of the same path when it is not empty. Names are
// converted from Mac Roman to UTF-8 and a '/' in a name becomes ':'. The volume is random access: reading from a
// zip-entry Blob works but restarts the entry on every backward read, so spill a big image to a file first (sniff.h).
std::unique_ptr<Source> OpenHfs(std::shared_ptr<Blob> blob, std::string& err);
// A MacBinary I, II or III file (macbinary.cpp): one file with a data fork and a resource fork. Kind() is "macbinary".
std::unique_ptr<Source> OpenMacBinary(std::shared_ptr<Blob> blob, std::string& err);
// A Microsoft Cabinet (cab.cpp) that starts at byte `offset` of the blob (0 for a bare .cab; FindCabinet gives the
// offset in an installer .exe). Folders compressed with MSZIP or stored are readable. Kind() is "cab".
std::unique_ptr<Source> OpenCab(std::shared_ptr<Blob> blob, uint64_t offset, std::string& err);
// The offset of the first plausible cabinet header ("MSCF" with consistent sizes) in the first maxScan bytes of a blob,
// or -1 when there is none. Never a fixed offset: installers move it.
int64_t FindCabinet(Blob& blob, uint64_t maxScan, std::string& err);

// ---- Helpers (source.cpp)
// Hashes the rest of a stream. False with a message in err on a read error.
bool HashStream(Stream& stream, Sha256Digest& out, std::string& err);
// Reads the rest of a stream into memory, failing past maxBytes. For small entries.
bool ReadAll(Stream& stream, uint64_t maxBytes, std::vector<uint8_t>& out, std::string& err);

}  // namespace tpw::assets
