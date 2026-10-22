// Opens a file or an entry of a Source as a nested Source by its leading bytes: zip, ISO 9660 (cooked or raw), Apple
// partition map or bare HFS, MacBinary, and a Microsoft Cabinet inside an installer .exe (found by its signature). A
// container may hold another (a zip holds a toast that holds HFS, an exe holds a cab); the depth is limited to
// kMaxNesting open containers, so a hostile archive cannot nest forever. Nothing is loaded whole.
//
// Big entries of zip and cab sources are inflated forward only, and the HFS, ISO, zip and cab readers need to read
// behind themselves, so an entry over SniffOptions::spillOver bytes that sniffs as one of those is first copied to a
// temporary file in spillDir (checked against the free space), which is deleted when the returned Source is released.
#pragma once

#include <memory>
#include <string>

#include "source.h"

namespace tpw::assets {

// How many containers may be open at once in one chain: a container opened with `depth` containers above it needs
// depth < kMaxNesting. The deepest real case is the Gold edition inside a bundle: outer zip -> inner zip -> HFS image ->
// Gold zip -> cab -> files, five containers (the cab is opened with depth 4, which the old limit of 4 refused, so the
// edition stayed "absent" without a word). 6 leaves one spare level. Nothing else depends on the number: Enumerate
// recurses once per level with a small frame, each level holds only its own open reader and blob, and a spilled image
// is a temporary file deleted when its Source is released, so the spill folder holds at most one per level.
constexpr int kMaxNesting = 6;

struct SniffOptions {
    // Where big compressed entries are spilled. Empty: never spill (a big image then reads slowly but correctly).
    std::string spillDir;
    uint64_t spillOver = 64ull << 20;
    // How much of an .exe is searched for a cabinet.
    uint64_t exeScan = 32ull << 20;
};

// What an open attempt found. NotContainer is a plain file (no signature of any container matched, or only the
// signature-less MacBinary guess did): the caller treats it as an ordinary file and says nothing. Failed is a damaged
// or unreadable container, a nesting that is too deep, a full disk and the like: the caller reports it as an error.
enum class SniffStatus { Opened, NotContainer, Failed };

enum class Container { None, Zip, Iso9660, Hfs, MacBinary, Cab };

// A name for logs: "zip", "iso9660", "hfs", "macbinary", "cab" or "none".
const char* ContainerName(Container c);

// What the leading bytes of the blob say it is (for an .exe: whether a cabinet is in it; cabOffset gets its offset).
// This only looks at signatures; opening can still fail.
Container SniffContainer(Blob& blob, const SniffOptions& options, uint64_t& cabOffset, std::string& err);

// Opens a blob as the container it looks like. `depth` is the number of containers already open above it (0 for a
// file); a container at kMaxNesting is Failed ("nested too deeply"), a plain file there is still NotContainer. Null with
// a message in err when it is none of them (status NotContainer) or fails to open (status Failed). The overloads
// without a status give the same null and message and leave the two cases unseparated.
std::shared_ptr<Source> OpenSniffed(std::shared_ptr<Blob> blob, const SniffOptions& options, int depth,
                                    SniffStatus& status, std::string& err);
std::shared_ptr<Source> OpenSniffed(std::shared_ptr<Blob> blob, const SniffOptions& options, int depth, std::string& err);

// The same for an entry of another source, which stays alive as long as the result does. Spills big images (see above).
std::shared_ptr<Source> OpenEntrySniffed(std::shared_ptr<Source> parent, const Entry& entry, const SniffOptions& options,
                                         int depth, SniffStatus& status, std::string& err);
std::shared_ptr<Source> OpenEntrySniffed(std::shared_ptr<Source> parent, const Entry& entry, const SniffOptions& options,
                                         int depth, std::string& err);

// A path the user dropped: a folder opens as a folder, a file as the container it looks like (depth 0).
std::shared_ptr<Source> OpenPathSniffed(const std::string& path, const SniffOptions& options, SniffStatus& status,
                                        std::string& err);
std::shared_ptr<Source> OpenPathSniffed(const std::string& path, const SniffOptions& options, std::string& err);

}  // namespace tpw::assets
