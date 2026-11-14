// host_tpi_toilets.cpp: the TPI Worlds and Rides mod's men's and women's toilets (include/TpiToilets.h, TPW_MOD_HOOK
// tpi_worlds) on the host, without game data: each look's sex (include/TpiGuests.h, TpiGuestFemale), the kind of toilet
// from an item's folder, and the rule in a park with both kinds, with one kind, and as a second kind is built and the
// last of one sold. Exits nonzero on the first failed check's count.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "TpiGuests.h"
#include "TpiToilets.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// The kids set of a mod world as ESP_LoadSprites leaves it (as in host_tpi_guests.cpp).
static const char* const kBodies[] = {"generic:kids:SPR_BE", "generic:kids:SPR_SU", "generic:kids:SPR_FR", "generic:kids:SPR_KI",
                                      "generic:kids:SPR_BI", "generic:kids:SPR_CH", "generic:kids:SPR_SA", "generic:kids:SPR_TA",
                                      "arabian:Kids:SPR_BD", "arabian:Kids:SPR_FA", "arabian:Kids:SPR_GB", "arabian:Kids:SPR_GK",
                                      "arabian:Kids:SPR_GM", "arabian:Kids:SPR_GR", "arabian:Kids:SPR_KB", "arabian:Kids:SPR_MO"};
// Each look's sex as told from its pictures (1 a girl or woman), in kBodies' order.
static const int8_t kFemale[] = {0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1};

// A visitor as the game lays it out where TpiGuestFemale reads it: the thing index at +0, the sprite number at +0x20 and
// its set at +0x24.
struct Visitor {
    alignas(4) uint8_t bytes[0x40] = {};
    Visitor(uint16_t thing, int32_t sprite, int32_t set) {
        std::memcpy(bytes, &thing, sizeof thing);
        std::memcpy(bytes + 0x20, &sprite, sizeof sprite);
        std::memcpy(bytes + 0x24, &set, sizeof set);
    }
};

// The rule as the game applies it (TpiToiletFor, include/helpers/PersonHelpers.h): the park's toilets counted over its
// items, open or not.
static bool MayUse(const std::vector<std::string>& park, const std::string& toilet, const void* person) {
    const int8_t sex = tpw::TpiToiletSex(toilet.c_str());
    if (sex < 0) return true;
    const int8_t female = tpw::TpiGuestFemale(person);
    int32_t kinds[2] = {0, 0};
    for (const std::string& item : park) {
        const int8_t s = tpw::TpiToiletSex(item.c_str());
        if (s >= 0) kinds[s]++;
    }
    return tpw::TpiToiletOpenTo(sex, female, kinds[0], kinds[1]);
}

