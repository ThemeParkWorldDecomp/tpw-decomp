// Test library for PostGetCityListCallbackFunction (unit v5128), main app:
//   src/game/online/lobby_globe.cpp
//
// The context is a screen whose two virtual slots (0x30, 0x34) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "lobby.h"

using namespace calllog;

// The base class's members live in files this library does not compile; empty stand-ins for the fake screen.
CLBScreenComponent::CLBScreenComponent() : mVisible(0) {}
CLBScreenComponent::~CLBScreenComponent() {}
void CLBScreenComponent::Show() {}
void CLBScreenComponent::Hide() {}
int32_t CLBScreenComponent::IsVisible() { return 0; }

namespace {

class FakeScreen : public CLBCityListScreen {
public:
    void Init() override {}
    void Tick() override {}
    void Render() override {}
    void Close() override {}
    void WindowMessagesFn(int32_t, int32_t, int32_t) override {}
    uint32_t ProcessCancel() override { return 0; }
    void Show() override {}
    void Hide() override {}
    int32_t IsVisible() override { return 0; }
    void CityListReceived(TbPtrList<CFSOnlineCity>* cities) override { ev("Received", {lab(this), lab(cities)}); }
    void CityListFailed() override { ev("Failed", {lab(this)}); }
};

FakeScreen gScreen;

}  // namespace

extern "C" {

uint64_t tpw_pgcl_addr(const char* name) {
    if (std::strcmp(name, "screen") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gScreen));
    return 0;
}

uint64_t tpw_pgcl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pgcl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = 1;
    return n;
}

void tpw_pgcl_call(int32_t error, uint64_t context, uint64_t list) {
    PostGetCityListCallbackFunction(error, reinterpret_cast<const void*>(context),
                                    reinterpret_cast<TbPtrList<CFSOnlineCity>*>(list));
}

}  // extern "C"
