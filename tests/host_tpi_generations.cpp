// host_tpi_generations.cpp: the TPI Worlds and Rides mod's three generations of visitors (include/TpiGenerations.h,
// TPW_MOD_HOOK tpi_worlds) on the host, without game data (the values below are made up, in Theme Park Inc's file's
// form): the converter's TpiPeeps.sam read back, a visitor's type always of its look's generation and the same each time,
// each type of a generation as often as the others, the starting cash as the Mac works it out, the speed and preferred
// excitement read from the type, the kind of weather, the days the weather is felt on and its effect, and that nothing
// is read outside the mod's worlds. Exits nonzero on the first failed check's count.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "TpiGenerations.h"
#include "TpiGuests.h"

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

// A visitor as the game lays it out where the mod reads it: the thing index at +0, the sprite number at +0x20 and its
// set at +0x24.
struct Visitor {
    alignas(4) uint8_t bytes[0x40] = {};
    Visitor(uint16_t thing, int32_t sprite, int32_t set) {
        std::memcpy(bytes, &thing, sizeof thing);
        std::memcpy(bytes + 0x20, &sprite, sizeof sprite);
        std::memcpy(bytes + 0x24, &set, sizeof set);
    }
};

// A TpiPeeps.sam as the converter writes it (made-up values): line ends of the player's copy (CRLF), tabs and remarks
// after the values, a group the game does not read, two kids, two parents and three grandparents.
static const char kPeeps[] =
    "# TPI Worlds and Rides: Theme Park Inc's visitor types and weather effects, from the player's copy\n"
    "PeepInfo.StartingCashVarPc\t\t\t12\t\t\t% possible varience\r\n"
    "PeepInfo.WeatherModifierFrequency\t3\tModifiers get applied once every X days\r\n"
    "PeepHappinessWeatherEffects[0].Heatwave\t2.0\r\n"
    "PeepHappinessWeatherEffects[0].Rain\t-1.5\r\n"
    "PeepHappinessWeatherEffects[2].Heatwave\t-1.0\r\n"
    "PeepHappinessWeatherEffects[2].Snow\t-4.0\r\n"
    "PeepThirstWeatherEffects[2].Heatwave \t 3.0\r\n"
    "PeepHungerWeatherEffects[1].Storm \t-2.5\r\n"
    "PeepHungerWeatherEffects[1].Fog \t9.0\r\n"
    "PeepTypes[0].PreferredExcitement.StartingCash.BoredomThreshold.Generation\t80\t300\t40\t0\r\n"
    "PeepTypes[0].SpeedMultiplier 1.0\r\n"
    "PeepTypes[1].PreferredExcitement.StartingCash.BoredomThreshold.Generation\t70\t200\t45\t0\r\n"
    "PeepTypes[1].SpeedMultiplier 1.0\r\n"
    "PeepTypes[2].PreferredExcitement.StartingCash.BoredomThreshold.Generation\t50\t900\t50\t1\r\n"
    "PeepTypes[2].SpeedMultiplier 0.9\r\n"
    "PeepTypes[3].PreferredExcitement.StartingCash.BoredomThreshold.Generation\t40\t1100\t55\t1\r\n"
    "PeepTypes[3].SpeedMultiplier 0.85\r\n"
    "PeepTypes[4].PreferredExcitement.StartingCash.BoredomThreshold.Generation\t30\t150\t60\t2\r\n"
    "PeepTypes[4].SpeedMultiplier 0.4\r\n"
    "PeepTypes[5].PreferredExcitement.StartingCash.BoredomThreshold.Generation\t20\t250\t65\t2\r\n"
    "PeepTypes[5].SpeedMultiplier 0.38\r\n"
    "PeepTypes[6].PreferredExcitement.StartingCash.BoredomThreshold.Generation\t3\t350\t70\t2\r\n"
    "PeepTypes[6].SpeedMultiplier 0.36\r\n"
    "WeatherEffects.QualityForHeatWaveLow\t\t\t\t\t92\r\n";

// The Mac's starting cash written plainly: base * (draw mod (2 * var + 1) - var + 100) / 100, toward zero, at least 0.
static int32_t PlainCash(int32_t base, int32_t draw, int32_t var) {
    const int64_t rem = draw % (2 * var + 1);
    const int64_t cash = static_cast<int64_t>(base) * (rem - var + 100) / 100;
    return cash < 0 ? 0 : static_cast<int32_t>(cash);
}

