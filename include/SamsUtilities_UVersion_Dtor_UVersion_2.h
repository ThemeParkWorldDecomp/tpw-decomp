// Unit v51042 (src/game/ui/ui_main.cpp, src/game/ui/ui_parkStatus.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_Postcard.cpp): UVersion's destructor, the object-state
// text, the rank text object's Change, the lender list's callbacks, the thought panel's and the name edit's handlers.
// Mac offsets in the comments.
#pragma once
#include <cstdint>

#include "InterfaceWindow.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"

// 0x10187710 (112 bytes): SamsUtilities::UVersion::~UVersion(short deleteFlag), a free function (the class has no declared destructor, the
// game's own are the members'). Destroys the comment string (+0x18) then the text (+0xc) and, for a flag above 0 (a signed halfword),
// deletes the object; answers the object.
SamsUtilities::UVersion* UVersion_Dtor(SamsUtilities::UVersion* self, int16_t deleteFlag);

// 0x1018fb08 (156 bytes): InterfaceRankTextObject::Change(unsigned int, DOChange*), a free function (the class is not declared: its
// vtable would need the whole draw object chain). With flag 0x80 the rank text is made here, "%d" of the word at the change's +0
// (the format at 0x1024dd1a), or the string 0x16 bytes on when that word is 0x7fffffff, then drawn through the text sprite's Change
// with flag 0x100; any other flags go straight to it.
void InterfaceRankTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change);

// 0x102a7780 and 0x102a77ec (names invented): per object state, the text colour (red, green, blue, alpha in the high to low bytes) and the
// UI string id (a TbITable id) UIMain_SetObjectStateText uses. Only the first colour (0xffffffff) is known: the rest is not kept here.
extern uint32_t gUIObjectStateColours[27];   // 0x102a7780
extern uint32_t gUIObjectStateStrings[27];   // 0x102a77ec
// 0x10394a20 (name invented): the thought panel's state; ThoughtMsg stores the thought text's pointer at +4.
extern uint32_t gThoughtState[2];            // 0x10394a20

// Callees the host cannot call as the Mac does (the virtuals through __ptr_glue at the table at window +0x118: +0x28 GetValue, +0x2c
// SetText; the runtime's operator delete 0x10238218): weak definitions in the unit's file, faked by the test library.
namespace v51042_callee {
int32_t WindowGetValue(InterfaceWindow* window);                // vtable +0x28
void WindowSetText(InterfaceWindow* window, char16_t* text);    // vtable +0x2c
void OperatorDelete(void* block);                               // 0x10238218
}  // namespace v51042_callee
