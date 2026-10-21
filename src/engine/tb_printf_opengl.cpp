// tb_printf_opengl.cpp: name proven by __sinit_tb_printf_opengl_cpp in the engine library.

#include "platform/indirectx.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_printf_opengl.h"
#include "tb_proctexture.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_switch.h"
#include "tb_texturecache.h"
#include "Wavelet.h"
#include <cstdio>
#include <cstring>
#include <new>

// Unit v5953: ConvertSquareFrom32 (0x10003454), PT_Load (0x1000bed0), CSwitch::Do (0x100328ac),
// COpenGLPrintf::Close (0x10046c64).

extern "C" void glDeleteLists(GLuint list, GLsizei range);

bool COpenGLPrintf::Close() {
    int32_t* list = reinterpret_cast<int32_t*>(this);   // the display list base is the first word
    if (*list != 0) {
        glDeleteLists(static_cast<GLuint>(*list), 0x100);
        *list = 0;
    }
    return glGetError() == 0;
}

// OpenGL start-up, Mac 2.04b3. Log contexts identify tb_init_opengl.cpp;
// these functions are filed with tb_printf_opengl.cpp in the inventory.

void COpenGLPrintf::printf_xy(int32_t x, int32_t y, char* text) {
    mText = text; mX = x; mY = y;
}

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
