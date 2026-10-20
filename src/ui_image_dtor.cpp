// The destructor of the image control (main app, UI toolkit; unit w75; the inventory files it under ui_cityLobbyController.cpp). The
// compiler supplies what the Mac's version does around the body: the vtable store at +0x118 (0x102afaec), the base destructor
// (~InterfaceWindow, unit w51) and, for the deleting form (the SHORT in r4 positive), the operator delete of `this`. The Mac skips
// everything for a NULL `this`, which the host never has. The image has no members to release (mF138 and mValue are plain words).
#include "ui_controls.h"

// 0x101cfebc (104 bytes).
InterfaceImage::~InterfaceImage() {}
