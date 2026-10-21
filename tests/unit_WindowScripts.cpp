// Test library for the window scripts of src/ui_window_scripts.cpp (unit v5L42; libtpw_unit_WindowScripts.dylib,
// tests/test_WindowScripts.py), main app: the arrays and nothing else; tpw_ws_addr names them for the test.
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "MainWindowCallback.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

extern "C" {

uint64_t tpw_ws_addr(const char* name) {
    struct Entry {
        const char* name;
        const int16_t* address;
    };
    const Entry entries[] = {
        {"gAllShowsScript", gAllShowsScript},
        {"gOptionsPanelScript", gOptionsPanelScript},
        {"gProgBarScript", gProgBarScript},
        {"gCityInfoOverlayScript", gCityInfoOverlayScript},
        {"gOutboxPanelData", gOutboxPanelData},
        {"gOnlineParkInfoScript", gOnlineParkInfoScript},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

}  // extern "C"
