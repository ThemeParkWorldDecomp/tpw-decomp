#include "MapWho.h"
#include "Wavelet.h"
#include "m3d2.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "calllog.h"
#include "platform/arena.h"
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
using namespace calllog;
namespace {
constexpr int N = 32;
CMapWho* map;
alignas(CMapWho) unsigned char storage[sizeof(CMapWho)];
SMapLinkType nodes[N];
SMapLinkType* cells[6];
SWindow dummy[4];
std::vector<void*> allocations;
bool failAllocation;
bool floatAllocation;
struct Link : CMapLinkBase {
    int id, type;
    bool data;
    tag_sM3D2Header header;
    int32_t Prepare() override { return 1; }
    int32_t GetLinkType() override { ev("Type", {id}); return type; }
    void* GetData() override { ev("Data", {id}); return data ? &header : nullptr; }
} links[4];
int Node(const SMapLinkType* p) {
    if (p == nullptr) return -1;
    for (int i = 0; i < N; ++i) if (p == nodes + i) return i;
    return -2;
}
int Label(const CMapLinkBase* p) {
    if (p == nullptr) return 0;
    for (int i = 0; i < 4; ++i) if (p == &links[i]) return i + 1;
    return -1;
}
void* Malloc(uint32_t bytes) {
    ev("Malloc", {static_cast<int64_t>(bytes / (floatAllocation ? sizeof(float) : sizeof(SMapLinkType*)))});
    floatAllocation = false;
    if (failAllocation) return nullptr;
    void* p = std::calloc(1, bytes);
    allocations.push_back(p);
    return p;
}
float Float(uint32_t w) { float f; std::memcpy(&f, &w, 4); return f; }
}
TbMallocFn gMalloc = Malloc;
namespace { void Free(void*& p) { ev("Free"); p = nullptr; } }
TbFreeFn gFree = Free;
CMatrix view;
CMatrix* gei_ViewMatrix = &view;
CDisplay::CDisplay() {}
CDisplay gei_Display;
CSystem::CSystem() {}
CSystem gei_System;
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Error", {level, code}); }
CMapLinkBase::~CMapLinkBase() {}
CMapWho::CMapWho() {}
CMapWho::~CMapWho() {}
int32_t CMapWho::Close() { ev("Close"); mMap = nullptr; return 1; }
CGamut::CGamut() {}
void CGamut::SetYClipper(float lo, float hi) { ev("YClip", {fbits(lo), fbits(hi)}); mYClipLow = lo; mYClipHigh = hi; }
CHField::CHField() {}
CHField::~CHField() {}
void CHField::AdjustHField() { ev("Adjust"); }
CGamut gamut;
CGamut* gei_Gamut = &gamut;
CMapWho* gei_MapWho;