int main() {
    using namespace tpw;
    const char* const men = "data:levels:arabian:features:mtoilet:";
    const char* const women = "data:levels:arabian:features:ftoilet:";
    const char* const tpwToilet = "data:levels:arabian:features:toilet:";
    const char* const ride = "data:levels:arabian:rides:carpet:";

    // The kind of toilet from the folder: the converter's two, any case and separator, with or without .wad; nothing else.
    Check(TpiToiletSex(men) == 0, "mtoilet, the Mac's folder");
    Check(TpiToiletSex(women) == 1, "ftoilet, the Mac's folder");
    Check(TpiToiletSex("data/levels/water/features/FTOILET.WAD") == 1, "ftoilet, upper case .wad");
    Check(TpiToiletSex("data\\levels\\science\\features\\MToilet\\") == 0, "mtoilet, backslashes");
    Check(TpiToiletSex("mtoilet") == 0 && TpiToiletSex("ftoilet.wad:") == 1, "the name alone");
    Check(TpiToiletSex(tpwToilet) == -1, "Theme Park World's toilet is everyone's");
    Check(TpiToiletSex(ride) == -1, "a ride");
    Check(TpiToiletSex("data:levels:arabian:features:xmtoilet:") == -1 && TpiToiletSex("data:mtoilets:") == -1 &&
              TpiToiletSex("data:toilet:") == -1 && TpiToiletSex("data:ttoilet:") == -1, "names like it");
    Check(TpiToiletSex("") == -1 && TpiToiletSex(":::") == -1 && TpiToiletSex(nullptr) == -1, "no folder");

    // The rule: with both kinds each sex its own; with one kind (or none) everyone; an unknown sex or another item always.
    for (int8_t female = -1; female <= 1; ++female) {
        for (int32_t m = 0; m <= 2; ++m) {
            for (int32_t w = 0; w <= 2; ++w) {
                const bool both = m > 0 && w > 0;
                const std::string at = " (sex " + std::to_string(female) + ", " + std::to_string(m) + " men's, " +
                                       std::to_string(w) + " women's)";
                Check(TpiToiletOpenTo(0, female, m, w) == (!both || female != 1), "the men's" + at);
                Check(TpiToiletOpenTo(1, female, m, w) == (!both || female != 0), "the women's" + at);
                Check(TpiToiletOpenTo(-1, female, m, w), "not a toilet" + at);
            }
        }
    }

    // Each look's sex (the tagging): the table's, read only in one of the mod's worlds.
    TpiSetLooks(kBodies, 16, kBodies, 16);
    for (int32_t look = 0; look < 16; ++look) Check(gTpiLookFemale[look] == kFemale[look], std::string("sex of ") + kBodies[look]);
    gModTpiWorlds = true;
    gTpiThemeIsMod = true;
    // Without extra visitors loaded: the visitor's sprite number while its set is the kids'.
    gTpiExtraGuests[0] = 0;
    for (int32_t look = 0; look < 16; ++look) {
        const Visitor kid(1, look, 0);
        Check(TpiGuestFemale(kid.bytes) == kFemale[look], "sprite number " + std::to_string(look));
    }
    {
        const Visitor other(1, 3, 2), past(1, 16, 0), negative(1, -1, 0);
        Check(TpiGuestFemale(other.bytes) == -1, "another set: not known");
        Check(TpiGuestFemale(past.bytes) == -1 && TpiGuestFemale(negative.bytes) == -1, "no such look: not known");
    }
    // With extra visitors loaded: the thing index's look, whatever the sprite number; as many of each sex as the worn
    // looks give (five boys and five girls, a man and a woman of each kind of grown-up: half).
    gTpiExtraGuests[0] = 8;
    int32_t females = 0;
    const int32_t visitors = 10000;
    for (uint32_t thing = 1; thing <= static_cast<uint32_t>(visitors); ++thing) {
        const Visitor v(static_cast<uint16_t>(thing), 2, 0);
        const int32_t look = TpiGuestLook(static_cast<uint16_t>(thing), gTpiLookCount, gTpiLookPick);
        const int8_t female = TpiGuestFemale(v.bytes);
        Check(look >= 0 && female == kFemale[look], "the look's sex, thing " + std::to_string(thing));
        Check(female == TpiGuestFemale(v.bytes), "the same sex again");
        females += female == 1;
    }
    Check(females > visitors * 48 / 100 && females < visitors * 52 / 100, "half are girls or women: " + std::to_string(females));
    std::printf("tpi_toilets: %d of %d visitors girls or women\n", females, visitors);
    // Anywhere else no sex, and the visitor is not read.
    gTpiThemeIsMod = false;
    Check(TpiGuestFemale(nullptr) == -1, "an original world");
    gTpiThemeIsMod = true;
    gModTpiWorlds = false;
    Check(TpiGuestFemale(nullptr) == -1, "hook off");
    gModTpiWorlds = true;

    // A park through a game: one men's toilet (everyone uses it), a women's one built (each sex its own), a second men's
    // one (the same), the women's sold (everyone the men's again); Theme Park World's toilet and rides always everyone's.
    std::vector<std::string> park = {ride, tpwToilet, men};
    int32_t served[4][2] = {};
    for (int32_t step = 0; step < 4; ++step) {
        if (step == 1) park.push_back(women);
        if (step == 2) park.push_back("data/levels/arabian/features/MTOILET");
        if (step == 3) park.erase(park.begin() + 3);
        const bool both = step == 1 || step == 2;
        for (uint32_t thing = 1; thing <= 2000; ++thing) {
            const Visitor v(static_cast<uint16_t>(thing), 0, 0);
            const int8_t female = TpiGuestFemale(v.bytes);
            const std::string at = " (step " + std::to_string(step) + ", thing " + std::to_string(thing) + ")";
            Check(MayUse(park, ride, v.bytes) && MayUse(park, tpwToilet, v.bytes), "a ride and the TPW toilet" + at);
            const bool mens = MayUse(park, men, v.bytes);
            const bool womens = MayUse(park, women, v.bytes);
            if (both) {
                Check(mens == (female == 0) && womens == (female == 1), "each sex its own" + at);
            } else {
                Check(mens && womens, "one kind: everyone" + at);
            }
            served[step][0] += mens;
            served[step][1] += womens;
        }
        std::printf("tpi_toilets: step %d: %d may use the men's, %d the women's of 2000\n", step, served[step][0], served[step][1]);
    }
    Check(served[1][0] + served[1][1] == 2000 && served[2][0] == served[1][0], "both kinds: each visitor exactly one kind");
    std::printf("tpi_toilets: %s\n", gFailures ? "FAILED" : "ok");
    return gFailures;
}
