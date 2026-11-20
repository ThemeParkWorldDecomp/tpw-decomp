// The real ground placement check over the hidden hoarding beside the park gate, without game data or a window. Gate posts
// at (43, 17) and (51, 17), as in all three of the TPI mod's worlds: a path, queue or building may stand over a hidden post
// or block on the gate ground Theme Park Inc's hoardings.map owns, and nowhere else; the fence stays for land buying. Also
// checks the original worlds, the hook off, unowned land, the protected entrance path, hidden terrain, a gate cell the
// level marks as the gate's own, track, a loaded park's occupied ground, demolition, and the gate ground a load gives.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "World.h"
#include "WorldSavvy.h"
#if TPW_HOST_TPI_GATE_BUILD_FIXED
#include "TpiGateBuild.h"
#endif

static int gFailures = 0, gStandIns = 0, gChecks = 0;
void tpw_stub_called(const char* name, bool, bool first) {
    if (first) std::printf("unexpected stand-in: %s\n", name);
    ++gStandIns;
}

static void Check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++gFailures;
    }
}

static int Index(int x, int y) { return x + (y << 7); }

// A hoarding post at (x, y) and its three block cells (type 0xc, parent the post), as LLCC_DrawInitialHoarding leaves one.
static void Post(int x, int y) {
    const int post = Index(x, y);
    CTrackCell& fence = gGameWorld->mTrackCells[post];
    fence.mType = static_cast<CELL_MODES>(0x19);
    fence.mHoardingNeighbours = 0x44;
    fence.mNeighbours = 0x44;
    fence.mDirection = 4;
    for (int i : {post + 1, post + 128, post + 129}) {
        CTrackCell& cell = gGameWorld->mTrackCells[i];
        cell.mType = static_cast<CELL_MODES>(0xc);
        cell.mParentID = post + 1;
    }
}

