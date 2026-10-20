// Test library for CSwitch::ModeSwitch and twelve small functions (unit v51066; libtpw_unit_CSwitch_ModeSwitch.dylib,
// tests/test_CSwitch_ModeSwitch.py), engine program:
//   src/tb_capture_ConvertSquareFrom32_CSwitch_ModeSwitch.cpp
//
// CSwitch::Do, COpenGLPrintf::Close and operator delete are fakes recording their arguments. gei_Switch and gei_TextureSettings
// (defined by other files) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "tb_switch.h"
#include "tb_printf_opengl.h"
#include "tb_resource_texture.h"
#include "tb_mesh.h"
#include "tb_hfield.h"
#include "platform/samsutils.h"

using namespace calllog;

CSwitch* gei_Switch;
CTextureSettings gei_TextureSettings;

int32_t ToggleFullScreen();

// The real constructor (another file) is not linked; the global stays zero like bss, the tests write mFlags themselves.
CTextureSettings::CTextureSettings() {}

// ---- the fakes
int32_t CSwitch::Do(int32_t toggle) {
    ev("Do", {lab(this), toggle});
    return static_cast<int32_t>(ret("Do"));
}

bool COpenGLPrintf::Close() {
    ev("Close", {lab(this)});
    return ret("Close") != 0;
}

// operator delete: the unit's only caller is ~COpenGLPrintf on an arena block; record it instead of freeing.

extern "C" {

uint64_t tpw_cs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gei_Switch", &gei_Switch},
        {"gei_TextureSettings", &gei_TextureSettings},
        {"gei_ParameterSettings", &gei_ParameterSettings},
        {"gei_HFieldOne", &gei_HFieldOne},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_cs_new(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(::operator new(size))); }

int32_t tpw_cs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CSwitch, mF04));
    out[n++] = static_cast<int64_t>(offsetof(CSwitch, mF08));
    out[n++] = static_cast<int64_t>(sizeof(CSwitch));
    out[n++] = static_cast<int64_t>(offsetof(CTextureSettings, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(SOldCsTexture, mWhere));
    out[n++] = static_cast<int64_t>(sizeof(SOldCsTexture));
    return n;
}

void tpw_cs_modeswitch(uint64_t obj) { reinterpret_cast<CSwitch*>(obj)->ModeSwitch(); }
void tpw_cs_rezswitch(uint64_t obj, int32_t w, int32_t h) { reinterpret_cast<CSwitch*>(obj)->RezSwitch(w, h); }
uint32_t tpw_cs_toggle() { return static_cast<uint32_t>(ToggleFullScreen()); }
uint32_t tpw_cs_aligned(uint64_t cell) { return IsAlignedTo64Bit(*reinterpret_cast<void**>(cell)) ? 1 : 0; }
void tpw_cs_udebug(uint32_t a, uint32_t b, uint32_t c) { SamsUtilities::UDebug("x", a, b, c); }
void tpw_cs_oldcs(uint64_t p) { new (reinterpret_cast<void*>(p)) SOldCsTexture(); }
uint64_t tpw_cs_dtor(uint64_t p, int32_t flag) {
    COpenGLPrintf_destroy(reinterpret_cast<COpenGLPrintf*>(p), static_cast<int16_t>(flag));
    return p;
}
void tpw_cs_sinit_init() { __sinit_tb_init_cpp(); }
void tpw_cs_sinit_hfield() { __sinit_tb_hfield_cpp(); }
void tpw_cs_pt_enable() { PT_Enable(); }
void tpw_cs_pt_disable() { PT_Disable(); }
void tpw_cs_swap16(uint64_t a) { SamsUtilities::USwapInplace16UShort_10006218(*reinterpret_cast<uint16_t*>(a)); }
void tpw_cs_swap32(uint64_t a) { SamsUtilities::USwapInplace32UInt_100061d0(*reinterpret_cast<uint32_t*>(a)); }

}  // extern "C"
