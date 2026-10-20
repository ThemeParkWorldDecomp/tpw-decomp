#include "tb_printf_opengl.h"
#include "tb_error.h"
#include "tb_resource_display.h"
#include "platform/indirectx.h"
#include "platform/opengl.h"
#include <new>
#include <cstring>

extern "C" void __register_global_object(void* object, void* destructor, void* node);

// Static initialiser: the global COpenGLPrintf is constructed (word 1 = -1, the rest 0)
// and registered for destruction. The Mac object is 0x1c bytes; here the opaque words
// 4..0xf are set by byte offset and the three fields by name.
void __sinit_tb_printf_opengl_cpp() {
    COpenGLPrintf* p = &gOpenGLPrintf;
    uint32_t zero = 0, minus1 = 0xFFFFFFFFu;
    std::memcpy(p->mOpaque + 0xc, &zero, 4);
    std::memcpy(p->mOpaque + 8, &zero, 4);
    p->mY = 0;
    p->mX = 0;
    std::memcpy(p->mOpaque + 4, &minus1, 4);
    p->mText = nullptr;
    __register_global_object(p, reinterpret_cast<void*>(&COpenGLPrintf_destroy), gOpenGLPrintfNode);
}
