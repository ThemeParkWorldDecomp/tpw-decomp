// InterfaceCheckButtons::~InterfaceCheckButtons (main app, SimTheme Park 2.04b3, 0x101cc02c, 104 bytes; the inventory files it under
// ui_cityLobbyController.cpp). Unit w91.
#include "ui_controls.h"

// The Mac's destructor stores the class's vtable pointer (0x102af9c8, at +0x118), calls ~InterfaceWindow with the deleting flag 0 and
// then, when its own flag (r4) is positive, operator delete. On the host the vtable store, the call of the base destructor and the
// deleting form are the compiler's, so the body is empty: the destructor is the base's (src/InterfaceWindow_dtor.cpp).
InterfaceCheckButtons::~InterfaceCheckButtons() {}
