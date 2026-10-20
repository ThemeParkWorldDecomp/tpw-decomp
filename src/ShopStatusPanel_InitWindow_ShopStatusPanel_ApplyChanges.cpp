#include <cstdint>
#include <new>
#include "History.h"
#include "InterfaceScrollBar.h"
#include "MainWindowCallback.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

// 0x102ad644 (name invented): the window description; the Mac's first word is 0x00000001. Its real size is unknown.
int16_t gShopStatusDescription[2] = {0, 1};

// 0x101bd9bc (108 bytes)
void ShopStatusPanel::ApplyChanges() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->SetAmountOfSpecialIngredient(mIngredient);
    object->SetProductQuality(static_cast<uint8_t>(mQuality * 0x32));
    object->SetPricePerUse(mF2c);
}

// 0x101bdab8 (144 bytes)
void ShopStatusPanel::applytoallcallback(CThing* thing) {
    uint16_t mine = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->mObjectType;
    CObject* object = reinterpret_cast<CObject*>(thing);
    uint16_t other = object->mObjectType;
    if (other == mine) {
        object->SetAmountOfSpecialIngredient(mIngredient);
        object->SetProductQuality(static_cast<uint8_t>(mQuality * 0x32));
        object->SetPricePerUse(mF2c);
    }
}

// 0x101bdee0 (80 bytes)
void ShopStatusPanel::Show(CThing* thing) {
    gShopStatusObject = reinterpret_cast<CObject*>(thing);
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gShopStatusDescription, ShopStatusMsg, 0xc06c, 0xc06f, 0xc06d);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xd1));
}
