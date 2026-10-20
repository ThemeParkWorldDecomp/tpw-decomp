#include "keyboard.h"
#include "keytable_actions.h"
#include "tb_resource_display.h"

// 0x1014fce4: cycles the anaglyph bits (0xc) of the display flags: 0xc -> 0x4, 0x4 -> 0, anything else -> 0xc.
int32_t AnaglyphToggle() {
    uint32_t flags = gei_Display.mFlags;
    switch (flags & 0xc) {
    case 0xc:
        gei_Display.mFlags = flags & ~0xcu;
        gei_Display.mFlags |= 4;
        break;
    case 4:
        gei_Display.mFlags = flags & ~0xcu;
        break;
    default:
        gei_Display.mFlags |= 0xc;
        break;
    }
    return 1;
}
