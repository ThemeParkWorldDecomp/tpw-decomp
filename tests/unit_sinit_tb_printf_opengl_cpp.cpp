// Test library for __sinit_tb_printf_opengl_cpp (src/tb_printf_opengl_sinit_tb_printf_opengl_cpp.cpp).
// __register_global_object is a recording fake; the globals are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "tb_printf_opengl.h"

using namespace calllog;

COpenGLPrintf gOpenGLPrintf;
uint8_t gOpenGLPrintfNode[16];
void COpenGLPrintf_destroy(COpenGLPrintf*, int16_t) {}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    if (std::strcmp(name, "obj") == 0) return reinterpret_cast<uintptr_t>(&gOpenGLPrintf);
    if (std::strcmp(name, "node") == 0) return reinterpret_cast<uintptr_t>(gOpenGLPrintfNode);
    if (std::strcmp(name, "dtor") == 0) return reinterpret_cast<uintptr_t>(&COpenGLPrintf_destroy);
    return 0;
}

uint64_t tpw_sinit_alloc(uint32_t size) { return reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size)); }

int32_t tpw_sinit_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(COpenGLPrintf, mText));
    return n;
}

// text (0 or 1), x, y
void tpw_sinit_fields(uint32_t* out) {
    out[0] = gOpenGLPrintf.mText ? 1 : 0;
    out[1] = static_cast<uint32_t>(gOpenGLPrintf.mX);
    out[2] = static_cast<uint32_t>(gOpenGLPrintf.mY);
}

void tpw_sinit_call() { __sinit_tb_printf_opengl_cpp(); }

}  // extern "C"
