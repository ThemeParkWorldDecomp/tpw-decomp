// Unit v5145: the lenders list of the bank panel. FillLenderList 0x101a4a38, LCPostFormat 0x101a4660 (here LenderLCPostFormat) and
// LCFormat 0x101a44b8 (here LenderLCFormat): the names exist at other addresses.
#include "FillLenderList.h"

#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_main.h"

// The program's definitions of lender_callee (weak: the test library of this unit defines its own, recording ones).
#include <algorithm>
#include <new>

static_assert(sizeof(InterfaceTextObjectSprite) <= 0x400 && sizeof(InterfaceWindow) <= 0x400, "the allocation below is 0x400 at least");

namespace lender_callee {
// The Mac allocates the sizes of its objects; the host's are larger, so the block is at least 0x400 bytes.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
__attribute__((weak)) void SpriteCtor(void* mem) { new (mem) InterfaceTextObjectSprite(nullptr); }
__attribute__((weak)) void CashCtor(void* mem) { new (mem) InterfaceCashTextObject(nullptr); }
__attribute__((weak)) void NumberCtor(void* mem) { new (mem) InterfaceNumberTextObject(nullptr); }
__attribute__((weak)) void PercentCtor(void* mem) { new (mem) InterfacePercentTextObject(nullptr); }
__attribute__((weak)) void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top,
                                      int16_t right, int16_t bottom) {
    new (mem) InterfaceWindow(parent, attributes, id, left, top, right, bottom);
}
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace lender_callee

int32_t gLenderListMode;                  // 0x1039497c
int32_t gLenderRowHeight;                  // 0x10394978
uint32_t gLenderFontIndex = 7;             // 0x102aa838

// 0x101a4a38: empties the list, then adds the lenders of the bank account's eight loans. Mode 0 lists the loans on offer (available and
// not bought), mode 1 the loans taken. An item is: the lender's name, the amount, the period, the APR, the monthly repayment and the
// total (repayment times period). The loans taken show the months still to pay, and carry the negated number as their data.
void FillLenderList(InterfaceListControl2* list) {
    list->RemoveAllItems();
    for (uint32_t i = 0; i < 8; ++i) {
        CLoan loan = gGameWorld->GetBankAccount()->GetLoanInfo(i);
        UI_LCAdd item[6] = {};
        if (gLenderListMode == 0 && loan.mLoanBought == 0 && loan.mLoanAvailable != 0) {
            const unsigned char* name = reinterpret_cast<TbITable*>(gText.mResourcePointers[18])->GetConstString(loan.mLenderNameIndex);
            item[0].mText = UIMain_ConvertMulti(name);
            item[1].mInt = static_cast<int32_t>(loan.mAmountAvailable);
            item[2].mInt = static_cast<int32_t>(loan.mRepaymentPeriodInMonths);
            item[3].mInt = static_cast<int32_t>(loan.mAPRInPercent);
            item[4].mInt = static_cast<int32_t>(loan.mMonthlyRepayment);
            item[5].mInt = static_cast<int32_t>(loan.mMonthlyRepayment * loan.mRepaymentPeriodInMonths);
            list->AddItem(item, static_cast<int32_t>(i + 1), -1, 1);
        } else if (gLenderListMode == 1 && loan.mLoanBought != 0) {
            const unsigned char* name = reinterpret_cast<TbITable*>(gText.mResourcePointers[18])->GetConstString(loan.mLenderNameIndex);
            item[0].mText = UIMain_ConvertMulti(name);
            item[1].mInt = static_cast<int32_t>(loan.mAmountAvailable);
            const int32_t left = static_cast<int32_t>(loan.mRepaymentPeriodInMonths - loan.mMonthsRepaid);
            item[2].mInt = left;
            item[3].mInt = static_cast<int32_t>(loan.mAPRInPercent);
            item[4].mInt = static_cast<int32_t>(loan.mMonthlyRepayment);
            item[5].mInt = static_cast<int32_t>(loan.mMonthlyRepayment) * left;
            list->AddItem(item, -static_cast<int32_t>(i + 1), -1, 1);
        }
    }
}

// 0x101a44b8: the row window of the list (five controls of kinds 2 to 6). The row height is the font's height scaled to the UI's 1536
// lines, plus 6, worked out on the first call. (The Mac calls the font's virtual at slot 0xc of the table at +0x10: GetHeight.)
InterfaceWindow* LenderLCFormat() {
    if (gLenderRowHeight == 0) {
        InterfaceFont* font = GetFontTableEntry(gLenderFontIndex);
        const int16_t height = static_cast<int16_t>(font->GetHeight());
        gLenderRowHeight = static_cast<int32_t>(height) * 0x600 / gUIRealScreenHeight + 6;
    }
    void* memory = lender_callee::OperatorNew(0x138);
    if (memory != nullptr) lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(gLenderRowHeight));
    InterfaceWindow* row = static_cast<InterfaceWindow*>(memory);
    row->CreateObject(1, 1, 2, 0, 0, 0, 0);
    row->CreateObject(1, 1, 3, 0, 0, 0, 0);
    row->CreateObject(1, 1, 4, 0, 0, 0, 0);
    row->CreateObject(1, 1, 5, 0, 0, 0, 0);
    row->CreateObject(1, 1, 6, 0, 0, 0, 0);
    return row;
}

// 0x101a4660: gives the row and its five children their draw objects, font, handler and justification (each child's SetValue(0) is the
// window's virtual at slot 0x24).
void LenderLCPostFormat(InterfaceWindow* window) {
    using namespace lender_callee;
    void* object = OperatorNew(0x19c);
    if (object != nullptr) SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    window->SetFont(GetFontTableEntry(gLenderFontIndex));
    window->SetCallbackFunction(LendersItemsCallback);
    InterfaceWindow* child = window->GetChild(0);
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFontJust(2, 1);
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
    child = child->GetNext();
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) NumberCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
    child->SetFontJust(1, 1);
    child = child->GetNext();
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) PercentCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
    child->SetFontJust(1, 1);
    child = child->GetNext();
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFontJust(2, 1);
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
    child = child->GetNext();
    child->SetCallbackFunction(LendersItemsCallback);

    object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFontJust(2, 1);
    child->SetFont(GetFontTableEntry(gLenderFontIndex));
    WindowSetValue(child, 0);
}
