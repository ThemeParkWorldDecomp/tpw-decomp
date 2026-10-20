// Leaves the park's second-stage set-up calls (main app; unit w112a): the coaster track extruder's storage, the object instance
// table's set-up, the texture set loader and freer of the dynamic meshes (TPAdvisor.cpp range of the inventory) and the two
// static texture sets with their entries (initialised data of the image).
#include "LevelSystems.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "analyser.h"

// ---- The extruder's storage (names invented) ----
uint32_t gExtrudeFaceStoreCount;
void* gExtrudeFaceStoreB;
void* gExtrudeFaceStoreA;

// 0x1004e60c (96 bytes): stores the count first, then allocates (zeroed) `b` records of 0x1c bytes and `a` points of 0x10 bytes.
// The products are 32-bit and wrap. Nothing is freed first: a second call leaks the old blocks.
void CreateExtrudeFaceStorage(uint32_t a, uint32_t b) {
    gExtrudeFaceStoreCount = b;
    gExtrudeFaceStoreB = MReleaseAllocF(b * 0x1c, 1);
    gExtrudeFaceStoreA = MReleaseAllocF(a << 4, 1);
}

// 0x101ef1b0 (12 bytes): only the third counter is written (the object table's "lowest" word); the first two are not touched.
void SetupObjectInstanceTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest) { lowest = 1; }

// 0x1003013c (8 bytes): a stub that loads nothing and always answers 1.
int32_t DMLoadTextures(sDTextureSet* set) { return 1; }

// 0x10030178 (20 bytes): clears bit 0 of the set's first word. (The original answers 1; no caller reads it.)
void DMFreeTextures(sDTextureSet* set) { set->mFlags &= 0xfffffffeu; }

// ---- The static texture sets: initialised data (entry: file name, flags, +8, +0xc) ----
sDTextureEntry gRideHoardingTextureEntries[4] = {
    {const_cast<char*>("Closed.tga"), 0, 1, 0},
    {const_cast<char*>("Hoarding.tga"), 0, 1, 0},
    {const_cast<char*>("Condemn.tga"), 0, 1, 0},
    {const_cast<char*>("Upgrade.tga"), 0, 1, 0},
};
sDTextureEntry gDynamicCellTextureEntries[20] = {
    {const_cast<char*>("blue.tga"), 7, 0, 0},
    {const_cast<char*>("red.tga"), 7, 0, 0},
    {const_cast<char*>("orange.tga"), 7, 0, 0},
    {const_cast<char*>("m_enter.tga"), 7, 0, 0},
    {const_cast<char*>("m_exit.tga"), 7, 0, 0},
    {const_cast<char*>("m_direct.tga"), 7, 0, 0},
    {const_cast<char*>("m_front.tga"), 7, 0, 0},
    {const_cast<char*>("m_inout.tga"), 7, 0, 0},
    {const_cast<char*>("m_link.tga"), 7, 0, 0},
    {const_cast<char*>("m_break.tga"), 7, 0, 0},
    {const_cast<char*>("m_cross.tga"), 7, 0, 0},
    {const_cast<char*>("m_end.tga"), 7, 0, 0},
    {const_cast<char*>("m_nocash.tga"), 7, 0, 0},
    {const_cast<char*>("m_erase.tga"), 7, 0, 0},
    {const_cast<char*>("red.tga"), 7, 0, 0},
    {const_cast<char*>("gby_sur1.tga"), 3, 0, 0},
    {const_cast<char*>("gby_sur2.tga"), 3, 0, 0},
    {const_cast<char*>("gby_sur3.tga"), 3, 0, 0},
    {const_cast<char*>("gte_lgo1.tga"), 3, 0, 0},
    {const_cast<char*>("gte_lgo1.tga"), 3, 0, 0},
};
// (count, entries, flags, folder)
sDTextureSet gRideHoardingTextures = {4, gRideHoardingTextureEntries, 0, nullptr};
sDTextureSet gDynamicCellTextures = {20, gDynamicCellTextureEntries, 0, "Data:Generic:Dynamic:Textures"};
