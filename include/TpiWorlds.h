// TpiWorlds.h: the TPI Worlds and Rides mod's hooks (tpw-mods tpi-worlds; TPW_MOD_HOOK tpi_worlds and tpi_islands).
// The mod adds three worlds made from the player's own copy of Theme Park Inc, the game's sequel on the same engine
// (the launcher's converter, assets/tpi_convert.h; the content is the data folder's assets/tpi, platform/tpi_content.h):
// Arabian Nights, Polar Zone and Land of Invention (Theme Park Inc's three zones, under their own names), each with its own rides, shops, sideshows and scenery, played
// by Theme Park World's rules. With tpi_worlds on, the game's list of worlds has the three after the original four.
// With tpi_islands on, the island screen has a second group of islands for them, reached by a switch, and the original
// four stay as they are. Only with the port's fixes: off (the default) or in a build without them, the game has the
// original's four worlds and its island screen.
//
// The table of the three worlds and the helpers that read it live here, header only, so that the game's code (which
// sees include/ alone) and the platform layer (platform/tpi_content.h, which includes this) share one table. They are
// data and pure functions, outside the port fixes guard, because the launcher's converter names the worlds in every
// build; the hooks' flags are inside it.
#pragma once

#include <cctype>
#include <cstdint>
#include <string>

