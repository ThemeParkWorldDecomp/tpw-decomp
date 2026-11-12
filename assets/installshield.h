// An InstallShield cabinet set (the data1.hdr, data1.cab, data2.cab ... of a Windows installer of about 1999 to 2002),
// read as a Source. The header file holds the file table (directories, file descriptors, file groups, components) and
// each volume holds the bytes: a file is stored whole or deflated in chunks (a 16-bit length then a raw deflate stream
// for each), may be byte-obfuscated, and may run from the end of one volume into the next. Written from the format;
// the open-source tool unshield was the reference the output was compared with.
//
// Theme Park Inc's disc keeps the game's data in the file group "Data" of such a set, which installs as the Data folder
// (tpi_disc.h). Entries are listed as "<group>/<directory>/<name>" with '/' separators, so the Data group's files come
// out as "Data/levels/arabian/Standard.sam" and so on, the installed form.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "source.h"

namespace tpw::assets {

// Gives the volume data<n>.cab (n from 1) as a Blob, or null with a message (a missing volume: "data2.cab is missing").
// Called once per volume, when a file needs it.
using InstallShieldVolumes = std::function<std::shared_ptr<Blob>(int volume, std::string& err)>;

struct InstallShieldFile {
    std::string group;      // the file group ("Data", "Program DLLs", "<Support>English Files", ...)
    std::string path;       // "<directory>/<name>" with '/' separators, "<name>" for the root directory
    uint64_t size = 0;      // expanded
    int volume = 0;         // the volume its bytes start in (1-based)
    bool compressed = false;
    bool obfuscated = false;
    bool split = false;     // continues into the next volume(s)
    bool valid = true;      // false: a placeholder the installer never writes (not listed as an Entry)
};

class InstallShieldSource : public Source {
public:
    // 5, 6, 7 ... (the "major version" of the InstallShield release that built the set). 6 is the discs' layout; 5 is
    // the Fargus Russian release's (32-bit tables, no checksums, the volume headers say which volume holds which file).
    virtual int MajorVersion() const = 0;
    virtual const std::vector<std::string>& Groups() const = 0;
    // Every file of the table in table order; Entry::id is the index into this list.
    virtual const std::vector<InstallShieldFile>& Files() const = 0;
    // The volumes the group's files occupy, sorted, no repeats, found as reading finds them: from the table (version
    // 6) or by walking the volume headers (version 5), and on into the next volumes for a split file. Every one of them
    // is opened. False with a message when a volume is missing or is not a cabinet volume of this set.
    virtual bool VolumesOf(const std::string& group, std::vector<int>& volumes, std::string& err) = 0;
};

// True when the blob starts with the cabinet signature; majorVersion gets the version the header names (0 when the
// scheme is unknown) and header says whether the file carries the file table (data1.hdr, or an old data1.cab that
// holds it itself) rather than only volume bytes.
bool SniffInstallShield(Blob& blob, int& majorVersion, bool& header);

// Opens the set from its header (data1.hdr, or data1.cab when the table is inside it: SniffInstallShield says). The
// header is read whole (it is small); the volumes are read as needed through `volumes`, never whole. Null with a
// message when it is no InstallShield header, a version this reader does not know, or the tables are damaged.
std::shared_ptr<InstallShieldSource> OpenInstallShield(std::shared_ptr<Blob> header, InstallShieldVolumes volumes,
                                                       std::string& err);

// The MD5 of some bytes (RFC 1321), as the version 6 cabinets record for each file: for tests that write cabinets.
void InstallShieldMd5(const uint8_t* data, size_t n, uint8_t out[16]);

}  // namespace tpw::assets
