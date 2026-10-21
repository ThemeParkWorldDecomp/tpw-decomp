// Unit v5183 (src/game/ui/ui_loadSavePanel.cpp, ui_mainpanel2.cpp in the main app): the message functions of the load and save panels and the hire
// panel's state change.
#pragma once
#include <cstdint>

class InterfaceWindow;

// 0x101a2bdc (352 bytes): the load panel's message function. 5 closes the panel (answer 1); 0x14 clears gLoadPanelOpen, decides the keyboard
// capture again and unpauses the game; 0x100 with a == -2 closes it (answer 0); 0x400 (a list entry picked: `a` the list's id, `b` the
// entry) loads the park of that entry and closes the panel (answer 0). Anything else is the default dialog callback's.
int32_t LoadPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// 0x101a2a14 (408 bytes): the hire panel (gHirePanelWindow, list 0x248e) after staff slot `slot` changed: the entry of that slot is
// replaced, removed, or added (when the slot is of the panel's type, then the display is updated).
void UI_HirePanelStateChange(int slot);
// 0x101a2df4 (808 bytes): the save panel's message function. 5 closes the panel (1); 0x14 as the load panel's plus all key tables on;
// 0x400 puts the picked entry's name in the name box (window 0x23bce2); 0x100 with a == -1 saves under gSavePanelName (a dialog asks
// first when a file of that name exists), a == -2 closes (0); 0x802 and 0x804 post message 0x100 (-1 and -2) to the panel.
int32_t SavePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

// Original names unknown. 0x101a3934 and 0x101a3be4 (56 bytes each, stand-ins).
void RemoveLoadPanel();
void RemoveSavePanel();
// 0x101a2d74 (the save dialog's OK button handler; the Mac passes its TVector 0x10267be8).
int32_t SaveCallback_OK(void* unused);

extern int32_t gHirePanelType;          // 0x10394920
extern InterfaceWindow* gHirePanelWindow;   // 0x10394924
extern char16_t gSavePanelName[34];     // 0x1039492c: the name typed in the save panel (0x44 bytes up to 0x10394970)
extern int32_t gSavePanelOpen;          // 0x10394970
extern int32_t gLoadPanelOpen;          // 0x10394974

// Unit v5554 (src/game/ui/ui_hirePanel.cpp): the hire panel's list row attach function and staff type button handler.
enum StaffType : int32_t;
void FillHireList(StaffType type, int32_t flag);      // 0x101a0fc8 (stand-in)
void HirePanelLCPostFormat(InterfaceWindow* window);  // 0x101a18a4 (LCPostFormat of ui_mainpanel2.cpp's hire list)
// 0x101a19f8 (592 bytes): message 0x101 with b == 1 and a in 0x2490..0x2494 picks a staff type (0, 4, 2, 1, 3), refills the list, sets the
// panel's title (string 0x8b..0x8f), kills the list script and resets the two selection globals; then the default handler.
int32_t HirePanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
extern uint32_t gHireLCFontIndex;       // 0x102aa31c (name invented; 7 at start)
extern int32_t gHireListScript;         // 0x1039490c (name invented)
extern int32_t gHireListSelA;           // 0x102aa328 (name invented; -1 at start)
extern int32_t gHireListSelB;           // 0x102aa32c (name invented; -1 at start)

// Unit v5702 (src/game/ui/ui_loadSavePanel.cpp): the save panel's row maker (LCFormat of ui_mainpanel2.cpp at 0x101a3154; named so because
// LCFormat exists at other addresses). The window is 100 wide and gSavePanelListFormat high, with one child (kind 1, id 1).
InterfaceWindow* SavePanelLCFormat();
extern int32_t gSavePanelListFormat;    // 0x10394928 (name invented; 0 until the first row maker call)

// Unit v51019 (src/game/ui/ui_hirePanel.cpp): CreateHireListPanel (ui_main.h). Names invented; the addresses are the Mac's.
extern int16_t gHirePanelScript[];     // 0x102a9ee8: the hire panel's window script (the image's words, generated at configure time)
extern int32_t gHireColumnColour;       // 0x102aa320: the column buttons' colour (8 at start)
extern int32_t gHireSortColumn;         // 0x102aa324: the column the list is sorted on (1 at start)
extern int32_t gHireListWord10;         // 0x10394910: the history at 0x1f7f0's newest value
extern int32_t gHireListWord14;         // 0x10394914: word 18 less word 10
extern int32_t gHireListWithdrawals;    // 0x10394918: the withdrawals history's newest value (0x1f5a0)
extern int32_t gHireListDeposits;       // 0x1039491c: the deposits history's newest value (0x1fc90)
extern char gHireListStrings[24];       // 0x1024e0c3: "hilight", then at +8 "ridestatbar.wct"
InterfaceWindow* HireListLCFormat();    // 0x101a1798 (stand-in: the hire list's row maker, TVector 0x10267bb8)
int32_t HirePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101a1314 (stand-in; TVector 0x10267bc8)
// Callees the host cannot call as the Mac does (allocation, constructors of classes whose vtables live elsewhere, the virtual calls at
// vtable +0x24 / +0x2c of the table at window +0x118): weak definitions in the unit's file, faked by the test library.
namespace hirelist_callee {
int32_t Labs(int32_t value);                                   // labs, glue 0x10239928
void* OperatorNew(uint32_t size);                              // 0x102384b8
void TitleCtor(void* mem);                                     // 0x1018fbe8 InterfaceTitleTextObject(NULL)
void SpriteCtor(void* mem);                                    // 0x1018d11c InterfaceTextObjectSprite(NULL)
void CashCtor(void* mem);                                      // 0x1018de60 InterfaceCashTextObject(NULL)
void GradientCtor(void* mem, const char* mesh);                // 0x101898c8 InterfaceDrawEngineHGradient(mesh)
void WindowSetValue(InterfaceWindow* window, int32_t value);   // virtual slot 0x24
void WindowSetText(InterfaceWindow* window, char16_t* text);   // virtual slot 0x2c
}  // namespace hirelist_callee

// Unit v5984 (src/game/ui/ui_hirePanel.cpp): the hire panel's staff preview, row maker and message function.
class InterfaceFont;
void UpdateViewWindow(int32_t slot);   // 0x101a10f4 (504 bytes): puts the animated preview of staff slot `slot` in the hire panel (window 0x2484)
extern int32_t gHireListFormat;        // 0x10394908 (name invented): the hire list row height HireListLCFormat works out once (0 until then)
void RemoveHireListPanel();            // 0x101a29b0 (56 bytes, stand-in; a weak empty definition in the unit's file)
