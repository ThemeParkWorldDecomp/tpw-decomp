// TPDialog.cpp: name proven by __sinit_TPDialog_cpp in the main application.

#include "TPDialog.h"

// TPDialog.cpp (SimTheme Park 2.04b3): 0x10237950..0x10237cb0.
// Balance members preceding these functions belong to ResolutionBalance.cpp.

uint32_t gTPDialogTextureMegabytes; // BSS 0x103bc65c
uint32_t gTPDialogLanguageBits;     // BSS 0x103bc660
uint32_t gTPDialogOptions; // BSS 0x103bc664

uint32_t GetOptionsSetting() { // 0x10237950, priority matters when several bits are set
    if (gTPDialogOptions & 0x2000u) return 0x2000;
    if (gTPDialogOptions & 0x1000u) return 0x1000;
    if (gTPDialogOptions & 0x0800u) return 0x0800;
    if (gTPDialogOptions & 0x4000u) return 0x4000;
    return 0x1000;
}

int32_t GetNumKidSprites() { // 0x102379c8
    if (gTPDialogOptions & 0x08u) return 8;
    if (gTPDialogOptions & 0x10u) return 16;
    if (gTPDialogOptions & 0x20u) return 32;
    return 8;
}

// __sinit_TPDialog_cpp at 0x10237a30; definition order preserves construction order.
TbDynamicStringTemplate<char> gTPDialogLanguages[16] = { // 0x103bc6a8, 20-byte original slots
    TbDynamicStringTemplate<char>("English"),
    TbDynamicStringTemplate<char>("French"),
    TbDynamicStringTemplate<char>("German"),
    TbDynamicStringTemplate<char>("Spanish"),
    TbDynamicStringTemplate<char>("Swedish"),
    TbDynamicStringTemplate<char>("Italian"),
    TbDynamicStringTemplate<char>("Dutch"),
    TbDynamicStringTemplate<char>("Polish"),
    TbDynamicStringTemplate<char>("Danish"),
    TbDynamicStringTemplate<char>("Japanese"),
    TbDynamicStringTemplate<char>("TraditionalChinese"),
    TbDynamicStringTemplate<char>("SimplifiedChinese"),
    TbDynamicStringTemplate<char>("Thai"),
    TbDynamicStringTemplate<char>("Korean"),
    TbDynamicStringTemplate<char>("Hebrew"),
    TbDynamicStringTemplate<char>("American")
};
TbDynamicStringTemplate<char> gTPDialogTitle("Theme Park World"); // 0x103bc688
TbDynamicStringTemplate<char> gTPDialogLanguage("Language");     // 0x103bc668