int main() {
    // CWorld's cell arrays, as a level or save has filled them, without constructing the unrelated game systems.
    auto bytes = std::make_unique<unsigned char[]>(sizeof(CWorld));
    std::memset(bytes.get(), 0, sizeof(CWorld));
    gGameWorld = reinterpret_cast<CWorld*>(bytes.get());
    gWorldMap = gGameWorld->GetMap();
    for (int i = 0; i < 0x4000; ++i) {
        gGameWorld->mMapCells[i].mCellId = i + 1;
        gGameWorld->mTrackCells[i].mCellId = i + 1;
    }
    gLLCCDB4C = &gGameWorld->mMapCells[0];
    gTrackCostFree = 1;
    gActionRecorder.mRecording = 1;
    gHoardingLeft = 43;
    gHoardingTop = 17;
    gHoardingRight = 51;
    gHoardingBottom = 17;
    // Posts whose blocks are all gate ground (the road's west side, the end posts, the entrance rows' outer posts), one
    // whose block is not (row 11, before the gate ground) and one on a park's far edge.
    const int gatePosts[][2] = {{41, 13}, {41, 15}, {53, 15}, {43, 17}, {51, 17}, {41, 17}, {53, 17}};
    const int otherPosts[][2] = {{41, 11}, {53, 11}, {23, 27}};
    for (const auto& p : gatePosts) Post(p[0], p[1]);
    for (const auto& p : otherPosts) Post(p[0], p[1]);
    auto query = [&](CMapCell& cell, int mode) {
        gLLCCC200 = gLLCCC204 = gLLCCC208 = gLLCCC210 = gLLCCC214 = 0;
        return cell.ConnectionType(static_cast<CELL_MODES>(mode), 0);
    };
    auto active = [](const char* theme, bool on) {
#if TPW_HOST_TPI_GATE_BUILD_FIXED
        gModTpiWorlds = on;
        tpw::TpiNoteTheme(theme);
#else
        (void)theme;
        (void)on;
#endif
    };
    for (const char* theme : {"arabian", "water", "science", "jungle"}) {
        for (bool on : {false, true}) {
            active(theme, on);
            for (int list = 0; list < 2; ++list) {
                bool allowed = false;
#if TPW_HOST_TPI_GATE_BUILD_FIXED
                allowed = list == 0 && on && std::strcmp(theme, "jungle") != 0;
#endif
                const int count = list == 0 ? 7 : 3;
                for (int p = 0; p < count; ++p) {
                    const int px = list == 0 ? gatePosts[p][0] : otherPosts[p][0];
                    const int py = list == 0 ? gatePosts[p][1] : otherPosts[p][1];
                    for (int index : {Index(px, py), Index(px + 1, py), Index(px, py + 1), Index(px + 1, py + 1)}) {
                        CMapCell& cell = gGameWorld->mMapCells[index];
                        const std::string tag = std::string(theme) + (on ? " on" : " off") + " cell (" +
                                                std::to_string(index & 0x7f) + "," + std::to_string(index >> 7) + ")";
                        for (int mode : {1, 3, 4, 9, 10, 0x18}) {
                            cell.mType = static_cast<CELL_MODES>(0);
                            cell.mFlags = 0;
                            Check((query(cell, mode) == 0) == allowed, tag + ": owned ground, mode " + std::to_string(mode));
                            cell.mStatusFlags = 1;
                            Check(query(cell, mode) == 1, tag + ": a cell the level marks as the gate's stays refused");
                            cell.mStatusFlags = 0;
                            cell.mFlags = 0x40;
                            Check(query(cell, mode) == 1, tag + ": unowned ground still refused");
                            cell.mFlags = 0;
                            cell.mType = static_cast<CELL_MODES>(7);
                            Check(query(cell, mode) == 1, tag + ": hidden terrain still refused");
                        }
                        cell.mType = static_cast<CELL_MODES>(1);
                        cell.mFlags = 0x20;
                        Check(query(cell, 4) == 1 && query(cell, 3) == 1, tag + ": the protected entrance path stays protected");
                        cell.mType = static_cast<CELL_MODES>(0);
                        cell.mFlags = 0;
                        for (int mode : {0xb, 0x10, 0x15, 0x16})
                            Check(query(cell, mode) == 1, tag + ": track and pylons still refused");
                    }
                }
            }
        }
    }
    active("arabian", true);
#if TPW_HOST_TPI_GATE_BUILD_FIXED
    // The gate ground, cell for cell, as hoardings.map has it in all three worlds: both entrance rows from x 41 to 54,
    // and x 41..43 and 52..54 on rows 13..16 (the road between, x 44..51, stays outside).
    int ground = 0;
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            const bool expected = ((y == 17 || y == 18) && x >= 41 && x <= 54) ||
                                  (y >= 13 && y <= 16 && ((x >= 41 && x <= 43) || (x >= 52 && x <= 54)));
            ground += tpw::TpiGateGround(x, y, 43, 51, 17);
            if (tpw::TpiGateGround(x, y, 43, 51, 17) != expected)
                Check(false, "gate ground: cell (" + std::to_string(x) + "," + std::to_string(y) + ")");
        }
    Check(ground == 2 * 14 + 4 * 6, "gate ground: 52 cells, got " + std::to_string(ground));
    // A park saved before the fix: the gate ground partly unowned, beside other unowned land. A load gives exactly the
    // gate ground and keeps every other flag; a second load, an original world, the hook off and extents not yet known
    // change nothing.
    for (int y = 8; y <= 22; ++y)
        for (int x = 36; x <= 60; ++x) gGameWorld->mMapCells[Index(x, y)].mFlags = (x + y) % 3 == 0 ? 0x400 : 0x440;
    int unowned = 0;
    for (int y = 8; y <= 22; ++y)
        for (int x = 36; x <= 60; ++x)
            unowned += tpw::TpiGateGround(x, y, 43, 51, 17) && (gGameWorld->mMapCells[Index(x, y)].mFlags & 0x40) != 0;
    active("jungle", true);
    Check(tpw::TpiOwnGateGround(gGameWorld->mMapCells, 43, 51, 17) == 0, "an original world's land is unchanged");
    active("arabian", false);
    Check(tpw::TpiOwnGateGround(gGameWorld->mMapCells, 43, 51, 17) == 0, "the hook off leaves the land unchanged");
    active("arabian", true);
    Check(tpw::TpiOwnGateGround(gGameWorld->mMapCells, -1, -1, -1) == 0, "no extents yet, no land given");
    Check(tpw::TpiOwnGateGround(gGameWorld->mMapCells, 43, 51, 17) == unowned, "a load gives the unowned gate ground");
    for (int y = 8; y <= 22; ++y)
        for (int x = 36; x <= 60; ++x) {
            const uint16_t flags = gGameWorld->mMapCells[Index(x, y)].mFlags;
            const bool gate = tpw::TpiGateGround(x, y, 43, 51, 17);
            if (gate ? flags != 0x400 : flags != ((x + y) % 3 == 0 ? 0x400 : 0x440))
                Check(false, "load: cell (" + std::to_string(x) + "," + std::to_string(y) + ") flags " + std::to_string(flags));
        }
    Check(tpw::TpiOwnGateGround(gGameWorld->mMapCells, 43, 51, 17) == 0, "a second load changes nothing");
    for (int y = 8; y <= 22; ++y)
        for (int x = 36; x <= 60; ++x) gGameWorld->mMapCells[Index(x, y)].mFlags = 0;
