// LLCellMouse.cpp: name proven by __sinit_LLCellMouse_cpp in the main application.

#define TPW_GLOBE_INIT_REAL 1
#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "Bitmap.h"
#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "email.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "HLSprite.h"
#include "intrface.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "Main.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Quickload.h"
#include "RideDB.h"
#include "rsse.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "TrackCell.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// The cell system's mesh loaders (main app, LLCellMouse.cpp range; unit w112a): the queue, go-kart track, water ride track and
// hoarding meshes. Each loader sets the size pair LoadRideType gets, formats the theme's folder for its mesh set and loads the
// ride type of every table entry; each destroyer hands the entries' types to DestroyRideInstance (the original passes the TYPE
// number where an instance number belongs, and never clears the table). The tables are initialised data of the image; the names
// of the tables, their entries and the size pairs are invented.

namespace {

// The unique id bases of the four sets (GetUniqueID's class), 0x4268, 0x42cc, 0x4330, 0x4394.
constexpr int32_t kQIdBase = 17000;
constexpr int32_t kKTrackIdBase = 17100;
constexpr int32_t kWTrackIdBase = 17200;
constexpr int32_t kHoardingIdBase = 17300;

// LoadRideType's flag words: 0x12423 (lis 1; addi 0x2423) and 0x100000 added when the entry's queue flag byte is set; 0x10422
// for the two track sets; 0x422 for the hoardings.
constexpr uint32_t kQFlags = 0x12423;
constexpr uint32_t kQQueueFlag = 0x100000;
constexpr uint32_t kTrackFlags = 0x10422;
constexpr uint32_t kHoardingFlags = 0x422;

constexpr uint32_t kQCount = 8;
constexpr uint32_t kKTrackCount = 20;
constexpr uint32_t kWTrackCount = 25;
constexpr uint32_t kHoardingCount = 28;

// The folder text buffers: 1,024 bytes in the Q and hoarding loaders' frames, 1,036 in the track loaders' (the original's sprintf
// is unbounded; snprintf cuts instead).
constexpr uint32_t kQFolderSize = 1024;
constexpr uint32_t kTrackFolderSize = 1036;
constexpr uint32_t kHoardingFolderSize = 1024;

// LoadRideType for one table entry: the entry's name, the size pair as both size arguments, one time-control argument (1) and
// no signs (the original passes zero for the three stack arguments as well).
uint32_t LoadEntry(char* folder, const sLLCMMesh& entry, int32_t* size, uint32_t flags, uint32_t id) {
    return LoadRideType(folder, const_cast<char*>(entry.mName), size, size, flags, id, 1, nullptr, nullptr, 0, 0);
}

// The track and hoarding loaders: a NULL name copies the previous entry's type (the word 12 bytes back, so entry 0 reads the
// last entry of the table in front of it: the tables are contiguous on the Mac), a name that starts with '-' is skipped when
// `skipDash` (type 0, no call), every other name is loaded. `latch` is the directory set-up's answer: it is asked again for
// every loaded entry until it answers nonzero, and it never decides whether the entry loads.
void LoadTrackTable(sLLCMMesh* table, uint32_t count, uint32_t previousType, bool skipDash, char* folder, int32_t base,
                    uint32_t flags) {
    int32_t latch = 0;
    for (uint32_t i = 0; i < count; ++i) {
        sLLCMMesh& entry = table[i];
        if (entry.mName == nullptr) {
            entry.mRideType = i == 0 ? previousType : table[i - 1].mRideType;
        } else if (skipDash && entry.mName[0] == '-') {
            entry.mRideType = 0;
        } else {
            if (latch == 0) latch = SetupDirectoryForMeshLoad(const_cast<char*>(entry.mName), folder, nullptr);
            const uint32_t id = GetUniqueID(static_cast<eIDClass>(base), i);
            entry.mRideType = LoadEntry(folder, entry, gLLCMTrackSize, flags, id);
        }
    }
}

}  // namespace

// ---- The tables: initialised data of the image (all ride types 0; name, then the four bytes at +8) ----
sLLCMMesh gLLCMQMeshes[8] = {
    {0, "quedead.md2", 0x00, 0x01, 0x00, 0x00},
    {0, "quedead.md2", 0x00, 0x01, 0x00, 0x00},
    {0, "questra.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "quebnd2.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "quebnd1.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "queend.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "quebin1.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "quebin2.md2", 0x00, 0x00, 0x00, 0x00},
};
sLLCMMesh gLLCMKTrackMeshes[20] = {
    {0, "-", 0x00, 0x00, 0x00, 0x00},
    {0, "gk_trckQ.md2", 0x01, 0x01, 0x00, 0x00},
    {0, "gk_trckS.md2", 0x11, 0x00, 0x00, 0x00},
    {0, "gk_trckB.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "gk_trckX.md2", 0x55, 0x00, 0x00, 0x00},
    {0, nullptr, 0x15, 0x00, 0x00, 0x00},
    {0, nullptr, 0x54, 0x00, 0x00, 0x00},
    {0, nullptr, 0x51, 0x00, 0x00, 0x00},
    {0, nullptr, 0x45, 0x00, 0x00, 0x00},
    {0, "gk_trckH.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x01, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "gk_trckH_U.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x01, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "gk_trckH_D.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x01, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "gk_trckH_A.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "gk_trckB2.md2", 0x00, 0x00, 0x00, 0x00},
};
sLLCMMesh gLLCMWTrackMeshes[25] = {
    {0, "-", 0x00, 0x00, 0x00, 0x00},
    {0, "wr_trckQ.md2", 0x01, 0x01, 0x00, 0x00},
    {0, "wr_trckS.md2", 0x11, 0x00, 0x00, 0x00},
    {0, "wr_trckB.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "wr_trckX.md2", 0x55, 0x00, 0x00, 0x00},
    {0, nullptr, 0x15, 0x00, 0x00, 0x00},
    {0, nullptr, 0x54, 0x00, 0x00, 0x00},
    {0, nullptr, 0x51, 0x00, 0x00, 0x00},
    {0, nullptr, 0x45, 0x00, 0x00, 0x00},
    {0, "wr_trckH.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x01, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "wr_trckH_U.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x01, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "wr_trckH_D.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x01, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "wr_trckH_A.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "wr_trckB2.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "wr_trckX2.md2", 0x55, 0x00, 0x00, 0x00},
    {0, nullptr, 0x15, 0x00, 0x00, 0x00},
    {0, nullptr, 0x54, 0x00, 0x00, 0x00},
    {0, nullptr, 0x51, 0x00, 0x00, 0x00},
    {0, nullptr, 0x45, 0x00, 0x00, 0x00},
};
sLLCMMesh gLLCMHoardingMeshes[28] = {
    {0, "ho_fos1.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "ho_end1.md2", 0x01, 0x00, 0x00, 0x00},
    {0, "ho_fos1.md2", 0x11, 0x00, 0x00, 0x00},
    {0, "ho_fob1.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "ho_fos1.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x15, 0x00, 0x00, 0x00},
    {0, nullptr, 0x54, 0x00, 0x00, 0x00},
    {0, nullptr, 0x51, 0x00, 0x00, 0x00},
    {0, nullptr, 0x45, 0x00, 0x00, 0x00},
    {0, "ho_fos4.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x00, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "ho_fos4.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x00, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "ho_fos4.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x00, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "ho_fos4.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "ho_fob2.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "ho_fos2.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x00, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "ho_fos3.md2", 0x00, 0x00, 0x00, 0x00},
    {0, nullptr, 0x01, 0x00, 0x00, 0x00},
    {0, nullptr, 0x11, 0x00, 0x00, 0x00},
    {0, "ho_fob3.md2", 0x00, 0x00, 0x00, 0x00},
    {0, "ho_fob4.md2", 0x00, 0x00, 0x00, 0x00},
};
int32_t gLLCMQSize[2];
int32_t gLLCMTrackSize[2];
int32_t gLLCMIllegalPlacement;

// 0x10081a98 (264 bytes): the queue meshes. The size pair {1, 1} is stored before anything is called. No NULL test and no '-' test
// here: all 8 names are loaded, and the directory set-up call repeats (with the entry's own name) until it answers nonzero.
void LLCM_LoadQMeshes() {
    char folder[kQFolderSize];
    gLLCMQSize[0] = 1;
    gLLCMQSize[1] = 1;
    std::snprintf(folder, sizeof folder, "%s:queue", get_theme_path());
    int32_t latch = 0;
    for (uint32_t i = 0; i < kQCount; ++i) {
        sLLCMMesh& entry = gLLCMQMeshes[i];
        if (latch == 0) latch = SetupDirectoryForMeshLoad(const_cast<char*>(entry.mName), folder, nullptr);
        uint32_t flags = kQFlags;
        if (entry.mQueueFlag != 0) flags |= kQQueueFlag;
        const uint32_t id = GetUniqueID(static_cast<eIDClass>(kQIdBase), i);
        entry.mRideType = LoadEntry(folder, entry, gLLCMQSize, flags, id);
    }
}

// 0x10081bc8 (100 bytes): destroys the "instance" numbered like each entry's type, for every entry with a name. The types stay.
void LLCM_DestroyQMeshes() {
    for (uint32_t i = 0; i < kQCount; ++i)
        if (gLLCMQMeshes[i].mName != nullptr) DestroyRideInstance(gLLCMQMeshes[i].mRideType);
}

// The four ride-mesh instancers of the cell system (LLCellMouse.cpp of the main app; unit w120): LLCCM_CreateQMesh (0x10081c58, 224 bytes),
// LLCCM_CreateKTrackMesh (0x10081fb8, 284), LLCCM_CreateWTrackMesh (0x10082350, 364) and LLCCM_CreateHoardingMesh (0x10082720, 284).
// CMapCell::SetHFieldTile and CTrackCell::SetHFieldTile call them with restore 0 to put a queue, track or hoarding mesh on a cell, and the
// RefreshHFieldTile functions with restore 1 to find the mesh again after a load. Each answers the instance number, which the caller keeps
// in the cell. The Q instancer is the plain form; the others move the ride's corner cell for the quarter turns. All four place the mesh with
// the flags 0x33a (place from the parameters, drawing on, skirt deform, world bounding area, coaster).

namespace {

// The ride's turn: 360 less the tile's rotation (a rotation of 0 gives 0, not 360).
int32_t RideRotation(const CCellBase::CELL_IndexType& index) {
    int32_t rotation = static_cast<int32_t>(0x168u - index.mRotation);
    if (rotation == 0x168) rotation = 0;
    return rotation;
}

// The corner cell of a mesh that is turned by 90, 180 or 270 degrees moves one cell (180 moves both ways).
void ShiftForRotation(int32_t rotation, int32_t& x, int32_t& y) {
    if (rotation == 0xb4) {
        x = x + 1;
        y = y + 1;
    } else if (rotation < 0xb4) {
        if (rotation == 0x5a) y = y + 1;
    } else if (rotation == 0x10e) {
        x = x + 1;
    }
}

// Makes the instance (restore 0) or finds the restored one (restore nonzero). The parameters' last word is the address of the size pair
// (the ride's width and depth in cells), which both calls read through the record.
uint32_t PlaceMesh(int32_t x, int32_t y, int32_t rotation, const sLLCMMesh& mesh, int32_t* sizePair, int32_t restore) {
    sFixedRideParams params;
    params.mX = x;
    params.mZ = y;
    params.mRotation = rotation;
    params.mF0C = sizePair;
    if (restore != 0) {
        return RestoreRideInstance(0, mesh.mRideType, 0x33a, &params, mesh.mTimeControls, nullptr, 0, 0, 0);
    }
    return InstanceRide(mesh.mRideType, 0x33a, &params, mesh.mTimeControls, 0, nullptr, 0, 0, 0xffffffff, 0);
}

}  // namespace

// 0x10081c58 (224 bytes): no cell shift, the one-cell size pair and the queue table.
uint32_t LLCCM_CreateQMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    const int32_t rotation = RideRotation(index);
    return PlaceMesh(x, y, rotation, gLLCMQMeshes[index.mIndex], gLLCMQSize, restore);
}

// Globals of the two static initialisers that no header names.
int32_t gLLCCDB74;      // 0x102ddb74
int32_t gLLCCDB78;      // 0x102ddb78
// 0x10081d80
void LLCCM_DestroyQMesh(uint32_t mesh) { DestroyRideInstance(mesh); }

// 0x10081dd0 (284 bytes): the go-kart track meshes; the table in front of this one is the queue table.
void LLCM_LoadKTrackMeshes() {
    char folder[kTrackFolderSize];
    gLLCMTrackSize[0] = 2;
    gLLCMTrackSize[1] = 2;
    std::snprintf(folder, sizeof folder, "%s:rides:gokarts", get_theme_path());
    LoadTrackTable(gLLCMKTrackMeshes, kKTrackCount, gLLCMQMeshes[kQCount - 1].mRideType, true, folder, kKTrackIdBase,
                   kTrackFlags);
}

// 0x10081f18 (112 bytes): the same, skipping names that start with '-'.
void LLCM_DestroyKTrackMeshes() {
    for (uint32_t i = 0; i < kKTrackCount; ++i) {
        const sLLCMMesh& entry = gLLCMKTrackMeshes[i];
        if (entry.mName != nullptr && entry.mName[0] != '-') DestroyRideInstance(entry.mRideType);
    }
}

// 0x10081fb8 (284 bytes): the go-kart track table.
uint32_t LLCCM_CreateKTrackMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    const int32_t rotation = RideRotation(index);
    ShiftForRotation(rotation, x, y);
    return PlaceMesh(x, y, rotation, gLLCMKTrackMeshes[index.mIndex], gLLCMTrackSize, restore);
}

// 0x10082120
void LLCCM_DestroyKTrackMesh(uint32_t mesh) { DestroyRideInstance(mesh); }

// 0x10082174 (284 bytes): the water ride track meshes, the K loader's shape; the table in front is the go-kart table.
void LLCM_LoadWTrackMeshes() {
    char folder[kTrackFolderSize];
    gLLCMTrackSize[0] = 2;
    gLLCMTrackSize[1] = 2;
    std::snprintf(folder, sizeof folder, "%s:rides:wateride", get_theme_path());
    LoadTrackTable(gLLCMWTrackMeshes, kWTrackCount, gLLCMKTrackMeshes[kKTrackCount - 1].mRideType, true, folder, kWTrackIdBase,
                   kTrackFlags);
}

// 0x100822bc (100 bytes): the Q destroyer's shape, with no '-' test (entry 0, "-", destroys instance 0).
void LLCM_DestroyWTrackMeshes() {
    for (uint32_t i = 0; i < kWTrackCount; ++i)
        if (gLLCMWTrackMeshes[i].mName != nullptr) DestroyRideInstance(gLLCMWTrackMeshes[i].mRideType);
}

// 0x10082350 (364 bytes): the water track table; a new or restored instance (not 0) starts animation 12 of its set, looping (7 is the loop
// mode), at speed 1.
uint32_t LLCCM_CreateWTrackMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    const int32_t rotation = RideRotation(index);
    ShiftForRotation(rotation, x, y);
    const uint32_t instance = PlaceMesh(x, y, rotation, gLLCMWTrackMeshes[index.mIndex], gLLCMTrackSize, restore);
    if (instance != 0) {
        SetCurRideAnim(gRideAnimSets[instance], 0xc, 0, 7, 1.0f, 0);
    }
    return instance;
}

// 0x10082508
void LLCCM_DestroyWTrackMesh(uint32_t mesh) { DestroyRideInstance(mesh); }

// 0x1008255c (252 bytes): the hoarding meshes; like the track loaders but a name that starts with '-' is loaded too. The table
// in front is the water table.
void LLCM_LoadHoardingMeshes() {
    char folder[kHoardingFolderSize];
    gLLCMTrackSize[0] = 2;
    gLLCMTrackSize[1] = 2;
    std::snprintf(folder, sizeof folder, "%s:hoarding", get_theme_path());
    LoadTrackTable(gLLCMHoardingMeshes, kHoardingCount, gLLCMWTrackMeshes[kWTrackCount - 1].mRideType, false, folder,
                   kHoardingIdBase, kHoardingFlags);
}

// 0x10082688 (100 bytes): the same for the hoardings.
void LLCM_DestroyHoardingMeshes() {
    for (uint32_t i = 0; i < kHoardingCount; ++i)
        if (gLLCMHoardingMeshes[i].mName != nullptr) DestroyRideInstance(gLLCMHoardingMeshes[i].mRideType);
}

// 0x10082720 (284 bytes): the hoarding table.
uint32_t LLCCM_CreateHoardingMesh(int32_t x, int32_t y, CCellBase::CELL_IndexType index, int32_t restore) {
    const int32_t rotation = RideRotation(index);
    ShiftForRotation(rotation, x, y);
    return PlaceMesh(x, y, rotation, gLLCMHoardingMeshes[index.mIndex], gLLCMTrackSize, restore);
}

// 0x1008288c
void LLCCM_DestroyHoardingMesh(uint32_t mesh) { DestroyRideInstance(mesh); }

// Unit v5059 (main app, LLCellMouse.cpp on the Mac): LLCM_CoverUpgrade (0x100828e4, 888 bytes) and LLCM_RebuildUpgradeBridges
// (0x10082cf0, 1116 bytes). Both look at the four neighbours of the cell (x, y) of the map: a neighbour that is a track cell flagged
// 0x10 with the same id halfword (+0x10 of its map cell) pushes the coordinates by two cells.

namespace {

uint8_t* WorldBytes() { return reinterpret_cast<uint8_t*>(gGameWorld); }

uint16_t IdOf(CCellBase* mapCell) { return *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(mapCell) + 0x10); }

// The cell of (x, y) as the two functions find it: the track cell (this) and the map cell that holds the id.
void FindCells(int32_t x, int32_t y, CTrackCell*& track, CCellBase*& map) {
    const uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
    map = reinterpret_cast<CCellBase*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x44);
    track = map->GetTrackCellEquiv();
    if (track->IsTrackX()) {
        const uint16_t id = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(track) + 0x10);
        track = reinterpret_cast<CTrackCell*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x28 + 0x110000);
        map = reinterpret_cast<CCellBase*>(track->GetMapCellEquiv());
    }
}

