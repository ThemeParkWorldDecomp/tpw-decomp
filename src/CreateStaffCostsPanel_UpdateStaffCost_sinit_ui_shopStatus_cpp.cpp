#include <cstdint>
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_iterators.h"
#include "ui_main.h"

// Static initialiser of ui_shopStatus.cpp: constructs gShopStatusPanel as a UIStatusPanel, stores the second
// table at +0x20 and the word 1 at +0.
void __sinit_ui_shopStatus_cpp() {
    ShopStatusPanel* panel = &gShopStatusPanel;
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(panel));
    panel->mVtable.set(ShopStatusPanelTable());
    *reinterpret_cast<int32_t*>(panel->mF00) = 1;
}