extern "C" {
void tpw_ms_begin(uint32_t flags, int32_t windowCount, int32_t ownWindow) {
    for (auto* p : allocations) std::free(p);
    allocations.clear();
    std::memset(storage, 0xa5, sizeof storage);
    map = new (storage) CMapWho;
    gei_MapWho = map;
    std::memset(nodes, 0xa5, sizeof nodes);
    for (int i = 0; i < N; ++i) nodes[i].mNext = i + 1 < N ? &nodes[i + 1] : nullptr;
    map->mNodes = nodes; map->mFree = nodes; map->mNodeCount = N; map->mNodesUsed = 0;
    std::memset(cells, 0, sizeof cells);
    map->mMap = cells; map->mWidth = 3; map->mHeight = 2;
    for (auto& p : map->mF14) p = nullptr;
    map->mF70 = windowCount;
    for (int i = 0; i < 4; ++i) map->mWindows[i] = i == ownWindow ? &map->mGameWindow : &dummy[i];
    gei_System.mFlags = flags;
    for (int i = 0; i < 4; ++i) { links[i].id = i + 1; links[i].type = i % 3; links[i].data = true; }
    failAllocation = false; floatAllocation = false;
}
int32_t tpw_ms_op(int32_t op, const int32_t* a) {
    auto* link = &links[a[0] & 3];
    switch (op) {
    case 1: return map->Link(link, a[1]);
    case 2: return map->Unlink(link, a[1]);
    case 3: return map->Reset(a[0]);
    case 4: return map->Link(a[1], a[2], a[3], a[4], link, 0);
    case 5: return map->Unlink(a[1], a[2], a[3], a[4], link);
    case 6: return map->Reset(a[0], a[1], a[2], a[3]);
    case 7: return map->ResetAll();
    case 8: gei_System.mFlags = a[0]; return 0;
    case 9:
        link->type = a[1]; link->data = a[2];
        for (int i = 0; i < 4; ++i) link->header.mFA8[i] = a[3 + i];
        return 0;
    case 10: map->mFree = a[0] < 0 ? nullptr : &nodes[a[0]]; return 0;
    case 11: map->QSetup_Game(); return 0;
    case 12: failAllocation = a[2]; return map->Init(a[0], a[1]);
    case 13: map->mMap = nullptr; return 0;
    }
    return -99;
}
int32_t tpw_ms_state(int32_t* out) {
    int n = 0;
    out[n++] = gei_System.mFlags; out[n++] = map->mNodesUsed; out[n++] = Node(map->mFree);
    for (auto* p : map->mF14) out[n++] = Node(p);
    out[n++] = map->mWidth; out[n++] = map->mHeight; out[n++] = map->mMap != nullptr;
    if (map->mMap) for (int i = 0; i < map->mWidth * map->mHeight; ++i) out[n++] = Node(map->mMap[i]);
    for (const auto& p : nodes) { out[n++] = Label(p.mLink); out[n++] = Node(p.mNext); }
    out[n++] = map->mF70;
    out[n++] = map->mGameWindow.mFlags;
    for (auto v : {map->mGameWindow.mX, map->mGameWindow.mY, map->mGameWindow.mWidth, map->mGameWindow.mHeight}) out[n++] = fbits(v);
    out[n++] = map->mGameWindow.mMatrix == gei_ViewMatrix ? 1 : -1;
    for (int i = 0; i < 4; ++i) out[n++] = map->mWindows[i] == &map->mGameWindow ? 1 : 10 + i;
    return n;
}
void tpw_ms_gamut(uint32_t fov, uint32_t aspect, uint32_t* out) {
    CGamut gamut;
    std::memset(&gamut, 0xa5, sizeof gamut);
    gei_Display.mF04 = Float(aspect);
    gamut.Init(Float(fov));
    std::memcpy(out, &gamut, sizeof gamut);
}
void tpw_ms_hfield(const uint32_t* seed, uint32_t* out) {
    tpw_ms_begin(0, 0, -1);
    static auto* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    static auto* field = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    static auto* heights = static_cast<float*>(tpw::ArenaAllocZeroed(48));
    static float oldBuffer[12];
    for (int i = 0; i < 12; ++i) { heights[i] = Float(seed[11 + i]); oldBuffer[i] = 42.0f; }
    header->mHField = field;
    field->mHeights = heights; field->mHeightCount = 12;
    field->mF18[0] = seed[3]; field->mF18[1] = seed[4];
    for (int i = 0; i < 4; ++i) field->mF08[i] = Float(seed[5 + i]);
    field->mF20[0] = Float(seed[9]); field->mF20[1] = Float(seed[10]);
    CHField h;
    std::memset(&h, 0xa5, sizeof h); std::memset(&gamut, 0xa5, sizeof gamut);
    h.mF12970 = seed[1] ? oldBuffer : nullptr;
    gei_Display.mFlags = seed[0]; failAllocation = seed[2]; floatAllocation = (seed[0] & 0x10000) != 0;
    h.Init(header);
    int n = 0;
    out[n++] = h.mF00 == field; out[n++] = h.mF04 != nullptr;
    out[n++] = h.mGridWidth; out[n++] = h.mGridHeight;
    out[n++] = fbits(h.mInvCellX); out[n++] = fbits(h.mInvCellZ); out[n++] = fbits(h.mInvHeightRange);
    out[n++] = h.mF20; out[n++] = h.mF24; out[n++] = h.mF12968;
    out[n++] = h.mF12970 != nullptr;
    for (int i = 0; i < 12; ++i) out[n++] = h.mF12970 ? fbits(static_cast<float*>(h.mF12970)[i]) : 0;
    out[n++] = map->mWidth; out[n++] = map->mHeight; out[n++] = map->mMap != nullptr; out[n++] = map->mF70;
    std::memcpy(out + n, &gamut, sizeof gamut);
}
}
