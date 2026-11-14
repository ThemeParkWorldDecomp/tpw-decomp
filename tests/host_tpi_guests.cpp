// host_tpi_guests.cpp: the TPI Worlds and Rides mod's extra visitors (include/TpiGuests.h, TPW_MOD_HOOK tpi_worlds) on
// the host, without game data: which visitors wear an extra sprite and which, that the body and the head match, that
// nothing changes without extras or for other sets, and when the mod's sprites are used. Exits nonzero on the first
// failed check's count.
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

int main() {
    using namespace tpw;
    // Eight kids and two grandparents: about one visitor in five is a grandparent, each of the two about as often, the
    // same visitor always the same one, and a kid keeps its own sprite number.
    int32_t counts[3] = {0, 0, 0};
    for (uint32_t thing = 1; thing < 0x10000; ++thing) {
        const int32_t extra = TpiGuestPick(static_cast<uint16_t>(thing), 8, 2);
        Check(extra >= -1 && extra < 2, "pick in range, thing " + std::to_string(thing));
        Check(extra == TpiGuestPick(static_cast<uint16_t>(thing), 8, 2), "same pick again, thing " + std::to_string(thing));
        counts[extra + 1]++;
        const int32_t index = static_cast<int32_t>(thing % 8);
        const int32_t body = TpiGuestSprite(0, index, static_cast<uint16_t>(thing), 8, 2, 8, 2);
        const int32_t head = TpiGuestSprite(1, index, static_cast<uint16_t>(thing), 8, 2, 8, 2);
        if (extra < 0) {
            Check(body == index && head == index, "a kid keeps its sprite, thing " + std::to_string(thing));
        } else {
            Check(body == 8 + extra && head == 8 + extra, "the extra's body and head match, thing " + std::to_string(thing));
        }
        // Heads without the extras (a set loaded without them): the kid's head stays.
        Check(TpiGuestSprite(1, index, static_cast<uint16_t>(thing), 8, 2, 8, 0) == index, "no extra heads");
        // Other sets (costumes, entertainers, staff) never change.
        for (int32_t set = 2; set < 14; ++set)
            Check(TpiGuestSprite(set, index, static_cast<uint16_t>(thing), 8, 2, 8, 2) == index, "set " + std::to_string(set));
        // No extras loaded (Theme Park World's worlds, the mod off): every visitor is the Mac's.
        Check(TpiGuestPick(static_cast<uint16_t>(thing), 8, 0) == -1, "no extras, thing " + std::to_string(thing));
        Check(TpiGuestSprite(0, index, static_cast<uint16_t>(thing), 8, 0, 8, 0) == index, "no extras, the Mac's sprite");
    }
    const int32_t total = counts[0] + counts[1] + counts[2];
    Check(counts[1] + counts[2] > total * 17 / 100 && counts[1] + counts[2] < total * 23 / 100, "about one in five");
    Check(counts[1] > total * 8 / 100 && counts[2] > total * 8 / 100, "both grandparents");
    // Visitors arrive with thing indexes in a row: the grandparents are spread among them, not in runs.
    int32_t run = 0, longest = 0;
    for (uint32_t thing = 100; thing < 1100; ++thing) {
        run = TpiGuestPick(static_cast<uint16_t>(thing), 8, 2) >= 0 ? run + 1 : 0;
        if (run > longest) longest = run;
    }
    Check(longest <= 4, "no long run of grandparents, longest " + std::to_string(longest));
    // Fewer kid sprites (the player's number of kid sprites): the extras still come after them.
    for (uint32_t thing = 1; thing < 1000; ++thing) {
        const int32_t body = TpiGuestSprite(0, 1, static_cast<uint16_t>(thing), 4, 2, 4, 2);
        Check(body == 1 || body == 4 || body == 5, "four kids: past them, thing " + std::to_string(thing));
    }
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
    // of the mod's worlds, with the counts ESP_LoadSprites keeps.
    gTpiGuestCounts[0] = 8;
    gTpiGuestCounts[1] = 8;
    gTpiExtraGuests[0] = 2;
    gTpiExtraGuests[1] = 2;
    alignas(4) uint8_t person[0x40] = {};
    for (uint32_t thing = 1; thing < 2000; ++thing) {
        const uint16_t id = static_cast<uint16_t>(thing);
        *reinterpret_cast<uint16_t*>(person) = id;
        Check(TpiThingIndex(person) == id, "thing index at +0");
        const int32_t extra = TpiGuestPick(id, 8, 2);
        const int32_t want = extra < 0 ? 3 : 8 + extra;
        Check(TpiGuestSpriteOf(0, 3, person) == want && TpiGuestSpriteOf(1, 3, person) == want,
              "a person's body and head, thing " + std::to_string(thing));
        Check(TpiGuestSpriteOf(3, 3, person) == 3, "costume heads unchanged");
    }
    gTpiThemeIsMod = false;
    Check(TpiGuestSpriteOf(0, 3, nullptr) == 3, "an original world: the person is not read");
    gTpiThemeIsMod = true;
    gModTpiWorlds = false;
    Check(TpiGuestSpriteOf(1, 5, nullptr) == 5, "hook off: the person is not read");
    std::printf("tpi_guests: kids %d, grandparents %d and %d of %d; %s\n", counts[0], counts[1], counts[2], total,
                gFailures ? "FAILED" : "ok");
    return gFailures;
}
