// Test library for CSYS_TPFileInfo::~CSYS_TPFileInfo (unit v5714), main app:
//   src/FS_System_CSYS_TPFileInfo_Dtor_CSYS_TPFileInfo.cpp
//
// The string destructor and operator delete are fakes that record the object as a label.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_System.h"

using namespace calllog;

template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    ev("StrDtor", {lab(this)});
}

// Referenced only by the vtable the destructor above creates; never called.
template <>
void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <>
void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <>
void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>&) { return *this; }
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

// The strong definition replaces the weak one in the src file (the program's operator delete is the arena's here).
void CSYS_TPFileInfo::operator delete(void* p) { ev("Delete", {lab(p)}); }

extern "C" {

uint64_t tpw_fi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_fi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CSYS_TPFileInfo, mRemotePath));
    out[n++] = static_cast<int64_t>(offsetof(CSYS_TPFileInfo, mDataPath));
    out[n++] = static_cast<int64_t>(offsetof(CSYS_TPFileInfo, mLevelPath));
    out[n++] = static_cast<int64_t>(offsetof(CSYS_TPFileInfo, mLanguagePath));
    out[n++] = static_cast<int64_t>(sizeof(CSYS_TPFileInfo));
    return n;
}

// flag: the hidden delete-storage short; positive means the delete-expression.
void tpw_fi_dtor(uint64_t object, int16_t flag) {
    CSYS_TPFileInfo* p = reinterpret_cast<CSYS_TPFileInfo*>(static_cast<uintptr_t>(object));
    if (flag > 0)
        delete p;
    else if (p)
        p->~CSYS_TPFileInfo();
}

}  // extern "C"
