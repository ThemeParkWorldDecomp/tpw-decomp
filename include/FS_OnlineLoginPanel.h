#pragma once
// Declarations for CreateOnlineLoginPanel (0x101e27bc, src/game/ui/ui_onlineLogin.cpp).
#include <cstdint>

#include "ui_DrawObject.h"

// 0x1018dc38 (60 bytes, untranslated): the password edit box's draw object, an InterfaceTextObjectSprite that shows stars.
class InterfacePasswordTextObject : public InterfaceCursorPosTextObjectSprite {   // (unit v51115: the Mac constructor calls 0x1018f1d0)
public:
    explicit InterfacePasswordTextObject(void* data);   // 0x1018dc38
};

extern int16_t gOnlineLoginPanelScript[];       // 0x102b02fc: the Dialog script
extern char16_t gOnlineLoginEmptyText[2];      // 0x1024ee76: the text given to the two edit boxes (empty)
extern void (*gOnlineLoginFailureFn)();        // 0x10395d28: the failure function CreateOnlineLoginPanel was given
