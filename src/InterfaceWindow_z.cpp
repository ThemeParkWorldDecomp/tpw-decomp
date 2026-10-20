// InterfaceWindow::setZ (main app, SimTheme Park 2.04b3, 0x101dd8bc, 540 bytes) and GetClientRect (0x101dba4c, 56 bytes); the inventory
// files both under ui_cityLobbyController.cpp. Unit w43b. SetClientRect (0x101db3fc, 56 bytes): unit v5L14.
#include "InterfaceWindow.h"
#include "ui_DrawObject.h"

namespace {

constexpr int16_t kMaxZ = 0x100;   // a window's depth is cut to this above (not below)
constexpr uint32_t kChangeDepth = 2;

// Keeps the depth in the window (cut at kMaxZ) and tells its draw object that depth and its client draw object one more.
void PlaceDepth(InterfaceWindow* window, int16_t z) {
    window->mZ = z;
    if (window->mZ > kMaxZ) window->mZ = kMaxZ;
    DOChange change{};
    change.mDepth = window->mZ;
    if (window->mDrawObject != nullptr) window->mDrawObject->Change(kChangeDepth, &change);
    change.mDepth = static_cast<int16_t>(window->mZ + 1);
    if (window->mClientDrawObject != nullptr) window->mClientDrawObject->Change(kChangeDepth, &change);
}

}  // namespace

// The Mac unrolls three levels (the window, its children with z + 2, theirs with z + 4) and calls setZ for the windows below those with
// z + 6, each of which starts the same walk again. The depths are 16-bit: the additions wrap.
void InterfaceWindow::setZ(int16_t z) {
    PlaceDepth(this, z);
    for (InterfaceWindow* child = mChild; child != nullptr; child = child->mNext) {
        PlaceDepth(child, static_cast<int16_t>(z + 2));
        for (InterfaceWindow* grandchild = child->mChild; grandchild != nullptr; grandchild = grandchild->mNext) {
            PlaceDepth(grandchild, static_cast<int16_t>(z + 4));
            for (InterfaceWindow* deeper = grandchild->mChild; deeper != nullptr; deeper = deeper->mNext)
                deeper->setZ(static_cast<int16_t>(z + 6));
        }
    }
}

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    if (rect == nullptr) return 0;
    rect->mLeft = mClientRect.mLeft;
    rect->mTop = mClientRect.mTop;
    rect->mRight = mClientRect.mRight;
    rect->mBottom = mClientRect.mBottom;
    return 1;
}

int32_t InterfaceWindow::SetClientRect(CoolRect* rect) {
    if (rect == nullptr) return 0;
    mClientRect.mLeft = rect->mLeft;
    mClientRect.mTop = rect->mTop;
    mClientRect.mRight = rect->mRight;
    mClientRect.mBottom = rect->mBottom;
    return 1;
}
