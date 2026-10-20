// The destructors of the bar and the text box (main app, UI toolkit; unit w54). Both are called by ~InterfaceWindow's
// `while (mChild) delete mChild`. The compiler supplies what the Mac's versions do around the body: the vtable store at +0x118 (0x102a7c78
// for the bar, 0x102afd14 for the text box), the base destructor (~InterfaceWindow, unit w51) and, for the deleting form (the SHORT in r4
// positive), the operator delete of `this`. The Mac skips everything for a NULL `this`, which the host never has.
#include "ui_controls.h"

// 0x101c87a4 (120 bytes). The Mac's body is the bar's rectangle member (+0x148) destroyed (CoolRect::~CoolRect, translated, nothing
// to do on the host) before the base class; nothing else.
InterfaceBar::~InterfaceBar() {}

// 0x101d7cc8 (124 bytes). Frees the text block (+0x138) if there is one, with the plain operator delete (the block is made with operator
// new[] on the Mac by SetText, 0x101d7f50, which is not translated; the host takes the same call so that the heap traffic is the
// Mac's). The pointer is not cleared.
InterfaceTextBox::~InterfaceTextBox() {
    if (mText != nullptr) operator delete(mText);
}
