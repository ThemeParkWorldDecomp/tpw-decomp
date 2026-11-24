// host_tpi_umbrellas.cpp: the TPI Worlds and Rides mod's umbrellas (include/TpiUmbrellas.h, TPW_MOD_HOOK tpi_worlds) on
// the host, without game data (the values below are made up, in the converter's file's form): TpiUmbrellas.sam read back,
// the umbrella shops known only in the mod's worlds with the umbrella sprite loaded, a visitor holding one by its balloon
// life, the balloon thrown away for it as DiscardBalloonPctModifier says, its colour and each look's height, its place
// moved toward the camera, its place in the hand in first person (the FP lines, each facing's hand place, and a file from
// before the FP lines placed as in third person), the bad weather it keeps off, and that nothing changes outside the
// mod's worlds or for a visitor with a balloon. Exits nonzero on the first failed check's count.
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

// The same file with Theme Park Inc's first-person lines as its Standard.sam has them: the kids' row 0, the fat bloke's
// row 4 and the old bloke's row 9 whole, row 5 short of its FPHandPosDistance, row 8 with a field commented out, row 3 a
// field out of range, and a field not known.
static const char kUmbrellasFp[] =
    "UmbrellaShop\t6230\t\t# shops/brolly\n"
    "PersonSpriteUmbrellaModifiers[0].Y\t\t\t\t\t5.7\t\t# the child\r\n"
    "PersonSpriteUmbrellaModifiers[0].FPY\t\t\t\t2.0\r\n"
    "PersonSpriteUmbrellaModifiers[0].FPCentreOffsetX\t0.2\r\n"
    "PersonSpriteUmbrellaModifiers[0].FPCentreOffsetZ\t0.0\r\n"
    "PersonSpriteUmbrellaModifiers[0].FPHandPosAngle\t\t60\r\n"
    "PersonSpriteUmbrellaModifiers[0].FPHandPosDistance\t0.8\r\n"
    "PersonSpriteUmbrellaModifiers[4].Y\t6.2\n"
    "PersonSpriteUmbrellaModifiers[4].FPY\t3.0\n"
    "PersonSpriteUmbrellaModifiers[4].FPCentreOffsetX\t0.2\n"
    "PersonSpriteUmbrellaModifiers[4].FPCentreOffsetZ\t0.0\n"
    "PersonSpriteUmbrellaModifiers[4].FPHandPosAngle\t65\n"
    "PersonSpriteUmbrellaModifiers[4].FPHandPosDistance\t1.2\n"
    "PersonSpriteUmbrellaModifiers[5].FPY\t3.0\n"
    "PersonSpriteUmbrellaModifiers[5].FPCentreOffsetX\t0.2\n"
    "PersonSpriteUmbrellaModifiers[5].FPCentreOffsetZ\t0.0\n"
    "PersonSpriteUmbrellaModifiers[5].FPHandPosAngle\t45\n"
    "PersonSpriteUmbrellaModifiers[8].FPY\t2.0\n"
    "PersonSpriteUmbrellaModifiers[8].FPCentreOffsetX\t0.2\n"
    "#PersonSpriteUmbrellaModifiers[8].FPCentreOffsetZ\t0.0\n"
    "PersonSpriteUmbrellaModifiers[8].FPHandPosAngle\t55\n"
    "PersonSpriteUmbrellaModifiers[8].FPHandPosDistance\t0.9\n"
    "PersonSpriteUmbrellaModifiers[3].FPY\t2.0\n"
    "PersonSpriteUmbrellaModifiers[3].FPCentreOffsetX\t0.2\n"
    "PersonSpriteUmbrellaModifiers[3].FPCentreOffsetZ\t0.0\n"
    "PersonSpriteUmbrellaModifiers[3].FPHandPosAngle\t60\n"
    "PersonSpriteUmbrellaModifiers[3].FPHandPosDistance\t-0.8\n"
    "PersonSpriteUmbrellaModifiers[9].FPY 2.2\n"
    "PersonSpriteUmbrellaModifiers[9].FPCentreOffsetX 0.2\n"
    "PersonSpriteUmbrellaModifiers[9].FPCentreOffsetZ 0.0\n"
    "PersonSpriteUmbrellaModifiers[9].FPHandPosAngle 45\n"
    "PersonSpriteUmbrellaModifiers[9].FPHandPosDistance 1.0\n"
    "PersonSpriteUmbrellaModifiers[9].FPWobble 3.0\n"
    "PersonSpriteUmbrellaModifiers[10].FPY 2.0\n";

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

    // The FP lines read back: each row's five fields, rows short of one not whole, a field out of range or not known passed over.
    TpiUmbrellaTable fp;
    Check(TpiParseUmbrellas(kUmbrellasFp, sizeof kUmbrellasFp - 1, fp), "the file with FP lines reads");
    Check(fp.fpSet[0] == kTpiUmbrellaFpAll && fp.fpSet[4] == kTpiUmbrellaFpAll && fp.fpSet[9] == kTpiUmbrellaFpAll,
          "rows 0, 4 and 9 whole");
    Check(Near(fp.fpY[0], 2.0f) && Near(fp.fpCentre[0][0], 0.2f) && Near(fp.fpCentre[0][1], 0.0f) && Near(fp.fpAngle[0], 60.0f) &&
              Near(fp.fpDistance[0], 0.8f) && Near(fp.fpY[4], 3.0f) && Near(fp.fpAngle[4], 65.0f) && Near(fp.fpDistance[4], 1.2f) &&
              Near(fp.fpY[9], 2.2f),
          "the FP values");
    Check(fp.fpSet[5] == (kTpiUmbrellaFpAll & ~kTpiUmbrellaFpDistance) &&
              fp.fpSet[8] == (kTpiUmbrellaFpAll & ~kTpiUmbrellaFpCentreZ) &&
              fp.fpSet[3] == (kTpiUmbrellaFpAll & ~kTpiUmbrellaFpDistance) && fp.fpSet[1] == 0,
          "a field missing, commented out or out of range: the row not whole");
    Check(fp.ySet == ((1u << 0) | (1u << 4)) && Near(fp.y[0], 5.7f) && Near(fp.y[4], 6.2f), "Y beside the FP lines");
    // The old file (no FP lines): no row has any FP field, so no hand place.
    bool noFp = true;
    for (int32_t r = 0; r < kTpiUmbrellaRows; ++r) noFp = noFp && t.fpSet[r] == 0;
    Check(noFp, "a file from before the FP lines: no FP fields");

    // Each facing's hand place, worked in true radians: FPCentreOffsetX + sin(a) * distance, FPCentreOffsetZ + cos(a) *
    // distance, a = facing * 45 + FPHandPosAngle.
    bool hands = true;
    for (int32_t f = 0; f < kTpiUmbrellaFacings; ++f) {
        const double a0 = (45.0 * f + 60.0) * 3.14159265358979323846 / 180.0, a4 = (45.0 * f + 65.0) * 3.14159265358979323846 / 180.0;
        hands = hands && Near(fp.hand[0][f][0], 0.2f + static_cast<float>(std::sin(a0)) * 0.8f) &&
                Near(fp.hand[0][f][1], static_cast<float>(std::cos(a0)) * 0.8f) &&
                Near(fp.hand[4][f][0], 0.2f + static_cast<float>(std::sin(a4)) * 1.2f) &&
                Near(fp.hand[4][f][1], static_cast<float>(std::cos(a4)) * 1.2f);
        if (!hands) std::printf("facing %d: %f %f\n", f, fp.hand[0][f][0], fp.hand[0][f][1]);
    }
    Check(hands, "the eight facings' hand places, rows 0 and 4");
    // Some by hand: facing 0 at 60 degrees, sin 0.866 and cos 0.5; facing 6 (270) at 330 degrees, sin -0.5 and cos 0.866.
    Check(Near(fp.hand[0][0][0], 0.2f + 0.8660254f * 0.8f) && Near(fp.hand[0][0][1], 0.5f * 0.8f) &&
              Near(fp.hand[0][6][0], 0.2f - 0.5f * 0.8f) && Near(fp.hand[0][6][1], 0.8660254f * 0.8f),
          "the hand place at 60 and 330 degrees");
    // Not Theme Park Inc's whole radians: 60 degrees is 1.047 radians, which it cuts to 1 (sin 0.841, not 0.866).
    Check(std::fabs(fp.hand[0][0][0] - (0.2f + 0.84147f * 0.8f)) > 0.01f, "the true radians, not cut to whole ones");
    // The angle taken modulo 360, below 0 as well.
    float h[2], g[2];
    TpiUmbrellaHandAt(315.0f, 0.2f, 0.0f, 60.0f, 0.8f, h);
    TpiUmbrellaHandAt(0.0f, 0.2f, 0.0f, 15.0f, 0.8f, g);
    Check(Near(h[0], g[0]) && Near(h[1], g[1]), "315 + 60 is 15");
    TpiUmbrellaHandAt(0.0f, 0.0f, 0.1f, -90.0f, 1.0f, h);
    Check(Near(h[0], -1.0f) && Near(h[1], 0.1f), "-90 is 270");
    Check(fp.hand[5][0][0] == 0.0f && fp.hand[8][3][1] == 0.0f, "rows not whole: no hand place");

    // The facing from the facing word: eighths of a turn of 2048, rounded.
    Check(TpiUmbrellaFacing(0) == 4 && TpiUmbrellaFacing(0x37f) == 7 && TpiUmbrellaFacing(0x380) == 0 && TpiUmbrellaFacing(0x400) == 0 &&
              TpiUmbrellaFacing(0x47f) == 0 && TpiUmbrellaFacing(0x480) == 1 && TpiUmbrellaFacing(0x7ff) == 4,
          "the facing from the facing word");

    // The hand place: the visitor's place plus its row's hand place for its facing, FPY high, not moved toward the camera.
    Check(TpiUmbrellaHandPlace(10.0f, 20.0f, 9, 2, gTpiUmbrellaRow, 16, fp, at), "the fat bloke: a hand place");
    Check(Near(at[0], 10.0f + fp.hand[4][2][0] * kTpiUmbrellaFpUnit) && Near(at[1], 3.0f * kTpiUmbrellaFpUnit) &&
              Near(at[2], 20.0f + fp.hand[4][2][1] * kTpiUmbrellaFpUnit),
          "the fat bloke's hand, facing 2");
    Check(TpiUmbrellaHandPlace(10.0f, 20.0f, 13, 7, gTpiUmbrellaRow, 16, fp, at) && Near(at[1], 2.2f * kTpiUmbrellaFpUnit) &&
              Near(at[0], 10.0f + fp.hand[9][7][0] * kTpiUmbrellaFpUnit),
          "the old bloke's own row");
    Check(TpiUmbrellaHandPlace(10.0f, 20.0f, 15, 1, gTpiUmbrellaRow, 16, fp, at) && Near(at[1], 2.0f * kTpiUmbrellaFpUnit) &&
              Near(at[2], 20.0f + fp.hand[0][1][1] * kTpiUmbrellaFpUnit),
          "the tall woman's row not whole: row 0's");
    Check(TpiUmbrellaHandPlace(10.0f, 20.0f, -1, 9, gTpiUmbrellaRow, 16, fp, at) && Near(at[0], 10.0f + fp.hand[0][1][0] * kTpiUmbrellaFpUnit),
          "a look not known: row 0's; facing taken modulo 8");
    // Off to one side of the visitor, under its overhead height.
    const float side = std::sqrt((at[0] - 10.0f) * (at[0] - 10.0f) + (at[2] - 20.0f) * (at[2] - 20.0f));
    Check(side > 0.1f && at[1] < TpiUmbrellaHeight(0, gTpiUmbrellaRow, 16, fp), "to one side, at the hand");
    // A file from before the FP lines: no hand place, out not touched, so the third-person place is taken.
    at[0] = at[1] = at[2] = -7.0f;
    Check(!TpiUmbrellaHandPlace(10.0f, 20.0f, 9, 2, gTpiUmbrellaRow, 16, t, at) && at[0] == -7.0f && at[1] == -7.0f && at[2] == -7.0f,
          "the old file: no hand place");
    Check(!TpiUmbrellaHandPlace(10.0f, 20.0f, 0, 0, gTpiUmbrellaRow, 16, TpiUmbrellaTable{}, at), "no table: no hand place");

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
