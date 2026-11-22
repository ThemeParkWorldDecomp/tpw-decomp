// host_tpi_umbrellas.cpp: the TPI Worlds and Rides mod's umbrellas (include/TpiUmbrellas.h, TPW_MOD_HOOK tpi_worlds) on
// the host, without game data (the values below are made up, in the converter's file's form): TpiUmbrellas.sam read back,
// the umbrella shops known only in the mod's worlds with the umbrella sprite loaded, a visitor holding one by its balloon
// life, the balloon thrown away for it as DiscardBalloonPctModifier says, its colour and each look's height, its place
// moved toward the camera, the bad weather it keeps off, and that nothing changes outside the mod's worlds or for a
// visitor with a balloon. Exits nonzero on the first failed check's count.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "TpiUmbrellas.h"

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
                                      "water:Kids:SPR_BD", "water:Kids:SPR_FA", "water:Kids:SPR_GB", "water:Kids:SPR_GK",
                                      "water:Kids:SPR_GM", "water:Kids:SPR_GR", "water:Kids:SPR_KB", "water:Kids:SPR_MO"};

// A visitor as the game lays it out where the mod reads it: the thing index at +0, the balloon life at +0x214.
struct Visitor {
    alignas(4) uint8_t bytes[0x220] = {};
    Visitor(uint16_t thing, uint32_t life) {
        std::memcpy(bytes, &thing, sizeof thing);
        std::memcpy(bytes + 0x214, &life, sizeof life);
    }
};

