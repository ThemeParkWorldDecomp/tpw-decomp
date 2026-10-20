// Unit v5335: the sideshows list rows and the all-items list's row window.
//   UpdateSideshowListEntries 0x10196208, LCFormat 0x10196388 (here AllItemsListLCFormat: the name LCFormat exists at other addresses, the
//   all-kids panel's is in src/ui_allKidsPanel_AllKidsPanelMsg.cpp).
#include <cstdint>

#include "History.h"
#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ppc_math.h"
#include "ui_controls.h"
#include "ui_main.h"

// Globals no other file names (names invented).
int32_t gAllItemsRowHeight;               // 0x103947d4 the row window's height, worked out once (0 until then)
uint32_t gAllItemsFontIndex = 7;          // 0x102a8cf0 the font table index the row height is measured with (7 at start)

namespace {

LCWord Number(int32_t value) {
    LCWord word;
    word.mRaw = 0;
    word.mInt = value;
    return word;
}

// The runtime's double to unsigned conversion (glue 0x10237cd8, __cvt_fp2unsigned): the word the listing keeps.
int32_t SumWord(double sum) {
    if (sum >= 2147483648.0) return static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(sum - 2147483648.0)) + 0x80000000u);
    return ppc_fctiwz(sum);
}

int32_t Div100(int32_t value) { return value / 100; }

int32_t PercentOfByte(uint32_t value) { return Div100(static_cast<int32_t>((value & 0xffu) << 10)); }

}  // namespace

// The virtual call through the font entry's object (slot 0xc of the object at +0x10): its class is not known, the font's height stands in
// for the answer. Weak so that the test library's recording fake replaces it.
namespace allitems_callee {
__attribute__((weak)) int16_t FontVirtual(InterfaceFont* font) { return static_cast<int16_t>(font->GetHeight()); }
}  // namespace allitems_callee

// 0x10196208 (312 bytes): rebuilds the cells of every row of the sideshows list, last row first. The row's data word is the object.
void UpdateSideshowListEntries(InterfaceListControl2* list) {
    if (list == nullptr) return;
    for (int32_t position = list->mItemCount - 1; position >= 0; --position) {
        int32_t data;
        list->GetData(position, &data);
        Ptr32<CObject> pointer;
        pointer.mOffset = static_cast<uint32_t>(data);
        CObject* object = pointer.get();
        UI_LCAdd cells[6];
        cells[0] = Number(0);
        double sum;
        object->mHistory1A4.GetSumOfLast(sum, 30);
        cells[1] = Number(SumWord(sum));
        cells[2] = Number(PercentOfByte(object->GetExcitementLevel(false)));
        cells[3] = Number(object->GetProfitability());
        cells[4] = Number(object->GetTotalRevenue());
        int32_t satisfaction = object->GetCustomerSatisfaction();
        cells[5] = Number(Div100(static_cast<int32_t>(static_cast<uint32_t>(satisfaction) << 10)));
        list->ChangeItem(position, cells, 0x3e, position == 0 ? 1 : 0, -1);
    }
}

// 0x10196388 (836 bytes): makes the all-items list's row window; its height (once) is the font's height (a virtual call on the font
// entry's object) scaled by 0x600 over the real screen height, plus 6. The children depend on the kind of list shown.
InterfaceWindow* AllItemsListLCFormat() {
    if (gAllItemsRowHeight == 0) {
        InterfaceFont* font = GetFontTableEntry(gAllItemsFontIndex);
        int16_t height = allitems_callee::FontVirtual(font);
        gAllItemsRowHeight = (static_cast<int32_t>(height) * 0x600) / static_cast<int32_t>(gUIRealScreenHeight) + 6;
    }
    void* memory = lender_callee::OperatorNew(0x138);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    if (window != nullptr)
        lender_callee::WindowCtor(window, nullptr, 0, 0, 0, 0, 0, static_cast<int16_t>(gAllItemsRowHeight));
    window->SetCallbackFunction(AllItemsLCEntryHandler);
    __asm__ volatile("" : "+r"(window));   // the listing tests the pointer again: keep the compiler from assuming it is not NULL
    if (window != nullptr) {
        switch (gAllItemsListType) {
            case 2:
                window->CreateObject(1, 1, 2, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 3, 0, 0, 0, 0);
                window->CreateObject(1, 1, 4, 0, 0, 0, 0);
                window->CreateObject(1, 1, 5, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 6, 0, 0, 0, 0);
                break;
            case 1:
                window->CreateObject(1, 1, 3, 0, 0, 0, 0);
                window->CreateObject(1, 1, 4, 0, 0, 0, 0);
                window->CreateObject(1, 1, 5, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 6, 0, 0, 0, 0);
                break;
            case 0:
                window->CreateObject(1, 1, 4, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 5, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 6, 0, 0, 0, 0);
                window->CreateObject(9, 0x11, 3, 0, 0, 0, 0);
                break;
            case 3:
                window->CreateObject(1, 1, 4, 0, 0, 0, 0);
                break;
            default:
                break;
        }
    }
    return window;
}
