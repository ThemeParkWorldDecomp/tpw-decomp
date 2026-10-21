// Test library for the static initialiser of ui_Postcard.cpp (unit v5204; libtpw_unit_sinit_ui_Postcard_cpp.dylib).
//   src/game/ui/ui_Postcard.cpp   __sinit_ui_Postcard_cpp and the bss block gPostcardBlob
//
// The string constructors, __register_global_object and the two destructor glues are fakes that record their arguments.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TbDynamicStringTemplate.h"

using namespace calllog;

extern unsigned char gPostcardBlob[0x260];
void __sinit_ui_Postcard_cpp();

void UIMain_DestroyString(void*) {}
void UIMain_DestroyWString(void*) {}

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("ctorC", {lab(this)}, {text});
}
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("ctorW", {lab(this)}, {narrow(text).c_str()});
}

// The wide string's other virtuals are never called here; they exist only so its vtable links.
#define VIRTUALS(CH)                                                                  \
    template <> void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}           \
    template <> void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}          \
    template <> void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}              \
    template <> TbDynamicStringTemplate<CH>::~TbDynamicStringTemplate() {}           \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate<CH>&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH*) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }
VIRTUALS(char16_t)
VIRTUALS(char)
#undef VIRTUALS

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    int64_t which = destructor == reinterpret_cast<void*>(&UIMain_DestroyString) ? 1 : 2;
    ev("reg", {lab(object), which, lab(node)});
}

extern "C" {

uint64_t tpw_ps_blob() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gPostcardBlob)); }

void tpw_ps_run() { __sinit_ui_Postcard_cpp(); }

}  // extern "C"
