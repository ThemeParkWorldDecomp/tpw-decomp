// host_tpi_guests.cpp: the TPI Worlds and Rides mod's extra visitors (include/TpiGuests.h, TPW_MOD_HOOK tpi_worlds) on
// the host, without game data: the looks' tags, the spread of new visitors (70 in 100 kids, 20 parents, 10
// grandparents, the worn kid looks evenly, Theme Park World's TA and CH never), that a visitor's body and head match and
// stay the same, that nothing changes without extras or for other sets, and when the mod's sprites are used. Exits nonzero on the first failed check's count.
#include <cstdint>
#include <cstdio>
#include <string>

#include "TpiGuests.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// The kids set of a mod world as ESP_LoadSprites leaves it: Theme Park World's kids (the four preferred first, as
// ESP_FindESPFiles loads them), then the extras in name order; the heads the same names, as paths.
static const char* const kBodies[] = {"generic:kids:SPR_BE", "generic:kids:spr_su", "generic:kids:SPR_FR", "generic:kids:SPR_KI",
                                      "generic:kids:SPR_BI", "generic:kids:SPR_CH", "generic:kids:SPR_SA", "generic:kids:SPR_TA",
                                      "arabian:Kids:SPR_BD", "arabian:Kids:SPR_FA", "arabian:Kids:SPR_GB", "arabian:Kids:SPR_GK",
                                      "arabian:Kids:SPR_GM", "arabian:Kids:SPR_GR", "arabian:Kids:SPR_KB", "arabian:Kids:SPR_MO"};
static const char* const kHeads[] = {"generic/kidsheads/SPR_BE", "generic/kidsheads/SPR_SU", "generic/kidsheads/SPR_FR",
                                     "generic/kidsheads/SPR_KI", "generic/kidsheads/SPR_BI", "generic/kidsheads/SPR_CH",
                                     "generic/kidsheads/SPR_SA", "generic/kidsheads/SPR_TA", "arabian/Kidsheads/SPR_BD",
                                     "arabian/Kidsheads/SPR_FA", "arabian/Kidsheads/SPR_GB", "arabian/Kidsheads/SPR_GK",
                                     "arabian/Kidsheads/SPR_GM", "arabian/Kidsheads/SPR_GR", "arabian/Kidsheads/SPR_KB",
                                     "arabian/Kidsheads/SPR_MO"};