// Whether the neighbour of `track` at `delta` is a flagged (0x10) track cell with the same id.
bool NeighbourMatches(CTrackCell* track, const MapDelta& delta, uint16_t id) {
    MapDelta copy = delta;
    CTrackCell* other = track->GetNeighbouringCell(copy, 2);
    if (other == nullptr) return false;
    CCellBase* otherMap = reinterpret_cast<CCellBase*>(other->GetMapCellEquiv());
    if (!other->IsFlag(0x10)) return false;
    if (IdOf(otherMap) != id) return false;
    return other->IsTrack() != 0;
}

}  // namespace

// 0x100828e4 (888 bytes)
void LLCM_CoverUpgrade(int32_t* x, int32_t* y) {
    CTrackCell* track;
    CCellBase* map;
    FindCells(*x, *y, track, map);
    if (!track->IsFlag(0x10)) return;
    const uint16_t id = IdOf(map);
    if (NeighbourMatches(track, gMapDeltaMinusY, id)) *y = *y - 2;
    if (NeighbourMatches(track, gMapDeltaPlusX, id)) *x = *x + 2;
    if (NeighbourMatches(track, gMapDeltaPlusY, id)) *y = *y + 2;
    if (NeighbourMatches(track, gMapDeltaMinusX, id)) *x = *x - 2;
}

// 0x10082cf0 (1116 bytes)
void LLCM_RebuildUpgradeBridges(int32_t x, int32_t y) {
    CTrackCell* track;
    CCellBase* map;
    int32_t dx = 0;
    int32_t dy = 0;
    const uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
    CCellBase* first = reinterpret_cast<CCellBase*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x44);
    track = first->GetTrackCellEquiv();
    map = first;
    if (track->IsTrackX()) {
        const uint16_t id = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(track) + 0x10);
        track = reinterpret_cast<CTrackCell*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x28 + 0x110000);
        map = reinterpret_cast<CCellBase*>(track->GetMapCellEquiv());
        const int32_t cellIndex = *reinterpret_cast<uint16_t*>(map);
        x = (cellIndex - 1) & 0x7f;
        y = static_cast<int32_t>(static_cast<uint32_t>(cellIndex - 1) >> 7);
    }
    if (track->IsFlag(0x10)) {
        const uint16_t id = IdOf(map);
        if (NeighbourMatches(track, gMapDeltaMinusY, id)) dy = -2;
        if (NeighbourMatches(track, gMapDeltaPlusX, id)) dx = 2;
        if (NeighbourMatches(track, gMapDeltaPlusY, id)) dy = 2;
        if (NeighbourMatches(track, gMapDeltaMinusX, id)) dx = -2;
    }
    int32_t endX = x + dx;
    int32_t endY = y + dy;
    int32_t fromX = x;
    int32_t fromY = y;
    if (x > endX) {
        endX -= 2;
        fromX += 2;
    } else if (x < endX) {
        endX += 2;
        fromX -= 2;
    }
    if (y > endY) {
        endY -= 2;
        fromY += 2;
    } else if (y < endY) {
        endY += 2;
        fromY -= 2;
    }
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x85), fromX, fromY, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x86), fromX, fromY, &endX, &endY);
    LLCC_DrawTrackLine(static_cast<CELL_MODES>(0x81), fromX, fromY, &endX, &endY);
}

// Unit v5633: LLCM_MouseMoved (0x10083180, 6228 bytes), the cell system's mouse move handler: per mode it snaps or clamps the cell
// under the mouse, draws the preview lines, places the ride preview, checks hoarding / clear land / patrol / pylon legality, then sets
// the cursor. Translated from the listing; the words it defines are new here (names invented from their Mac addresses).

uint16_t gLLCC9F34 = 0;    // 0x1029df34
int32_t gLLCC9F38 = 1;     // 0x1029df38
int32_t gLLCC9F3C = 1;     // 0x1029df3c
uint16_t gLLCC9F40 = 0;    // 0x1029df40
int32_t gLLCCC20C = 0;     // 0x102dc20c
int32_t gLLCCDB38 = 0;     // 0x102ddb38
int32_t gLLCCDB3C = 0;     // 0x102ddb3c
int32_t gLLCCDB60 = 0;     // 0x102ddb60
int32_t gLLCCDE0B4 = 0;    // 0x102de0b4
int32_t gLLCCDE0B8 = 0;    // 0x102de0b8

namespace {

uint8_t* MMWorldBytes() { return reinterpret_cast<uint8_t*>(gGameWorld); }

// The map cell / track cell of a 1-based cell id (id 0 reaches the slot before the first, as on the Mac).
CMapCell* MMMapCell(uint16_t id) {
    return reinterpret_cast<CMapCell*>(MMWorldBytes() + 0x2d8 + (static_cast<int32_t>(id) - 1) * 0x44);
}

CTrackCell* MMTrackCell(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(MMWorldBytes() + 0x1102d8 + (static_cast<int32_t>(id) - 1) * 0x28);
}

uint16_t MMCellId(int32_t x, int32_t y) { return static_cast<uint16_t>(x + (y << 7) + 1); }

int32_t MMAbs(int32_t v) { return v < 0 ? -v : v; }   // the MSL abs (wraps at INT_MIN under -fwrapv)

int32_t MMSlot(RideInfo* ride, int32_t slot) { return ride->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }

RideInfo* MMRide(uint16_t type) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
    id.mIndex = type;
    return gRideDatabase.GetRideInfo(id);
}

