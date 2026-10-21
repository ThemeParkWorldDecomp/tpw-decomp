// Test library for DeInitOnlineUI (unit v5769; libtpw_unit_DeInitOnlineUI.dylib, tests/test_DeInitOnlineUI.py), main app:
//   src/game/ui/ui_main.cpp   DeInitOnlineUI
//
// The callees outside the unit are fakes that record what the real ones read: RemoveOnlinePanel, UI_SendMessage,
// UI_DisableHelpText, CMapWho::Unlink. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "UI_Base.h"
#include "ui_main.h"

using namespace calllog;

InterfaceWindow* gFirstPersonWindow = nullptr;
CMapWho* gei_MapWho = nullptr;
CLight gUILight;
CLight* gev_LightChain = nullptr;

CMapLinkBase::~CMapLinkBase() {}

void RemoveOnlinePanel() { ev("RemoveOnlinePanel", {}); }

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

void UI_DisableHelpText() { ev("UI_DisableHelpText", {}); }

int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    ev("Unlink", {lab(this), lab(link), list});
    return 0;
}

extern "C" {

uint64_t tpw_dio_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    static CMapLinkBase* base = static_cast<CMapLinkBase*>(&gUILight);
    const Entry entries[] = {
        {"gFirstPersonWindow", &gFirstPersonWindow},
        {"gei_MapWho", &gei_MapWho},
        {"lightbase", &base},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dio_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_dio_layout(int64_t* out) {
    (void)out;
    return 0;
}

void tpw_dio_run() { DeInitOnlineUI(); }

}  // extern "C"
