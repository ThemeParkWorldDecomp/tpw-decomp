// ui_shopStatus.cpp: name proven by __sinit_ui_shopStatus_cpp in the main application.

#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "helpers/ui_RideStatusHelpers.h"
#include "History.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstdint>
#include <cstring>
#include <new>

// Unit v51121: the small functions of ui_ridepanel.cpp (RideStatusPanel, ShopStatusPanel), ui_StaffCostsPanel.cpp, the research panel and
// the tank panel message function.

void ShopStatusPanel::InitViewWindow() { InitView(this); }            // 0x101bc468
void ShopStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }   // 0x101bc58c
void ShopStatusPanel::UpdateViewWindow() { ShowPreview(this); }       // 0x101bc5e8

// ShopStatusPanel::InitStatusWindow (0x101bc6c4) and ShopStatusPanel::UpdateWindow (0x101bd730), main app (ui_shopStatus.cpp).

// 0x1024e580: the mesh names of the two gradients (the second at +0x10).
const char gShopStatusMeshNames[0x20] = "ridestatbar.wct\0happygrad.wct";

namespace {

// A figure label: sprite text object (0x19c bytes), the font table's entry 6 and the string (NULL: no text).
void TextWindow(InterfaceWindow* panelWindow, int32_t id, int32_t stringId, bool hasString) {
    InterfaceWindow* window = panelWindow->GetWindow(id);
    void* mem = lender_callee::OperatorNew(0x19c);
    lender_callee::SpriteCtor(mem);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(mem));
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(hasString ? const_cast<char16_t*>(UIMain_GetString(stringId)) : nullptr);
}

// A cash figure: cash text object, font 6, the value 0 through the window's virtual SetValue.
void CashWindow(InterfaceWindow* panelWindow, int32_t id) {
    InterfaceWindow* window = panelWindow->GetWindow(id);
    void* mem = lender_callee::OperatorNew(0x19c);
    lender_callee::CashCtor(mem);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(mem));
    window->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(window, 0);
}

// A gradient bar: indented client, the gradient of the mesh, the value.
void GradientWindow(InterfaceWindow* panelWindow, int32_t id, const char* mesh, int32_t value) {
    InterfaceWindow* window = panelWindow->GetWindow(id);
    UIMain_IndentClient(window, 0, 10);
    void* mem = lender_callee::OperatorNew(0x124);
    parkstatus_callee::GradientCtor(mem, mesh);
    window->SetClientDrawObject(reinterpret_cast<InterfaceDrawObject*>(mem));
    lender_callee::WindowSetValue(window, value);
}

}  // namespace

// 0x101bc6c4 (1892 bytes)
void ShopStatusPanel::InitStatusWindow() {
    if (mStatusWindow == nullptr) return;
    InterfaceWindow* parent = mStatusWindow;
    TextWindow(parent, 0xc075, 0x20, true);
    TextWindow(parent, 0xc071, 0x21, true);
    TextWindow(parent, 0xc072, 0x22, true);
    TextWindow(parent, 0xc073, 0x23, true);
    TextWindow(parent, 0xc074, 0x24, true);
    TextWindow(parent, 0xc07a, 0x25, true);
    TextWindow(parent, 0xc07b, 0x26, true);
    TextWindow(parent, 0xc070, 0, false);
    CashWindow(parent, 0xc076);
    CashWindow(parent, 0xc077);
    CashWindow(parent, 0xc078);
    GradientWindow(parent, 0xc079, gShopStatusMeshNames, 0x32);
    CashWindow(parent, 0xc07d);
    GradientWindow(parent, 0xc07c, gShopStatusMeshNames + 0x10, 100);
}

void ShopStatusPanel::DeInitStatusWindow() {}                         // 0x101bce60

// ShopStatusPanel (main app, ui_shopStatus.cpp): the shop's status panel.
//   0x101bd28c InitWindow (1080 bytes): lists the objects, hides the arrows when there are fewer than two, sets up the scroll bars,
//     the money figure's draw object and the three text objects, and clears mIngredient.
//   0x101bcea0 UpdateStatusWindow (944 bytes): writes the figures of the shop (gStaffRoomPanelThing) into the panel's windows and,
//     when the shop's state changed, tells the panel's table (slot 9).

