// Test library for the online islands screen's small members (unit v5255; libtpw_unit_CLBScreenComponentOnlineIslands_GotoNextIsland.dylib,
// tests/test_CLBScreenComponentOnlineIslands_GotoNextIsland.py), main app:
//   src/CLBScreenComponentOnlineIslands_GotoNextIsland.cpp   Tick, SelectCurrentIsland, EnterCurrentIsland, GotoNextIsland,
//                                                            DownloadFailFunction, HandleOnlineBacktrack
//
// The callees outside the unit are recording fakes. Objects are told apart in the log by a tag word at offset 0x200 of their block. The virtual
// calls of HandleOnlineBacktrack reach FakeComp objects (placement new into the block), whose Show and Hide record.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "lobby.h"

using namespace calllog;

CLBLobby* gLobby = nullptr;
CLBScreenSystem* gLobbyScreens = nullptr;

namespace {

constexpr size_t kTagOffset = 0x200;
constexpr size_t kBlock = 0x240;

int64_t Tag(const void* p) {
    if (!p) return 0;
    uint32_t v;
    std::memcpy(&v, static_cast<const char*>(p) + kTagOffset, 4);
    return v;
}

class FakeComp : public CLBScreenComponent {
public:
    void Init() override {}
    void Tick() override {}
    void Render() override {}
    void Close() override {}
    void WindowMessagesFn(int32_t, int32_t, int32_t) override {}
    uint32_t ProcessCancel() override { return 0; }
    void Show() override { ev("Show", {Tag(this)}); }
    void Hide() override { ev("Hide", {Tag(this)}); }
};

}  // namespace

CLBScreenComponent::CLBScreenComponent() { mVisible = 0; }
CLBScreenComponent::~CLBScreenComponent() {}
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() { return mVisible; }

// ---- the fakes
void CLBScreenComponentIslands::TickIslands() { ev("IslandsTick", {Tag(this)}); }
void CLBScreenComponentIslands::GotoNextIsland() { ev("IslandsGotoNext", {Tag(this)}); }
void ShowCityLobbyController() { ev("ShowCityLobbyController", {}); }
void HideCityLobbyController() { ev("HideCityLobbyController", {}); }
void UpdateCityLobbyUI(CLBIsland* island) { ev("UpdateCityLobbyUI", {Tag(island)}); }
void CLBMeshInstance::PlayAnim(int32_t anim, int32_t loop) { ev("PlayAnim", {Tag(this), anim, loop}); }
CGameType::CGameType() { ev("CGameType", {}); }
void CGameType::SetGameType(GameType type) { ev("SetGameType", {static_cast<int32_t>(type)}); }
void IslandOnlineParkInfoPanelCancelCallback() {}
void OnlineIslandParkDownloadCompleteCallback(int32_t, const void*) {}
void CreateOnlineParkInfoPanel(const CFSOnlinePark* park, InterfaceWindow* parent, void (*onCancel)(),
                               void (*onDownload)(int32_t, const void*), void* context) {
    ev("CreateOnlineParkInfoPanel",
       {Tag(park), static_cast<int64_t>(reinterpret_cast<uintptr_t>(parent)), onCancel == IslandOnlineParkInfoPanelCancelCallback ? 1 : 0,
        onDownload == OnlineIslandParkDownloadCompleteCallback ? 1 : 0, Tag(context)});
}

extern "C" {

uint64_t tpw_oi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLobby", &gLobby},
        {"gLobbyScreens", &gLobbyScreens},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_oi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the fields the module writes by name.
int32_t tpw_oi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenComponentOnlineIslands, mCurrent));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenComponentOnlineIslands, mCameraState));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenComponentOnlineIslands, mFE4));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenComponentOnlineIslands, mFE8));
    out[n++] = static_cast<int64_t>(offsetof(CLBIsland, mPark));
    out[n++] = static_cast<int64_t>(offsetof(CLBIsland, mMeshB));
    out[n++] = static_cast<int64_t>(offsetof(CLBLobby, mState));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenSystem, mDownloadGlobe));
    out[n++] = static_cast<int64_t>(sizeof(CLBScreenComponentOnlineIslands));
    out[n++] = static_cast<int64_t>(kTagOffset);
    return n;
}

// Makes a FakeComp (vtable with recording Show and Hide) in the block.
void tpw_oi_vobj(uint64_t block) { new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) FakeComp(); }

static CLBScreenComponentOnlineIslands* O(uint64_t p) {
    return reinterpret_cast<CLBScreenComponentOnlineIslands*>(static_cast<uintptr_t>(p));
}

void tpw_oi_Tick(uint64_t p) { O(p)->CLBScreenComponentOnlineIslands::Tick(); }
void tpw_oi_SelectCurrentIsland(uint64_t p) { O(p)->SelectCurrentIsland(); }
void tpw_oi_EnterCurrentIsland(uint64_t p) { O(p)->EnterCurrentIslandOnline(); }
void tpw_oi_GotoNextIsland(uint64_t p) { O(p)->GotoNextIsland(); }
void tpw_oi_DownloadFailFunction(uint64_t p) { O(p)->DownloadFailFunction(); }
void tpw_oi_HandleOnlineBacktrack(uint64_t p) { O(p)->HandleOnlineBacktrack(); }

}  // extern "C"
