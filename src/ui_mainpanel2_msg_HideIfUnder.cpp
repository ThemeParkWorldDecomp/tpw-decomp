#include <cstdint>
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "UI_Base.h"
#include "World.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "ui_main.h"

// 0x102aad44: the animation the bobble window gets when the aerial window moves (the image's frames; the same as gTagShiftAnim).
UI_RectDelta gAerialSlide[15] = {
    {-2, 2, 2, 0}, {-2, 2, 2, 0}, {-2, 2, 2, 0},
    {-2, 2, 2, 0}, {-2, 2, 2, 0}, {-2, 2, 2, 0},
    {4, -4, -4, 0}, {4, -4, -4, 0}, {-2, 2, 2, 0},
    {-2, 2, 2, 0}, {2, -2, -2, 0}, {2, -2, -2, 0},
    {2, -2, -2, 0}, {2, -2, -2, 0}, {kAnimEnd, 0, 0, 0},
};

namespace {
// One level of HideIfUnder: a window whose right edge is above the limit is shown, any other hidden.
void Apply(InterfaceWindow* window, CoolRect& rect, int16_t limit) {
    if (window != nullptr) {
        window->GetPosition(&rect, 1);
        if (rect.mRight <= limit) window->Show(0);
        else window->Show(1);
    }
}

// The Mac passes windows through the message arguments as 32-bit words; on the host such a word is the window's offset in the arena
// (platform/arena.h; 0 is NULL). The tag windows are the exception the port fix below handles (unit v5L39b).
InterfaceWindow* AsWindow(int32_t word) {
    if (word == 0) return nullptr;
    return reinterpret_cast<InterfaceWindow*>(tpw::gArenaBase + static_cast<uint32_t>(word));
}
}  // namespace

// 0x101a6c44 (692 bytes): four levels of loops, then the recursion.
void HideIfUnder(InterfaceWindow* window, int16_t limit) {
    CoolRect r0;
    Apply(window, r0, limit);
    for (InterfaceWindow* a = window->GetChild(0); a != nullptr; a = a->GetNext()) {
        CoolRect r1;
        Apply(a, r1, limit);
        for (InterfaceWindow* b = a->GetChild(0); b != nullptr; b = b->GetNext()) {
            CoolRect r2;
            Apply(b, r2, limit);
            for (InterfaceWindow* c = b->GetChild(0); c != nullptr; c = c->GetNext()) {
                CoolRect r3;
                Apply(c, r3, limit);
                for (InterfaceWindow* d = c->GetChild(0); d != nullptr; d = d->GetNext()) HideIfUnder(d, limit);
            }
        }
    }
}

// 0x101a68d0 (824 bytes): the aerial window's message function (window 0x2d).
int32_t AerialCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100001) {
        // a window to hang above this one: it is linked in, enabled, and this window moves up by its height
        void* extra0 = window->getExtraParam(0);
        void* extra1 = window->getExtraParam(1);
        if (a != 0) {
            InterfaceWindow* child = AsWindow(a);
            CoolRect aerial;
            CoolRect childRect;
            window->link(child, 1, 0);
            entry_callee::WindowEnable(child, 1);
            UI_PutDeferredMessage(child, 6, 1, 0, 500);
            child->GetPosition(&childRect, 1);
            int16_t width, height;
            child->GetDims(&width, &height, 1);
            height = static_cast<int16_t>(height + 10);
            window->GetPosition(&aerial, 1);
            window->AdjustPosition(0, static_cast<int16_t>(-height), 0, 0);
            window->GetPosition(&aerial, 1);
            int32_t half = (static_cast<int32_t>(aerial.mRight) - aerial.mLeft) >> 1;
            int16_t x = static_cast<int16_t>(aerial.mLeft + (half - (width >> 1)));
            child->ShiftPosition(static_cast<int16_t>(x - childRect.mLeft),
                                 static_cast<int16_t>(aerial.mTop + 8 - childRect.mTop));
            window->setExtraParam(0, extra0);
            window->setExtraParam(1, extra1);
            InterfaceWindow* bobble = window->GetWindow(0x2e);
            if (bobble != nullptr && bobble->mAnimating == 0) bobble->startAnim(gAerialSlide, 0x32);
        }
    } else if (message < 0x100001) {
        if (message == 0x14) gTagFirstWindow = nullptr;
    } else if (message == 0x100003) {
        InterfaceWindow* w = gTagFirstWindow;
        if (w != nullptr) {
            int32_t n = 1;
            for (; w != nullptr; w = w->GetNext()) {
                UI_PutDeferredMessage(w, 5, 1, 0, static_cast<uint32_t>(n * 100));
                n = n + 1;
            }
        }
    } else if (message < 0x100003) {
        if (a != 0) {
            InterfaceWindow* w = AsWindow(a);
            if ((w->GetAttr() & 1) != 0) {
                w->Show(0);
                if (b == 0) UI_PutDeferredMessage(w, 5, 1, 0, 500);
                else UI_SendMessage(w, 5, 1, 0);
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
