// ui_playerLogin.cpp's static initialiser (0x101afa90): two wide strings built from the data at 0x1024e19e and registered for
// destruction at exit.
#include <cstdint>
#include <cstring>
#include <new>
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "email.h"
#include "platform/msl.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "AdvisorScoring.h"

void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// The data at 0x1024e19e starts "/\" (0x002f 0x005c); the two strings sit at +0x14 and +0x22. Their text is not known yet.
const char16_t gPlayerLoginStrings[0x30] = {u'/', u'\\'};
alignas(16) uint8_t gPlayerLoginStringA[64];   // 0x10394ae4
alignas(16) uint8_t gPlayerLoginStringB[64];   // 0x10394ac4
uint8_t gPlayerLoginNodeA[16];                 // 0x10394af8
uint8_t gPlayerLoginNodeB[16];                 // 0x10394ad8

void __sinit_ui_playerLogin_cpp() {
    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyWString);
    auto* a = new (gPlayerLoginStringA) TbDynamicStringTemplate<char16_t>(gPlayerLoginStrings + 0x14 / 2);
    __register_global_object(a, destroy, gPlayerLoginNodeA);
    auto* b = new (gPlayerLoginStringB) TbDynamicStringTemplate<char16_t>(gPlayerLoginStrings + 0x22 / 2);
    __register_global_object(b, destroy, gPlayerLoginNodeB);
}
