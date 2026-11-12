// host_tpi_worlds.cpp: the TPI Worlds and Rides mod's hook helpers (include/TpiWorlds.h, TPW_MOD_HOOK tpi_worlds) on the
// host, without game data: the world table and its lookups, the gate words of each title, the settings file's theme
// numbers, and the wide-text helpers the hooks in Game.cpp, RideDB.cpp and FS_MasterSave.cpp answer with. Exits nonzero
// on the first failed check's count.
#include <cstdio>
#include <string>
#include <cstring>

#include "TpiWorlds.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

static std::string Narrow(const char16_t* text) {
    std::string out;
    for (; *text != 0; ++text) out += static_cast<char>(*text);
    return out;
}

int main() {
    using namespace tpw;
    // The table: three worlds, Theme Park Inc's folders and names, each with an original world as its donor.
    Check(sizeof(kTpiWorlds) / sizeof(kTpiWorlds[0]) == 3, "three worlds");
    Check(std::string(kTpiWorlds[0].folder) == "arabian" && std::string(kTpiWorlds[0].title) == "Arabian Nights" && std::string(kTpiWorlds[0].donor) == "jungle", "arabian row");
    Check(std::string(kTpiWorlds[1].folder) == "water" && std::string(kTpiWorlds[1].title) == "Polar Zone" && std::string(kTpiWorlds[1].donor) == "hallow", "water row");
    Check(std::string(kTpiWorlds[2].folder) == "science" && std::string(kTpiWorlds[2].title) == "Land of Invention" && std::string(kTpiWorlds[2].donor) == "space", "science row");
    for (const TpiWorld& world : kTpiWorlds) {
        const std::string donor = world.donor;
        Check(donor == "fantasy" || donor == "hallow" || donor == "jungle" || donor == "space", "donor is an original world: " + donor);
        Check(TpiWorldOf(world.donor) == nullptr, "a donor is not a mod world: " + donor);
    }
    Check(sizeof(kTpiOwnFolders) / sizeof(kTpiOwnFolders[0]) == 7 && std::string(kTpiOwnFolders[0]) == "rides", "own folders");

    // Names equal in any case.
    Check(TpiEqualNoCase("Arabian", "aRABIAN"), "equal in any case");
    Check(!TpiEqualNoCase("arabian", "arabia"), "a shorter name differs");
    Check(!TpiEqualNoCase("arabia", "arabian"), "a longer name differs");
    Check(TpiEqualNoCase("", ""), "two empty names are equal");

    // The lookup by folder: any case, the originals and strangers are none, so is a null name.
    Check(TpiWorldOf("arabian") == &kTpiWorlds[0] && TpiWorldOf("WATER") == &kTpiWorlds[1] && TpiWorldOf(std::string("Science")) == &kTpiWorlds[2], "lookup by folder");
    for (const char* original : {"fantasy", "hallow", "jungle", "space", "", "arabian2", "polar"}) Check(TpiWorldOf(original) == nullptr, std::string("no world for ") + original);
    Check(TpiWorldOf(static_cast<const char*>(nullptr)) == nullptr, "no world for a null name");

    // The wide lookup, as the game keeps the theme name.
    Check(TpiWorldOfWide(u"arabian") == &kTpiWorlds[0] && TpiWorldOfWide(u"SCIENCE") == &kTpiWorlds[2], "wide lookup");
    Check(TpiWorldOfWide(u"jungle") == nullptr && TpiWorldOfWide(nullptr) == nullptr && TpiWorldOfWide(u"") == nullptr, "wide lookup: originals, null, empty");
    Check(TpiWorldOfWide(u"arabiän") == nullptr, "wide lookup: a character outside ASCII is no world");
    Check(TpiWorldOfWide(u"arabian arabian arabian arabian arabian") == nullptr, "wide lookup: a name longer than the narrow buffer is no world");
    char narrow[8];
    Check(TpiNarrowAscii(u"science", narrow, 8) && std::string(narrow) == "science", "narrow: fits exactly with its terminator");
    std::memset(narrow, 'x', sizeof(narrow));
    Check(!TpiNarrowAscii(u"sciences", narrow, 8) && narrow[0] == 'x', "narrow: one too long, buffer untouched");
    Check(!TpiNarrowAscii(u"a", narrow, 0), "narrow: size 0");

    // The gate words: the title split at its last space.
    std::string first, second;
    TpiGateNames(kTpiWorlds[0], first, second);
    Check(first == "Arabian" && second == "Nights", "gate words: Arabian Nights");
    TpiGateNames(kTpiWorlds[1], first, second);
    Check(first == "Polar" && second == "Zone", "gate words: Polar Zone");
    TpiGateNames(kTpiWorlds[2], first, second);
    Check(first == "Land of" && second == "Invention", "gate words: Land of Invention");
    const TpiWorld one = {"one", "Oneword", "jungle"};
    TpiGateNames(one, first, second);
    Check(first == "Oneword" && second.empty(), "gate words: a one-word title");

    // The settings file's theme numbers (ConverttoCurrentTheme's).
    Check(std::string(TpiSettingsThemeFolder(3)) == "fantasy" && std::string(TpiSettingsThemeFolder(1)) == "jungle" && std::string(TpiSettingsThemeFolder(2)) == "hallow" && std::string(TpiSettingsThemeFolder(4)) == "space", "settings theme numbers");
    Check(std::string(TpiSettingsThemeFolder(0)) == "jungle" && std::string(TpiSettingsThemeFolder(7)) == "jungle" && std::string(TpiSettingsThemeFolder(-1)) == "jungle", "settings theme numbers: others are jungle");

    // ASCII into a wide buffer: cut to size - 1, terminated, nothing for size 0, empty for a null text.
    char16_t wide[8];
    TpiCopyAscii("Arabian Nights", wide, 8);
    Check(Narrow(wide) == "Arabian", "copy: cut to seven characters");
    TpiCopyAscii("Zone", wide, 8);
    Check(Narrow(wide) == "Zone", "copy: whole text");
    TpiCopyAscii(nullptr, wide, 8);
    Check(wide[0] == 0, "copy: null text is empty");
    wide[0] = u'x';
    TpiCopyAscii("Zone", wide, 0);
    Check(wide[0] == u'x', "copy: size 0 writes nothing");
    TpiCopyAscii("Zone", wide, 1);
    Check(wide[0] == 0, "copy: size 1 is the terminator alone");

    // A record's own name split for the two signs.
    TpiSplitName(u"Belly Bounce", true, wide, 8);
    Check(Narrow(wide) == "Belly", "split: first word");
    TpiSplitName(u"Belly Bounce", false, wide, 8);
    Check(Narrow(wide) == "Bounce", "split: second word");
    TpiSplitName(u"Great Big Wheel", true, wide, 8);
    Check(Narrow(wide) == "Great B", "split: the first part is everything before the LAST blank, cut to the buffer");
    TpiSplitName(u"Great Big Wheel", false, wide, 8);
    Check(Narrow(wide) == "Wheel", "split: the second part is after the last blank");
    TpiSplitName(u"Slither", true, wide, 8);
    Check(Narrow(wide) == "Slither", "split: a name without a blank is all first");
    TpiSplitName(u"Slither", false, wide, 8);
    Check(wide[0] == 0, "split: and has no second part");
    TpiSplitName(nullptr, true, wide, 8);
    Check(wide[0] == 0, "split: a null name is empty");
    TpiSplitName(u"", false, wide, 8);
    Check(wide[0] == 0, "split: an empty name is empty");
    wide[0] = u'x';
    TpiSplitName(u"Belly Bounce", true, wide, 0);
    Check(wide[0] == u'x', "split: size 0 writes nothing");
    TpiSplitName(u"Belly Bounce", true, wide, 1);
    Check(wide[0] == 0, "split: size 1 is the terminator alone");

    // A world's texts: the title and the two gate words into a wide buffer; none for no world, a bad part or size 0.
    char16_t text[0x20];
    Check(TpiWorldText(&kTpiWorlds[2], 0, text, 0x20) && Narrow(text) == "Land of Invention", "world text: title");
    Check(TpiWorldText(&kTpiWorlds[2], 1, text, 0x20) && Narrow(text) == "Land of", "world text: first gate words");
    Check(TpiWorldText(&kTpiWorlds[2], 2, text, 0x20) && Narrow(text) == "Invention", "world text: second gate word");
    Check(TpiWorldText(&kTpiWorlds[0], 0, text, 8) && Narrow(text) == "Arabian", "world text: cut to the buffer");
    text[0] = u'x';
    Check(!TpiWorldText(nullptr, 0, text, 0x20) && text[0] == u'x', "world text: no world writes nothing");
    Check(!TpiWorldText(&kTpiWorlds[0], 3, text, 0x20) && !TpiWorldText(&kTpiWorlds[0], -1, text, 0x20) && text[0] == u'x', "world text: a part outside 0..2 writes nothing");
    Check(!TpiWorldText(&kTpiWorlds[0], 0, text, 0) && text[0] == u'x', "world text: size 0 writes nothing");
    Check(!TpiWorldText(TpiWorldOf("jungle"), 0, text, 0x20) && text[0] == u'x', "world text: an original world is none");

    if (gFailures != 0) {
        std::printf("%d check(s) failed\n", gFailures);
        return gFailures;
    }
    std::printf("tpi_worlds: all checks passed\n");
    return 0;
}
