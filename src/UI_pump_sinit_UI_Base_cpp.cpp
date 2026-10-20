// UI_Base.cpp's static initialiser and UI_ToggleHelpText (unit v5295).
#include <new>

#include "UI_Base.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

char gUIDefaultHelpTextNode[16];   // 0x10395ba4

// The destructor TVector at 0x10268238 (D1 with the hidden delete flag).
void UI_DestroyDefaultHelpText(InterfaceHelpText* object, int16_t deleteFlag) {
    (void)deleteFlag;
    object->~InterfaceHelpText();
}

// 0x101ca9d0
void UI_ToggleHelpText() {
    InterfaceHelpText* handler = gUIHelpText;
    if (handler->mEnabled != 0)
        handler->Disable();   // vtable +0x18
    else
        handler->Enable();    // vtable +0x14
}

// 0x101cac94
void __sinit_UI_Base_cpp() {
    new (&gUIDefaultHelpText) InterfaceHelpText();
    __register_global_object(&gUIDefaultHelpText, reinterpret_cast<void*>(&UI_DestroyDefaultHelpText), gUIDefaultHelpTextNode);
    // The Mac stores the vtable of 0x102a7ae4, then the one of 0x102af94c, in the clock object (same slot).
    new (&gUIDefaultClock) clockTickFunction();
}