CELL_MODES MMPreview(int32_t mode) { return static_cast<CELL_MODES>(mode | 0x100); }

// Keeps the move on the axis of the drag start (gLLCC4804, gLLCC4800).
void MMSnap(int32_t& x, int32_t& y) {
    int32_t dy = MMAbs(y - gLLCC4800);
    int32_t dx = MMAbs(x - gLLCC4804);
    if (!(dx < dy)) y = gLLCC4800;
    dy = MMAbs(y - gLLCC4800);
    dx = MMAbs(x - gLLCC4804);
    if (!(dx > dy)) x = gLLCC4804;
}

typedef void (*MMLineFn)(CELL_MODES, int32_t, int32_t, int32_t*, int32_t*);

// The four sides of the rectangle between the drag start and the cell, each line a call (mode < 0: the current mode | 0x100).
void MMRectangle(MMLineFn line, int32_t mode, int32_t step, int32_t minX, int32_t minY, int32_t maxX, int32_t maxY) {
    int32_t endX = maxX;
    int32_t endY = minY;
    int32_t startX = minX;
    if (minX != maxX) {
        startX += step;
        endX -= step;
    }
    line(mode < 0 ? MMPreview(LLCC_SystemGetMode()) : static_cast<CELL_MODES>(mode), startX, minY, &endX, &endY);
    endX = maxX;
    endY = maxY;
    line(mode < 0 ? MMPreview(LLCC_SystemGetMode()) : static_cast<CELL_MODES>(mode), maxX, minY, &endX, &endY);
    endX = maxX;
    endY = maxY;
    startX = minX;
    if (minX != maxX) {
        startX += step;
        endX -= step;
    }
    line(mode < 0 ? MMPreview(LLCC_SystemGetMode()) : static_cast<CELL_MODES>(mode), startX, maxY, &endX, &endY);
    endX = minX;
    endY = minY;
    line(mode < 0 ? MMPreview(LLCC_SystemGetMode()) : static_cast<CELL_MODES>(mode), minX, maxY, &endX, &endY);
}

// Marks every cell of the potential pylon area where a pylon may stand.
void MMPylonArea() {
    uint32_t x0, y0, x1, y1;
    GetPotentialPylonPositionArea(&x0, &y0, &x1, &y1);
    for (uint32_t j = y0; j < y1; ++j) {
        for (uint32_t i = x0; i < x1; ++i) {
            CMapCell* cell = MMMapCell(MMCellId(static_cast<int32_t>(i), static_cast<int32_t>(j)));
            if (IsValidPylonPosition(i, j, nullptr, 0, 0, 0, 0) == 0) continue;
            if (cell->IsFlag(0x40) != 0) continue;
            if (cell->IsHoarding() != 0) continue;
            if (cell->IsTrack() != 0) continue;
            if (cell->IsEntrance() != 0) continue;
            if (cell->IsExit() != 0) continue;
            if (cell->IsQueue() != 0) continue;
            if (cell->IsRide() != 0) continue;
            if (cell->IsFreeform() != 0) continue;
            if (cell->IsPath() != 0 && cell->IsFlag(0x20) != 0) continue;
            LLCC_DrawLine(static_cast<CELL_MODES>(0x11f), static_cast<int32_t>(i), static_cast<int32_t>(j),
                          reinterpret_cast<int32_t*>(&i), reinterpret_cast<int32_t*>(&j));
        }
    }
}

void MMPlaceRidePreview() {
    gLLCCDB54 = gLLCCDB3C;
    gLLCCDB50 = gLLCCDB38;
    int32_t rotation = LLCC_GetRideRotation();
    if (LLCR_PlaceRide(gLLCCDB3C, gLLCCDB38, 4, rotation, 0, 1) == 0) gLLCC47C8 = 1;
    rotation = LLCC_GetRideRotation();
    LLCR_PlaceRide(gLLCCDB3C, gLLCCDB38, 4, rotation, 0, 0);
    gDynamicCellF9BD4 = 0;
}

// x and y are the caller's: the boundary moves them and the tail reads the moved cell.
void MMRideMode(int32_t& x, int32_t& y) {
    gTrackCostFree = 1;
    RideInfo* ride = MMRide(gLLCCE0E0);
    bool ticketFree = false;
    if (MMSlot(ride, 46) > 0) {   // Mac +0xc4
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
        id.mIndex = gLLCCE0E0;
        const CGoldenTicketControl* tickets = reinterpret_cast<const CGoldenTicketControl*>(MMWorldBytes() + 0x2c0);
        if (!tickets->HasBeenBought(id)) ticketFree = true;
    }
    if (ticketFree) {
        LLCC_SystemSetSessionCost(0);
    } else {
        LLCC_SystemSetSessionCost(MMSlot(MMRide(gLLCCE0E0), 107));   // Mac +0x1b8
        if (gActionRecorder.mRecording == 0) {
            const int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
            if (LLCC_SystemGetSessionCost() > balance) {
                gLLCCC214 = 1;
                gLLCCC204 = 1;
            }
        }
    }
    gLLCC47C8 = 0;
    gLLCCDB3C = x;
    gLLCCDB38 = y;
    if (gLLCCE0E0 != gLLCC9F34) {
        gLLCCDE0B8 = MMSlot(MMRide(gLLCCE0E0), 18);   // Mac +0x54
        if (gLLCCE0DC != 0 && CMap::IsXYValid(x, y) != 0) {
            RideInfo* info = MMRide(gLLCCE0E0);
            int32_t rotation = LLCC_GetRideRotation();
            LLCM_TrackCellBoundary(gLLCC9F18, gLLCC9F1C, &x, &y, &gLLCCDB3C, &gLLCCDB38);
            if (CMap::IsXYValid(x, y) != 0) {
                CTrackCell* track = MMTrackCell(MMCellId(gLLCCDB3C, gLLCCDB38));
                if (track->IsTrackX() != 0) track = MMTrackCell(track->mParentID);
                if (track->IsTrack() != 0) {
                    switch (track->GetDirection()) {
                    case 0x10: rotation = 0xb4; break;
                    case 0x04: rotation = 0x10e; break;
                    case 0x01: rotation = 0; break;
                    case 0x40: rotation = 0x5a; break;
                    default: break;
                    }
                    LLCC_SetRideRotation(rotation, false);
                }
                switch (LLCC_GetRideRotation()) {
                case 0:
                    gLLCCDB3C += MMSlot(info, 38);   // Mac +0xa4
                    gLLCCDB38 += MMSlot(info, 39);   // +0xa8
                    break;
                case 0x10e:
                    gLLCCDB3C += MMSlot(info, 40);   // +0xac
                    gLLCCDB38 += MMSlot(info, 41);   // +0xb0
                    break;
                case 0xb4:
                    gLLCCDB3C += MMSlot(info, 42);   // +0xb4
                    gLLCCDB38 += MMSlot(info, 43);   // +0xb8
                    break;
                case 0x5a:
                    gLLCCDB3C += MMSlot(info, 44);   // +0xbc
                    gLLCCDB38 += MMSlot(info, 45);   // +0xc0
                    break;
                default: break;
                }
                // The current object (gLLCCDB6C, a 32-bit pointer word on the Mac; the host keeps the whole pointer in gLLCMCurrentObject
                // while the two agree), read without a NULL check as the original does.
                const CObject* current =
                    (gLLCCDB6C != 0 && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C))
                        ? gLLCMCurrentObject
                        : nullptr;
                switch (current->mRotation) {
                case 0xb4:
                    gLLCCDB3C -= 1;
                    gLLCCDB38 += 1;
                    break;
                case 0x5a: gLLCCDB3C -= 1; break;
                case 0x10e: gLLCCDB38 += 1; break;
                default: break;
                }
            }
        }
        MMPlaceRidePreview();
    }
    gTrackCostFree = 0;
}

}  // namespace