CObject* gShopStatusObject;   // 0x103959dc (name invented)

namespace shop_callee {
// runtime operator new (glue 0x102384b8). The host's draw objects are larger than the Mac's 0x19c bytes, so the block is never smaller
// than 0x800. (Weak: the unit's test library supplies a recording one.)
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(size > 0x800 ? size : 0x800); }
}  // namespace shop_callee

namespace {

// The runtime's double to unsigned conversion (glue 0x10237cd8, CodeWarrior's __cvt_fp2unsigned): fctiwz of the value, or of the value
// less 2^31 plus 2^31 from 2^31 up; fctiwz saturates.
uint32_t CvtFp2Unsigned(double value) {
    auto truncate = [](double v) -> int32_t {
        if (v != v) return INT32_MIN;
        if (v >= 2147483648.0) return INT32_MAX;
        if (v <= -2147483649.0) return INT32_MIN;
        return static_cast<int32_t>(v);
    };
    if (value >= 2147483648.0) return static_cast<uint32_t>(truncate(value - 2147483648.0)) + 0x80000000u;
    return static_cast<uint32_t>(truncate(value));
}

// The division by 100 of (v << 10) the compiler made with a multiply by 0x51eb851f: round towards zero.
int32_t Percent(int32_t v) {
    int32_t x = static_cast<int32_t>(static_cast<uint32_t>(v) << 10);
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * static_cast<int64_t>(x)) >> 32);
    q >>= 5;
    return q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
}

template <class T>
T* MakeDrawObject() {
    void* memory = shop_callee::OperatorNew(0x19c);
    T* object = static_cast<T*>(memory);
    if (object != nullptr) object = new (memory) T(nullptr);
    return object;
}

void SetUpText(InterfaceWindow* window) {
    window->SetFontJust(1, 1);
    window->SetFont(GetFontTableEntry(6));
    window->SetFontColour(0, 0, 0, 0xff);
}

}  // namespace

