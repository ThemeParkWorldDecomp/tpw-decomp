// The text objects of the main panel's number windows (Mac file ui_textmanager.cpp; unit w118): InterfaceNumberTextObject constructor
// 0x1018d930 (164 bytes) and destructor 0x1018da0c (188), InterfaceCashAndGTickTextObject constructor 0x1018e190 (164) and destructor
// 0x1018e274 (188). Their Change (0x1018db00 and 0x1018e36c) is untranslated.
//
// The Mac constructors are InterfaceTextObjectSprite's (0x1018d11c, src/ui_DrawObjectText_ctor.cpp) with the base constructor inlined, in
// the same order, and the class's own vtable stored last; the host chains to that constructor. The destructors are the sprite chain
// inlined (the formatter, the sprite list, the engine data, the draw object), which the language's member destruction repeats.
#include "ui_DrawObject.h"

InterfaceNumberTextObject::InterfaceNumberTextObject(void* data) : InterfaceTextObjectSprite(data) {}

InterfaceNumberTextObject::~InterfaceNumberTextObject() {}

InterfaceCashAndGTickTextObject::InterfaceCashAndGTickTextObject(void* data) : InterfaceTextObjectSprite(data) {}

InterfaceCashAndGTickTextObject::~InterfaceCashAndGTickTextObject() {}