// 0x10083180 (6228 bytes)
void LLCM_MouseMoved(int32_t x, int32_t y, uint16_t flag) {
    LLCM_SetIllegalPlacement(0);
    gLLCCC214 = 0;
    gLLCCC210 = 0;
    gLLCCC20C = 0;
    gLLCCC208 = 0;
    gLLCCC204 = 0;
    gLLCCC200 = 0;
    gLLCCDB54 = x;
    gLLCCDB50 = y;
    DC_ResetSquareFaceListPtr();
    gLLCCDE0B8 = 0;
    gLLCCDE0B4 = 0;
    if (LLCC_SystemIsMode(0) == 0) {
        if (LLCC_SystemIsMode(1) != 0 || LLCC_SystemIsMode(3) != 0) {
            if (LLCC_SystemIsMode(1) != 0) gLLCCDE0B8 = 2;
            if (LLCC_SystemIsMode(3) != 0) gLLCCDE0B8 = 3;
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                MMSnap(x, y);
                gLLCCDB54 = x;
                gLLCCDB50 = y;
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawLine(mode, gLLCC4804, gLLCC4800, &x, &y);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0x38) != 0) {
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                const int32_t fromX = gLLCC4804;
                const int32_t fromY = gLLCC4800;
                const CELL_MODES mode = MMPreview(LLCC_SystemGetLastMode());
                LLCC_DrawLine(mode, fromX, fromY, &x, &y);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetLastMode());
                LLCC_DrawLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(4) != 0) {
            MMRideMode(x, y);
        } else if (LLCC_SystemIsMode(0x3b) != 0) {
            gTrackCostFree = 1;
            gLLCC47C8 = 0;
            gLLCCDB3C = x;
            gLLCCDB38 = y;
            LLCC_SystemSetSessionCost(MMSlot(MMRide(gLLCCE0E0), 107));
            gLLCCDE0B8 = MMSlot(MMRide(gLLCCE0E0), 18);
            int32_t rotation = LLCC_GetRideRotation();
            if (LLCR_PlaceRide(gLLCCDB3C, gLLCCDB38, 4, rotation, 0, 1) == 0) gLLCC47C8 = 1;
            rotation = LLCC_GetRideRotation();
            LLCR_PlaceRide(gLLCCDB3C, gLLCCDB38, 4, rotation, 0, 0);
            gDynamicCellF9BD4 = 0;
            gTrackCostFree = 0;
        } else if (LLCC_SystemIsMode(0xb) != 0 || LLCC_SystemIsMode(0x10) != 0 || LLCC_SystemIsMode(0x19) != 0) {
            gLLCCDE0B8 = 3;
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                MMSnap(x, y);
                gLLCCDB54 = x;
                gLLCCDB50 = y;
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawTrackLine(mode, gLLCC4804, gLLCC4800, &x, &y);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawTrackLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0x39) != 0) {
            if (x < gLLCC9F38) x = gLLCC9F38;
            if (y < gLLCC9F3C) y = gLLCC9F3C;
            const int32_t limitX = gLLCCWidth - 2;
            if (limitX < x) x = limitX;
            const int32_t limitY = gLLCCHeight - 3;
            if (limitY < y) y = limitY;
            LLCM_TrackCellBoundary(gHoardingRight, gHoardingBottom, &x, &y, &x, &y);
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                gLLCCC214 = LLCC_IsHoardingEditLegal(gLLCC4804, gLLCC4800, x, y, true) == 0 ? 1 : 0;
                const int32_t minX = x < gLLCC4804 ? x : gLLCC4804;
                const int32_t minY = y < gLLCC4800 ? y : gLLCC4800;
                const int32_t maxX = gLLCC4804 < x ? x : gLLCC4804;
                const int32_t maxY = gLLCC4800 < y ? y : gLLCC4800;
                MMRectangle(LLCC_DrawTrackLine, -1, 2, minX, minY, maxX, maxY);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawTrackLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0x3a) != 0) {
            LLCC_SystemSetSessionCost(-9999);
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                gLLCCC214 = LLCC_IsClearLandLegal(gLLCC4804, gLLCC4800, x, y) == 0 ? 1 : 0;
                const int32_t minX = x < gLLCC4804 ? x : gLLCC4804;
                const int32_t minY = y < gLLCC4800 ? y : gLLCC4800;
                const int32_t maxX = gLLCC4804 < x ? x : gLLCC4804;
                const int32_t maxY = gLLCC4800 < y ? y : gLLCC4800;
                MMRectangle(LLCC_DrawLine, 0x13a, 1, minX, minY, maxX, maxY);
            } else {
                LLCC_DrawLine(static_cast<CELL_MODES>(0x13a), x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0x36) != 0) {
            LLCC_SystemSetSessionCost(-9999);
            if (gLLCC4804 != -1 && gLLCC4800 != -1) {
                const int32_t minX = x < gLLCC4804 ? x : gLLCC4804;
                const int32_t minY = y < gLLCC4800 ? y : gLLCC4800;
                const int32_t maxX = gLLCC4804 < x ? x : gLLCC4804;
                const int32_t maxY = gLLCC4800 < y ? y : gLLCC4800;
                gLLCCC214 = LLCC_IsPatrolAreaLegal(minX, minY, maxX, maxY) ? 0 : 1;
                MMRectangle(LLCC_DrawLine, -1, 1, minX, minY, maxX, maxY);
            } else {
                const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                LLCC_DrawLine(mode, x, y, &x, &y);
            }
        } else if (LLCC_SystemIsMode(0xe) != 0 || LLCC_SystemIsMode(0x13) != 0) {
            if (x != 0 || y != 0) LLCM_StartTrackEditMode(x, y, flag);
        } else if (LLCC_SystemIsMode(0x1a) != 0) {
            if (gLLCCDB68 != gLLCC9F40) {
                LLCM_TrackCellBoundary(gLLCC9F18, gLLCC9F1C, &x, &y, &gLLCCDB54, &gLLCCDB50);
                if (static_cast<int16_t>(gLLCC9F24) == -1) {
                    const CELL_MODES mode = MMPreview(LLCC_SystemGetMode());
                    LLCC_DrawTrackLine(mode, x, y, &x, &y);
                }
                if (gLLCCReviseAnswer == 0) {
                    LLCC_SystemConstructHoardingClickStack();
                    LLCM_LeftMouseButtonUp(gLLCC4804, gLLCC4800, flag, 0);
                    if (LLCC_SystemIsMode(0x1a) != 0) LLCC_SystemSetModeNR(0x19, true);
                } else if (static_cast<int16_t>(gLLCC9F24) != -1) {
                    gLLCCDB58 = MMCellId(x, y);
                    gLLCC9F26 = static_cast<uint16_t>(MMTrackCell(gLLCCDB58)->GetSegmentNumber());
                    CTrackCell* cell = MMTrackCell(gLLCCDB58);
                    if (cell->ConnectionType(static_cast<CELL_MODES>(LLCC_SystemGetMode()), 0, 0) != 9) {
                        gLLCCDB58 = gLLCCDB5A;
                        gLLCC9F26 = gLLCC9F24;
                    }
                    if (static_cast<int16_t>(gLLCC9F24) > static_cast<int16_t>(gLLCC9F26))
                        LLCC_TraceTrack(MMTrackCell(gLLCCDB58), MMTrackCell(gLLCCDB5A), 0);
                    else
                        LLCC_TraceTrack(MMTrackCell(gLLCCDB5A), MMTrackCell(gLLCCDB58), 0);
                }
            }
        } else if (LLCC_SystemIsMode(0x15) != 0) {
            gLLCCDE0B8 = 3;
            gLLCCC214 = IsValidPylonPosition(static_cast<uint32_t>(x), static_cast<uint32_t>(y), nullptr, 0, 0, 0, 0) == 0 ? 1 : 0;
            LLCC_DrawLine(static_cast<CELL_MODES>(0x115), x, y, &x, &y);
            MMPylonArea();
        } else if (LLCC_SystemIsMode(0x16) != 0) {
            gLLCCDE0B8 = 3;
            if (gLLCCE090 != 0)
                gLLCCC214 = IsValidPylonPosition(static_cast<uint32_t>(x), static_cast<uint32_t>(y), nullptr, 0, 0, 0, 0) == 0 ? 1 : 0;
            if (gLLCC9F14 == -1) gLLCC9F14 = MMMapCell(MMCellId(x, y))->GetPylonIndex();
            LLCC_DrawLine(static_cast<CELL_MODES>(0x116), x, y, &x, &y);
            if (gLLCCE090 != 0) MMPylonArea();
        }
        // Every mode but 0: the ride under the cell, when there is one, names the info word.
        if (CMap::IsXYValid(x, y) != 0) {
            CMapCell* cell = MMMapCell(MMCellId(x, y));
            if (cell->IsRide() != 0) {
                const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId = MMMapCell(cell->mParentID)->GetObjectThingId();
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
                const CThing* thing = static_cast<const CThing*>(gThingArray[thingId.mIndex].mThing);
                if (thing->mKind == 3)
                    gLLCCDE0B4 = MMSlot(MMRide(reinterpret_cast<const CObject*>(thing)->mObjectType), 18);
            }
        }
    }
    LLCC_SystemSetCursor();
    gLLCCDB60 = (gLLCC4804 == x && gLLCC4800 == y) ? 1 : 0;
}

// LLCM_LeftMouseButtonDown 0x10084a00 (624 bytes): the left button goes down on a cell.

uint8_t gLLCCDB34;   // 0x102ddb34
uint8_t gLLCCE090;   // 0x102de090

void LLCM_LeftMouseButtonDown(int32_t x, int32_t y, uint16_t shift, int32_t flag) {
    gLLCCDB34 = 1;
    if (gActionRecorder.mRecording != 0) {
        gLLCCC214 = 0;
        if (gLLCCE0DC == 0) LLCM_MouseMoved(x, y, shift);
    }
    if (flag != 0) {
        x = gLLCCDB54;
        y = gLLCCDB50;
    }
    if (LLCM_GetIllegalPlacement() == 0) {
        if (shift == 0)
            gActionRecorder.RecordAction(static_cast<RecordedAction>(0xf), static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        else
            gActionRecorder.RecordAction(static_cast<RecordedAction>(0x28), static_cast<uint8_t>(x), static_cast<uint8_t>(y), shift);
    }
    if (LLCC_SystemIsMode(0x38) == 0 && LLCM_GetIllegalPlacement() != 0) return;
    if (LLCC_SystemIsMode(0) != 0) return;
    if (LLCC_SystemIsMode(0x16) == 0) return;
    LLCM_SetCurrentObject(false);
    if (gLLCCE090 == 0) {
        CoasterAcceptLeftMouseClick(1);
        return;
    }
    LLCC_SystemAddToClickStack(x, y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x87), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x15), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x83), x, y, &x, &y);
    LLCC_DrawLine(static_cast<CELL_MODES>(0x81), x, y, &x, &y);
    const uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
    uint8_t* world = reinterpret_cast<uint8_t*>(gGameWorld);
    const uint32_t result = DoMovePylon(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
    *reinterpret_cast<uint32_t*>(world + 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x28 + 0x110000 + 4) = result;
    gLLCC9F14 = -1;
    gLLCCE090 = 0;
}

// LLCM_LeftMouseButtonUp (main app 0x10084ca4, 14,520 bytes; unit v5883): the cell system's left button release. Each edit mode
// finishes its drag here: paths and queues (1, 3), ride placement (4, 0x3b), track (0xb, 0x10, 0xe, 0x13, 0x1a), hoardings (0x19,
// 0x39), land clearing (0x3a), ride deletion (0x33), the line modes (0x32, 0x34, 0x35), staff patrol areas (0x36), queue editing
// (0x14), coaster pylons (0x15, 0x16), and the illegal placement's tidy up. Every exit clears gLLCCDB34 (set by the button down).
//
// Notes from the listing:
// - The cells are found by address arithmetic on the world block: the map cell of index i at +0x2d8 + (i - 1) * 0x44, the track
//   cell at +0x2d8 + 0x110000 + (i - 1) * 0x28 (the index is 1 + x + (y << 7), a halfword).
// - LLCC_DrawLine, LLCC_DrawTrackLine and LLCC_TraceTrack are declared void, but the listing reads their r3 (the 0x81 line's answer
//   decides whether the drag ends, TraceTrack's is compared with -1): they are called through an int-answering pointer type.
// - The click modes re-read the current object word gLLCCDB6C each time; gLLCCDB64 is the selected peep.
// - gCalcNeighboursPrevA is stored twice where the other modes store PrevA and PrevB (the original's slip, kept).
// - The illegal path's case 0x100884f8 stores the original arguments back into their own slots; nothing reads them after.

int32_t gLLCC9F28 = -1;
int32_t gLLCC9F2C = -1;
int32_t gLLCC9F44 = 1;
int32_t gLLCC9F48 = 1;
int32_t gLLCCDB30;
int32_t gLLCCDB44;
CCellBase* gLLCCDB4C;
CThing* gLLCCDB64;

namespace {

using LineFn = int32_t (*)(CELL_MODES, int32_t, int32_t, int32_t*, int32_t*);
using TraceFn = int32_t (*)(CTrackCell*, CTrackCell*, int32_t);

int32_t DrawLine(int32_t mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    return reinterpret_cast<LineFn>(&LLCC_DrawLine)(static_cast<CELL_MODES>(mode), x, y, endX, endY);
}

int32_t DrawTrackLine(int32_t mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY) {
    return reinterpret_cast<LineFn>(&LLCC_DrawTrackLine)(static_cast<CELL_MODES>(mode), x, y, endX, endY);
}

int32_t TraceTrack(CTrackCell* from, CTrackCell* to, int32_t flag) {
    return reinterpret_cast<TraceFn>(&LLCC_TraceTrack)(from, to, flag);
}

uint16_t CellIndex(int32_t x, int32_t y) { return static_cast<uint16_t>(x + (y << 7) + 1); }

CMapCell* MapCellAt(uint16_t index) {
    return reinterpret_cast<CMapCell*>(WorldBytes() + 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x44);
}

CTrackCell* TrackCellAt(uint16_t index) {
    return reinterpret_cast<CTrackCell*>(WorldBytes() + 0x2d8 + 0x110000 + (static_cast<int32_t>(index) - 1) * 0x28);
}

int32_t CellX(uint16_t index) { return static_cast<int32_t>((static_cast<uint32_t>(index) - 1) & 0x7f); }
int32_t CellY(uint16_t index) { return static_cast<int32_t>((static_cast<uint32_t>(index) - 1) >> 7); }

// The current object: the Mac's word gLLCCDB6C; the host keeps the whole pointer in gLLCMCurrentObject while the two agree.
CObject* Current() {
    if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C)) return gLLCMCurrentObject;
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
}

void SetCurrent(CObject* object) {
    gLLCMCurrentObject = object;
    gLLCCDB6C = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(object)));
}

int32_t Recording() { return gActionRecorder.mRecording; }

