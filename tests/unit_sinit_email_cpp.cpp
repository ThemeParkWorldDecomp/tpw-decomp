// Test library for email.cpp's static initialiser (unit v5567; libtpw_unit_sinit_email_cpp.dylib, tests/test_sinit_email_cpp.py), main app:
//   src/game/online/email.cpp   __sinit_email_cpp and the globals it builds
//
// The callees outside the unit (the string constructors, __register_global_object, operator new, the two string destructor glues) are
// fakes that record what the real ones read. The destructors are only addresses here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "Ptr32.h"
#include "TbDynamicStringTemplate.h"

using namespace calllog;

void UIMain_DestroyString(void*) {}
void UIMain_DestroyWString(void*) {}

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("TbStr", {lab(this)}, {text});
}
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("TbWStr", {lab(this)}, {narrow(text).c_str()});
}
// The virtual the vtable names (never called: the constructors are fakes).
#define TPW_FAKE_VIRTUALS(C)                                                                                                  \
    template <> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}                                                     \
    template <> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}                                                        \
    template <> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}                                                     \
    template <> TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {}                                                     \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; }   \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) { return *this; } \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) { return *this; }                 \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }                 \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) { return *this; }  \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }                \
    template <> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
TPW_FAKE_VIRTUALS(char)
TPW_FAKE_VIRTUALS(char16_t)

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("reg", {lab(object), lab(destructor), lab(node)});
}


void* EmailOperatorNew(std::size_t size) {
    ev("new", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(static_cast<uint32_t>(size ? size : 4));
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"strings", gEmailLanguageStrings}, {"app", gEmailAppNameString}, {"key", gEmailLanguageKeyString},
        {"wide", gEmailWideString},         {"nodes", gEmailNodes},       {"blobnode", gEmailBlobNode},
        {"blobdtor", gEmailBlobDtorVec},    {"byte", &gEmailDefaultByte}, {"blob", &gEmailBlob},
        {"dtor", reinterpret_cast<const void*>(&UIMain_DestroyString)},
        {"wdtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sinit_deref(uint32_t offset) { return offset ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase + offset)) : 0; }

uint64_t tpw_sinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_sinit_stride() { return sizeof(TbDynamicStringTemplate<char>); }

int32_t tpw_sinit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(EmailBlob, mLength));
    out[n++] = static_cast<int64_t>(offsetof(EmailBlob, mFlag));
    out[n++] = static_cast<int64_t>(offsetof(EmailBlob, mDataOffset));
    out[n++] = static_cast<int64_t>(sizeof(EmailBlob));
    return n;
}

void tpw_sinit_run() { __sinit_email_cpp(); }

}  // extern "C"
