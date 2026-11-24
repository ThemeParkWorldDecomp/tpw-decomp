// TPW_MOD_HOOK tpi_worlds: synthetic two-storey terrain, fixed floor route and saved roof paths. No game data.
#include <array>
#include <cstdio>
#include "TpiTunnelTop.h"
#include "TpiFootbridge.h"
int main() {
    using namespace tpw;
    int checks = 0, failures = 0;
    auto check = [&](bool ok) { ++checks; if (!ok) { ++failures; std::printf("FAIL check %d\n", checks); } };
    TpiTunnelTop top;
    check(TpiParseTunnelTop("tpitop 1 1 6 6 2 2 4 4 20 0ff0 ffff", top));
    for (const char* bad : {"tunneltop", "tpitop 1 1 6 6 2 2 4 4 nan 0", "tpitop 1 1 6 6 0 2 4 4 20 0",
                           "tpitop 0 0 128 128 0 0 128 128 20 0", "tpitop 1 1 6 6 2 2 4 4 20 0 extra"})
        check(!TpiParseTunnelTop(bad, top));
    std::array<float, 81> floor{}, live{};
    floor.fill(0); live = floor;
    check(!TpiTunnelTopHeights(top, false, 8, 8, live.data(), floor.data()));
    check(live == floor); // before purchase, including the slopes and tunnel mouths
    check(TpiTunnelTopHeights(top, true, 8, 8, live.data(), floor.data()));
    for (int y = 0; y <= 8; ++y) for (int x = 0; x <= 8; ++x) {
        check(live[y * 9 + x] == (x >= 2 && x <= 6 && y >= 2 && y <= 6 ? 20 : 0));
        check(floor[y * 9 + x] == 0); // shared corners never overwrite the saved floor source
    }
    check(!TpiTunnelTopHeights(top, true, 8, 8, live.data(), floor.data())); // second load/tick
    check(TpiTunnelTopHeights(top, false, 8, 8, live.data(), floor.data()));
    check(live == floor); // another park loaded in the same level, with no purchased land
    auto lip = top;
    lip.raised = 0x777f; // full first row, three columns elsewhere: keep the canyon corner outside the patch
    check(TpiTunnelTopHeights(lip, true, 8, 8, live.data(), floor.data()));
    check(live[4 * 9 + 6] == 0); // the excluded outer corner stays at canyon height
    check(live[2 * 9 + 6] == 20); // the adjoining mouth lip is raised with the roof
    check(!TpiOnTunnelTop(lip, 5, 3));
    check(TpiTunnelTopWasHidden(lip, 5, 3)); // hidden floor flags remain independent of the raised surface mask
    live[4 * 9 + 6] = 7; // a corner of the rectangle off the top, land the park reshaped
    check(!TpiTunnelTopHeights(lip, true, 8, 8, live.data(), floor.data()));
    check(live[4 * 9 + 6] == 7);
    check(TpiTunnelTopHeights(lip, false, 8, 8, live.data(), floor.data()));
    check(live[4 * 9 + 6] == 7 && live[2 * 9 + 6] == 0);
    live[4 * 9 + 6] = 0;
    // The lighting's slopes on the cleared top: its edge over the canyon is lit flat, the canyon and the interior as before.
    std::array<float, 81> lit{};
    check(TpiTunnelTopHeights(top, true, 8, 8, lit.data(), floor.data()));
    const auto slopes = [&](const TpiTunnelTop& t, int x, int y, float& sx, float& sz) {
        const float* p = lit.data() + y * 9 + x;
        sx = p[-1] - p[1]; sz = p[-9] - p[9];
        TpiTunnelTopSlopes(t, x, y, 9, 9, p, sx, sz);
    };
    float sx = 0, sz = 0;
    slopes(top, 6, 4, sx, sz); check(sx == 0 && sz == 0); // east edge, beside the canyon (20 - 0 without the fix)
    slopes(top, 4, 2, sx, sz); check(sx == 0 && sz == 0); // north edge
    slopes(top, 2, 2, sx, sz); check(sx == 0 && sz == 0); // a corner
    slopes(top, 4, 4, sx, sz); check(sx == 0 && sz == 0); // inside
    slopes(top, 7, 4, sx, sz); check(sx == 20 && sz == 0); // the canyon's point beside the top keeps its slope
    slopes(top, 4, 1, sx, sz); check(sx == 0 && sz == -20);
    lit = floor;
    check(TpiTunnelTopHeights(lip, true, 8, 8, lit.data(), floor.data()));
    slopes(lip, 6, 4, sx, sz); check(sx == 20 && sz == 20); // a point off the top, the excluded canyon corner, as before
    slopes(lip, 6, 3, sx, sz); check(sx == 0 && sz == 0); // the top's corner beside it
    gModTpiWorlds = true; TpiNoteTheme("arabian");
    gTpiTunnelTop = top; gTpiTunnelTopField = live.data(); gTpiTunnelTopOpen = true;
    for (int y = 1; y < 7; ++y) for (int x = 1; x < 7; ++x) {
        const auto id = uint16_t(1 + x + y * 128);
        check(TpiTunnelFloorHeight(live.data(), x, y, 0x1e));
        check(!TpiTunnelFloorHeight(live.data(), x, y, 1));
        check(TpiTunnelTopLevel(id) == (TpiOnTunnelTop(top, x, y) ? 1 : 0)); // no route cell covered here
        check(TpiTunnelTopLevel(id, 0) == TpiTunnelTopLevel(id));
        check(TpiTunnelTopWasHidden(top, x, y) == (x >= 2 && x < 6 && y >= 3 && y < 5));
    }
    const TpiFootbridgeCell under{0x1e, 0}, roof{1, 0, 1}, mouth{1, 0};
    check(!TpiFootbridgeStep(under, roof)); check(!TpiFootbridgeStep(roof, under));
    check(TpiFootbridgeStep(under, mouth)); check(TpiFootbridgeStep(mouth, under));
    for (int nav : {0, 1, 2, 3}) {
        check(TpiFootbridgeWall(under, roof, nav)); check(TpiFootbridgeWall(roof, under, nav));
        check(!TpiFootbridgeWall(under, mouth, nav));
    }
    for (const char* theme : {"jungle", "hallow", "space", "fantasy"}) {
        TpiNoteTheme(theme);
        check(!TpiTunnelFloorHeight(live.data(), 3, 3, 0x1e));
        check(TpiTunnelTopLevel(uint16_t(1 + 3 + 3 * 128)) == 0);
    }
    TpiNoteTheme("arabian"); gModTpiWorlds = false;
    check(!TpiTunnelFloorHeight(live.data(), 3, 3, 0x1e));
    check(TpiTunnelTopLevel(uint16_t(1 + 3 + 3 * 128)) == 0);
    std::printf("tpi_tunnel_top: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