// The click sound at the cell (x, y): the cell scaled by 10 in single precision, truncated (fctiwz).
void Sound(int32_t event, int32_t x, int32_t y) {
    const int32_t px = static_cast<int32_t>(10.0f * static_cast<float>(x));
    const int32_t py = static_cast<int32_t>(10.0f * static_cast<float>(y));
    SOUND_SendEvent(0, gSoundBanks[3], event, px, 0, py);
}

void Advise(int32_t message) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> nobody;
    nobody.mIndex = 0;
    CAdvisorRequest request(static_cast<AdvisorMessage>(message), nobody, 0, -1, false, -1);
    gGameWorld->GetAdvisor()->QueueRequest(request);
}

// The end of a drag: no line start, the neighbour state back to its start.
void EndDragWords() {
    gLLCC4804 = -1;
    gLLCC4800 = -1;
    gCalcNeighboursPrevA = 0x80;
    gCalcNeighboursPrevA = 0x80;
    gLLCCE288 = 0x80;
    gLLCCE28C = 0x80;
}

// Snaps (x, y) to the line from (gLLCC4804, gLLCC4800): the shorter offset goes.
void Snap(int32_t* x, int32_t* y) {
    int32_t a = std::abs(*y - gLLCC4800);
    int32_t b = std::abs(*x - gLLCC4804);
    if (!(b < a)) *y = gLLCC4800;
    a = std::abs(*y - gLLCC4800);
    b = std::abs(*x - gLLCC4804);
    if (!(b > a)) *x = gLLCC4804;
}

uint8_t DirectionOfRotation(int32_t rotation) {
    if (rotation == 0xb4) return 0x40;
    if (rotation == 0x5a) return 1;
    if (rotation == 0) return 4;
    if (rotation == 0x10e) return 0x10;
    return 0;
}

// The track switches of modes 0xe and 0x13 after the click stack is rebuilt.
void TrackStackAgain() {
    LLCC_SystemConstructTrackClickStack(Current());
    if (LLCC_SystemIsMode(0xe) != 0) LLCC_SystemSetModeNR(0xb, false);
    if (LLCC_SystemIsMode(0x13) != 0) LLCC_SystemSetModeNR(0x10, false);
}

// The track cell of (x, y), or of its parent for a track X cell.
CTrackCell* TrackOrParent(uint16_t index) {
    CTrackCell* cell = TrackCellAt(index);
    if (cell->IsTrackX() != 0) cell = TrackCellAt(cell->mParentID);
    return cell;
}

// The illegal path's jump table (0x1029df4c, 55 words) by GetMode: 0 the exit, 1 SetMode(0), 2 the dead slot stores, 3 the pylon abort.
// (Modes 1, 3, 0xb, 0xe, 0x10, 0x13 and 0x36 end the mode, 4 has the slot stores, 0x16 aborts the pylon move.)
const uint8_t kIllegalCase[0x37] = {
    0, 1, 0, 1, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 3, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
};

// The ride placement modes 4 and 0x3b after the prelude (block A): the ride, its track start and its queue start.
void PlaceRideClick(int32_t& x, int32_t& y, uint16_t shift, bool editMode) {
    // LLCR_PlaceRide answers its block (gLLCRPlacement) as a 32-bit word, on the host the low half of the address; the host takes
    // the block from LLCR_PlaceRideBlock, the body that answers it whole. (Unit v5L28: the word, widened, was an address in the
    // unmapped first 4 GB; placing a bought ride faulted in LLCR_StartQueue.)
    LLCR_PRR* const block = reinterpret_cast<LLCR_PRR*>(LLCR_PlaceRideBlock(x, y, 4, LLCC_GetRideRotation(), 1, 0));
    if (LLCC_SystemGetRideQueue() == 0) {
        if (shift != 0xc) LLCC_SystemSetModeNR(0, false);
    } else {
        if (LLCC_SystemGetRideTrack() != 0) {
            gLLCCDB4C = LLCR_StartQueue(block, &gLLCC9F28, &gLLCC9F2C);
            if (LLCC_SystemGetRideTrack() != 3) {
                gLLCCDB48 = LLCR_StartTrack(block, &gLLCC4804, &gLLCC4800, LLCC_SystemGetRideTrack());
                const uint16_t start = gLLCCDB48->mCellId;
                int32_t lx = CellX(start);
                int32_t ly = CellY(start);
                LLCC_SystemResetClickStack();
                const int32_t track = LLCC_SystemGetRideTrack();
                if (track == 1)
                    LLCC_SystemSetModeNR(0xb, false);
                else if (track == 2)
                    LLCC_SystemSetModeNR(0x10, false);
                if (!editMode) {
                    DrawTrackLine(0x87, lx, ly, &lx, &ly);
                    DrawTrackLine(LLCC_SystemGetMode(), lx, ly, &lx, &ly);
                    DrawTrackLine(0x80, lx, ly, &lx, &ly);
                    CTrackCell* cell = TrackCellAt(CellIndex(lx, ly));
                    cell->ModifyDirection(DirectionOfRotation(Current()->mRotation));
                    DrawTrackLine(0x83, lx, ly, &lx, &ly);
                    DrawTrackLine(0x85, lx, ly, &lx, &ly);
                    DrawTrackLine(0x86, lx, ly, &lx, &ly);
                    DrawTrackLine(0x81, lx, ly, &lx, &ly);
                } else {
                    DrawTrackLine(0x87, lx, ly, &lx, &ly);
                    DrawTrackLine(LLCC_SystemGetMode(), lx, ly, &lx, &ly);
                    DrawTrackLine(0x80, lx, ly, &lx, &ly);
                    DrawTrackLine(0x82, lx, ly, &lx, &ly);
                    DrawTrackLine(0x83, lx, ly, &lx, &ly);
                    DrawTrackLine(0x85, lx, ly, &lx, &ly);
                    DrawTrackLine(0x86, lx, ly, &lx, &ly);
                    DrawTrackLine(0x81, lx, ly, &lx, &ly);
                }
                LLCC_SystemAddToClickStack(lx, ly);
                gLLCCFirstBlockX = CellX(gLLCCDB68);
                gLLCCFirstBlockY = CellY(gLLCCDB68);
                gLLCCE034 = gLLCCFirstBlockX;
                gLLCCE038 = gLLCCFirstBlockY;
                LLCR_OffsetStartOfTrack(&gLLCCFirstBlockX, &gLLCCFirstBlockY, gLLCC9F20);
                LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
                lx = gLLCCE034;
                ly = gLLCCE038;
                uint8_t dir;
                const int32_t dx = gLLCCFirstBlockX - lx;
                if (dx < 0) {
                    dir = 0x40;
                } else if (dx > 0) {
                    dir = 4;
                } else {
                    const int32_t dy = gLLCCFirstBlockY - ly;
                    if (dy < 0)
                        dir = 1;
                    else if (dy > 0)
                        dir = 0x10;
                    else
                        dir = 0;
                }
                DrawTrackLine(0x87, lx, ly, &lx, &ly);
                DrawTrackLine(LLCC_SystemGetMode(), lx, ly, &lx, &ly);
                DrawTrackLine(0x80, lx, ly, &lx, &ly);
                CTrackCell* end = TrackCellAt(CellIndex(lx, ly));
                end->ModifyNeighbours(dir);
                end->ModifyDirection(dir);
                CTrackCell* beyond = end->GetNeighbouringCell(dir, 2);
                if (beyond != nullptr) beyond->ModifyNeighbours(OPPOSITE(dir));
                if (editMode) DrawTrackLine(0x82, lx, ly, &lx, &ly);
                DrawTrackLine(0x83, lx, ly, &lx, &ly);
                DrawTrackLine(0x85, lx, ly, &lx, &ly);
                DrawTrackLine(0x86, lx, ly, &lx, &ly);
                DrawTrackLine(0x81, lx, ly, &lx, &ly);
            }
            CObject* object = Current();
            if (object != nullptr && gLLCCE0DC == 0) {
                object->mF28 = 0;
                gLLCCDB68 = static_cast<uint16_t>(object->mBase.mCellX + (object->mBase.mCellY << 7) + 1);
                gLLCC9F18 = gLLCCFirstBlockX = CellX(gLLCCDB68);
                gLLCC9F1C = gLLCCFirstBlockY = CellY(gLLCCDB68);
                gLLCC9F20 = Current()->mRotation;
                gLLCCE034 = gLLCCFirstBlockX;
                gLLCCE038 = gLLCCFirstBlockY;
                if (LLCC_SystemGetRideTrack() != 3) {
                    gLLCCE0CC = LLCR_OffsetStartOfTrack(&gLLCCFirstBlockX, &gLLCCFirstBlockY, gLLCC9F20);
                    LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
                }
            }
        }
        gLLCCDB4C = LLCR_StartQueue(block, &gLLCC4804, &gLLCC4800);
        const uint16_t queue = gLLCCDB4C->mCellId;
        int32_t qx = CellX(queue);
        int32_t qy = CellY(queue);
        LLCC_SystemResetClickStack();
        LLCC_SystemSetModeNR(3, false);
        if (gLLCC4804 != x || gLLCC4800 != y || LLCC_SystemGetClickStackEntries() == 1) {
            LLCC_SystemAddToClickStack(qx, qy);
            if (!editMode && gLLCCDB4C != nullptr) gLLCCDB4C = nullptr;
            DrawLine(0x85, qx, qy, &qx, &qy);
            DrawLine(0x86, qx, qy, &qx, &qy);
            DrawLine(0x83, qx, qy, &qx, &qy);
            DrawLine(0x81, qx, qy, &qx, &qy);
            Current()->RecalculateBackOfQueue();
            gCalcNeighboursPrevA = 0x80;
            gCalcNeighboursPrevB = 0x80;
            if (static_cast<int32_t>(MapCellAt(queue)->mType) == 1) LLCC_SystemSetMode(0, false);
        }
    }
    if (shift != 0xc) LLCC_SetRideRotation(0, false);
    gTrackCostFree = 0;
}

void IllegalPlacement(int32_t x, int32_t y) {
    if (Recording() == 0 && gLLCCDB34 != 0) Sound(0xaf, x, y);
    const uint32_t mode = static_cast<uint32_t>(LLCC_SystemGetMode());
    if (mode > 0x36) return;
    switch (kIllegalCase[mode]) {
    case 1:
        LLCC_SystemSetMode(0, false);
        break;
    case 3:
        AbortMovePylon();
        LLCM_SetIllegalPlacement(0);
        gLLCCE090 = 0;
        break;
    default:
        break;
    }
}

