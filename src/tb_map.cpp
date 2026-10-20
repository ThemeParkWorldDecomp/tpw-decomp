#include "tpw_probe.h"
// Map setup and list operations from the original engine's tb_map.cpp.
#include "MapWho.h"
#include "m3d2.h"
#include "tb_matrix.h"
#include "tb_error.h"
#include "tb_resource_system.h"
#include "Wavelet.h"
#include <cstring>

int32_t CMapWho::Init(int32_t width, int32_t height) {
    if (mMap != nullptr) Close();
    mWidth = width;
    mHeight = height;
    const uint32_t cells = static_cast<uint32_t>(width * height);
    mMap = static_cast<SMapLinkType**>(gMalloc(cells * sizeof(SMapLinkType*)));
    if (mMap == nullptr) gei_ErrorHandler.Push(3, kErrOutOfMemory);
    else {
        std::memset(mMap, 0, cells * sizeof(SMapLinkType*));
        QSetup_Game();
    }
    return mMap != nullptr;
}

void CMapWho::QSetup_Game() {
    mGameWindow.mFlags = 0x1f31;
    mGameWindow.mX = mGameWindow.mY = -1.0f;
    mGameWindow.mWidth = mGameWindow.mHeight = 2.0f;
    mGameWindow.mMatrix = gei_ViewMatrix;
    if (mF70 < 4) {
        int32_t i = 0;
        while (i < mF70 && mWindows[i] != &mGameWindow) ++i;
        if (i == mF70) mWindows[mF70++] = &mGameWindow;
    }
    // The original keeps just one window even if it was already registered later.
    mF70 = 1;
}

int32_t CMapWho::Link_Add(SMapLinkType** head, CMapLinkBase* link) {
    tpw_probe::Hit(tpw_probe::kMapLinkAdd);
    SMapLinkType* node = mFree;
    if (node != nullptr) mFree = node->mNext;
    ++mNodesUsed;
    if (node == nullptr) { tpw_probe::Hit(tpw_probe::kMapLinkNoNode); return 0; }
    node->mLink = link;
    for (auto* existing = *head; existing != nullptr; existing = existing->mNext)
        if (existing->mLink == link) { tpw_probe::Hit(tpw_probe::kMapLinkDuplicate); return 1; }
    // Duplicate links consume a free node in the original too.
    SMapLinkType* previous = nullptr;
    SMapLinkType* next = *head;
    if (link->GetLinkType() != 1) {
        while (next != nullptr && next->mLink->GetLinkType() == 1) {
            previous = next;
            next = next->mNext;
        }
    }
    node->mNext = next;
    if (previous == nullptr) *head = node;
    else previous->mNext = node;
    gei_System.mFlags |= 0x80000000u;
    return 1;
}

int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    if ((list & 0x70) != 0) return Link_Add(&mF14[(list >> 4) - 1], link);
    if ((list & 1) != 0 && link->GetLinkType() == 2) {
        auto* data = static_cast<tag_sM3D2Header*>(link->GetData());
        if (data != nullptr) return Link(static_cast<int8_t>(data->mFA8[3]), static_cast<int8_t>(data->mFA8[1]),
                                       static_cast<int8_t>(data->mFA8[2]), static_cast<int8_t>(data->mFA8[0]), link, 0);
    }
    return 0;
}

namespace {
int32_t Remove(CMapWho& map, SMapLinkType*& head, CMapLinkBase* link) {
    SMapLinkType* previous = nullptr;
    SMapLinkType* node = head;
    while (node != nullptr && node->mLink != link) { previous = node; node = node->mNext; }
    if (node == nullptr) { gei_System.mFlags |= 0x80000000u; return 0; }
    if (previous == nullptr) head = node->mNext;
    else previous->mNext = node->mNext;
    node->mNext = map.mFree;
    map.mFree = node;
    --map.mNodesUsed;
    return 1;
}
void Clear(CMapWho& map, SMapLinkType*& head) {
    while (head != nullptr) {
        auto* next = head->mNext;
        head->mNext = map.mFree;
        map.mFree = head;
        --map.mNodesUsed;
        head = next;
    }
}
bool Clip(CMapWho& map, int32_t& x0, int32_t& z0, int32_t& x1, int32_t& z1) {
    if (x0 >= map.mWidth || x1 < 0 || z0 >= map.mHeight || z1 < 0) return false;
    if (x0 < 0) x0 = 0;
    if (map.mWidth <= x1) x1 = map.mWidth - 1;
    if (z0 < 0) z0 = 0;
    if (map.mHeight <= z1) z1 = map.mHeight - 1;
    return true;
}
}

int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    if ((list & 0x70) == 0) return 0;
    return Remove(*this, mF14[(list >> 4) - 1], link);
}
int32_t CMapWho::Reset(int32_t list) {
    if ((list & 0x70) == 0) return 0;
    Clear(*this, mF14[(list >> 4) - 1]);
    gei_System.mFlags |= 0x80000000u;
    return 1;
}
int32_t CMapWho::Link(int32_t x0, int32_t z0, int32_t x1, int32_t z1, CMapLinkBase* link, int32_t) {
    if (!Clip(*this, x0, z0, x1, z1)) return 0;
    int32_t result = 1;
    for (int32_t x = x0; x <= x1; ++x)
        for (int32_t z = z0; z <= z1; ++z) result &= Link_Add(&mMap[x + z * mWidth], link);
    return result;
}
int32_t CMapWho::Unlink(int32_t x0, int32_t z0, int32_t x1, int32_t z1, CMapLinkBase* link) {
    if (mMap == nullptr) return 1;
    if (!Clip(*this, x0, z0, x1, z1)) return 0;
    int32_t result = 1;
    for (int32_t z = z0; z <= z1; ++z)
        for (int32_t x = x0; x <= x1; ++x) result &= Remove(*this, mMap[x + z * mWidth], link);
    return result;
}
int32_t CMapWho::Reset(int32_t x0, int32_t z0, int32_t x1, int32_t z1) {
    if (!Clip(*this, x0, z0, x1, z1)) return 0;
    for (int32_t z = z0; z <= z1; ++z)
        for (int32_t x = x0; x <= x1; ++x) {
            Clear(*this, mMap[x + z * mWidth]);
            gei_System.mFlags |= 0x80000000u;
        }
    return 1;
}
int32_t CMapWho::ResetAll() {
    Reset(0, 0, mWidth - 1, mHeight - 1);
    for (auto& head : mF14) { Clear(*this, head); gei_System.mFlags |= 0x80000000u; }
    return 1;
}
