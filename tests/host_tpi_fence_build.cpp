// The real placement checks on the cells the level's own fences, walls and banks stand on, without game data or a window.
// A band laid as Polar Zone's wooden fence lies in its Base.map (attribute 1 on x 64 and 65, rows 21..41, the fence on
// the line between them) on owned ground: in the TPI mod's worlds every placement that builds (path, queue, building,
// track) refuses a cell of the band and a track cell touching it, and the open ground either side keeps the Mac's answer.
// Also checks the other attributes with bit 1, the walkable ones, the modes that do not build (demolishing, queue editing,
// the land tool, the hoarding), the original worlds and the hook off.
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
#if TPW_HOST_TPI_FENCE_BUILD_FIXED
#include "TpiFenceBuild.h"
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

static void ClearWhy() { gLLCCC200 = gLLCCC204 = gLLCCC208 = gLLCCC210 = gLLCCC214 = 0; }

int main() {
    // CWorld's cell arrays, as a level or save has filled them, without constructing the unrelated game systems.
    auto bytes = std::make_unique<unsigned char[]>(sizeof(CWorld));
    std::memset(bytes.get(), 0, sizeof(CWorld));
    gGameWorld = reinterpret_cast<CWorld*>(bytes.get());
    gWorld = gGameWorld;   // the map cells' neighbours (CMapCell::GetNeighbouringCell) are found from it
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
    for (int y = 21; y <= 41; ++y) {
        gGameWorld->mMapCells[Index(64, y)].mStatusFlags = 1;
        gGameWorld->mMapCells[Index(65, y)].mStatusFlags = 1;
    }

    auto active = [&](const char* theme, bool on) {
#if TPW_HOST_TPI_FENCE_BUILD_FIXED
        gModTpiWorlds = on;
        tpw::TpiNoteTheme(theme);
#else
        (void)theme;
        (void)on;
#endif
    };
    // The answer with the hook off: what the Mac answers, whatever the theme.
    auto withMac = [&](auto&& ask) {
#if TPW_HOST_TPI_FENCE_BUILD_FIXED
        const bool saved = gModTpiWorlds;
        gModTpiWorlds = false;
        const int answer = ask();
        gModTpiWorlds = saved;
        return answer;
#else
        return ask();
#endif
    };
    auto mapAsk = [&](int x, int y, int mode) {
        ClearWhy();
        return gGameWorld->mMapCells[Index(x, y)].ConnectionType(static_cast<CELL_MODES>(mode), 0);
    };
    auto trackAsk = [&](int x, int y, int mode) {
        ClearWhy();
        return gGameWorld->mTrackCells[Index(x, y)].ConnectionType(static_cast<CELL_MODES>(mode), 0, 0);
    };

#if TPW_HOST_TPI_FENCE_BUILD_FIXED
    for (int mode = 0; mode < 0x100; ++mode) {
        const bool builds = mode == 1 || mode == 3 || mode == 4 || mode == 9 || mode == 10 || mode == 0xb || mode == 0xe ||
                            mode == 0x10 || mode == 0x13 || mode == 0x15 || mode == 0x16 || mode == 0x18 || mode == 0x1a;
        Check(tpw::TpiBuildMode(mode) == builds, "build mode " + std::to_string(mode));
    }
#endif

    const int groundModes[] = {1, 0x18, 3, 4, 9, 10};
    const int otherMapModes[] = {0xb, 0x10, 0x15, 0x16, 0x14, 0x32, 0x33, 0x34, 0x35, 0x36, 0x86, 0x19, 0x39};
    const int trackModes[] = {0xb, 0x10, 0xe, 0x13, 0x1a};
    const int otherTrackModes[] = {0x19, 0x39, 0x32, 0x86};
    for (const char* theme : {"arabian", "water", "science", "jungle"}) {
        for (bool on : {false, true}) {
            active(theme, on);
            const bool refuses =
#if TPW_HOST_TPI_FENCE_BUILD_FIXED
                on && std::strcmp(theme, "jungle") != 0;
#else
                false;
#endif
            const std::string tag0 = std::string(theme) + (on ? " on" : " off");
            for (int y : {21, 30, 41}) {
                // The fence band and the open ground either side of it, on owned plain terrain.
                for (int x : {63, 64, 65, 66}) {
                    const bool band = x == 64 || x == 65;
                    const std::string tag = tag0 + " cell (" + std::to_string(x) + "," + std::to_string(y) + ")";
                    for (int mode : groundModes) {
                        const int mac = withMac([&] { return mapAsk(x, y, mode); });
                        Check(mac == 0, tag + ": the Mac builds on owned plain ground, mode " + std::to_string(mode));
                        Check(mapAsk(x, y, mode) == (band && refuses ? 1 : mac),
                              tag + (band ? ": the fence refuses mode " : ": open ground allows mode ") + std::to_string(mode));
                    }
                    for (int mode : otherMapModes) {
                        const int mac = withMac([&] { return mapAsk(x, y, mode); });
                        const bool builds = mode == 0xb || mode == 0x10 || mode == 0x15 || mode == 0x16;
                        Check(mapAsk(x, y, mode) == (band && refuses && builds ? 1 : mac),
                              tag + ": map mode " + std::to_string(mode) + " as expected");
                    }
                }
                // Track cells: the one west of the band touches x 64, the band's own two and the open side's do not
                // reach past them.
                for (int x : {62, 63, 64, 65, 66}) {
                    const bool touches = x == 63 || x == 64 || x == 65;
                    const std::string tag = tag0 + " track (" + std::to_string(x) + "," + std::to_string(y) + ")";
                    for (int mode : trackModes) {
                        const int mac = withMac([&] { return trackAsk(x, y, mode); });
                        Check(trackAsk(x, y, mode) == (touches && refuses ? 1 : mac),
                              tag + ": track mode " + std::to_string(mode) + " as expected");
                        // The Mac refuses 0xe and 0x13 on plain ground too, so only the other modes check the open side.
                        if (!touches && mode != 0xe && mode != 0x13)
                            Check(mac == 0, tag + ": the Mac lays track on owned plain ground, mode " + std::to_string(mode));
                    }
                    for (int mode : otherTrackModes) {
                        const int mac = withMac([&] { return trackAsk(x, y, mode); });
                        Check(trackAsk(x, y, mode) == mac, tag + ": track mode " + std::to_string(mode) + " is the Mac's");
                    }
                }
            }
            // The other attributes: bit 1 with others refuses, bit 1 with the walkable bit and no bit 1 do not.
            for (int attribute : {3, 0x11, 0x91, 0x95, 0x80, 0x10, 2, 8}) {
                CMapCell& cell = gGameWorld->mMapCells[Index(30, 30)];
                cell.mStatusFlags = static_cast<uint8_t>(attribute);
                const bool fence = (attribute & 1) != 0 && (attribute & 0x80) == 0;
                for (int mode : groundModes) {
                    const int mac = withMac([&] { return mapAsk(30, 30, mode); });
                    Check(mapAsk(30, 30, mode) == (fence && refuses ? 1 : mac),
                          tag0 + ": attribute " + std::to_string(attribute) + ", mode " + std::to_string(mode));
                }
                cell.mStatusFlags = 0;
            }
            // Unowned and occupied ground are refused as before; a band cell that holds a path (a park built before the
            // fix) keeps it, and nothing is changed by asking.
            CMapCell& band = gGameWorld->mMapCells[Index(64, 30)];
            band.mFlags = 0x40;
            Check(mapAsk(64, 30, 4) == 1, tag0 + ": unowned band ground still refused");
            band.mFlags = 0;
            band.mType = static_cast<CELL_MODES>(1);
            const CMapCell before = band;
            mapAsk(64, 30, 4);
            mapAsk(64, 30, 0x32);
            Check(std::memcmp(&before, &band, sizeof band) == 0, tag0 + ": asking changes no cell");
            Check(band.mType == 1, tag0 + ": an existing path on the band stays");
            band.mType = static_cast<CELL_MODES>(0);
        }
    }
    Check(gStandIns == 0, "no stand-in was reached");
    std::printf("tpi_fence_build: %d checks, %d failures, %d stand-ins\n", gChecks, gFailures, gStandIns);
    return gFailures != 0;
}
