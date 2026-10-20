// The cell system's mesh loaders (main app, LLCellMouse.cpp range; unit w112a): the queue, go-kart track, water ride track and
// hoarding meshes. Each loader sets the size pair LoadRideType gets, formats the theme's folder for its mesh set and loads the
// ride type of every table entry; each destroyer hands the entries' types to DestroyRideInstance (the original passes the TYPE
// number where an instance number belongs, and never clears the table). The tables are initialised data of the image; the names
// of the tables, their entries and the size pairs are invented.
#include <cstdio>

#include "Debug.h"
#include "LLCellSystem.h"
#include "Main.h"
#include "Quickload.h"
#include "TPEngine.h"

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

// 0x10081dd0 (284 bytes): the go-kart track meshes; the table in front of this one is the queue table.
void LLCM_LoadKTrackMeshes() {
    char folder[kTrackFolderSize];
    gLLCMTrackSize[0] = 2;
    gLLCMTrackSize[1] = 2;
    std::snprintf(folder, sizeof folder, "%s:rides:gokarts", get_theme_path());
    LoadTrackTable(gLLCMKTrackMeshes, kKTrackCount, gLLCMQMeshes[kQCount - 1].mRideType, true, folder, kKTrackIdBase,
                   kTrackFlags);
}

// 0x10082174 (284 bytes): the water ride track meshes, the K loader's shape; the table in front is the go-kart table.
void LLCM_LoadWTrackMeshes() {
    char folder[kTrackFolderSize];
    gLLCMTrackSize[0] = 2;
    gLLCMTrackSize[1] = 2;
    std::snprintf(folder, sizeof folder, "%s:rides:wateride", get_theme_path());
    LoadTrackTable(gLLCMWTrackMeshes, kWTrackCount, gLLCMKTrackMeshes[kKTrackCount - 1].mRideType, true, folder, kWTrackIdBase,
                   kTrackFlags);
}

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

// 0x10081bc8 (100 bytes): destroys the "instance" numbered like each entry's type, for every entry with a name. The types stay.
void LLCM_DestroyQMeshes() {
    for (uint32_t i = 0; i < kQCount; ++i)
        if (gLLCMQMeshes[i].mName != nullptr) DestroyRideInstance(gLLCMQMeshes[i].mRideType);
}

// 0x10081f18 (112 bytes): the same, skipping names that start with '-'.
void LLCM_DestroyKTrackMeshes() {
    for (uint32_t i = 0; i < kKTrackCount; ++i) {
        const sLLCMMesh& entry = gLLCMKTrackMeshes[i];
        if (entry.mName != nullptr && entry.mName[0] != '-') DestroyRideInstance(entry.mRideType);
    }
}

// 0x100822bc (100 bytes): the Q destroyer's shape, with no '-' test (entry 0, "-", destroys instance 0).
void LLCM_DestroyWTrackMeshes() {
    for (uint32_t i = 0; i < kWTrackCount; ++i)
        if (gLLCMWTrackMeshes[i].mName != nullptr) DestroyRideInstance(gLLCMWTrackMeshes[i].mRideType);
}

// 0x10082688 (100 bytes): the same for the hoardings.
void LLCM_DestroyHoardingMeshes() {
    for (uint32_t i = 0; i < kHoardingCount; ++i)
        if (gLLCMHoardingMeshes[i].mName != nullptr) DestroyRideInstance(gLLCMHoardingMeshes[i].mRideType);
}

// 0x10088934 (12 bytes): one word store.
void LLCM_SetIllegalPlacement(int32_t placement) { gLLCMIllegalPlacement = placement; }