// 0x101bcea0 (944 bytes)
void ShopStatusPanel::UpdateStatusWindow() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->GetDBInfo();
    if (mStatusWindow != nullptr) {
        InterfaceWindow* window = mStatusWindow->GetWindow(0xc070);
        double sum1;
        object->mHistory1A4.GetSumOfLast(sum1, 30);
        CvtFp2Unsigned(sum1);
        CvtFp2Unsigned(sum1);
        uint32_t customers = CvtFp2Unsigned(sum1);
        double sum2;
        object->mHistory22C.GetSumOfLast(sum2, 30);
        uint32_t walkedAway = CvtFp2Unsigned(sum2);
        double sum3;
        object->mHistory1A4.GetSumOfLast(sum3, 30);
        CvtFp2Unsigned(sum3);
        uint32_t total = CvtFp2Unsigned(sum3) + walkedAway;
        TbIntegerVTag first(0x19, static_cast<int32_t>(customers));
        TbIntegerVTag second(0x1a, static_cast<int32_t>(total));
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cc, &first, &second);
        window->SetText(gUIMainConvertBuffer);

        window = mStatusWindow->GetWindow(0xc076);
        window->SetValue(object->GetProspectiveCostOfGoods(static_cast<float>(mQuality * 0x32), static_cast<float>(mIngredient)));

        window = mStatusWindow->GetWindow(0xc077);
        double sum4;
        object->mHistory6C.GetSumOfLast(sum4, 30);
        window->SetValue(static_cast<int32_t>(CvtFp2Unsigned(sum4)));

        window = mStatusWindow->GetWindow(0xc079);
        window->SetValue(Percent(object->GetCustomerSatisfaction()));

        window = mStatusWindow->GetWindow(0xc078);
        window->SetValue(object->GetProfitability());

        window = mStatusWindow->GetWindow(0xc07d);
        window->SetValue(static_cast<int32_t>(object->GetCurrentResidualValue()));

        window = mStatusWindow->GetWindow(0xc07c);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(object);
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
        cell.mIndex = static_cast<uint16_t>(bytes[4] + (bytes[6] * 128 + 1));
        window->SetValue(Percent(gGameWorld->GetParkAnalyser()->GetAverageHappinessAroundPoint(cell, 1)));
    }
    int32_t state = UIMain_GetObjectState(object, 2);
    if (state != mState) {
        mState = state;
        UIStatusPanel_CallSlot(this, 0x2c);
    }
}
// 0x101bd28c (1080 bytes)
void ShopStatusPanel::InitWindow() {
    int32_t listed = reinterpret_cast<UIStatusPanel*>(this)->FillObjectList(reinterpret_cast<CThing*>(gShopStatusObject));
    if (listed < 2) {
        mWindow->GetWindow(0xc069)->Show(0);
        mWindow->GetWindow(0xc06a)->Show(0);
    }
    CoolRect rect;
    InterfaceWindow* window = mWindow->GetWindow(0xc06b);
    if (window != nullptr) static_cast<InterfaceScrollBar*>(window)->SetMinMax(0, 2);
    window = mWindow->GetWindow(0xc086);
    if (window != nullptr) static_cast<InterfaceScrollBar*>(window)->SetMinMax(0, 100);

    window = mWindow->GetWindow(0xc085);
    if (window != nullptr) {
        InterfaceCashTextObject* cash = MakeDrawObject<InterfaceCashTextObject>();
        window->SetClientDrawObject(cash);
        int32_t value = 0;
        int32_t maximum = 500;
        static_cast<InterfaceVaryBox*>(window)->SetMinMaxValues(&value, &maximum);
        value = 1;
        static_cast<InterfaceVaryBox*>(window)->SetInc(&value);
        window->SetFont(GetFontTableEntry(6));
        window->SetFontJust(1, 1);
        window->SetFontColour(0, 0, 0, 0xff);
    }

    window = mWindow->GetWindow(0xc07e);
    if (window != nullptr) {
        window->SetClientDrawObject(MakeDrawObject<InterfaceTextObjectSprite>());
        SetUpText(window);
        window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x27)));
    }

    window = mWindow->GetWindow(0xc084);
    if (window != nullptr) {
        window->SetClientDrawObject(MakeDrawObject<InterfaceTextObjectSprite>());
        SetUpText(window);
        window->SetClientDrawText(UIMain_ConvertMulti(gShopStatusObject->GetSpecialIngredientName()));
    }

    window = mWindow->GetWindow(0xc083);
    if (window != nullptr) {
        window->SetClientDrawObject(MakeDrawObject<InterfaceTextObjectSprite>());
        SetUpText(window);
        window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x28)));
    }

    reinterpret_cast<UIStatusPanel*>(this)->InitEditBox(0xc06e);
    mIngredient = 0;
}

void ShopStatusPanel::DeInitWindow() {}                               // 0x101bd6f8

// 0x101bd730 (600 bytes)
void ShopStatusPanel::UpdateWindow() {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    const char16_t* answer = reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(UIStatusPanel_CallSlot(this, 0x3c)));
    InterfaceWindow* window1c = reinterpret_cast<Ptr32<InterfaceWindow>*>(&mF14[8])->get();   // the Mac's pointer at 0x1c
    parkstatus_callee::WindowSetText(window1c, answer);
    UIStatusPanel_CallSlot(this, 0x20);
    UIStatusPanel_CallSlot(this, 0x1c);

    InterfaceWindow* window = mWindow->GetWindow(0xc082);
    if (window != nullptr) {
        InterfaceButton* button = static_cast<InterfaceButton*>(window);
        if (object->IsClosed()) button->SetState(1, 1);
        else button->SetState(0, 1);
    }

    window = mWindow->GetWindow(0xc06b);
    if (window != nullptr) {
        uint32_t quality = object->GetProductQuality();
        lender_callee::WindowSetValue(window, static_cast<int32_t>(quality / 0x32));
    }

    window = mWindow->GetWindow(0xc086);
    if (window != nullptr) lender_callee::WindowSetValue(window, object->GetAmountOfSpecialIngredient());

    InterfaceWindow* amountWindow = mWindow->GetWindow(0xc086);
    InterfaceWindow* nameWindow = mWindow->GetWindow(0xc084);
    char16_t* text = UIMain_ConvertMulti(object->GetSpecialIngredientName());
    if (text != nullptr) {
        amountWindow->Show(1);
        nameWindow->Show(1);
        nameWindow->SetClientDrawText(text);
    } else {
        amountWindow->Show(0);
        nameWindow->Show(0);
    }

    window = mWindow->GetWindow(0xc085);
    if (window != nullptr) lender_callee::WindowSetValue(window, static_cast<int32_t>(object->GetPricePerUse()));
}