#endif
    const int post = Index(41, 15);
    CTrackCell& fence = gGameWorld->mTrackCells[post];
    CMapCell& cell = gGameWorld->mMapCells[post + 1];
    cell.mType = static_cast<CELL_MODES>(0);
    cell.mFlags = 0;
    // A corner's track shape must not block the ground either, and no check may change the fence's topology.
    fence.mNeighbours = 5;
    CTrackCell before = fence;
#if TPW_HOST_TPI_GATE_BUILD_FIXED
    Check(query(cell, 4) == 0, "a hidden corner permits a building");
    Check(cell.ModifyTypeBody(static_cast<CELL_MODES>(4)) == 1, "the permitted building changes the actual map cell");
    Check(cell.mType == 4 && std::memcmp(&before, &fence, sizeof fence) == 0, "building keeps the fence post intact");
    Check(gGameWorld->mTrackCells[post + 1].mParentID == post + 1, "building keeps the fence block's parent");
    // LLCR_DeleteRide's track step after clearing a ground object. The real hoarding resolver sees the post through
    // each block; no one of the four demolished ground cells may delete the fence underneath it.
    for (int index : {post, post + 1, post + 128, post + 129}) {
        CMapCell& ground = gGameWorld->mMapCells[index];
        ground.mType = static_cast<CELL_MODES>(0);
        Check(ground.IsHoarding() != 0, "the real resolver finds the fence beneath the cleared ground");
        auto* track = ground.GetTrackCellEquiv();
        if (!tpw::TpiKeepGroundHoarding(*track, gGameWorld->mTrackCells)) track->Delete(0, 0, true);
        Check(std::memcmp(&before, &fence, sizeof fence) == 0, "demolishing ground keeps the fence post intact");
    }
    active("jungle", true);
    Check(!tpw::TpiKeepGroundHoarding(fence, gGameWorld->mTrackCells), "an original world's demolition is the Mac's");
    active("arabian", true);
    cell.mParentID = cell.mCellId;
    // A save restores occupied ground and the fence independently. A loaded queue still refuses a building.
    cell.mType = static_cast<CELL_MODES>(3);
    Check(query(cell, 4) == 1, "a loaded queue still occupies its ground");
#else
    Check(query(cell, 4) == 1, "the original refuses the corner");
#endif
    cell.mType = static_cast<CELL_MODES>(0);
    cell.mParentID = 0;
    fence.mType = static_cast<CELL_MODES>(0xb);
    Check(query(cell, 4) == 1, "real ride track still refuses a building");
#if TPW_HOST_TPI_GATE_BUILD_FIXED
    Check(!tpw::TpiKeepGroundHoarding(fence, gGameWorld->mTrackCells), "demolition still deletes real ride track");
#endif
    Check(gStandIns == 0, "no stand-in was reached");
    std::printf("tpi_gate_build: %d checks, %d failures, %d stand-ins\n", gChecks, gFailures, gStandIns);
    return gFailures != 0;
}
