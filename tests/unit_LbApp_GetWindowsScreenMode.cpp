// Test library for the bullfrog LbSCBack.cpp helpers (unit v5284; libtpw_unit_LbApp_GetWindowsScreenMode.dylib,
// tests/test_LbApp_GetWindowsScreenMode.py):
//   src/LbFile_LbApp_GetWindowsScreenMode.cpp   LbApp_GetWindowsScreenMode, SetWindowName, EnableWindowsScreenSaver,
//   ProcessWindowMessages, __sinit_LbSCBack_cpp, and the globals gLbWindowName and gLbSCBack.
//
// The NS_MacDoze calls, __construct_array and __register_global_object are fakes that record what the real ones read and answer from the
// test's queues. strncpy and operator new are the real ones (the PowerPC side stubs them with the same behaviour).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LbSCBack.h"

using namespace calllog;

namespace {
int32_t gPeekCount = 0;

// Only some messages are logged (a full 2000-message run would overflow the log); the index is in every event.
bool Logged(int32_t n) { return n <= 40 || n >= 1995 || n % 250 == 0; }
}  // namespace

namespace NS_MacDoze {

InternalHandle* CreateDC(const char* driver, const char* device, const char* output, const _devicemodeA* mode) {
    ev("CreateDC", {lab(const_cast<char*>(device)), lab(const_cast<char*>(output)), lab(const_cast<_devicemodeA*>(mode))}, {driver});
    return reinterpret_cast<InternalHandle*>(static_cast<uintptr_t>(ret("CreateDC")));
}

int32_t GetDeviceCaps(InternalHandle* dc, int32_t index) {
    ev("GetDeviceCaps", {lab(dc), index});
    return static_cast<int32_t>(ret("GetDeviceCaps"));
}

int32_t DeleteDC(InternalHandle* dc) {
    ev("DeleteDC", {lab(dc)});
    return 0;
}

int32_t DispatchMessage(const tagMSG* msg) {
    uint32_t n;
    std::memcpy(&n, msg, 4);
    if (Logged(static_cast<int32_t>(n))) ev("DispatchMessage", {n});
    return 0;
}

int32_t TranslateMessage(const tagMSG* msg) {
    uint32_t n;
    std::memcpy(&n, msg, 4);
    if (Logged(static_cast<int32_t>(n))) ev("TranslateMessage", {n});
    return 0;
}

int32_t PeekMessage(tagMSG* msg, GrafPort* window, uint32_t min, uint32_t max, uint32_t remove) {
    int32_t n = ++gPeekCount;
    uint32_t stamp = static_cast<uint32_t>(n);
    std::memcpy(msg, &stamp, 4);
    if (Logged(n)) ev("PeekMessage", {n, lab(window), min, max, remove});
    return n <= ret("PeekLimit") ? 1 : 0;
}

void InitializeCriticalSection(void** section) { ev("InitializeCriticalSection", {lab(section)}); }

int32_t SystemParametersInfoA(uint32_t action, uint32_t param, void* data, uint32_t winIni) {
    ev("SystemParametersInfoA", {action, param, lab(data), winIni});
    return static_cast<int32_t>(ret("SystemParametersInfoA"));
}

}  // namespace NS_MacDoze

extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count) {
    ev("__construct_array", {lab(array), lab(ctor), lab(dtor), size, count});
}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("__register_global_object", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_lbapp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"name", gLbWindowName},
        {"obj", &gLbSCBack},
        {"node", gLbSCBack.mNode},
        {"ctor", reinterpret_cast<const void*>(&LbSEFunctionList_Construct)},
        {"dtor", reinterpret_cast<const void*>(&LbSEFunctionList_Destruct)},
        {"cbdtor", reinterpret_cast<const void*>(&LbSystemEventCallback_Destruct)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lbapp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lbapp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(LbSCBackGlobals, mCriticalSection));
    out[n++] = static_cast<int64_t>(offsetof(LbSCBackGlobals, mNode));
    out[n++] = static_cast<int64_t>(sizeof(TbScreenMode));
    out[n++] = static_cast<int64_t>(sizeof(LbAppResult));
    out[n++] = static_cast<int64_t>(sizeof(gLbWindowName));
    return n;
}

void tpw_lbapp_reset() { gPeekCount = 0; }

int32_t tpw_lbapp_setname(uint64_t name) { return LbApp_SetWindowName(reinterpret_cast<const char*>(static_cast<uintptr_t>(name))).mCode; }
int32_t tpw_lbapp_saver(int32_t enable) { return LbApp_EnableWindowsScreenSaver(enable).mCode; }
void tpw_lbapp_pump() { LbApp_ProcessWindowMessages(); }
void tpw_lbapp_mode(uint64_t mode) { LbApp_GetWindowsScreenMode(*reinterpret_cast<TbScreenMode*>(static_cast<uintptr_t>(mode))); }
void tpw_lbapp_sinit() { __sinit_LbSCBack_cpp(); }

}  // extern "C"