namespace tpw {

// The mod's worlds, Theme Park Inc's three zones under their own names (the maintainer, 2026-11-11), in the order the island
// screen's second group shows them: the level folder's name (data/levels/<folder>, Theme Park Inc's own folder names,
// and the name a saved park's folder keeps its world by), the name the player sees, and the donor: the original world
// whose files stand in for what the converted world lacks (platform/tpi_content.h's table).
struct TpiWorld {
    const char* folder;
    const char* title;
    const char* donor;
};
inline constexpr TpiWorld kTpiWorlds[] = {
    {"arabian", "Arabian Nights", "jungle"},
    {"water", "Polar Zone", "hallow"},
    {"science", "Land of Invention", "space"},
};

// The folders of a world that never fall back to the donor (platform/tpi_content.h's table), in any case.
inline constexpr const char* kTpiOwnFolders[] = {"rides", "shops", "sideshow", "features", "upgrades", "facilities", "obstacles"};

// Two names equal in any case.
inline bool TpiEqualNoCase(const char* a, const char* b) {
    for (;; ++a, ++b) {
        const int ca = std::tolower(static_cast<unsigned char>(*a)), cb = std::tolower(static_cast<unsigned char>(*b));
        if (ca != cb) return false;
        if (ca == 0) return true;
    }
}

// The mod's world whose level folder is `folder` (any case), or nullptr: the original four and any other name are none.
inline const TpiWorld* TpiWorldOf(const char* folder) {
    if (folder == nullptr) return nullptr;
    for (const TpiWorld& world : kTpiWorlds)
        if (TpiEqualNoCase(folder, world.folder)) return &world;
    return nullptr;
}
inline const TpiWorld* TpiWorldOf(const std::string& folder) { return TpiWorldOf(folder.c_str()); }

// Whether a game file name is a file of one of the mod's worlds: one under data/levels/<world>/ or data/lobby/<world>/,
// the world a mod world (TpiWorldOf), the parts parted by ':' as the engine's names are
// ("data:levels:arabian:Features:parasol:textures:parasol.tga") or by '/' or '\', in any case. Those are the content
// folder's (platform/tpi_content.h), but for the few a world takes from its donor (its Advisor, sky and sound folders).
// False for a null name, the original four, a file directly in data/levels or data/lobby and any other name.
inline bool TpiWorldFile(const char* name) {
    if (name == nullptr) return false;
    std::string parts[3];
    for (std::string& part : parts) {
        for (; *name != 0 && *name != ':' && *name != '/' && *name != '\\'; ++name) part += *name;
        if (*name++ == 0) return false;   // the file's own name came before the world's folder
    }
    return TpiEqualNoCase(parts[0].c_str(), "data") &&
           (TpiEqualNoCase(parts[1].c_str(), "levels") || TpiEqualNoCase(parts[1].c_str(), "lobby")) &&
           TpiWorldOf(parts[2]) != nullptr && *name != 0;
}

// The two words on a world's gate, for the island screen and the gate signs: its title split at its last space ("Land
// of" and "Invention"; a one-word title is the first word, with an empty second).
inline void TpiGateNames(const TpiWorld& world, std::string& first, std::string& second) {
    const std::string title = world.title;
    const size_t space = title.rfind(' ');
    first = space == std::string::npos ? title : title.substr(0, space);
    second = space == std::string::npos ? std::string() : title.substr(space + 1);
}

// The folder the settings file's old theme number names (ConverttoCurrentTheme's: 3 fantasy, 1 jungle, 2 hallow, 4 space,
// anything else jungle), so that the number finds its folder among seven as it did among four.
inline const char* TpiSettingsThemeFolder(int number) {
    switch (number) {
    case 3: return "fantasy";
    case 2: return "hallow";
    case 4: return "space";
    default: return "jungle";
    }
}

// `text` (ASCII, or nullptr for an empty text) into a wide buffer of `size` characters, cut to size - 1 of them and
// terminated; nothing for a size of 0.
inline void TpiCopyAscii(const char* text, char16_t* buffer, uint32_t size) {
    if (size == 0) return;
    uint32_t n = 0;
    for (; text != nullptr && n + 1 < size && text[n] != 0; ++n) buffer[n] = static_cast<char16_t>(static_cast<unsigned char>(text[n]));
    buffer[n] = 0;
}

// A wide name (a theme folder as the game keeps it) narrowed into `buffer` of `size` characters: true when it fits whole
// (with its terminator) and every character is ASCII; false, with the buffer untouched, for a null name, a longer one
// or a character above 0x7f (a level folder's name never has one).
inline bool TpiNarrowAscii(const char16_t* name, char* buffer, uint32_t size) {
    if (name == nullptr || size == 0) return false;
    uint32_t n = 0;
    for (; name[n] != 0; ++n)
        if (n + 1 >= size || name[n] > 0x7f) return false;
    for (uint32_t i = 0; i < n; ++i) buffer[i] = static_cast<char>(name[i]);
    buffer[n] = 0;
    return true;
}

// The mod's world whose level folder a wide name is (any case), or nullptr, as TpiWorldOf.
inline const TpiWorld* TpiWorldOfWide(const char16_t* name) {
    char narrow[0x20];
    return TpiNarrowAscii(name, narrow, sizeof(narrow)) ? TpiWorldOf(narrow) : nullptr;
}

// The two sign parts of an item's own wide name (a ride record's type name), split at its last blank: `first` gives
// the part before it (the whole of a name without one), otherwise the part after it (empty without one); cut to
// size - 1 characters and terminated, nothing for a size of 0, an empty text for a null name.
inline void TpiSplitName(const char16_t* name, bool first, char16_t* buffer, uint32_t size) {
    if (size == 0) return;
    if (name == nullptr) { buffer[0] = 0; return; }
    const char16_t* end = name;
    const char16_t* split = nullptr;
    for (; *end != 0; ++end)
        if (*end == u' ') split = end;
    const char16_t* from = first ? name : (split == nullptr ? end : split + 1);
    uint32_t n = 0;
    for (; n + 1 < size && from[n] != 0 && !(first && from + n == split); ++n) buffer[n] = from[n];
    buffer[n] = 0;
}

// A world's text into a wide buffer of `size` characters: `part` 0 its title, 1 the first word group of the title, 2 the
// second (TpiGateNames), cut to size - 1 and terminated; true when written, false, with nothing written, for a null
// world (the original four and any other name, TpiWorldOf), any other part or a size of 0.
inline bool TpiWorldText(const TpiWorld* world, int part, char16_t* buffer, uint32_t size) {
    if (world == nullptr || size == 0 || part < 0 || part > 2) return false;
    std::string first, second;
    TpiGateNames(*world, first, second);
    TpiCopyAscii(part == 0 ? world->title : part == 1 ? first.c_str() : second.c_str(), buffer, size);
    return true;
}

}  // namespace tpw

