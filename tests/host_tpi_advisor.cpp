// host_tpi_advisor.cpp: the TPI Worlds and Rides mod's advisor rule (include/TpiAdvisor.h, TPW_MOD_HOOK tpi_worlds in
// ADV_Call) on the host, without game data: the rows are made up here in the response table's form. In the mod's three
// worlds a line of the world's own speech becomes the stand-in (its sound, lip-sync file, sequence, hat and hand, the
// global bank) with the line's id, model and tag; a global line and every line in the original four worlds stay as they
// are; with no usable stand-in the line is left unsaid. The world's folder is read off the end of get_theme_path's path.
// Exits with the number of failed checks.
#include <cstdio>
#include <cstring>
#include <string>

#include "TpiAdvisor.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

static bool Same(const AdvisorLine& a, const AdvisorLine& b) { return std::memcmp(&a, &b, sizeof a) == 0; }

int main() {
    using namespace tpw;
    Check(kTpiAdvisorStandIn == 390, "the stand-in is response 390");

    // Made-up rows: a line of the world's bank (sound set 1, model 1), a global line (sound set 0, model 1) and a stand-in
    // (sound set 0, model 0, the lobby's), every field different so that a copied one shows.
    const AdvisorLine world = {7, 11, 12, 16, 0x00010001, -1, 3, 383};
    const AdvisorLine global = {8, 21, 22, -1, 0x00000001, 5, 6, 384};
    const AdvisorLine standIn = {kTpiAdvisorStandIn, 31, 32, -1, 0x00000000, 0, 9, 385};
    const AdvisorLine untouched = {-5, -5, -5, -5, 0xdeadbeef, -5, -5, -5};

    // The mod's worlds, in any case: the world's line is said as the stand-in, by the line's model, with its id and tag.
    for (const char* theme : {"arabian", "water", "science", "WATER", "Science"}) {
        AdvisorLine out = untouched;
        Check(TpiAdvisorLine(world, &standIn, theme, out), std::string("said in ") + theme);
        Check(out.mId == world.mId && out.mTag == world.mTag, std::string("the line's id and tag in ") + theme);
        Check(out.mSound == standIn.mSound && out.mLipFile == standIn.mLipFile && out.mSequence == standIn.mSequence &&
                  out.mHat == standIn.mHat && out.mHand == standIn.mHand,
              std::string("the stand-in's sound, lip-sync, sequence, hat and hand in ") + theme);
        Check(out.mWhichAndSoundSet == 0x00000001, std::string("the line's model, the global bank in ") + theme);

        // A global line there is itself.
        out = untouched;
        Check(TpiAdvisorLine(global, &standIn, theme, out) && Same(out, global), std::string("a global line as it is in ") + theme);

        // No stand-in, or a stand-in of a world's bank: the world's line is left unsaid and `out` untouched; a global line
        // is still said.
        out = untouched;
        Check(!TpiAdvisorLine(world, nullptr, theme, out) && Same(out, untouched), std::string("no stand-in: unsaid in ") + theme);
        out = untouched;
        Check(!TpiAdvisorLine(world, &world, theme, out) && Same(out, untouched), std::string("a world stand-in: unsaid in ") + theme);
        out = untouched;
        Check(TpiAdvisorLine(global, nullptr, theme, out) && Same(out, global), std::string("no stand-in, a global line in ") + theme);
    }

    // The original four, any other name and no name: every line as it is, the stand-in or none.
    for (const char* theme : {"fantasy", "hallow", "jungle", "space", "", "polar", "arabian2"}) {
        for (const AdvisorLine* stand : {&standIn, static_cast<const AdvisorLine*>(nullptr)}) {
            AdvisorLine out = untouched;
            Check(TpiAdvisorLine(world, stand, theme, out) && Same(out, world), std::string("the world's line as it is in ") + theme);
            out = untouched;
            Check(TpiAdvisorLine(global, stand, theme, out) && Same(out, global), std::string("a global line as it is in ") + theme);
        }
    }
    AdvisorLine out = untouched;
    Check(TpiAdvisorLine(world, &standIn, nullptr, out) && Same(out, world), "no theme name: as it is");

    // Any sound set above 0 is the world's (the table has 1 only).
    const AdvisorLine set2 = {9, 41, 42, 16, 0x00020001, -1, 0, 2};
    out = untouched;
    Check(TpiAdvisorLine(set2, &standIn, "water", out) && out.mSound == standIn.mSound && out.mWhichAndSoundSet == 0x00000001 &&
              out.mId == 9 && out.mTag == 2,
          "sound set 2 is a world's");

    // The world's folder from get_theme_path's kind of path, with either kind of separator.
    Check(std::strcmp(TpiThemeOfPath("Data::levels:water"), "water") == 0, "the folder of a Mac path");
    Check(std::strcmp(TpiThemeOfPath("/x/data//levels/science"), "science") == 0, "the folder of a host path");
    Check(std::strcmp(TpiThemeOfPath("C:\\data\\levels\\arabian"), "arabian") == 0, "the folder of a Windows path");
    Check(std::strcmp(TpiThemeOfPath("hallow"), "hallow") == 0, "a bare name");
    Check(std::strcmp(TpiThemeOfPath("levels:"), "") == 0 && std::strcmp(TpiThemeOfPath(nullptr), "") == 0, "no name");
    Check(TpiWorldOf(TpiThemeOfPath("Data::levels:water")) != nullptr && TpiWorldOf(TpiThemeOfPath("Data::levels:hallow")) == nullptr,
          "a path's folder names the world");

    if (gFailures == 0) std::printf("host_tpi_advisor: all checks passed\n");
    return gFailures;
}
