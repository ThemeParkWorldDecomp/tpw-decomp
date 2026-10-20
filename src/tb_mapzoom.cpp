// tb_mapzoom.cpp range of the engine library (SimTheme Park 2.04b3; CMapWho is from
// tb_map.cpp by its own assertion): the light manager's and the map lists' start-up and
// shutdown. PARTIAL: the rest of the range (lighting, linking, rendering) is still to do.
#include "MapWho.h"

#include <new>

#include "Wavelet.h"
#include "tb_error.h"

// ---- The light manager ------------------------------------------------------------------
CLightManager* gei_LightManager = nullptr;
float GlobalDiffuse[4];
class CLight;
CLight* gev_LightChain;

int32_t LightManager_Init() {
    if (gei_LightManager != nullptr) return 0;
    gei_LightManager = new (std::nothrow) CLightManager;
    if (gei_LightManager == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t LightManager_Close() {
    if (gei_LightManager == nullptr) return 0;
    delete gei_LightManager;
    gei_LightManager = nullptr;
    return 1;
}

CLightManager::CLightManager() {
    mF00 = 0;
    mF04 = -1;
    GlobalDiffuse[0] = 1.0f;
    GlobalDiffuse[1] = 0.15f;
    GlobalDiffuse[2] = 0.15f;
    GlobalDiffuse[3] = 0.15f;
}

// ---- Link bases -------------------------------------------------------------------------
CMapLinkBase* gLinkBases[0x200];
int32_t gLinkBaseCount = 0;

int32_t RegisterLinkBase(CMapLinkBase* link) {
    if (gLinkBaseCount < 0x200) {
        gLinkBases[gLinkBaseCount] = link;
        ++gLinkBaseCount;
    }
    return 1;
}

int32_t FreeLinkBase(CMapLinkBase* link) {
    int32_t i = gLinkBaseCount - 1;
    while (i >= 0 && gLinkBases[i] != link) --i;
    if (i < 0) return 0;
    const int32_t count = gLinkBaseCount;
    gLinkBaseCount = count - 1;
    gLinkBases[i] = gLinkBases[count - 1];
    return 1;
}

CMapLinkBase::~CMapLinkBase() {}

// ---- CMapWho ----------------------------------------------------------------------------
CMapWho* gei_MapWho = nullptr;

int32_t MapWho_Init() {
    if (gei_MapWho != nullptr) return 0;
    gei_MapWho = new (std::nothrow) CMapWho;
    if (gei_MapWho == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t MapWho_Close() {
    CMapWho* map = gei_MapWho;
    if (map == nullptr) return 0;
    map->Close();
    delete gei_MapWho;
    gei_MapWho = nullptr;
    return 1;
}

CMapWho::CMapWho() {
    mNodes = nullptr;
    mFree = nullptr;
    mNodeCount = 0;
    mNodesUsed = 0;
    mF40 = 0x30;
    mHeight = 0;
    mWidth = 0;
    mF70 = 0;
    mF30 = 0;
    mMap = nullptr;
    mF34 = 0;
    mFinalCallback = nullptr;
    mF74 = -1;
    for (auto& value : mF14) value = nullptr;
    // Every entry goes on the free list, the last one first in line.
    bool ready = false;
    mNodes = static_cast<SMapLinkType*>(gMalloc(kNodeCount * sizeof(SMapLinkType)));
    if (mNodes != nullptr) {
        for (int32_t i = 0; i < kNodeCount; ++i) {
            mNodes[i].mNext = mFree;
            mFree = &mNodes[i];
        }
        mNodeCount = kNodeCount;
        ready = true;
        mNodesUsed = 0;
    }
    if (!ready && mNodes != nullptr) {
        gFree(reinterpret_cast<void*&>(mNodes));
        mNodeCount = 0;
        mNodesUsed = 0;
    }
}

CMapWho::~CMapWho() {
    if (mNodes != nullptr) {
        gFree(reinterpret_cast<void*&>(mNodes));
        mNodeCount = 0;
        mNodesUsed = 0;
    }
    Close();
}

void CMapWho::SetFinalCallback(void (*callback)()) { mFinalCallback = callback; }

int32_t CMapWho::Close() {
    if (mMap != nullptr) gFree(reinterpret_cast<void*&>(mMap));
    return 1;
}
