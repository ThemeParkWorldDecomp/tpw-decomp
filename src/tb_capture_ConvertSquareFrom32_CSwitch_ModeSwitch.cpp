#include "tb_capture.h"
#include <cstdio>
#include <cstring>
#include "Wavelet.h"
#include "platform/indirectx.h"
#include "ppc_math.h"
#include "tb_ddraw.h"
#include "tb_error.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_printf_opengl.h"
#include "tb_switch.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_proctexture.h"
#include "platform/upixels.h"
#include "platform/samsutils.h"

int32_t ToggleFullScreen();   // 0x1003281c (keytable_actions.h declares it void; the original answers 1)

uint32_t gei_ParameterSettings[4];   // 0x100af128
float gei_HFieldOne;                 // 0x100af120

// 0x10032aa4: width and height 0 mean "the current display", toggles the mode.
void CSwitch::ModeSwitch() {
    mF08 = 0;
    mF04 = 0;
    Do(1);
}

// 0x10032b00
void CSwitch::RezSwitch(int32_t width, int32_t height) {
    mF04 = width;
    mF08 = height;
    Do(0);
}

// 0x1003281c
int32_t ToggleFullScreen() {
    if (gei_Switch != nullptr) gei_Switch->ModeSwitch();
    return 1;
}

// 0x100391c8
bool IsAlignedTo64Bit(void*& pointer) {
    return (reinterpret_cast<uintptr_t>(pointer) & 7) == 0;
}

namespace SamsUtilities {
// 0x10043b20: only stores its argument registers into the caller's frame; nothing observable.
// Weak: the platform layer (log.cpp) defines the logging port of the same function and wins at link time.
__attribute__((weak)) void UDebug(const char*, ...) {}

// 0x10006218 (byte twin of USwapInplace16<unsigned short> 0x10043db8)
void USwapInplace16UShort_10006218(uint16_t& value) { value = static_cast<uint16_t>((value >> 8) | (value << 8)); }

// 0x100061d0 (byte twin of USwapInplaceFloat 0x10021bb4)
void USwapInplace32UInt_100061d0(uint32_t& value) { value = __builtin_bswap32(value); }
}  // namespace SamsUtilities

// 0x10046bdc: ~COpenGLPrintf, the hidden delete flag is the second argument.
void COpenGLPrintf_destroy(COpenGLPrintf* self, int16_t flag) {
    if (self != nullptr) {
        self->Close();
        if (flag > 0) operator delete(self);
    }
}

// 0x100141bc
void __sinit_tb_init_cpp() {
    gei_ParameterSettings[0] = 0;
    gei_ParameterSettings[1] = 0;
    gei_ParameterSettings[2] = 0;
    gei_ParameterSettings[3] = 0;
}

// 0x1001271c
void __sinit_tb_hfield_cpp() { gei_HFieldOne = 1.0f; }

// 0x1000be98
void PT_Disable() { gei_TextureSettings.mFlags &= ~CTextureSettings::kProcTexturesOn; }

// 0x1000be5c
void PT_Enable() {
    uint32_t flags = gei_TextureSettings.mFlags;
    if (flags & CTextureSettings::kProcTexturesBlocked) return;
    gei_TextureSettings.mFlags = flags | CTextureSettings::kProcTexturesOn;
}
