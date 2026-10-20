// extern "C" wrappers for tests/test_tb_shadow.py (ctypes).
#include <new>

#include "tb_shadow.h"

extern "C" {
void tpw_CShadow_ResetLightSources(CShadow* s) { s->ResetLightSources(); }
int32_t tpw_CShadow_AddLightSource(CShadow* s, const D3DVECTOR* dir) { return s->AddLightSource(*dir); }
// Matrix pointers come back as offsets into the object (-1 for NULL), so both sides compare.
int32_t tpw_CShadow_GetFirstShadowMatrix(CShadow* s) {
    CMatrix* m = s->GetFirstShadowMatrix();
    return m ? static_cast<int32_t>(reinterpret_cast<char*>(m) - reinterpret_cast<char*>(s)) : -1;
}
int32_t tpw_CShadow_GetNextShadowMatrix(CShadow* s) {
    CMatrix* m = s->GetNextShadowMatrix();
    return m ? static_cast<int32_t>(reinterpret_cast<char*>(m) - reinterpret_cast<char*>(s)) : -1;
}
void tpw_sinit_tb_shadow() { new (&gShadow) CShadow(); }
// Destructor convention (as the pilot's Vector2d test): storage is freed when flag > 0.
int32_t tpw_CShadow_dtor(CShadow* s, int16_t flag) { return (s != nullptr && flag > 0) ? 1 : 0; }
}
