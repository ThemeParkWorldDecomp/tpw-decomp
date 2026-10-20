// ShopStatusPanel (main app, ui_shopStatus.cpp): the shop's status panel.
//   0x101bd28c InitWindow (1080 bytes): lists the objects, hides the arrows when there are fewer than two, sets up the scroll bars,
//     the money figure's draw object and the three text objects, and clears mIngredient.
//   0x101bcea0 UpdateStatusWindow (944 bytes): writes the figures of the shop (gStaffRoomPanelThing) into the panel's windows and,
//     when the shop's state changed, tells the panel's table (slot 9).
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
        mOps->Notify(this);
    }
}