void Modes(int32_t& x, int32_t& y, uint16_t shift, int32_t limitX, int32_t limitY) {
    bool lineDone = false;   // r20

    // ---- paths and queues
    if (LLCC_SystemIsMode(1) != 0 || LLCC_SystemIsMode(3) != 0) {
        if (Recording() == 0) Sound(0x65, x, y);
        if (LLCC_SystemIsMode(1) != 0) gLLCCDE0B8 = 2;
        if (LLCC_SystemIsMode(3) != 0) gLLCCDE0B8 = 3;
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            CMapCell* cell = MapCellAt(CellIndex(x, y));
            if (!(LLCC_SystemIsMode(1) != 0 && cell->ConnectionType(static_cast<CELL_MODES>(1), 0) != 1)) {
                if (LLCC_SystemIsMode(3) == 0) return;
                if (cell->ConnectionType(static_cast<CELL_MODES>(3), 0) == 1) return;
            }
            gLLCC4804 = x;
            gLLCC4800 = y;
            LLCC_SystemResetClickStack();
            LLCC_SystemAddToClickStack(x, y);
            if (LLCC_SystemIsMode(1) != 0) Advise(0xb8);
            gLLCCDB30 = 0;
            return;
        }
        gLLCCDB30 = gLLCCDB30 + 1;
        if (gLLCCDB30 == 3) Advise(0xd6);
        Snap(&x, &y);
        if (gLLCC4804 != x || gLLCC4800 != y || LLCC_SystemGetClickStackEntries() == 1) {
            LLCC_SystemAddToClickStack(x, y);
            DrawLine(0x87, gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(LLCC_SystemGetMode(), gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(0x80, gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(0x82, gLLCC4804, gLLCC4800, &x, &y);
            if (gLLCCDB4C != nullptr) gLLCCDB4C = nullptr;
            DrawLine(0x85, gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(0x86, gLLCC4804, gLLCC4800, &x, &y);
            DrawLine(0x83, gLLCC4804, gLLCC4800, &x, &y);
            lineDone = DrawLine(0x81, gLLCC4804, gLLCC4800, &x, &y) == 0;
        }
        if (LLCC_SystemIsMode(3) != 0) {
            Current()->RecalculateBackOfQueue();
            if (Recording() == 0 && gLLCCC208 != 0) Sound(0x8b, x, y);
        }
        if ((gLLCC4804 == x && gLLCC4800 == y) || lineDone) {
            gLLCCDB40 = 0;
            EndDragWords();
            const bool queue = LLCC_SystemIsMode(3) != 0;
            LLCC_SystemSetMode(0, false);
            if (queue && Recording() == 0 && gLLCCC208 == 0) Advise(0x151);
            return;
        }
        gLLCC4804 = x;
        gLLCC4800 = y;
        return;
    }

    // ---- ride placement
    if (LLCC_SystemIsMode(4) != 0) {
        gTrackCostFree = 1;
        gLLCCDB4C = nullptr;
        gLLCCDB48 = nullptr;
        gLLCCDB44 = 0;
        if (gLLCCE0DC != 0) {
            LLCM_SetCurrentObject(false);
            CTrackCell* cell = TrackCellAt(CellIndex(x, y));
            if (cell->IsTrack() == 0 && cell->IsTrackX() == 0) {
                CTRACE(6, "How'd we get here?\n");
            } else {
                if (cell->IsTrackX() != 0) cell = TrackCellAt(cell->mParentID);
                CMapCell* mapCell = TrackCellAt(cell->mParentID)->GetMapCellEquiv();
                const uint16_t id = mapCell->GetObjectThingId().mIndex;
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
                SetCurrent(static_cast<CObject*>(gThingArray[id].mThing));
                BMP_FreeTrack(Current()->mF24);
                LLCR_PlaceRide(x, y, 4, LLCC_GetRideRotation(), 1, 0);
                LLCM_RebuildUpgradeBridges(x, y);
                CObject* object = Current();
                gLLCCDB68 = static_cast<uint16_t>(object->mBase.mCellX + (object->mBase.mCellY << 7) + 1);
                gLLCC9F18 = gLLCCFirstBlockX = CellX(gLLCCDB68);
                gLLCC9F1C = gLLCCFirstBlockY = CellY(gLLCCDB68);
                gLLCC9F20 = Current()->mRotation;
                gLLCCE034 = gLLCCFirstBlockX;
                gLLCCE038 = gLLCCFirstBlockY;
                gLLCCE0CC = LLCR_OffsetStartOfTrack(&gLLCCFirstBlockX, &gLLCCFirstBlockY, gLLCC9F20);
                LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
                const int32_t whole = LLCC_BuildTrackDB(false);
                Current()->mF28 = whole;
                if (whole != 0) {
                    LLCC_SetTrackAnim(LLCM_GetCurrentObject(), true);
                } else {
                    LLCC_SetTrackAnim(LLCM_GetCurrentObject(), false);
                    LLCC_SetTrackAnimReverse(LLCM_GetCurrentObject(), false);
                }
                if (whole != 0 && Current()->IsOpenable() != 0) Current()->Reopen();
            }
            if (shift != 0xc) {
                LLCC_SystemSetModeNR(0, false);
                gLLCCE0DC = 0;
            }
            LLCC_SetRideRotation(0, false);
            gTrackCostFree = 0;
            return;
        }
        PlaceRideClick(x, y, shift, false);
        return;
    }
    if (LLCC_SystemIsMode(0x3b) != 0) {
        gTrackCostFree = 1;
        gLLCCDB4C = nullptr;
        gLLCCDB48 = nullptr;
        gLLCCDB44 = 0;
        PlaceRideClick(x, y, shift, true);
        return;
    }

    // ---- track laying
    if (LLCC_SystemIsMode(0xb) != 0 || LLCC_SystemIsMode(0x10) != 0) {
        if (Recording() == 0) Sound(0xce, x, y);
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            LLCC_SystemResetClickStack();
            LLCC_SystemAddToClickStack(x, y);
            Advise(0xcc);
            return;
        }
        Snap(&x, &y);
        if (gLLCC4804 != x || gLLCC4800 != y || (gLLCCFirstBlockX == x && gLLCCFirstBlockY == y)) {
            DrawTrackLine(0x87, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(LLCC_SystemGetMode(), gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x80, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x82, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x83, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x85, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x86, gLLCC4804, gLLCC4800, &x, &y);
            lineDone = DrawTrackLine(0x81, gLLCC4804, gLLCC4800, &x, &y) == 0;
            LLCC_SystemAddToClickStack(x, y);
        }
        if ((gLLCC4804 == x && gLLCC4800 == y) || lineDone) {
            const int32_t whole = LLCC_BuildTrackDB(false);
            Current()->mF28 = whole;
            if (whole != 0 && Current()->IsOpenable() != 0) Current()->Reopen();
            LLCR_FixBridges(Current(), true, false, false);
            LLCR_FixBridgesReverse(Current(), false, false, false);
            if (whole != 0) {
                LLCC_SetTrackAnim(LLCM_GetCurrentObject(), false);
            } else {
                LLCC_SetTrackAnim(LLCM_GetCurrentObject(), false);
                LLCC_SetTrackAnimReverse(LLCM_GetCurrentObject(), false);
            }
            EndDragWords();
            if (gLLCCDB4C == nullptr) {
                if (shift != 0xc) LLCC_SystemSetMode(0, false);
                return;
            }
            gLLCC4804 = gLLCC9F28;
            gLLCC4800 = gLLCC9F2C;
            gLLCCDB5A = 0;
            gLLCC9F24 = 0xffff;
            gLLCCDB58 = 0;
            gLLCC9F26 = 0xffff;
            gLLCC9F14 = -1;
            gLLCCE0DC = 0;
            gLLCC47C8 = 0;
            LLCC_SystemResetClickStack();
            LLCC_SystemSetModeNR(3, false);
            if (gLLCC4804 != x || gLLCC4800 != y || LLCC_SystemGetClickStackEntries() == 1) {
                LLCC_SystemAddToClickStack(gLLCC9F28, gLLCC9F2C);
                if (gLLCCDB4C != nullptr) gLLCCDB4C = nullptr;
                DrawLine(0x85, gLLCC9F28, gLLCC9F2C, &gLLCC9F28, &gLLCC9F2C);
                DrawLine(0x86, gLLCC9F28, gLLCC9F2C, &gLLCC9F28, &gLLCC9F2C);
                DrawLine(0x83, gLLCC9F28, gLLCC9F2C, &gLLCC9F28, &gLLCC9F2C);
                DrawLine(0x81, gLLCC9F28, gLLCC9F2C, &gLLCC9F28, &gLLCC9F2C);
                gCalcNeighboursPrevA = 0x80;
                gCalcNeighboursPrevB = 0x80;
                if (static_cast<int32_t>(MapCellAt(CellIndex(gLLCC9F28, gLLCC9F2C))->mType) == 1) LLCC_SystemSetMode(0, false);
            }
            return;
        }
        gLLCC4804 = x;
        gLLCC4800 = y;
        if (Current() != nullptr) {
            LLCR_FixBridges(Current(), false, false, false);
            LLCR_FixBridgesReverse(Current(), true, false, false);
        }
        return;
    }

    // ---- hoardings
    if (LLCC_SystemIsMode(0x19) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            LLCC_SystemResetClickStack();
            LLCC_SystemAddToClickStack(x, y);
            return;
        }
        Snap(&x, &y);
        const int32_t startX = gLLCC4804;
        if (startX != x || gLLCC4800 != y || (gHoardingRight == x && gHoardingBottom == y)) {
            const int32_t mode = LLCC_SystemGetMode();
            DrawTrackLine(mode, startX, gLLCC4800, &x, &y);
            DrawTrackLine(0x80, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x82, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x85, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x86, gLLCC4804, gLLCC4800, &x, &y);
            DrawTrackLine(0x81, gLLCC4804, gLLCC4800, &x, &y);
            LLCC_SystemAddToClickStack(x, y);
        }
        if (gLLCC4804 == x && gLLCC4800 == y) {
            EndDragWords();
            LLCC_SystemSetMode(0, false);
        } else {
            gLLCC4804 = x;
            gLLCC4800 = y;
        }
        return;
    }
    if (LLCC_SystemIsMode(0x39) != 0) {
        if (x < limitX) x = limitX;
        if (y < limitY) y = limitY;
        const int32_t right = gLLCCWidth - 2;
        if (right < x) x = right;
        const int32_t bottom = gLLCCHeight - 3;
        if (bottom < y) y = bottom;
        LLCM_TrackCellBoundary(gHoardingRight, gHoardingBottom, &x, &y, &x, &y);
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            LLCC_CreateCurrentHoardingImage();
            return;
        }
        if (LLCC_IsHoardingEditLegal(gLLCC4804, gLLCC4800, x, y, true) != 0) {
            LLCC_ReviseHoarding(gLLCC4804, gLLCC4800, x, y);
            const int32_t cost = LLCC_SystemGetSessionCost();
            gGameWorld->GetBankAccount()->MakeWithdrawal(static_cast<uint32_t>(cost));
        }
        gLLCC4804 = -1;
        gLLCC4800 = -1;
        LLCC_SystemSetMode(0, false);
        UI_SetGameWindowCursor(0);
        return;
    }

    // ---- land, deletion, lines, patrol areas
    if (LLCC_SystemIsMode(0x3a) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            return;
        }
        if (LLCC_IsClearLandLegal(gLLCC4804, gLLCC4800, x, y) != 0) LLCC_Kill_Them_KILL_THEM_ALL(gLLCC4804, gLLCC4800, x, y);
        gLLCC4804 = -1;
        gLLCC4800 = -1;
        LLCC_SystemSetMode(0, false);
        return;
    }
    if (LLCC_SystemIsMode(0x33) != 0) {
        gLLCCDB4C = nullptr;
        gLLCCDB48 = nullptr;
        gLLCCDB44 = 0;
        LLCC_SetRideRotation(0, false);
        LLCR_DeleteRide(x, y, 0);
        return;
    }
    if (LLCC_SystemIsMode(0x32) != 0) {
        if (static_cast<int32_t>(MapCellAt(CellIndex(x, y))->mType) != 4) {
            DrawLine(0x32, x, y, &x, &y);
            DrawLine(0x86, x, y, &x, &y);
        }
        return;
    }
    if (LLCC_SystemIsMode(0x34) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            return;
        }
        DrawLine(0x32, gLLCC4804, gLLCC4800, &x, &y);
        DrawLine(0x86, gLLCC4804, gLLCC4800, &x, &y);
        gLLCC4804 = x;
        gLLCC4800 = y;
        return;
    }
    if (LLCC_SystemIsMode(0x35) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            return;
        }
        DrawTrackLine(0x32, gLLCC4804, gLLCC4800, &x, &y);
        gLLCC4804 = x;
        gLLCC4800 = y;
        return;
    }
    if (LLCC_SystemIsMode(0x36) != 0) {
        if (gLLCC4804 == -1 && gLLCC4800 == -1) {
            gLLCC4804 = x;
            gLLCC4800 = y;
            gLLCCDB64 = gUIInterface.GetSelectedPeep();
            return;
        }
        if (gLLCCDB64 != nullptr) {
            const int32_t minX = x < gLLCC4804 ? x : gLLCC4804;
            const int32_t minY = y < gLLCC4800 ? y : gLLCC4800;
            const int32_t maxX = gLLCC4804 < x ? x : gLLCC4804;
            const int32_t maxY = gLLCC4800 < y ? y : gLLCC4800;
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> low;
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> high;
            low.mIndex = CellIndex(minX, minY);
            high.mIndex = CellIndex(maxX, maxY);
            reinterpret_cast<CStaff*>(gLLCCDB64)->SetPatrolArea(low, high);
        }
        gLLCC4804 = -1;
        gLLCC4800 = -1;
        LLCC_SystemSetMode(0, false);
        return;
    }

    // ---- queue and track editing
    if (LLCC_SystemIsMode(0x14) != 0) {
        LLCC_SystemConstructQueueClickStack(Current());
        Current()->RecalculateBackOfQueue();
        LLCC_SystemSetModeNR(3, false);
        return;
    }
    if (LLCC_SystemIsMode(0xe) != 0 || LLCC_SystemIsMode(0x13) != 0) {
        bool fixBridges = true;   // r13
        if (Current()->mF28 == 0) {
            TrackStackAgain();
        } else if (static_cast<int16_t>(gLLCC9F24) == -1) {
            CTrackCell* cell = TrackOrParent(CellIndex(x, y));
            if (cell != nullptr && cell->IsTrackStartBlock() == 0) {
                int32_t coverX = x;
                int32_t coverY = y;
                LLCM_CoverUpgrade(&coverX, &coverY);
                const int16_t here = cell->GetSegmentNumber();
                const int16_t covered = TrackCellAt(CellIndex(coverX, coverY))->GetSegmentNumber();
                if (covered < here) {
                    gLLCCDB54 = coverX;
                    gLLCCDB50 = coverY;
                }
                gLLCCDB5A = CellIndex(x, y);
                gLLCC9F24 = static_cast<uint16_t>(cell->GetSegmentNumber());
                if (cell->ConnectionType(static_cast<CELL_MODES>(0xe), 0, 0) == 1) {
                    gLLCCDB5A = 0;
                    gLLCC9F24 = 0xffff;
                }
                const int32_t whole = LLCC_BuildTrackDB(true);
                Current()->mF28 = whole;
                if (whole != 0 && Current()->IsOpenable() != 0) Current()->Reopen();
                fixBridges = false;
                if (whole == 0) TrackStackAgain();
            }
        } else {
            CTrackCell* cell = TrackOrParent(CellIndex(x, y));
            if (cell != nullptr && cell->IsTrackStartBlock() == 0) {
                int32_t coverX = x;
                int32_t coverY = y;
                LLCM_CoverUpgrade(&coverX, &coverY);
                const int16_t here = cell->GetSegmentNumber();
                const int16_t covered = TrackCellAt(CellIndex(coverX, coverY))->GetSegmentNumber();
                if (covered > here) {
                    gLLCCDB54 = coverX;
                    gLLCCDB50 = coverY;
                }
                gLLCCDB58 = CellIndex(x, y);
                gLLCC9F26 = static_cast<uint16_t>(TrackCellAt(gLLCCDB58)->GetSegmentNumber());
                CTrackCell* end = TrackOrParent(gLLCCDB58);
                if (end->ConnectionType(static_cast<CELL_MODES>(0xe), 0, 0) != 9) {
                    LLCC_SystemSetMode(0, false);
                } else {
                    if (static_cast<int16_t>(gLLCC9F24) > static_cast<int16_t>(gLLCC9F26))
                        TraceTrack(TrackCellAt(gLLCCDB58), TrackCellAt(gLLCCDB5A), 1);
                    else
                        TraceTrack(TrackCellAt(gLLCCDB5A), TrackCellAt(gLLCCDB58), 1);
                    LLCC_FixFirstBlock(false);
                    if (LLCR_FixBridges(Current(), true, false, false) == 0) LLCR_FixBridgesReverse(Current(), false, false, false);
                    gLLCCDB5A = 0;
                    gLLCC9F24 = 0xffff;
                    gLLCCDB58 = 0;
                    gLLCC9F26 = 0xffff;
                    gLLCC4804 = CellX(end->mCellId);
                    gLLCC4800 = CellY(end->mCellId);
                    const int32_t whole = LLCC_BuildTrackDB(false);
                    Current()->mF28 = whole;
                    if (whole != 0) {
                        LLCC_SetTrackAnim(LLCM_GetCurrentObject(), true);
                    } else {
                        LLCC_SetTrackAnim(LLCM_GetCurrentObject(), false);
                        LLCC_SetTrackAnimReverse(LLCM_GetCurrentObject(), false);
                    }
                    if (whole == 0) {
                        TrackStackAgain();
                    } else {
                        if (Current()->IsOpenable() != 0) Current()->Reopen();
                        LLCC_SystemSetModeNR(0, false);
                    }
                }
            }
        }
        if (fixBridges) {
            LLCR_FixBridges(Current(), false, false, false);
            LLCR_FixBridgesReverse(Current(), true, false, false);
        }
        return;
    }
    if (LLCC_SystemIsMode(0x1a) != 0) {
        LLCM_TrackCellBoundary(gLLCC9F18, gLLCC9F1C, &x, &y, &gLLCCDB54, &gLLCCDB50);
        if (static_cast<int16_t>(gLLCC9F24) == -1) {
            gLLCCDB5A = CellIndex(gLLCCDB54, gLLCCDB50);
            gLLCC9F24 = static_cast<uint16_t>(TrackCellAt(gLLCCDB5A)->GetSegmentNumber());
            if (TrackCellAt(gLLCCDB5A)->ConnectionType(static_cast<CELL_MODES>(0x1a), 0, 0) == 1) {
                gLLCCDB5A = 0;
                gLLCC9F24 = 0xffff;
            }
            return;
        }
        gLLCCDB58 = CellIndex(gLLCCDB54, gLLCCDB50);
        gLLCC9F26 = static_cast<uint16_t>(TrackCellAt(gLLCCDB58)->GetSegmentNumber());
        CTrackCell* end = TrackOrParent(gLLCCDB58);
        if (end->ConnectionType(static_cast<CELL_MODES>(0x1a), 0, 0) != 9) {
            gLLCCDB58 = gLLCCDB5A;
            gLLCC9F26 = gLLCC9F24;
            return;
        }
        int32_t traced;
        if (static_cast<int16_t>(gLLCC9F24) > static_cast<int16_t>(gLLCC9F26))
            traced = TraceTrack(TrackCellAt(gLLCCDB58), TrackCellAt(gLLCCDB5A), 1);
        else
            traced = TraceTrack(TrackCellAt(gLLCCDB5A), TrackCellAt(gLLCCDB58), 1);
        gLLCCDB5A = 0;
        gLLCC9F24 = 0xffff;
        gLLCCDB58 = 0;
        gLLCC9F26 = 0xffff;
        gLLCC4804 = CellX(end->mCellId);
        gLLCC4800 = CellY(end->mCellId);
        gLLCCReviseAnswer = 0;
        if (traced != -1) {
            LLCC_SystemConstructHoardingClickStack();
            LLCC_SystemSetModeNR(0x19, false);
        }
        return;
    }

    // ---- coaster pylons
    if (LLCC_SystemIsMode(0x15) != 0) {
        if (Recording() == 0) Sound(0x65, x, y);
        CoasterAcceptLeftMouseClick(0);
        Advise(0xcd);
        LLCC_SystemAddToClickStack(x, y);
        DrawLine(0x87, x, y, &x, &y);
        DrawLine(LLCC_SystemGetMode(), x, y, &x, &y);
        DrawLine(0x83, x, y, &x, &y);
        DrawLine(0x84, x, y, &x, &y);
        if (DrawLine(0x81, x, y, &x, &y) != 0) {
            PlacePylon(static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0, &TrackCellAt(CellIndex(x, y))->mMeshInstance);
            return;
        }
        EndTrack(0);
        if (gLLCCDB4C == nullptr) {
            LLCC_SystemSetMode(0, false);
            return;
        }
        LLCC_SystemSetModeNR(3, false);
        gLLCC4804 = gLLCC9F28;
        gLLCC4800 = gLLCC9F2C;
        if (gLLCC4804 != x || gLLCC4800 != y || LLCC_SystemGetClickStackEntries() == 1) {
            int32_t lx = gLLCC4804;
            int32_t ly = gLLCC4800;
            const uint16_t index = CellIndex(lx, ly);
            LLCC_SystemAddToClickStack(lx, ly);
            DrawLine(0x85, lx, ly, &lx, &ly);
            DrawLine(0x86, lx, ly, &lx, &ly);
            DrawLine(0x83, lx, ly, &lx, &ly);
            DrawLine(0x81, lx, ly, &lx, &ly);
            gCalcNeighboursPrevA = 0x80;
            gCalcNeighboursPrevB = 0x80;
            gLLCCDB4C = nullptr;
            if (static_cast<int32_t>(MapCellAt(index)->mType) == 1) LLCC_SystemSetMode(0, false);
        }
        return;
    }
    if (LLCC_SystemIsMode(0x16) != 0) {
        if (gLLCCE090 != 0) {
            LLCC_SystemAddToClickStack(x, y);
            DrawLine(0x87, x, y, &x, &y);
            DrawLine(0x15, x, y, &x, &y);
            DrawLine(0x83, x, y, &x, &y);
            DrawLine(0x81, x, y, &x, &y);
            const uint16_t index = CellIndex(x, y);
            const uint32_t result = DoMovePylon(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
            TrackCellAt(index)->mMeshInstance = result;
            gLLCC9F14 = -1;
            gLLCCE090 = 0;
        } else {
            CoasterAcceptLeftMouseClick(0);
        }
    }
}

}  // namespace

