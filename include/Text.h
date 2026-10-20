// Text.cpp. The inline destructor at 0x1015ee20 follows its sinit.
// Member declaration order gives params, 52 wrappers in reverse, then manager.
#pragma once
#include "InterfaceFontWrapper.h"
#include "TbFontRenderParams.h"
#include "TbTextResourceManager.h"
#include "TbDynamicStringTemplate.h"
class CText {
public:
    // Inline construction in __sinit_Text_cpp. Four resource fields and the language
    // id are left alone by the original (zero from BSS on the first construction).
    CText() : mInitialised(0), mResolutionGroup(1) {
        for (int i = 0; i < 23; ++i)
            if (i != 5 && i != 17 && i != 21 && i != 22) mResourcePointers[i] = nullptr;
        for (int id = 0; id < 13; ++id)
            for (int group = 0; group < 4; ++group) {
                auto& font = mFonts[group * 13 + id];
                font.mFont = nullptr;
                font.SetID(id);
            }
    }
    int32_t Init();              // 0x1015be6c, nonzero on success
    void UnInit();               // 0x1015e524
    void SetRez(int32_t width);  // 0x1015e4b8
    void SetLanguageId(uint32_t id);   // 0x1015ea0c: a Windows language id (TPConvertTPLangDefs)
    // 0x1015e93c: the language name buffer (a global at 0x102b2e18; `this` is unused).
    char* GetLanguageName();
    uint32_t GetLanguageId() const;   // 0x1015e9d8
    bool IsHebrew() const;           // 0x1015ea40
    void InitThemeSpecific();        // 0x1015e970, no-op
    void DeInitThemeSpecific();      // 0x1015e9a4, no-op
    ~CText() = default; // 0x1015ee20
    uint32_t mInitialised;       // 0x00
    uint32_t mResolutionGroup;   // 0x04 SetRez's 0..3
    uint32_t mLanguageId;        // 0x08 a Windows language id
    TbTextResourceManager mResources; // 0x0c
    InterfaceFontWrapper mFonts[52];  // 0x1c, original stride 0x18
    TbFontRenderParams mRenderParams; // 0x4fc
    void* mResourcePointers[23];      // 0x54c..0x5a4, ltms TbResource subclasses;
                                      // [3] (0x558) is the TbITable of UI strings
};
extern CText gText;   // 0x1037ff18, original name unknown
extern TbDynamicStringTemplate<char> gTextLanguages[16]; // 0x1038050c
extern TbDynamicStringTemplate<char> gTextTitle;          // 0x103804ec
extern TbDynamicStringTemplate<char> gTextLanguage;       // 0x103804cc
