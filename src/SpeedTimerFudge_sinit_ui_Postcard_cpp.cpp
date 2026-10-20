// __sinit_ui_Postcard_cpp 0x101b30c8: the static initialiser of ui_Postcard.cpp. It constructs eighteen
// narrow strings and one wide string in bss and registers each for destruction at exit.
#include <cstdint>
#include <new>

#include "TbDynamicStringTemplate.h"


extern "C" void __register_global_object(void* object, void* destructor, void* node);
void UIMain_DestroyString(void* object);    // 0x103bcd38, the narrow string destructor glue
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// The bss block 0x10395700 to 0x10395960 of ui_Postcard.cpp:
//   0x000 wide string (0x14)   0x014 node   0x020 string   0x034 node   0x040 string   0x054 node
//   0x060 sixteen strings (0x14 each, the languages)   0x1a0 sixteen nodes (12 each, in reverse order)
alignas(16) unsigned char gPostcardBlob[0x260];

namespace {
const char* const kLanguages =
    "English\0French\0German\0Spanish\0Swedish\0Italian\0Dutch\0Polish\0Danish\0Japanese\0"
    "TraditionalChinese\0SimplifiedChinese\0Thai\0Korean\0Hebrew\0American";
}

void __sinit_ui_Postcard_cpp() {
    void* dtor = reinterpret_cast<void*>(&UIMain_DestroyString);
    const char* text = kLanguages;
    for (int i = 0; i < 16; i++) {
        unsigned char* obj = gPostcardBlob + 0x60 + 0x14 * i;
        new (obj) TbDynamicStringTemplate<char>(text);
        __register_global_object(obj, dtor, gPostcardBlob + 0x1a0 + 0xc * (15 - i));
        while (*text) text++;
        text++;
    }
    new (gPostcardBlob + 0x40) TbDynamicStringTemplate<char>("Theme Park World");
    __register_global_object(gPostcardBlob + 0x40, dtor, gPostcardBlob + 0x54);
    new (gPostcardBlob + 0x20) TbDynamicStringTemplate<char>("Language");
    __register_global_object(gPostcardBlob + 0x20, dtor, gPostcardBlob + 0x34);
    new (gPostcardBlob) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(gPostcardBlob, reinterpret_cast<void*>(&UIMain_DestroyWString), gPostcardBlob + 0x14);
}