int main() {
    using namespace tpw;
    // The tags: twelve kids (six boys, six girls), two parents and two grandparents (a man and a woman each); all worn but
    // Theme Park World's TA and CH (Theme Park Inc's BD and GB are the same two), so ten kids are worn, five boys and five
    // girls; names in any case, with or without a path; other names have none.
    int32_t kinds[3][2] = {};
    int32_t worn[3][2] = {};
    for (const TpiLookTag& tag : kTpiLookTags) {
        kinds[tag.kind][tag.female]++;
        worn[tag.kind][tag.female] += tag.worn;
        Check(tag.worn == (std::string(tag.name) != "SPR_TA" && std::string(tag.name) != "SPR_CH"),
              std::string("worn: ") + tag.name);
    }
    Check(kinds[kTpiKid][0] == 6 && kinds[kTpiKid][1] == 6, "six boys and six girls");
    Check(worn[kTpiKid][0] == 5 && worn[kTpiKid][1] == 5, "five boys and five girls worn");
    Check(kinds[kTpiParent][0] == 1 && kinds[kTpiParent][1] == 1, "a father and a mother");
    Check(kinds[kTpiGrandparent][0] == 1 && kinds[kTpiGrandparent][1] == 1, "a grandfather and a grandmother");
    Check(worn[kTpiParent][0] + worn[kTpiParent][1] + worn[kTpiGrandparent][0] + worn[kTpiGrandparent][1] == 4,
          "the grown-ups all worn");
    Check(TpiLookTagOf("SPR_TA")->female == 0 && TpiLookTagOf("SPR_BD")->female == 0 && TpiLookTagOf("SPR_CH")->female == 1 &&
              TpiLookTagOf("SPR_GB")->female == 1, "TA and BD boys, CH and GB girls");
    Check(TpiLookTagOf("spr_gm") != nullptr && TpiLookTagOf("spr_gm")->female == 1, "grandmother, lower case");
    Check(TpiLookTagOf("x:y\\SPR_FA") != nullptr && TpiLookTagOf("x:y\\SPR_FA")->female == 0, "father, a path");
    Check(TpiLookTagOf("SPR_FAX") == nullptr && TpiLookTagOf("SPR_F") == nullptr && TpiLookTagOf("") == nullptr &&
              TpiLookTagOf(nullptr) == nullptr, "no tag for other names");

    // The looks from the names: each body's head is the head of its name, wherever it is.
    TpiSetLooks(kBodies, 16, kHeads, 16);
    Check(gTpiLookCount == 16, "sixteen looks");
    for (int32_t b = 0; b < 16; ++b) Check(gTpiHeadOf[b] == b, "head of look " + std::to_string(b));
    const char* const swapped[] = {kHeads[1], kHeads[0]};
    TpiSetLooks(kBodies, 2, swapped, 2);
    Check(gTpiHeadOf[0] == 1 && gTpiHeadOf[1] == 0, "heads found by name, not place");
    const char* const unknown[] = {"generic:kids:SPR_ZZ"};
    TpiSetLooks(unknown, 1, kHeads, 16);
    Check(gTpiLookKind[0] == kTpiKid && gTpiLookFemale[0] == -1 && gTpiHeadOf[0] == -1 && gTpiLookPick[0] == kTpiKid,
          "a sprite without a tag is a worn kid");

    // TA and CH (places 7 and 5) stay kids with their sex and head, but are never picked; every other look is picked as
    // its kind.
    TpiSetLooks(kBodies, 16, kHeads, 16);
    for (int32_t b = 0; b < 16; ++b)
        Check(gTpiLookPick[b] == (b == 5 || b == 7 ? -1 : gTpiLookKind[b]), "pick of look " + std::to_string(b));
    Check(gTpiLookKind[5] == kTpiKid && gTpiLookFemale[5] == 1 && gTpiHeadOf[5] == 5, "CH a girl");
    Check(gTpiLookKind[7] == kTpiKid && gTpiLookFemale[7] == 0 && gTpiHeadOf[7] == 7, "TA a boy");

    // The spread of 10,000 visitors arriving one after another: 70/20/10 within one in a hundred, each of the ten worn kid
    // looks within a tenth of its even share and TA and CH never, each parent and grandparent look within a fifth of its,
    // the same visitor always the same look.
    for (uint32_t first : {1u, 2000u, 40000u}) {
        int32_t perLook[16] = {};
        int32_t perKind[3] = {};
        const int32_t visitors = 10000;
        for (uint32_t thing = first; thing < first + visitors; ++thing) {
            const int32_t look = TpiGuestLook(static_cast<uint16_t>(thing), gTpiLookCount, gTpiLookPick);
            Check(look >= 0 && look < 16, "look in range, thing " + std::to_string(thing));
            if (look < 0 || look >= 16) continue;
            Check(look == TpiGuestLook(static_cast<uint16_t>(thing), gTpiLookCount, gTpiLookPick), "same look again");
            perLook[look]++;
            perKind[gTpiLookKind[look]]++;
        }
        const std::string from = " (from thing " + std::to_string(first) + ")";
        for (int32_t k = 0; k < 3; ++k) {
            const int32_t want = visitors * kTpiKindShare[k] / 100;
            Check(perKind[k] > want - visitors / 100 && perKind[k] < want + visitors / 100,
                  "kind " + std::to_string(k) + ": " + std::to_string(perKind[k]) + " of " + std::to_string(visitors) + from);
        }
        for (int32_t look = 0; look < 16; ++look) {
            const int32_t kind = gTpiLookPick[look];
            if (kind < 0) {
                Check(perLook[look] == 0, "look " + std::string(kBodies[look]) + " not worn: " + std::to_string(perLook[look]) + from);
                continue;
            }
            const int32_t even = visitors * kTpiKindShare[kind] / 100 / (kind == kTpiKid ? 10 : 2);
            const int32_t slack = even / (kind == kTpiKid ? 10 : 5);
            Check(perLook[look] > even - slack && perLook[look] < even + slack,
                  "look " + std::string(kBodies[look]) + ": " + std::to_string(perLook[look]) + ", even " + std::to_string(even) + from);
        }
        std::printf("tpi_guests: from thing %u: kids %d, parents %d, grandparents %d of %d\n", first, perKind[0], perKind[1],
                    perKind[2], visitors);
    }
    // Every thing index: always a look; no long run of one kind among visitors in a row.
    int32_t run = 0, longest = 0, last = -1;
    for (uint32_t thing = 0; thing < 0x10000; ++thing) {
        const int32_t look = TpiGuestLook(static_cast<uint16_t>(thing), gTpiLookCount, gTpiLookPick);
        Check(look >= 0 && look < 16 && look != 5 && look != 7, "a worn look for every thing index");
        const int32_t kind = look >= 0 ? gTpiLookKind[look] : -1;
        run = kind != kTpiKid && kind == last ? run + 1 : 1;
        last = kind;
        if (run > longest) longest = run;
    }
    Check(longest <= 6, "no long run of grown-ups, longest " + std::to_string(longest));

    // Fewer kid sprites (the player's number of kid sprites): the kid looks are those loaded but TA and CH, and Theme
    // Park Inc's four, evenly; the grown-ups keep their shares. Four (BE, SU, FR, KI): eight kid looks; six (and BI, CH):
    // nine, CH never.
    for (int32_t kids : {4, 6}) {
        const char* fewer[14];
        int32_t n = 0;
        for (int32_t b = 0; b < kids; ++b) fewer[n++] = kBodies[b];
        for (int32_t b = 8; b < 16; ++b) fewer[n++] = kBodies[b];
        TpiSetLooks(fewer, n, kHeads, 16);
        const int32_t kidLooks = 4 + (kids == 6 ? 5 : 4);
        const std::string what = std::to_string(kids) + " kids: ";
        int32_t perLook[14] = {};
        for (uint32_t thing = 1; thing <= 10000; ++thing) perLook[TpiGuestLook(static_cast<uint16_t>(thing), n, gTpiLookPick)]++;
        for (int32_t look = 0; look < n; ++look) {
            const int32_t kind = gTpiLookPick[look];
            if (kind < 0) {
                Check(perLook[look] == 0, what + "look " + std::to_string(look) + " not worn");
                continue;
            }
            const int32_t even = 10000 * kTpiKindShare[kind] / 100 / (kind == kTpiKid ? kidLooks : 2);
            Check(perLook[look] > even * 9 / 10 && perLook[look] < even * 11 / 10, what + "look " + std::to_string(look));
        }
        Check(gTpiHeadOf[kids] == 8 && gTpiHeadOf[n - 1] == 15, what + "heads by name");
    }
    // Only Theme Park World's kids (a world converted without Theme Park Inc's visitors): every visitor a worn kid.
    TpiSetLooks(kBodies, 8, kHeads, 8);
    for (uint32_t thing = 1; thing < 2000; ++thing) {
        const int32_t look = TpiGuestLook(static_cast<uint16_t>(thing), 8, gTpiLookPick);
        Check(look >= 0 && look < 8 && look != 5 && look != 7, "only kids: a worn kid");
    }
    Check(TpiGuestLook(5, 0, gTpiLookPick) == -1, "no looks: none");
    // Only looks that are not worn: none.
    const char* const unworn[] = {kBodies[5], kBodies[7]};
    TpiSetLooks(unworn, 2, kHeads, 16);
    Check(TpiGuestLook(5, 2, gTpiLookPick) == -1, "only TA and CH: none");

    // The mod's sprites are used only with the hook on and one of the mod's worlds in play.
    gModTpiWorlds = false;
    gTpiThemeIsMod = true;
    Check(!TpiWorldInPlay(), "hook off");
    gModTpiWorlds = true;
    gTpiThemeIsMod = false;
    Check(!TpiWorldInPlay(), "an original world");
    gTpiThemeIsMod = true;
    Check(TpiWorldInPlay(), "a mod world");
    // The visitor's own sprite (TpiGuestSpriteOf): the thing index is the half-word at the person's +0, read only in one
    // of the mod's worlds with extras loaded; the body is the look, the head the look's head.
    TpiSetLooks(kBodies, 16, swapped, 2);
    gTpiExtraGuests[0] = 8;
    gTpiExtraGuests[1] = 8;
    alignas(4) uint8_t person[0x40] = {};
    for (uint32_t thing = 1; thing < 4000; ++thing) {
        const uint16_t id = static_cast<uint16_t>(thing);
        *reinterpret_cast<uint16_t*>(person) = id;
        Check(TpiThingIndex(person) == id, "thing index at +0");
        const int32_t look = TpiGuestLook(id, 16, gTpiLookPick);
        Check(look != 5 && look != 7 && TpiGuestSpriteOf(0, 3, person) == look, "a person's body, thing " + std::to_string(thing));
        const int32_t head = look == 0 ? 1 : look == 1 ? 0 : 3;
        Check(TpiGuestSpriteOf(1, 3, person) == head, "a person's head, thing " + std::to_string(thing));
        for (int32_t set = 2; set < 14; ++set) Check(TpiGuestSpriteOf(set, 3, person) == 3, "set " + std::to_string(set));
    }
    gTpiExtraGuests[0] = 0;
    Check(TpiGuestSpriteOf(0, 3, nullptr) == 3, "no extras loaded: the Mac's sprite, the person is not read");
    gTpiExtraGuests[0] = 8;
    gTpiThemeIsMod = false;
    Check(TpiGuestSpriteOf(0, 3, nullptr) == 3, "an original world: the person is not read");
    gTpiThemeIsMod = true;
    gModTpiWorlds = false;
    Check(TpiGuestSpriteOf(1, 5, nullptr) == 5, "hook off: the person is not read");
    std::printf("tpi_guests: %s\n", gFailures ? "FAILED" : "ok");
    return gFailures;
}
