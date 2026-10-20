// Unit v5145 (src/FillLenderList.cpp): the lenders list of the bank panel (ui_mainpanel2.cpp and its neighbours): the list fill, and the row
// format and row attach functions of the lenders list control. The Mac offsets are in the comments.
#pragma once
#include <cstdint>

#include "InterfaceWindow.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

// 0x1018e874 is the constructor of this one (stand-in: no header named it before this unit).
class InterfacePercentTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfacePercentTextObject(void* data);   // 0x1018e874
    ~InterfacePercentTextObject() override;            // 0x1018e950 (unit v51115)
};

// Globals of ui_mainpanel2.cpp's neighbourhood (names invented).
extern int32_t gLenderListMode;       // 0x1039497c  0: loans on offer, 1: loans taken (what FillLenderList lists)
extern int32_t gLenderRowHeight;      // 0x10394978  the lender row height LCFormat works out once (0 until then)
extern uint32_t gLenderFontIndex;     // 0x102aa838  the font table index of the list's rows (7 at start)

// 0x101a4ce4 (untranslated; its TVector is 0x10267c10): the message function of the row windows.
int32_t LendersItemsCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

void FillLenderList(InterfaceListControl2* list);          // 0x101a4a38
void LenderLCPostFormat(InterfaceWindow* window);          // 0x101a4660 (LCPostFormat of ui_mainpanel2.cpp)
InterfaceWindow* LenderLCFormat();                         // 0x101a44b8 (LCFormat of ui_mainpanel2.cpp)

// Callees outside the unit that the host cannot call as the Mac does (a constructor of a class whose vtable lives elsewhere, the
// allocation, a virtual call): defined in src/FillLenderList_callees.cpp for the program, faked by the test library.
namespace lender_callee {
void* OperatorNew(uint32_t size);                                       // 0x102384b8
void SpriteCtor(void* mem);                                             // 0x1018d11c InterfaceTextObjectSprite(NULL)
void CashCtor(void* mem);                                               // 0x1018de60 InterfaceCashTextObject(NULL)
void NumberCtor(void* mem);                                             // 0x1018d930 InterfaceNumberTextObject(NULL)
void PercentCtor(void* mem);                                            // 0x1018e874 InterfacePercentTextObject(NULL)
void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                int16_t bottom);                                        // 0x101d98d8
void WindowSetValue(InterfaceWindow* window, int32_t value);            // the virtual SetValue (slot 0x24 of the table at +0x118)
}  // namespace lender_callee

// Unit v5698 (src/FillLenderList_CreateLoansPanel.cpp): CreateLoansPanel 0x101a4de4 (ui_main.h).
extern int16_t gLoansScript[];          // 0x102aa4a4: the loans panel's window script (the image's words, generated at configure time)
extern int32_t gLenderColumnColour;      // 0x102aa83c: the lender list's column button colour (8 at start)
// 0x101a4bec (untranslated; its TVector is 0x10267bf8): the message function of the lender list.
int32_t LenderListCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// The game type object (0x102b3aa0) and its guard byte (0x102b3aa4) as CreateLoansPanel inlines them: constructed on first use.
// Raw storage so that no constructor runs at start-up.
extern unsigned char gLoansGameTypeStorage[4];   // 0x102b3aa0 (a CGameType)
extern int8_t gLoansGameTypeGuard;               // 0x102b3aa4
