// Test library for the static initialiser of FS_LoadSave.cpp (unit v5720):
//   src/ui_cursor_classes_CUIGrapher_DrawGraph_sinit_FS_LoadSave_cpp.cpp
// __register_global_object is a recording fake (object, destructor and node as labels).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_LoadSave.h"

using namespace calllog;

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_sl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"words", gLoadSaveWords},
        {"node", gLoadSaveNode},
        {"dtor", gLoadSaveDtorVector},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gLoadSaveWords));
    return n;
}

void tpw_sl_sinit() { __sinit_FS_LoadSave_cpp(); }

}  // extern "C"
