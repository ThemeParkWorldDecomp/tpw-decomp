// Real map placement, both height lookups and roof repair, on synthetic terrain with no game data or window.
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include "TpiTunnelTop.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "HFieldTextures.h"
#include "LevelSystems.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "World.h"
#include "WorldSavvy.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
float DetermineHFieldHeight(float x, float z, int32_t flag);
namespace {
int checks = 0, failures = 0, stubs = 0;
void Check(bool ok, const char* why) {
    ++checks; if (!ok) { ++failures; std::printf("FAIL: %s\n", why); }
}
}
void tpw_stub_called(const char* name, bool, bool first) {
    ++stubs; if (first) std::printf("unexpected stand-in: %s\n", name);
}
int TpiGroundColourTest();
int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--ground-colour") == 0) {
        const int result = TpiGroundColourTest();
        std::printf("tpi_ground_colour: %d stand-ins\n", stubs);
        return result || stubs ? 1 : 0;
    }
    auto bytes = std::make_unique<unsigned char[]>(sizeof(CWorld));
    std::memset(bytes.get(), 0, sizeof(CWorld));
    gGameWorld = reinterpret_cast<CWorld*>(bytes.get());
    gWorld = gGameWorld;
    gWorldMap = gGameWorld->GetMap();
    gLLCCDB4C = &gGameWorld->mMapCells[0];
    gTrackCostFree = 1;
    gActionRecorder.mRecording = 1;
    struct Fixture {
        std::array<float, 81> live{}, original{};
        std::array<sHFieldQuad, 64> quads{}, saved{};
        sHField field{}, copy{};
        tag_sNode node{};
        tag_sM3D2Header header{};
        char name[96]{};
    };
    auto* fixture = new (tpw::ArenaAllocZeroed(sizeof(Fixture))) Fixture{};
    auto& live = fixture->live; auto& original = fixture->original;
    auto& quads = fixture->quads; auto& saved = fixture->saved;
    auto& field = fixture->field; auto& copy = fixture->copy;
    field.mF18[0] = copy.mF18[0] = 8; field.mF18[1] = copy.mF18[1] = 8;
    field.mF08[2] = field.mF08[3] = copy.mF08[2] = copy.mF08[3] = 10;
    field.mHeights = live.data(); copy.mHeights = original.data();
    field.mQuads = quads.data(); copy.mQuads = saved.data();
    auto& node = fixture->node;
    std::strcpy(fixture->name, "tpitop 1 1 6 6 2 2 4 4 20 ff1 ffff");
    node.mName = fixture->name;
    auto& header = fixture->header;
    header.mHField = &field; header.mNodes = &node; header.mNodeCount = 1; header.mFlags = 2;
    gHeightfieldHeader = &header; gHeightfieldCopy = &copy;
    gHeightfieldWidth = gHeightfieldHeight = 8;
    for (int i = 0; i < 0x4000; ++i) {
        auto& map = gGameWorld->mMapCells[i]; auto& track = gGameWorld->mTrackCells[i];
        map.mCellId = track.mCellId = i + 1; map.mFlags = 0x40; map.mKind = 1; track.mKind = 2;
    }
    for (int y = 3; y <= 4; ++y) for (int x = 1; x <= 6; ++x) {
        auto& cell = gGameWorld->mMapCells[x + y * 128];
        cell.mType = static_cast<CELL_MODES>(0x1e); cell.mStatusFlags = 0x80;
        quads[x + y * 8].mF00 = saved[x + y * 8].mF00 = 1;
    }
    // A roof cell an old converter hid, beside the fixed floor.
    quads[2 + 2 * 8].mF00 = saved[2 + 2 * 8].mF00 = 1;
    gGameWorld->mMapCells[2 + 2 * 128].mType = static_cast<CELL_MODES>(7);
    gGameWorld->mTrackCells[2 + 2 * 128].mType = static_cast<CELL_MODES>(7);
    gModTpiWorlds = true; tpw::TpiNoteTheme("arabian");
    TpiRepairTunnelTop();
    Check(live == original, "unbought roof keeps all original heights");
    Check(gGameWorld->mMapCells[2 + 2 * 128].mType == 7, "unbought cells stay closed");
    auto& oldPath = gGameWorld->mMapCells[3 + 2 * 128];
    oldPath.mType = static_cast<CELL_MODES>(1); oldPath.mNeighbours = 0x44;
    for (int y = 1; y <= 6; ++y) for (int x = 1; x <= 6; ++x)
        gGameWorld->mMapCells[x + y * 128].mFlags &= uint16_t(~0x40u);
    const auto originalMask = saved;
    TpiPrepareTunnelTop();
    Check(saved[2 + 2 * 8].mF00 == originalMask[2 + 2 * 8].mF00, "early object-height preparation keeps original hidden mask");
    Check(DetermineHFieldHeightQuick(25, 25) == 20, "objects rebuild at roof height before map drawing");
    TpiRepairTunnelTop();
    auto& top = gGameWorld->mMapCells[2 + 2 * 128];
    Check(top.mType == 0, "purchased roof's formerly hidden cell is normal land");
    gLLCCC200 = gLLCCC204 = gLLCCC208 = gLLCCC210 = gLLCCC214 = 0;
    Check(top.ConnectionType(static_cast<CELL_MODES>(1), 0) == 0, "real path placement accepts roof land");
    Check(oldPath.mType == 1 && oldPath.mNeighbours == 0x44, "saved invisible path and its links are kept");
    // The route: rows 3 and 4 from x 1 to 6. Its cells under the top (x 2 to 5) are the top's ground now, the route
    // kept apart (tpw::gTpiTunnelCovered); the mouths' cells (x 1 and 6) stay the level's walkway.
    for (int y = 3; y <= 4; ++y) for (int x = 1; x <= 6; ++x) {
        auto& cell = gGameWorld->mMapCells[x + y * 128];
        const bool under = x >= 2 && x <= 5;
        Check(cell.mStatusFlags == 0x80, "the route's attribute survives");
        Check(tpw::TpiTunnelCovered(uint16_t(1 + x + y * 128)) == under, "the route under the top is covered");
        if (under) {
            Check(cell.mType == 0, "a route cell under the top is the top's ground");
            Check(cell.ConnectionType(static_cast<CELL_MODES>(1), 0) == 0, "real path placement accepts the ground over the route");
            Check(DetermineHFieldHeightQuick(x * 10 + 5, y * 10 + 5) == 20, "quick lookup stands the top's ground at twenty");
            Check(DetermineHFieldHeight(x * 10 + 5, y * 10 + 5, 1) == 20, "full lookup stands the top's ground at twenty");
        } else {
            Check(cell.mType == 0x1e, "the mouths keep the walkway");
            Check(cell.ConnectionType(static_cast<CELL_MODES>(1), 0) != 0, "nothing is built in the mouths");
            Check(DetermineHFieldHeightQuick(x * 10 + 5, y * 10 + 5) == 0, "real quick lookup keeps the mouths' walkers on the floor");
            Check(DetermineHFieldHeight(x * 10 + 5, y * 10 + 5, 1) == 0, "real full lookup keeps the mouths' walkers on the floor");
        }
        Check(TpiTunnelFloorHeightQuick(x * 10 + 5, y * 10 + 5) == 0, "the route's floor stays at the level's height");
    }
    Check(DetermineHFieldHeightQuick(25, 25) == 20, "quick lookup places roof path at twenty");
    Check(DetermineHFieldHeight(25, 25, 1) == 20, "full lookup places roof path at twenty");
    const auto once = live; TpiRepairTunnelTop(); Check(live == once, "second repair is idempotent");
    Check(gGameWorld->mMapCells[3 + 3 * 128].mType == 0, "and keeps the top's ground");
    // A path over the top north to south, (3, 2) to (3, 5), across the route's cells (3, 3) and (3, 4). A walker on the
    // route carries tpw::kTpiTunnelNavBit (0x200) in the mode its callers pass; every other walker the Mac's mode.
    const NavMode surface = static_cast<NavMode>(0), route = static_cast<NavMode>(tpw::kTpiTunnelNavBit);
    for (int y = 3; y <= 5; ++y) {
        auto& cell = gGameWorld->mMapCells[3 + y * 128];
        cell.mType = static_cast<CELL_MODES>(1); cell.mNeighbours = y == 5 ? 0x01 : 0x11;
    }
    TpiRepairTunnelTop();
    for (int y = 3; y <= 5; ++y) Check(gGameWorld->mMapCells[3 + y * 128].mType == 1, "the top's path is kept");
    Check(MAP_wall_in_way(3, 2, 2, surface) == 0, "a top walker steps off the north path onto the path over the route");
    Check(MAP_wall_in_way(3, 3, 2, surface) == 0, "and along it");
    Check(MAP_wall_in_way(3, 4, 2, surface) == 0, "and onto the south end");
    Check(MAP_wall_in_way(3, 4, 0, surface) == 0, "and back north");
    Check(MAP_wall_in_way(2, 3, 3, surface) != 0 && MAP_wall_in_way(5, 4, 1, surface) != 0, "a top walker never drops into a mouth");
    Check(MAP_wall_in_way(1, 3, 1, surface) != 0 && MAP_wall_in_way(6, 4, 3, surface) != 0, "nor climbs onto the top from one");
    for (int x = 1; x <= 5; ++x)
        Check(MAP_wall_in_way(x, 3, 1, route) == 0 && MAP_wall_in_way(x + 1, 4, 3, route) == 0, "the tunnel walks mouth to mouth under the path");
    Check(MAP_wall_in_way(3, 3, 2, route) == 0, "between the route's two lanes");
    Check(MAP_wall_in_way(3, 3, 0, route) != 0 && MAP_wall_in_way(3, 4, 2, route) != 0, "a tunnel walker never climbs onto the top");
    Check(MAP_wall_in_way(1, 3, 0, route) != 0, "nor leaves the route's walkway sideways at a mouth");
    for (int y = 3; y <= 4; ++y) for (int x = 1; x <= 6; ++x) {
        Check(gGameWorld->mMapCells[x + y * 128].mStatusFlags == 0x80, "the path leaves the route's attribute as it was");
        Check(TpiTunnelFloorHeightQuick(x * 10 + 5, y * 10 + 5) == 0, "the tunnel floor keeps its height under the path");
    }
    for (int y = 3; y <= 5; ++y) {
        auto& cell = gGameWorld->mMapCells[3 + y * 128];
        cell.mType = static_cast<CELL_MODES>(0); cell.mNeighbours = 0;
    }
    TpiRepairTunnelTop();
    Check(gGameWorld->mMapCells[3 + 3 * 128].mType == 0, "a removed path leaves the top's ground");
    Check(MAP_wall_in_way(2, 3, 1, route) == 0, "and the tunnel still walks");
    // First person's walk as HandleCamera makes it: GetNextGridsquare square by square, tpw::TpiFirstPersonStep after
    // each, starting on the top's layer (ToggleFirstPersonWalkAround). `under` counts the steps it stood on the floor
    // under the top, `over` those on the top over the route.
    float pos[3] = {};
    int under = 0, over = 0;
    const auto walk = [&](float x, float z, float dx, float dz, int steps) {
        tpw::gTpiFirstPersonRoof = 1;
        pos[0] = x; pos[1] = 0; pos[2] = z; under = over = 0;
        for (int i = 0; i < steps; ++i) {
            s3DPoint step{dx, 0.0f, dz}, out{};
            int32_t more;
            do {
                more = GetNextGridsquare(reinterpret_cast<s3DPoint*>(pos), &step, &out);
                tpw::TpiFirstPersonStep(pos[0], pos[2], out.x, out.z);
                std::memcpy(pos, &out, sizeof out);
            } while (more != 0);
            if (tpw::TpiFirstPersonUnderTop(pos[0], pos[2]) && TpiTunnelFloorHeightQuick(pos[0], pos[2]) == 0) ++under;
            if (tpw::TpiTunnelCovered(tpw::TpiCellAt(pos[0], pos[2])) && !tpw::TpiFirstPersonUnderTop(pos[0], pos[2])) ++over;
        }
    };
    walk(15, 35, 2, 0, 25);
    Check(pos[0] >= 64 && under == 20 && over == 0, "first person walks in at the west mouth and through the tunnel to the east");
    walk(65, 45, -2, 0, 25);
    Check(pos[0] <= 16 && under == 20 && over == 0, "and back from the east mouth to the west");
    walk(35, 25, 0, 2, 15);
    Check(pos[2] >= 54 && under == 0 && over == 10, "first person from the top walks over the route");
    walk(25, 35, -2, 0, 5);
    Check(pos[0] >= 20 && under == 0, "and never drops from the top into a mouth");
    gModTpiWorlds = false;
    Check(DetermineHFieldHeightQuick(15, 35) != 0, "floor override is gated by the mod");
    Check(!tpw::TpiTunnelCovered(uint16_t(1 + 3 + 3 * 128)), "and so is the route under the top");
    Check(original == std::array<float, 81>{}, "level floor heights never change");
    Check(stubs == 0, "no unexpected stand-ins");
    std::printf("tpi_tunnel_top_engine: %d checks, %d failures, %d stand-ins\n", checks, failures, stubs);
    return failures || stubs ? 1 : 0;
}
