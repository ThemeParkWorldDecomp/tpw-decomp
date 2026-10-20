// 3DFont.cpp static initialiser: the font's global parallel light (unit v5109).
#include <new>

#include "Light.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyLight(void* object);    // the light's destructor word 0x102657b8

alignas(16) unsigned char gSinit3DFontLight[0x100];   // Mac 0x102b34c8, 0x74 bytes
unsigned char gSinit3DFontNode[16];                   // Mac 0x102b353c

static_assert(sizeof(CLightParallel) <= sizeof(gSinit3DFontLight), "storage too small");

// 0x1000757c: constructs the global (the Mac inlines the CLightParallel constructor: vtable words, the shared
// CMapLinkBase), flags 2 (a direction), normal (0, 0, 1), colour all ones, then registers its destructor.
void __sinit_3DFont_cpp() {
    CLightParallel* light = new (gSinit3DFontLight) CLightParallel;
    light->mFlags = 2;
    light->SetNormal(0.0f, 0.0f, 1.0f);
    light->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    __register_global_object(light, reinterpret_cast<void*>(&UIMain_DestroyLight), gSinit3DFontNode);
}
