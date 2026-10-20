// extern "C" wrappers for TPDialog.cpp, with native string accessors.
#include "TPDialog.h"
#include <cstring>
#include <new>

extern "C" {
uint32_t tpw_GetOptionsSetting() { return GetOptionsSetting(); }
int32_t tpw_GetNumKidSprites() { return GetNumKidSprites(); }
void tpw_sinit_TPDialog() {
    const char* const languages[16] = {
        "English", "French", "German", "Spanish", "Swedish", "Italian", "Dutch", "Polish",
        "Danish", "Japanese", "TraditionalChinese", "SimplifiedChinese", "Thai", "Korean",
        "Hebrew", "American"
    };
    for (int32_t i = 0; i < 16; ++i)
        new (&gTPDialogLanguages[i]) TbDynamicStringTemplate<char>(languages[i]);
    new (&gTPDialogTitle) TbDynamicStringTemplate<char>("Theme Park World");
    new (&gTPDialogLanguage) TbDynamicStringTemplate<char>("Language");
}
void* tpw_TPDialog_options() { return &gTPDialogOptions; }
TbDynamicStringTemplate<char>* tpw_TPDialog_string(int32_t i) {
    return i < 16 ? &gTPDialogLanguages[i] : i == 16 ? &gTPDialogTitle : &gTPDialogLanguage;
}
void tpw_TPDialog_seed(int32_t i, uint32_t capacity, uint32_t data, uint32_t length, uint32_t grow) {
    auto* p = tpw_TPDialog_string(i);
    std::memcpy(&p->mCapacity, &capacity, 4);
    p->mData = reinterpret_cast<char*>(static_cast<uintptr_t>(data));
    std::memcpy(&p->mLength, &length, 4);
    std::memcpy(&p->mGrowBy, &grow, 4);
}
void tpw_TPDialog_read(int32_t i, uint32_t* capacity, const char** text, uint32_t* length, uint32_t* grow) {
    auto* p = tpw_TPDialog_string(i);
    std::memcpy(capacity, &p->mCapacity, 4);
    *text = p->mData;
    std::memcpy(length, &p->mLength, 4);
    std::memcpy(grow, &p->mGrowBy, 4);
}
}
