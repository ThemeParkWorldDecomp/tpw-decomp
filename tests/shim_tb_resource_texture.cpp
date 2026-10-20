// extern "C" wrappers for tests/test_tb_resource_texture.py (ctypes).
#include "tb_resource_texture.h"

#include <new>

extern "C" {
void tpw_trt_ctor(void* p) { new (p) CTextureSettings(); }
uint32_t tpw_trt_GetAlphaMask(CTextureSettings* p) { return p->GetAlphaMask(); }
void tpw_trt_CalculateUVFixup(CTextureSettings* p) { p->CalculateUVFixup(); }
void tpw_trt_OutputDebugInfo(CTextureSettings* p) { p->OutputDebugInfo(); }
int32_t tpw_ConvertSurfaceSizeToFlag(int32_t size) { return ConvertSurfaceSizeToFlag(size); }
// __sinit_tb_resource_texture_cpp: construct the global in place again.
void tpw_trt_sinit() { new (&gei_TextureSettings) CTextureSettings(); }
void* tpw_trt_global() { return &gei_TextureSettings; }
}