void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) {
    const int32_t limitY = gLLCC9F48;
    const int32_t limitX = gLLCC9F44;
    if (x == 0 && y == 0) CTRACE(6, "HUH?\n");
    if (Recording() != 0 && gLLCC4818 == 0) {
        gLLCCC214 = 0;
        if (gLLCCE0DC == 0) LLCM_MouseMoved(x, y, shift);
    }
    gTrackCostFree = 0;
    if (flag != 0) {
        x = gLLCCDB54;
        y = gLLCCDB50;
    }
    if ((LLCM_GetIllegalPlacement() == 0 && gLLCC4818 == 0) || LLCC_SystemIsMode(0x16) != 0) {
        if (shift == 0)
            gActionRecorder.RecordAction(static_cast<RecordedAction>(0x14), static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        else
            gActionRecorder.RecordAction(static_cast<RecordedAction>(0x2d), static_cast<uint8_t>(x), static_cast<uint8_t>(y), shift);
    }
    if (LLCC_SystemIsMode(0) != 0) gLLCCDB64 = gUIInterface.GetSelectedPeep();
    if (LLCC_SystemIsMode(0x38) == 0 && LLCM_GetIllegalPlacement() != 0) {
        IllegalPlacement(x, y);
    } else {
        LLCM_SetCurrentObject(false);
        if (LLCC_SystemIsMode(0) == 0) Modes(x, y, shift, limitX, limitY);
    }
    gLLCCDB34 = 0;
}

// LLCellMouse.cpp of the main app (unit v5182): LLCM_SetCurrentObject 0x10088590 (832 bytes) and LLCM_StartTrackEditMode 0x10088a64 (1,372 bytes).

// New words of the unit (initialised data of the image, the real names are unknown).
int32_t gLLCC9F18 = -1;         // 0x1029df18
int32_t gLLCC9F1C = -1;         // 0x1029df1c
int32_t gLLCC9F20 = -1;         // 0x1029df20
uint16_t gLLCCE028 = 0;         // 0x1029e028
uint16_t gLLCCE02A = 0;         // 0x1029e02a  (the image holds 0; unit v5L39, was 0xffff)
int32_t gLLCCE034 = -1;         // 0x1029e034
int32_t gLLCCE038 = -1;         // 0x1029e038
CTrackCell* gLLCCDB48 = nullptr;   // 0x102ddb48
uint16_t gLLCCDB5C = 0;         // 0x102ddb5c
CObject* gLLCMCurrentObject = nullptr;   // host only, see LLCellSystem.h

namespace {

// The track cell of a 1 based cell id: the world + 0x1102d8 + (id - 1) * 0x28 (plain arithmetic, id 0 gives the cell before the first).
CTrackCell* CellOf(uint16_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x1102d8 +
                                         static_cast<int64_t>(static_cast<int32_t>(id) - 1) * 0x28);
}

// (x + (y << 7) + 1) & 0xffff
uint16_t CellId(int32_t x, int32_t y) {
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
}

// The current object: gLLCCDB6C is a 32-bit word (a pointer on the Mac), so the whole pointer is kept in gLLCMCurrentObject beside it.
CObject* Current_2() {
    uint32_t word = static_cast<uint32_t>(gLLCCDB6C);
    if (word != 0 && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == word) return gLLCMCurrentObject;
    return nullptr;
}

void SetCurrent_2(CObject* object) {
    gLLCMCurrentObject = object;
    gLLCCDB6C = static_cast<int32_t>(reinterpret_cast<uintptr_t>(object));
}

// A balance value of a ride record (the Mac reads the word at 0x00c + 4 * slot; the host record keeps 8-byte values behind a
// 64-bit table header, so the Mac's offsets name other bytes here). Unit v5L28.
int32_t RideValue(const RideInfo* info, RideBalanceSlot slot) {
    return info->Value(slot).mInteger;
}

// The cell id of an object's cell (its x and y bytes).
uint16_t ObjectCellId(const CObject* object) { return CellId(object->mBase.mCellX, object->mBase.mCellY); }

}  // namespace

