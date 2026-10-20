// GetFontTableEntry, main 0x10180270 (48 bytes); existing body separated for unit w43.
#include "InterfaceFont.h"
#include "Text.h"
#include "ui_main.h"

InterfaceFont* GetFontTableEntry(uint32_t index) {
    if (index >= 13) return nullptr;
    return &gText.mFonts[gText.mResolutionGroup * 13 + index];
}
