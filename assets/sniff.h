// Opens a file or an entry of a Source as a nested Source by its leading bytes: zip, ISO 9660 (cooked or raw), Apple
// partition map or bare HFS, MacBinary, and a Microsoft Cabinet inside an installer .exe (found by its signature). A
// container may hold another (a zip holds a toast that holds HFS, an exe holds a cab); the depth is limited to 4 open
// containers, so a hostile archive cannot nest forever. Nothing is loaded whole.
//
// Big entries of zip and cab sources are inflated forward only, and the HFS, ISO, zip and cab readers need to read
// behind themselves, so an entry over SniffOptions::spillOver bytes that sniffs as one of those is first copied to a
// temporary file in spillDir (checked against the free space), which is deleted when the returned Source is released.
#pragma once

#include <memory>
#include <string>

#include "source.h"

namespace tpw::assets {

constexpr int kMaxNesting = 4;

struct SniffOptions {
    // Where big compressed entries are spilled. Empty: never spill (a big image then reads slowly but correctly).
    std::string spillDir;
    uint64_t spillOver = 64ull << 20;
    // How much of an .exe is searched for a cabinet.
    uint64_t exeScan = 32ull << 20;
};

enum class Container { None, Zip, Iso9660, Hfs, MacBinary, Cab };

// A name for logs: "zip", "iso9660", "hfs", "macbinary", "cab" or "none".
const char* ContainerName(Container c);

// What the leading bytes of the blob say it is (for an .exe: whether a cabinet is in it; cabOffset gets its offset).
// This only looks at signatures; opening can still fail.
Container SniffContainer(Blob& blob, const SniffOptions& options, uint64_t& cabOffset, std::string& err);

// Opens a blob as the container it looks like. `depth` is the number of containers already open above it (0 for a
// file); at kMaxNesting the call fails. Null with a message in err when it is none of them or fails to open.
std::shared_ptr<Source> OpenSniffed(std::shared_ptr<Blob> blob, const SniffOptions& options, int depth, std::string& err);

// The same for an entry of another source, which stays alive as long as the result does. Spills big images (see above).
std::shared_ptr<Source> OpenEntrySniffed(std::shared_ptr<Source> parent, const Entry& entry, const SniffOptions& options,
                                         int depth, std::string& err);

// A path the user dropped: a folder opens as a folder, a file as the container it looks like (depth 0).
std::shared_ptr<Source> OpenPathSniffed(const std::string& path, const SniffOptions& options, std::string& err);

}  // namespace tpw::assets
