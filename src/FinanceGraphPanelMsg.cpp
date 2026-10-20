// FinanceGraphPanelMsg 0x1019f5e4: the message handler of the finance graph panel.
#include "FinanceGraphPanelMsg.h"

uint8_t gFinanceKissMyLines[16];
uint32_t gFinanceGraphFlags = 1;
CUIGrapher gFinanceGrapher;
int32_t gFinanceGraphMode = 0;
InterfaceWindow* gFinanceGraphPanel = nullptr;

namespace {

void Redraw(InterfaceWindow* window) {
    int32_t maxY = GetMaxYValue();
    int32_t minY = GetMinYValue();
    gFinanceGrapher.SetYRange(minY, maxY, 10);
    DrawMeGraph(window, gFinanceGrapher, reinterpret_cast<void (*)(CUIGrapher&)>(gFinanceKissMyLines));
}

void Release(InterfaceWindow* parent, int32_t id) {
    static_cast<InterfaceButton*>(parent->GetWindow(id))->SetState(0, 1);
}

// The buttons of the first group (0x12277, 0x1227b): bits 0..1; the second group clears bits 2..7.
void ToggleBit(InterfaceWindow* window, int32_t down, uint32_t bit, uint32_t groupMask, bool secondGroup,
               const int32_t* ids, int n, int32_t firstId) {
    uint32_t& flags = gFinanceGraphFlags;
    if (down == 1) {
        if ((flags & groupMask) != 0) {
            InterfaceWindow* panel = gFinanceGraphPanel->GetWindow(0x12272);
            if (secondGroup) {
                for (int i = 0; i < n; i++) Release(panel, ids[i]);
                flags &= 0xffffff03u;
            } else {
                Release(panel, firstId);
                Release(panel, 0x1227b);
                flags &= 0xfffffffcu;
            }
        }
        flags |= bit;
    } else {
        flags &= ~bit;
    }
    Redraw(window);
}

}  // namespace

int32_t FinanceGraphPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CoolRect rect;
    static const int32_t kSecond[6] = {0x12279, 0x12278, 0x1227c, 0x1227d, 0x1227a, 0x1227e};
    static const int32_t kSecondB[6] = {0x12279, 0x12278, 0x1227c, 0x1227d, 0x1227a, 0x1227e};
    (void)kSecondB;

    if (message == 0x100) {
        if (a == 0x1227a) {
            ToggleBit(window, b, 0x40, 3, false, nullptr, 0, 0x12277);
        } else if (a < 0x1227a) {
            if (a == 0x12277) {
                ToggleBit(window, b, 1, 0xfc, true, kSecond, 6, 0);
            } else if (a < 0x12277) {
                if (a == -2) {
                    RemoveFinancePanelPanel();
                    return 0;
                }
            } else if (a < 0x12279) {
                ToggleBit(window, b, 8, 3, false, nullptr, 0, 0x12277);
            } else {
                ToggleBit(window, b, 4, 3, false, nullptr, 0, 0x12277);
            }
        } else if (a == 0x1227d) {
            ToggleBit(window, b, 0x20, 3, false, nullptr, 0, 0x12277);
        } else if (a < 0x1227d) {
            if (a < 0x1227c) {
                ToggleBit(window, b, 2, 0xfc, true, kSecond, 6, 0);
            } else {
                ToggleBit(window, b, 0x10, 3, false, nullptr, 0, 0x12277);
            }
        } else if (a < 0x1227f) {
            ToggleBit(window, b, 0x80, 3, false, nullptr, 0, 0x12277);
        }
    } else if (message < 0x100) {
        if (message == 0x15) {
            window->GetClientRect(&rect);
            int32_t maxY = GetMaxYValue();
            int32_t minY = GetMinYValue();
            SetupGraph(gFinanceGrapher, rect, minY, maxY, 10, 0, 0xb, 0xc);
            DrawMeGraph(window, gFinanceGrapher, reinterpret_cast<void (*)(CUIGrapher&)>(gFinanceKissMyLines));
        }
    } else if (message < 0x102) {
        bool hit = false;
        if (a == 0x12275) {
            hit = true;
            gFinanceGraphMode = 0;
        } else if (a < 0x12275) {
            if (a > 0x12273) {
                hit = true;
                gFinanceGraphMode = 2;
            }
        } else if (a < 0x12277) {
            hit = true;
            gFinanceGraphMode = 1;
        }
        if (hit && b == 1) {
            int32_t mode = gFinanceGraphMode;
            if (mode == 1) {
                gFinanceGrapher.SetXRange(0, 0x23, 0x24);
            } else if (mode < 1) {
                if (mode >= 0) gFinanceGrapher.SetXRange(0, 0xb, 0xc);
            } else if (mode < 3) {
                gFinanceGrapher.SetXRange(0, 0x8f, 0x90);
            }
            Redraw(window);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