// A TpiUmbrellas.sam as the converter writes it: the shops with their folders as remarks, then Theme Park Inc's lines as
// its Standard.sam has them (CRLF, tabs, remarks), one row commented out, one out of range, a shop given twice.
static const char kUmbrellas[] =
    "# TPI Worlds and Rides: the world's umbrella shops and Theme Park Inc's umbrella values, from the player's copy\n"
    "UmbrellaShop\t6230\t\t# shops/brolly\n"
    "UmbrellaShop\t6240\t\t# shops/brolly2\n"
    "UmbrellaShop\t6230\t\t# shops/brolly again\n"
    "PeepInfo.UmbrellaModifier\t\t\t1\t\t\t\t# prevent one unhappiness point for bad weather\r\n"
    "PeepInfo.DiscardBalloonPctModifier\t100\r\n"
    "GenericUmbrellaModifiers.ScalingFactor\t1.25\r\n"
    "GenericUmbrellaModifiers.XAdjustCamera\t0.1\r\n"
    "GenericUmbrellaModifiers.YAdjustCamera\t0.1\r\n"
    "GenericUmbrellaModifiers.ZAdjustCamera\t0.1\r\n"
    "PersonSpriteUmbrellaModifiers[0].Y\t\t5.7\t\t# kids\r\n"
    "PersonSpriteUmbrellaModifiers[4].Y\t\t6.2\r\n"
    "PersonSpriteUmbrellaModifiers[5].Y\t\t6.5\r\n"
    "#PersonSpriteUmbrellaModifiers[8].Y\t\t9.9\r\n"
    "PersonSpriteUmbrellaModifiers[9].Y\t\t5.6\r\n"
    "PersonSpriteUmbrellaModifiers[12].Y\t\t7.0\r\n";

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int main() {
    using namespace tpw;

    // The file read back.
    TpiUmbrellaTable t;
    Check(TpiParseUmbrellas(kUmbrellas, sizeof kUmbrellas - 1, t), "the converter's file reads");
    Check(t.loaded && t.shopCount == 2 && t.shops[0] == 6230 && t.shops[1] == 6240, "two shops, each once");
    Check(t.modifier == 1 && t.discardPct == 100 && Near(t.scale, 1.25f), "UmbrellaModifier, DiscardBalloonPct, ScalingFactor");
    Check(Near(t.adjust[0], 0.1f) && Near(t.adjust[1], 0.1f) && Near(t.adjust[2], 0.1f), "the camera adjustment");
    Check(t.ySet == ((1u << 0) | (1u << 4) | (1u << 5) | (1u << 9)) && Near(t.y[4], 6.2f) && Near(t.y[9], 5.6f) && t.y[8] == 0.0f,
          "the rows given, a remark and a row out of range not");
    {
        TpiUmbrellaTable u;
        const char noShop[] = "PeepInfo.UmbrellaModifier 1\nGenericUmbrellaModifiers.ScalingFactor 1.25\n";
        Check(!TpiParseUmbrellas(noShop, sizeof noShop - 1, u) && !u.loaded, "no shop: not loaded");
        const char badShop[] = "UmbrellaShop 0\nUmbrellaShop 65535\nUmbrellaShop 12.5\nUmbrellaShop x\n";
        Check(!TpiParseUmbrellas(badShop, sizeof badShop - 1, u), "shop ids out of range: not loaded");
        Check(!TpiParseUmbrellas("", 0, u) && !TpiParseUmbrellas(nullptr, 0, u), "an empty file: not loaded");
        const char defaults[] = "UmbrellaShop 7230\n";
        Check(TpiParseUmbrellas(defaults, sizeof defaults - 1, u) && u.discardPct == 100 && u.scale == 1.0f && u.modifier == 0,
              "a shop alone: the defaults");
    }

    // Each look's row: Theme Park Inc's looks their own, Theme Park World's kids row 0.
    Check(TpiUmbrellaRowOf("water:Kids:SPR_FA") == 4 && TpiUmbrellaRowOf("spr_mo") == 5 && TpiUmbrellaRowOf("SPR_GM") == 8 &&
              TpiUmbrellaRowOf("water:Kids:SPR_GR") == 9 && TpiUmbrellaRowOf("SPR_KB") == 3,
          "Theme Park Inc's looks' rows");
    Check(TpiUmbrellaRowOf("generic:kids:SPR_BE") == 0 && TpiUmbrellaRowOf("nothing") == 0, "Theme Park World's kids: row 0");

    // Outside the mod's worlds: nothing is an umbrella shop, no visitor holds one, whatever the table says.
    gTpiUmbrellas = t;
    gTpiUmbrellaSprites = 1;
    gModTpiWorlds = false;
    gTpiThemeIsMod = true;
    const Visitor holder(7, kTpiUmbrellaLife), balloon(8, 200), none(9, 0);
    Check(!TpiUmbrellasInPlay() && !TpiIsUmbrellaShop(6230) && !TpiHoldsUmbrella(holder.bytes), "mod off: nothing");
    gModTpiWorlds = true;
    gTpiThemeIsMod = false;
    Check(!TpiUmbrellasInPlay() && !TpiIsUmbrellaShop(6230), "a Theme Park World world: nothing");
    gTpiThemeIsMod = true;
    gTpiUmbrellaSprites = 0;
    Check(!TpiUmbrellasInPlay() && !TpiIsUmbrellaShop(6230), "no umbrella sprite loaded: nothing");

    // In play.
    gTpiUmbrellaSprites = 1;
    Check(TpiUmbrellasInPlay() && TpiIsUmbrellaShop(6230) && TpiIsUmbrellaShop(6240) && !TpiIsUmbrellaShop(6250),
          "in play: the world's shops");
    Check(TpiHoldsUmbrella(holder.bytes) && !TpiHoldsUmbrella(balloon.bytes) && !TpiHoldsUmbrella(none.bytes) &&
              !TpiHoldsUmbrella(nullptr),
          "a holder by its balloon life");
    Check(kTpiUmbrellaLife > 255, "the umbrella's life is no balloon's (25 to 255)");

    // What a visitor gets from the shop: an umbrella with nothing held, or one held already; a balloon thrown away for it
    // DiscardBalloonPct times in 100, the same each time for the same visitor.
    Check(TpiUmbrellaLifeAfter(0, 1, 100) == kTpiUmbrellaLife && TpiUmbrellaLifeAfter(kTpiUmbrellaLife, 1, 0) == kTpiUmbrellaLife,
          "nothing held, or an umbrella: an umbrella");
    int32_t swapped = 0, keptAll = 0, swappedAll = 0;
    bool same = true;
    for (uint16_t thing = 0; thing < 10000; ++thing) {
        swapped += TpiUmbrellaLifeAfter(200, thing, 30) == kTpiUmbrellaLife;
        keptAll += TpiUmbrellaLifeAfter(200, thing, 0) == 200;
        swappedAll += TpiUmbrellaLifeAfter(25, thing, 100) == kTpiUmbrellaLife;
        same = same && TpiUmbrellaLifeAfter(90, thing, 50) == TpiUmbrellaLifeAfter(90, thing, 50);
    }
    Check(swappedAll == 10000 && keptAll == 10000, "DiscardBalloonPct 100 always swaps, 0 never");
    Check(swapped > 2700 && swapped < 3300, "DiscardBalloonPct 30: about 30 in 100 (" + std::to_string(swapped) + ")");
    Check(same, "the same visitor the same way each time");

    // The colour: one of the sprite's actions, fixed for the visitor, every colour used.
    int32_t colours[3] = {};
    for (uint16_t thing = 0; thing < 3000; ++thing) {
        const int32_t c = TpiUmbrellaColour(thing, 3);
        if (c >= 0 && c < 3) ++colours[c];
    }
    Check(colours[0] > 800 && colours[1] > 800 && colours[2] > 800 && colours[0] + colours[1] + colours[2] == 3000,
          "the three colours alike");
    Check(TpiUmbrellaColour(5, 0) == 0 && TpiUmbrellaColour(5, 1) == 0 && TpiUmbrellaColour(77, 3) == TpiUmbrellaColour(77, 3),
          "no actions or one: colour 0; the same each time");

    // Each look's height: its row's Y, row 0's for a row not given, a kid's 5.7 with no rows at all.
    TpiSetLooks(kBodies, 16, kBodies, 16);
    TpiSetUmbrellaRows(kBodies, 16);
    Check(gTpiUmbrellaRow[0] == 0 && gTpiUmbrellaRow[9] == 4 && gTpiUmbrellaRow[12] == 8 && gTpiUmbrellaRow[15] == 5,
          "the kids set's rows");
    Check(Near(TpiUmbrellaHeight(9, gTpiUmbrellaRow, 16, t), 6.2f * kTpiUmbrellaHeightUnit) &&
              Near(TpiUmbrellaHeight(15, gTpiUmbrellaRow, 16, t), 6.5f * kTpiUmbrellaHeightUnit) &&
              Near(TpiUmbrellaHeight(13, gTpiUmbrellaRow, 16, t), 5.6f * kTpiUmbrellaHeightUnit),
          "the parents' and grandparents' own heights");
    Check(Near(TpiUmbrellaHeight(12, gTpiUmbrellaRow, 16, t), 5.7f * kTpiUmbrellaHeightUnit) &&
              Near(TpiUmbrellaHeight(0, gTpiUmbrellaRow, 16, t), 5.7f * kTpiUmbrellaHeightUnit) &&
              Near(TpiUmbrellaHeight(-1, gTpiUmbrellaRow, 16, t), 5.7f * kTpiUmbrellaHeightUnit) &&
              Near(TpiUmbrellaHeight(40, gTpiUmbrellaRow, 16, t), 5.7f * kTpiUmbrellaHeightUnit),
          "a row not given or a look not known: row 0's");
    Check(Near(TpiUmbrellaHeight(9, gTpiUmbrellaRow, 16, TpiUmbrellaTable{}), 5.7f * kTpiUmbrellaHeightUnit),
          "no rows: a kid's 5.7");
    // Above a balloon's string end (gPeepBalloonBase 1.0, the hand), as the canopy is over the head.
    Check(TpiUmbrellaHeight(0, gTpiUmbrellaRow, 16, t) > 2.0f && TpiUmbrellaHeight(15, gTpiUmbrellaRow, 16, t) < 3.5f,
          "a kid's and the tall woman's heights over the head");

    // The place: moved toward the camera by the adjustment along each axis, not moved with no camera.
    float at[3];
    const float camera[3] = {13.0f, 2.0f, 4.0f};   // 3, 0, 4 from the visitor at 10, 2, 0: length 5
    TpiUmbrellaPlace(10.0f, 2.0f, 0.0f, camera, t, at);
    Check(Near(at[0], 10.0f + 0.1f * 3.0f / 5.0f) && Near(at[1], 2.0f) && Near(at[2], 0.1f * 4.0f / 5.0f), "moved toward the camera");
    TpiUmbrellaPlace(10.0f, 2.0f, 0.0f, nullptr, t, at);
    Check(at[0] == 10.0f && at[1] == 2.0f && at[2] == 0.0f, "no camera: not moved");
    TpiUmbrellaPlace(13.0f, 2.0f, 4.0f, camera, t, at);
    Check(at[0] == 13.0f && at[1] == 2.0f && at[2] == 4.0f, "at the camera: not moved");

    // The weather it keeps off: rain, a storm or snow takes `modifier` points less, never turned into a gain; a heatwave,
    // no weather or a gain as it is.
    Check(Near(TpiUmbrellaWeatherEffect(-1.5f, kTpiRain, 1), -0.5f) && Near(TpiUmbrellaWeatherEffect(-4.0f, kTpiSnow, 1), -3.0f) &&
              Near(TpiUmbrellaWeatherEffect(-2.0f, kTpiStorm, 1), -1.0f),
          "rain, snow and storms: one point less");
    Check(TpiUmbrellaWeatherEffect(-0.5f, kTpiRain, 1) == 0.0f, "never a gain");
    Check(TpiUmbrellaWeatherEffect(-1.0f, kTpiHeatwave, 1) == -1.0f && TpiUmbrellaWeatherEffect(-1.0f, kTpiNoWeather, 1) == -1.0f &&
              TpiUmbrellaWeatherEffect(2.0f, kTpiRain, 1) == 2.0f && TpiUmbrellaWeatherEffect(-1.0f, kTpiRain, 0) == -1.0f,
          "a heatwave, no weather, a gain, no modifier: as it is");

    if (gFailures != 0) std::printf("%d checks failed\n", gFailures);
    else std::printf("all umbrella checks passed\n");
    return gFailures;
}