int main() {
    using namespace tpw;

    // The file read back.
    TpiPeepTable t;
    Check(TpiParsePeeps(kPeeps, sizeof kPeeps - 1, t), "the converter's file reads");
    Check(t.loaded && t.typeCount == 7, "seven types");
    Check(t.cashVarPc == 12 && t.weatherEvery == 3 && t.heatwaveLow == 92, "PeepInfo and the heatwave quality");
    Check(t.types[3].excitement == 40 && t.types[3].cash == 1100 && t.types[3].boredom == 55 && t.types[3].generation == 1,
          "a type's four integers");
    Check(std::fabs(t.types[6].speed - 0.36f) < 1e-6f && t.types[0].speed == 1.0f, "a type's speed");
    Check(t.genCount[0] == 2 && t.genCount[1] == 2 && t.genCount[2] == 3, "the types by generation");
    Check(t.byGen[2][0] == 4 && t.byGen[2][2] == 6 && t.byGen[1][1] == 3, "each generation's types in order");
    Check(t.happiness[0][kTpiHeatwave] == 2.0f && t.happiness[0][kTpiRain] == -1.5f && t.happiness[2][kTpiSnow] == -4.0f,
          "the happiness table");
    Check(t.thirst[2][kTpiHeatwave] == 3.0f && t.hunger[1][kTpiStorm] == -2.5f && t.hunger[1][kTpiRain] == 0.0f,
          "the thirst and hunger tables");
    // Incomplete files are not loaded: a type without its speed, a generation with no type, nothing.
    {
        std::string noSpeed = kPeeps;
        noSpeed.erase(noSpeed.find("PeepTypes[5].SpeedMultiplier"), std::strlen("PeepTypes[5].SpeedMultiplier 0.38\r\n"));
        TpiPeepTable u;
        Check(!TpiParsePeeps(noSpeed.data(), noSpeed.size(), u) && !u.loaded, "a type without its speed: not loaded");
        const char noParents[] =
            "PeepTypes[0].PreferredExcitement.StartingCash.BoredomThreshold.Generation 1 2 3 0\n"
            "PeepTypes[0].SpeedMultiplier 1\n"
            "PeepTypes[1].PreferredExcitement.StartingCash.BoredomThreshold.Generation 1 2 3 2\n"
            "PeepTypes[1].SpeedMultiplier 0.5\n";
        Check(!TpiParsePeeps(noParents, sizeof noParents - 1, u), "no parents' type: not loaded");
        const char badGeneration[] =
            "PeepTypes[0].PreferredExcitement.StartingCash.BoredomThreshold.Generation 1 2 3 3\nPeepTypes[0].SpeedMultiplier 1\n";
        Check(!TpiParsePeeps(badGeneration, sizeof badGeneration - 1, u), "a fourth generation: not loaded");
        Check(!TpiParsePeeps("", 0, u) && !TpiParsePeeps("# nothing\n", 10, u), "an empty file: not loaded");
        const char shortLine[] = "PeepTypes[0].PreferredExcitement.StartingCash.BoredomThreshold.Generation 1 2 3\n";
        Check(!TpiParsePeeps(shortLine, sizeof shortLine - 1, u), "a line short of values: not loaded");
    }

    // The mod's world in play, its looks loaded with the extra visitors.
    TpiSetLooks(kBodies, 16, kBodies, 16);
    gModTpiWorlds = true;
    gTpiThemeIsMod = true;
    gTpiExtraGuests[0] = 8;
    gTpiPeeps = t;
    Check(TpiGenerationsInPlay(), "in play in a mod world with the table");

    // Each visitor's type: of its look's generation, the same each time, each of the generation's types alike.
    int32_t perType[kTpiMaxPeepTypes] = {};
    int32_t perGeneration[3] = {};
    bool sameGeneration = true, same = true;
    const int32_t kVisitors = 30000;
    for (int32_t i = 0; i < kVisitors; ++i) {
        const uint16_t thing = static_cast<uint16_t>(i * 7 + 3);
        const Visitor v(thing, 0, 0);
        const int32_t look = TpiGuestLook(thing, gTpiLookCount, gTpiLookPick);
        const int32_t type = TpiVisitorType(v.bytes);
        if (type < 0 || type >= t.typeCount) {
            sameGeneration = false;
            continue;
        }
        if (t.types[type].generation != gTpiLookKind[look] || TpiGuestGeneration(v.bytes) != gTpiLookKind[look])
            sameGeneration = false;
        if (TpiVisitorType(v.bytes) != type || TpiPeepTypeOf(thing, gTpiLookKind[look], t) != type) same = false;
        ++perType[type];
        ++perGeneration[t.types[type].generation];
    }
    Check(sameGeneration, "every visitor's type is of its look's generation");
    Check(same, "a visitor's type is the same each time it is asked");
    Check(std::abs(perGeneration[0] * 100 / kVisitors - 70) <= 2 && std::abs(perGeneration[1] * 100 / kVisitors - 20) <= 2 &&
              std::abs(perGeneration[2] * 100 / kVisitors - 10) <= 2,
          "the generations 70/20/10, the looks' share");
    for (int32_t g = 0; g < 3; ++g)
        for (int32_t k = 0; k < t.genCount[g]; ++k) {
            const int32_t type = t.byGen[g][k];
            const int32_t want = perGeneration[g] / t.genCount[g];
            Check(std::abs(perType[type] - want) * 10 <= want, "type " + std::to_string(type) + " about as often as the others");
        }
    // A grandparent's speed, excitement and cash are its type's; a kid's speed is 1.
    {
        int32_t kid = -1, grandparent = -1;
        for (uint16_t thing = 1; thing < 2000 && (kid < 0 || grandparent < 0); ++thing) {
            const Visitor v(thing, 0, 0);
            const int32_t g = TpiGuestGeneration(v.bytes);
            if (g == kTpiKid && kid < 0) kid = TpiVisitorType(v.bytes);
            if (g == kTpiGrandparent && grandparent < 0) grandparent = TpiVisitorType(v.bytes);
        }
        Check(kid >= 0 && kid <= 1 && t.types[kid].speed == 1.0f, "a kid's type walks at the kids' speed");
        Check(grandparent >= 4 && grandparent <= 6 && t.types[grandparent].speed < 0.5f, "a grandparent's type walks slower");
        Check(grandparent >= 4 && t.types[grandparent].excitement <= 30, "a grandparent's type likes gentler rides");
    }
    // Without extra visitors loaded every look is one of the kids set's own by its sprite number: Theme Park World's kids.
    gTpiExtraGuests[0] = 0;
    {
        bool kids = true;
        for (int32_t s = 0; s < 8; ++s) {
            const Visitor v(static_cast<uint16_t>(100 + s), s, 0);
            const int32_t type = TpiVisitorType(v.bytes);
            kids = kids && TpiGuestGeneration(v.bytes) == kTpiKid && type >= 0 && t.types[type].generation == kTpiKid;
        }
        Check(kids, "Theme Park World's kid looks get kids' types");
        const Visitor father(5, 9, 0), grandmother(6, 12, 0), other(7, 12, 1);
        Check(TpiGuestGeneration(father.bytes) == kTpiParent && TpiGuestGeneration(grandmother.bytes) == kTpiGrandparent,
              "a look by its sprite number");
        Check(TpiGuestGeneration(other.bytes) == kTpiKid, "a sprite of another set: a kid");
    }
    gTpiExtraGuests[0] = 8;

    // The starting cash: the Mac's arithmetic, the spread the file's, from the same draw.
    {
        bool plain = true;
        int32_t low = 1 << 30, high = 0;
        for (int64_t draw = -40000; draw <= 40000; draw += 7) {
            const int32_t d = static_cast<int32_t>(draw * 53717);
            const int32_t cash = TpiStartingCash(250, d, 12);
            plain = plain && cash == PlainCash(250, d, 12);
            if (d >= 0) {
                low = cash < low ? cash : low;
                high = cash > high ? cash : high;
            }
        }
        Check(plain, "the starting cash as the Mac works it out");
        Check(low == 220 && high == 280, "the starting cash within 12% either way");
        Check(TpiStartingCash(800, 15, 15) == 800 && TpiStartingCash(800, 0, 15) == 680 && TpiStartingCash(800, 30, 15) == 920,
              "the draw's remainder sets the spread");
        Check(TpiStartingCash(100, -1, 15) == 84 && TpiStartingCash(0, 12345, 15) == 0, "a negative draw and no cash");
    }

    // The kind of weather.
    Check(TpiWeatherKind(10, 50, 0, 1, 95) == kTpiStorm, "lightning: a storm");
    Check(TpiWeatherKind(10, 50, 1, 0, 95) == kTpiSnow && TpiWeatherKind(30, 50, 0, 0, 95) == kTpiRain, "snow and rain");
    Check(TpiWeatherKind(96, 0, 0, 0, 95) == kTpiHeatwave && TpiWeatherKind(95, 0, 0, 0, 95) == kTpiHeatwave, "a heatwave");
    Check(TpiWeatherKind(94, 0, 0, 0, 95) == kTpiNoWeather && TpiWeatherKind(100, 0, 0, 0, 101) == kTpiNoWeather,
          "fair weather, and no heatwave quality given");

    // The days: one count across months, years and a leap day; the weather felt every N days.
    {
        bool steps = true;
        int32_t previous = TpiParkDayNumber(99, 0, 1) - 1;
        const int32_t monthDays[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};   // 2000 is a leap year
        for (int32_t year = 99; year <= 101; ++year)
            for (int32_t month = 0; month < 12; ++month) {
                int32_t days = monthDays[month];
                if (month == 1 && year != 100) days = 28;
                for (int32_t day = 1; day <= days; ++day) {
                    const int32_t n = TpiParkDayNumber(year, month, day);
                    steps = steps && n == previous + 1;
                    previous = n;
                }
            }
        Check(steps, "each park day one more than the last");
        int32_t due = 0;
        for (int32_t n = 1000; n < 1030; ++n) due += TpiWeatherDue(n, 3);
        Check(due == 10 && !TpiWeatherDue(999, 0), "the weather felt every third day, never with no frequency");
    }

    // The weather's effect: the generation's values, kept within 0 to 100.
    {
        float happiness = 50.0f, thirst = 20.0f, hunger = 30.0f;
        TpiApplyWeather(happiness, thirst, hunger, kTpiGrandparent, kTpiHeatwave, t);
        Check(happiness == 49.0f && thirst == 23.0f && hunger == 30.0f, "a grandparent in a heatwave");
        TpiApplyWeather(happiness, thirst, hunger, kTpiKid, kTpiHeatwave, t);
        Check(happiness == 51.0f && thirst == 23.0f, "a kid in a heatwave");
        TpiApplyWeather(happiness, thirst, hunger, kTpiParent, kTpiStorm, t);
        Check(hunger == 27.5f, "a parent in a storm");
        float low = 1.0f, high = 99.0f, mid = 50.0f;
        TpiApplyWeather(low, high, mid, kTpiGrandparent, kTpiSnow, t);
        Check(low == 0.0f, "happiness kept at 0 or more");
        float h = 99.5f, th = 99.0f, hu = 0.0f;
        TpiApplyWeather(h, th, hu, kTpiKid, kTpiHeatwave, t);
        Check(h == 100.0f, "happiness kept at 100 or less");
        float a = 40.0f, b = 40.0f, c = 40.0f;
        TpiApplyWeather(a, b, c, kTpiKid, kTpiNoWeather, t);
        TpiApplyWeather(a, b, c, 3, kTpiRain, t);
        Check(a == 40.0f && b == 40.0f && c == 40.0f, "no weather or no generation: nothing");
    }

    // Nothing outside the mod's worlds: the mod off, an original world, or the world's file not read.
    const Visitor v(1234, 0, 0);
    Check(TpiVisitorType(v.bytes) >= 0, "a type in play");
    gModTpiWorlds = false;
    Check(!TpiGenerationsInPlay() && TpiVisitorType(v.bytes) == -1, "the mod off: no type");
    gModTpiWorlds = true;
    gTpiThemeIsMod = false;
    Check(!TpiGenerationsInPlay() && TpiVisitorType(v.bytes) == -1, "an original world: no type");
    gTpiThemeIsMod = true;
    gTpiPeeps = TpiPeepTable{};
    Check(!TpiGenerationsInPlay() && TpiVisitorType(v.bytes) == -1, "no file read: no type");
    Check(TpiPeepTypeOf(1234, kTpiKid, gTpiPeeps) == -1 && TpiPeepTypeOf(1234, 3, t) == -1, "no table or no generation: no type");

    std::printf("tpi_generations: %s\n", gFailures ? "FAILED" : "ok");
    return gFailures;
}
