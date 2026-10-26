// icon.h: the game's own application icon (the boy with his tongue out), taken from the player's files while they are
// imported and kept as <data folder>/icon.png, which the launcher shows in the Dock and the taskbar (launcher/app_icon.h).
// The icon is Bullfrog's artwork, so nothing of it is in this tree: only readers of where the player's copy holds it.
//   - the Mac program's resource fork (SimTheme Park on the US Mac CD: 'icl8' + 'ICN#' 128, 32 x 32 in 256 colours;
//     the European 2.1.x Mac applications: an 'icns' 128 resource with it32/t8mk, 128 x 128 in 32-bit colour),
//   - TP.ico beside the PC program (the US and Gold discs: 64 x 64 in 256 colours, 32 x 32 in 4-, 8- and 24-bit colour),
//   - the PC program itself (TP.EXE's RT_ICON resources, the same pictures as TP.ico).
// The best of what a drop holds wins: the most pixels, then the most bits per pixel. Nothing here needs more than zlib.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets {

// A decoded icon: width x height pixels, rows from the top, 8-bit RGBA with straight (not premultiplied) alpha.
struct IconImage {
    int width = 0, height = 0;
    int bitDepth = 0;   // of the source picture: 1, 4, 8, 24 or 32
    std::vector<uint8_t> rgba;
    bool Empty() const { return width <= 0 || height <= 0 || rgba.empty(); }
};

// Where a file of a drop may hold the icon, from its name alone (`path` as the source lists it, '/' separated;
// resourceFork: the entry is a resource fork): the Mac program's resource fork (a name with "theme park" in it), a .ico
// named after the game (TP.ico, TPW.ico, "theme park ..."), or the PC program (TP.EXE, TPW.EXE, ThemeParkWorld.exe, ...;
// never an installer or a patch). None for everything else.
enum class IconFile : uint8_t { None, ResourceFork, Ico, Exe };
IconFile IconFileKind(const std::string& path, bool resourceFork);
// Such a file is read whole when it is at most this big (the PC program is about 3.7 MB).
inline constexpr uint64_t kIconFileLimit = 16ull << 20;

// The best icon in a file of that kind (the most pixels, then the deepest colour; in a resource fork, the lowest id at a
// tie, which is the application's own). False, with `out` untouched, when it holds none it can read.
bool DecodeIcon(IconFile kind, const uint8_t* data, size_t size, IconImage& out);
// Whether `candidate` beats `current`: more pixels, then more bits per pixel. Anything beats an empty `current`.
bool BetterIcon(const IconImage& candidate, const IconImage& current);

// icon.png: an 8-bit RGBA PNG whose "Source" text chunk says where the picture came from, as "TP.ico, 64x64, 8-bit"
// (`source` is the file's name; the size and depth are added). Decoding reads only PNGs of this form (8-bit RGBA, not
// interlaced) and takes the depth back from that text (32 when there is none).
std::vector<uint8_t> EncodeIconPng(const IconImage& icon, const std::string& source);
bool DecodeIconPng(const uint8_t* data, size_t size, IconImage& out, std::string* source);
bool ReadIconPng(const std::string& path, IconImage& out, std::string* source);
bool WriteIconPng(const std::string& path, const IconImage& icon, const std::string& source, std::string& err);

}  // namespace tpw::assets
