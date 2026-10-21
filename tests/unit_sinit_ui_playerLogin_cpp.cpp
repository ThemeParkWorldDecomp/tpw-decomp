// Test library for ui_playerLogin.cpp's static initialiser (unit v5726; libtpw_unit_sinit_ui_playerLogin_cpp.dylib), main app:
//   src/game/ui/ui_playerLogin.cpp
//
// The wide string constructor and __register_global_object are fakes recording their arguments.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "TbDynamicStringTemplate.h"
#include "AdvisorScoring.h"

using namespace calllog;

void UIMain_DestroyWString(void* object) { (void)object; }

// ---- the fakes
namespace bullfrog_shared {}
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("ctor", {lab(this), 2 * static_cast<int64_t>(text - gPlayerLoginStrings)});
}

template <>
void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template <>
void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <>
void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_pl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"strings", gPlayerLoginStrings}, {"a", gPlayerLoginStringA}, {"b", gPlayerLoginStringB},
        {"nodea", gPlayerLoginNodeA},     {"nodeb", gPlayerLoginNodeB},
        {"dtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pl_layout(int64_t* out) {
    (void)out;
    return 0;
}

void tpw_pl_sinit() { __sinit_ui_playerLogin_cpp(); }

}  // extern "C"