void LLCM_SetCurrentObject(bool flag) {
    gLLCCDB5C = gLLCCDB68;
    gLLCCDB68 = 0;
    gLLCC9F18 = -1;
    gLLCC9F1C = -1;
    gLLCC9F20 = -1;
    bool done = false;
    if (LLCC_SystemIsMode(0x19) != 0 || LLCC_SystemIsMode(0x1a) != 0) {
        int32_t right = gHoardingRight;
        if (right != -1) {
            int32_t bottom = gHoardingBottom;
            gLLCCDB68 = CellId(right, bottom);
            gLLCCDB48 = CellOf(gLLCCDB68);
            gLLCC9F18 = right;
            gLLCCFirstBlockX = right;
            gLLCC9F1C = bottom;
            gLLCCFirstBlockY = bottom;
            gLLCC9F20 = 0;
            gLLCCE034 = gLLCCFirstBlockX;
            gLLCCE038 = gLLCCFirstBlockY;
            gLLCCE0CC = 4;
            LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
            done = true;
        }
    }
    if (!done) {
        CObject* object = gUIInterface.GetSelectedObject();
        SetCurrent_2(object);
        if (object != nullptr) {
            if (object->GetUpgradeParent().mIndex != gLLCCE028) {
                uint16_t parent = object->GetUpgradeParent().mIndex;
                // (the Mac reads the object again from the word)
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
                CObject* upgrade = static_cast<CObject*>(gThingArray[parent].mThing);
                RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{Current_2()->mObjectType});
                if (RideValue(info, kRideSlot_UpgradeType) == 1) {
                    SetCurrent_2(upgrade);
                    gUIInterface.SetSelectedObject(upgrade);
                }
            }
            CObject* cur = Current_2();
            gLLCCDB68 = ObjectCellId(cur);
            gLLCCDB48 = CellOf(gLLCCDB68);
            uint32_t index = static_cast<uint32_t>(gLLCCDB68) - 1u;
            gLLCC9F18 = static_cast<int32_t>(index & 0x7f);
            gLLCCFirstBlockX = gLLCC9F18;
            gLLCC9F1C = static_cast<int32_t>(index >> 7);
            gLLCCFirstBlockY = gLLCC9F1C;
            gLLCC9F20 = cur->mRotation;
            gLLCCE034 = gLLCCFirstBlockX;
            gLLCCE038 = gLLCCFirstBlockY;
            gLLCCE0CC = LLCR_OffsetStartOfTrack(&gLLCCFirstBlockX, &gLLCCFirstBlockY, gLLCC9F20);
            LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
        }
    }
    CObject* cur = Current_2();
    if (cur != nullptr) {
        if (gLLCCDB68 != gLLCCDB5C && flag) gActionRecorder.RecordAction(0x32, ObjectCellId(cur));
    }
}

// 0x100888fc: the Mac's word; the host keeps the whole pointer in gLLCMCurrentObject while the two agree.
CObject* LLCM_GetCurrentObject() {
    if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gLLCMCurrentObject)) == static_cast<uint32_t>(gLLCCDB6C)) return gLLCMCurrentObject;
    return reinterpret_cast<CObject*>(static_cast<uintptr_t>(static_cast<uint32_t>(gLLCCDB6C)));
}

// 0x10088934 (12 bytes): one word store.
void LLCM_SetIllegalPlacement(int32_t placement) { gLLCMIllegalPlacement = placement; }

// 0x10088970
int32_t LLCM_GetIllegalPlacement() { return gLLCMIllegalPlacement; }

// Unit v5932: LLCR_OverRide (the pointer overload, 0x1008905c), LLCM_TrackCellBoundary, LLCR_SetRide, LLCR_AddRideToDB.

// 0x100889ac: snaps (*cellX, *cellY) down to the even offset from (x, y); both outputs get the same pair.
void LLCM_TrackCellBoundary(int32_t x, int32_t y, int32_t* cellX, int32_t* cellY, int32_t* outX, int32_t* outY) {
    int32_t dx = *cellX - x;
    int32_t dy = *cellY - y;
    if (dx < 0 && dx - (dx / 2) * 2 != 0) dx -= 1;
    if (dy < 0 && dy - (dy / 2) * 2 != 0) dy -= 1;
    int32_t nx = x + (dx / 2) * 2;
    int32_t ny = y + (dy / 2) * 2;
    *cellX = nx;
    *outX = nx;
    *cellY = ny;
    *outY = ny;
}

void LLCM_StartTrackEditMode(int32_t x, int32_t y, uint16_t) {
    if (Current_2() != nullptr) {
        RideInfo* info = Current_2()->GetDBInfo();
        if (info != nullptr) {
            LLCC_SystemSetRideQueue(RideValue(info, kRideSlot_HasQueue) != 0 ? 1 : 0);
            LLCC_SystemSetRideTrack(RideValue(info, kRideSlot_WhichTrackType));
        }
    }
    int32_t track = LLCC_SystemGetRideTrack();
    if (track == 2) {
        if (LLCC_SystemIsMode(0x13) == 0) LLCC_SystemSetModeNR(0x13, true);
    } else if (track < 2) {
        if (track != 0 && track >= 0) {
            if (LLCC_SystemIsMode(0xe) == 0) LLCC_SystemSetModeNR(0xe, true);
        }
    } else if (track == 4) {
        if (LLCC_SystemIsMode(0x1a) == 0) LLCC_SystemSetModeNR(0x1a, true);
    }
    if (gLLCCDB68 == gLLCCE02A) return;

    int32_t startX = gLLCC9F18;
    int32_t startY = gLLCC9F1C;
    gLLCCFirstBlockX = startX;
    gLLCCFirstBlockY = startY;
    gLLCC9F20 = Current_2()->mRotation;
    gLLCCE034 = startX;
    gLLCCE038 = startY;
    gLLCCE0CC = LLCR_OffsetStartOfTrack(&startX, &startY, gLLCC9F20);
    LLCR_OffsetEndOfTrack(&gLLCCE034, &gLLCCE038, gLLCC9F20);
    int32_t dx = x - startX;
    int32_t dy = y - startY;
    if (dx < 0 && dx != (dx / 2) * 2) dx -= 1;
    if (dy < 0 && dy != (dy / 2) * 2) dy -= 1;
    x = startX + (dx / 2) * 2;
    gLLCCDB54 = x;
    y = startY + (dy / 2) * 2;
    gLLCCDB50 = y;
    if (static_cast<int16_t>(gLLCC9F24) == -1) {
        int32_t endX = x;
        int32_t endY = y;
        LLCM_CoverUpgrade(&endX, &endY);
        int32_t mode = LLCC_SystemGetMode();
        LLCC_DrawTrackLine(static_cast<CELL_MODES>(mode | 0x100), x, y, &endX, &endY);
    }
    CObject* object = Current_2();
    if (object->mF28 == 0) {
        LLCC_SystemConstructTrackClickStack(object);
        gDynamicCellF9BC4 = static_cast<float>(gLLCC4804);
        gDynamicCellF9BC0 = static_cast<float>(gLLCC4800);
    } else if (static_cast<int16_t>(gLLCC9F24) != -1) {
        CTrackCell* first = CellOf(CellId(x, y));
        int32_t coverX = x;
        int32_t coverY = y;
        LLCM_CoverUpgrade(&coverX, &coverY);
        CTrackCell* covered = CellOf(CellId(coverX, coverY));
        int16_t firstSegment = first->GetSegmentNumber();
        int16_t coveredSegment = covered->GetSegmentNumber();
        if (coveredSegment > firstSegment) {
            x = coverX;
            y = coverY;
        }
        gLLCCDB58 = CellId(x, y);
        gLLCC9F26 = static_cast<uint16_t>(CellOf(gLLCCDB58)->GetSegmentNumber());
        CTrackCell* cell = CellOf(gLLCCDB58);
        int32_t mode = LLCC_SystemGetMode();
        if (cell->ConnectionType(static_cast<CELL_MODES>(mode), 0, 0) != 9) {
            gLLCCDB58 = gLLCCDB5A;
            gLLCC9F26 = gLLCC9F24;
            gLLCMIllegalPlacement = 0;
            gLLCCC214 = 0;
            gLLCCC210 = 0;
        }
        if (static_cast<int16_t>(gLLCC9F24) > static_cast<int16_t>(gLLCC9F26)) {
            LLCC_TraceTrack(CellOf(gLLCCDB58), CellOf(gLLCCDB5A), 0);
        } else {
            LLCC_TraceTrack(CellOf(gLLCCDB5A), CellOf(gLLCCDB58), 0);
        }
    }
}

// 0x10088ff4
void __sinit_LLCellMouse_cpp() {
    gLLCCDB78 = 0;
    gLLCCDB74 = 0x10000;
    gLLCCDB68 = 0;
    gLLCCDB5C = 0;
    gLLCCDB5A = 0;
    gLLCCDB58 = 0;
}

// Unit v51119: the world globe's camera steps and list registration (CLBScreenComponentOnlineGlobe), and the small
// destructors and byte swaps the neighbouring addresses hold.

// 0x10082c88
MapDelta* MapDelta_DtorMac(MapDelta* self, int16_t flag) {
    if (self != nullptr && flag > 0) OstreamDelete(self);
    return self;
}
