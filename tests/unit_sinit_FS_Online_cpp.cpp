// Test library for the FS_Online.cpp static initialiser (unit v5742; libtpw_unit_sinit_FS_Online_cpp.dylib, tests/test_sinit_FS_Online_cpp.py), main app:
//   src/FS_Online_init_CFSOnlineSystem_CompleteFTPDownload_sinit_FS_Online_cpp.cpp   sinit_FS_Online_cpp
//
// The string constructor and __register_global_object are fakes that record their arguments; the globals the initialiser touches are
// defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"

using namespace calllog;

OnlineWideStringGlobal gUploadNameTail;
OnlineWideStringGlobal gOnlineSinitString2;
uint8_t gOnlineSinitNodeA[16];
uint8_t gOnlineSinitNodeB[16];

void UIMain_DestroyWString(void*) {}

// The class's virtuals, empty: the fake constructor stores the vtable, so the library needs them to link (never called).
using WStr = TbDynamicStringTemplate<char16_t>;
template <> void WStr::Format(const char16_t*, ...) {}
template <> void WStr::ResizeTo(uint32_t) {}
template <> void WStr::AllocBuffer(uint32_t) {}
template <> WStr::~TbDynamicStringTemplate() {}
template <> WStr& WStr::operator=(const TbStringBase<char16_t>&) { return *this; }
template <> WStr& WStr::operator=(const WStr&) { return *this; }
template <> WStr& WStr::operator=(const char16_t*) { return *this; }
template <> WStr& WStr::operator=(const char16_t&) { return *this; }
template <> WStr& WStr::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <> WStr& WStr::operator+=(const char16_t&) { return *this; }
template <> WStr& WStr::operator+=(const char16_t*) { return *this; }

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    std::string n = narrow(text);
    ev("ctor", {lab(static_cast<const void*>(this))}, {n.c_str()});
}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), destructor == reinterpret_cast<void*>(&UIMain_DestroyWString) ? 1 : 0, lab(node)});
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"str1", &gUploadNameTail},
        {"str2", &gOnlineSinitString2},
        {"nodeA", gOnlineSinitNodeA},
        {"nodeB", gOnlineSinitNodeB},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

int32_t tpw_sinit_layout(int64_t*) { return 0; }

void tpw_sinit_run() { sinit_FS_Online_cpp(); }

}  // extern "C"