// 0x102ad644 (name invented): the window description.
int16_t gShopStatusDescription[] = {TPW_WINDOW_SCRIPT_102AD644};   // 0x102ad644: the image's words (generated, devtools/gen_game_tables.py)

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

// UpdateStaffCost 0x101be030 (412 bytes), StaffCostsPanelMsg 0x101be1f4 (524 bytes), ShopStatusMsg 0x101bdb8c (796 bytes).

int32_t ShopStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ShopStatusPanel* panel = &gShopStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message == 0x10) {
        if (a == 0x80080) panel->UpdateStatusWindow();
    } else if (message == 5) {
        panel->Remove();
        return 1;
    } else if (message == 0x14) {
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xd1));
    } else if (message == 0x1e) {
        UIMain_TickRidePreview();
    } else if (message == 0x100) {
        if (a == 0xc06a) {
            base->GetNextObject();
        } else if (a > 0xc06a) {
            if (a == 0xc081) {
                // The shop's own callback (0x101bda5c, the tvector at 0x10267e20); the sideshow panel's _applytoall copied
                // that panel's values instead, so the shops' price became 0 and their quality and ingredient stayed. Found by
                // Piro (Discord).
                UI_IterateThingList(kUIIterShops, _applytoall_shop);
                UIStatusPanel_CallSlot(panel, 0x1c);
                return 0;
            }
            if (a > 0xc081) {
                if (a < 0xc083) {
                    base->CloseObject(b == 1 ? 1 : 0);
                    panel->UpdateStatusWindow();
                }
            } else if (a >= 0xc080) {
                base->Move();
                return 0;
            }
        } else {
            if (a == 0xc067) {
                CreateAllItemsPanel(kUIObjShop, gStaffRoomPanelThing);
                return 0;
            }
            if (a > 0xc067) {
                if (a >= 0xc069) {
                    base->GetPreviousObject();
                } else {
                    base->EraseObject();
                    return 0;
                }
            } else if (a < 0 && a >= -2) {
                panel->Remove();
                return 0;
            }
        }
    } else if (message == 0x800) {
        if (a == 0xc085) {
            panel->mF2c = b;
            UIStatusPanel_CallSlot(panel, 0x1c);
        } else if (a < 0xc085) {
            if (a == 0xc06b) {
                panel->mQuality = b;
                panel->mWindow->GetWindow(0xc07f)->SetValue((b & 0x20) != 0 ? 0 : static_cast<int32_t>(1u << (b & 0x1f)));   // slw: a shift of 32 to 63 gives 0
                UIStatusPanel_CallSlot(panel, 0x1c);
            }
        } else if (a < 0xc087) {
            panel->mIngredient = b;
            UIStatusPanel_CallSlot(panel, 0x1c);
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x101bdee0 (80 bytes)
void ShopStatusPanel::Show(CThing* thing) {
    gShopStatusObject = reinterpret_cast<CObject*>(thing);
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gShopStatusDescription, ShopStatusMsg, 0xc06c, 0xc06f, 0xc06d);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xd1));
}

void ShopStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101bdf64

// 0x101bda5c (the name is shared with the sideshow panel's 0x101c25c0, so the host name differs).
int32_t _applytoall_shop(CThing* thing) {
    gShopStatusPanel.applytoallcallback(thing);
    return 1;
}

// Static initialiser of ui_shopStatus.cpp: constructs gShopStatusPanel as a UIStatusPanel, stores the second
// table at +0x20 and the word 1 at +0.
void __sinit_ui_shopStatus_cpp() {
    ShopStatusPanel* panel = &gShopStatusPanel;
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(panel));
    panel->mVtable.set(ShopStatusPanelTable());
    *reinterpret_cast<int32_t*>(panel->mF00) = 1;
}
