// Test library for online.cpp's static initialiser, CheckShifts and the cdeque assign (unit v5907; libtpw_unit_sinit_online_cpp.dylib,
// tests/test_sinit_online_cpp.py), main app:
//   src/game/online/online.cpp, src/game/ui/ui_worldLobbyController.cpp   __sinit_online_cpp, CheckShifts, CdequeDoAssignArgumentIter
//
// The callees outside the unit are recording fakes: the string constructors, __register_global_object, GetKeyState and the runtime
// (new, delete, strlen, strcpy, throw). The throw fake raises a C++ exception that the wrapper catches (the original goes on after
// a returning __throw into a loop of a billion words; the PowerPC fake abandons the call the same way).
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "std_cdeque.h"
#include "TbDynamicStringTemplate.h"
#include "ui_ime.h"

using namespace calllog;

using CdequeT = std::__cdeque<const void*, std::allocator<const void*>>;
int16_t CheckShifts();
void UIMain_DestroyString(void*) {}
void UIMain_DestroyWString(void*) {}

namespace NS_MacDoze {
int16_t GetKeyState(int32_t key) {
    ev("GetKeyState", {key});
    return static_cast<int16_t>(ret("GetKeyState"));
}
}  // namespace NS_MacDoze

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("TbCtorChar", {newlab(this)}, {text});
}
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("TbCtorWide", {newlab(this)}, {std::string(narrow(text)).c_str()});
}

// The key function of the wide string's vtable (the constructor fake above stores the vtable); never called.
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

template <>
void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <>
void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <>
void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    int64_t id = 0;
    if (destructor == reinterpret_cast<void*>(&UIMain_DestroyString)) id = 0x103bcd38;
    else if (destructor == reinterpret_cast<void*>(&UIMain_DestroyWString)) id = 0x103bcbb8;
    ev("RegisterGlobalObject", {lab(object), id, lab(node)});
}

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];

struct StopAssign {};

static int64_t Hash(const char* text) {
    int64_t sum = 0;
    for (int i = 0; text[i] != 0; i++) sum += static_cast<int64_t>(i + 1) * static_cast<uint8_t>(text[i]);
    return sum;
}

size_t MslStrlen(const char* s) {
    ev("MslStrlen", {Hash(s)});
    return std::strlen(s);
}
char* MslStrcpy(char* d, const char* s) {
    ev("MslStrcpy", {lab(d), Hash(s)});
    return std::strcpy(d, s);
}
void* MslNew(uint32_t size) {
    ev("MslNew", {size});
    return tpw::ArenaAllocZeroed(size == 0 ? 4 : size);
}
void* MslNewArray(uint32_t size) {
    ev("MslNewArray", {size});
    return tpw::ArenaAllocZeroed(size == 0 ? 4 : size);
}
void MslDelete(void* p) { ev("MslDelete", {lab(p)}); }
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    auto* exception = static_cast<MslExceptionObject*>(object);
    ev("MslThrow", {Hash(typeName), exception->mVtbl == gMslLengthErrorVtbl ? 1 : 0, Hash(exception->mRc.mPtr), *exception->mRc.mCount,
                    dtorTV == gMslLengthErrorDtorTV ? 1 : 0});
    throw StopAssign();
}
}  // namespace IME

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    if (std::strcmp(name, "zero") == 0) return reinterpret_cast<uintptr_t>(&gOnlineSinitZero);
    if (std::strcmp(name, "mask") == 0) return reinterpret_cast<uintptr_t>(&gOnlineSinitMask);
    if (std::strncmp(name, "node", 4) == 0) return reinterpret_cast<uintptr_t>(gOnlineSinitNodes[std::atoi(name + 4)]);
    return 0;
}

uint64_t gMslMacAddressHigh;
void tpw_sinit_set_high(uint64_t high) { gMslMacAddressHigh = high; }

uint64_t tpw_sinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sinit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(std::__cdeque<const void*, std::allocator<const void*>>));
    out[n++] = static_cast<int64_t>(offsetof(CdequeT, mWordC));
    return n;
}

void tpw_sinit_sinit() { __sinit_online_cpp(); }

int32_t tpw_sinit_shifts() { return CheckShifts(); }

// answers 1 when the throw fake stopped the call
int32_t tpw_sinit_assign(uint64_t self, uint64_t firstDeque, uint32_t firstIndex, uint64_t firstPtr, uint64_t lastDeque, uint32_t lastIndex,
                         uint64_t lastPtr) {
    using Deque = std::__cdeque<const void*, std::allocator<const void*>>;
    MslCdequeIter first{reinterpret_cast<Deque*>(firstDeque), firstIndex, reinterpret_cast<uint32_t*>(firstPtr)};
    MslCdequeIter last{reinterpret_cast<Deque*>(lastDeque), lastIndex, reinterpret_cast<uint32_t*>(lastPtr)};
    try {
        CdequeDoAssignArgumentIter(reinterpret_cast<Deque*>(self), first, last);
    } catch (const IME::StopAssign&) {
        return 1;
    }
    return 0;
}

}  // extern "C"
