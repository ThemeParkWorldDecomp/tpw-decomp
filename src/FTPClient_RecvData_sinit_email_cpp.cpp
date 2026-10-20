// email.cpp's static initialiser (0x1020afc0, 856 B): builds the language name strings, the app name and the "Language" key, and a
// small blob (a 3 byte buffer holding the default byte), registering each for destruction at exit.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "FS_Online.h"
#include "Ptr32.h"
#include "platform/arena.h"
#include "TbDynamicStringTemplate.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyString(void* object);    // 0x103bcd38, the narrow string destructor glue
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// Weak: operator new (glue 0x102384b8); the test library replaces it with a recording fake. The blob is kept through a Ptr32, so
// its memory comes from the arena, as the fake's does (the host heap aborted the launcher: an address outside the arena).
__attribute__((weak)) void* EmailOperatorNew(std::size_t size) {
    return tpw::ArenaAllocZeroed(size != 0 ? size : 4);
}

namespace {
constexpr std::size_t kStringSize = sizeof(TbDynamicStringTemplate<char>);
}

alignas(16) uint8_t gEmailLanguageStrings[16 * kStringSize];
alignas(16) uint8_t gEmailAppNameString[kStringSize];
alignas(16) uint8_t gEmailLanguageKeyString[kStringSize];
alignas(16) uint8_t gEmailWideString[sizeof(TbDynamicStringTemplate<char16_t>)];
uint8_t gEmailNodes[19 * 12];
uint8_t gEmailBlobNode[12];
uint8_t gEmailBlobDtorVec[16];
uint8_t gEmailDefaultByte;
uint32_t gEmailBlob;

// 0x1020afc0
void __sinit_email_cpp() {
    static const char* const kLanguages[16] = {"English",  "French", "German",  "Spanish",
                                               "Swedish",  "Italian", "Dutch",  "Polish",
                                               "Danish",   "Japanese", "TraditionalChinese", "SimplifiedChinese",
                                               "Thai",     "Korean", "Hebrew",  "American"};
    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyString);
    // The wide string at 0x10250130 (kFailureType + 0x98 in the data).
    auto* wide = new (gEmailWideString) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(wide, reinterpret_cast<void*>(&UIMain_DestroyWString), gEmailNodes + 16 * 12);
    for (int i = 0; i < 16; i++) {
        auto* text = new (gEmailLanguageStrings + i * kStringSize) TbDynamicStringTemplate<char>(kLanguages[i]);
        __register_global_object(text, destroy, gEmailNodes + i * 12);
    }
    auto* app = new (gEmailAppNameString) TbDynamicStringTemplate<char>("Theme Park World");
    __register_global_object(app, destroy, gEmailNodes + 17 * 12);
    auto* key = new (gEmailLanguageKeyString) TbDynamicStringTemplate<char>("Language");
    __register_global_object(key, destroy, gEmailNodes + 18 * 12);

    gEmailBlob = 0;
    EmailBlob* blob = static_cast<EmailBlob*>(EmailOperatorNew(0x10));
    Ptr32<EmailBlob> link;
    link = blob;
    gEmailBlob = link.mOffset;
    if (blob != nullptr) {
        blob->mLength = 3;
        blob->mFlag = 1;
        Ptr32<char> data;
        data = static_cast<char*>(EmailOperatorNew(static_cast<std::size_t>(blob->mLength) + 1));
        blob->mDataOffset = data.mOffset;
    }
    Ptr32<EmailBlob> again;
    again.mOffset = gEmailBlob;
    reinterpret_cast<char*>(Ptr32<char>{again->mDataOffset}.get())[0] = static_cast<char>(gEmailDefaultByte);
    again->mF00 = 0;
    __register_global_object(&gEmailBlob, gEmailBlobDtorVec, gEmailBlobNode);
}
