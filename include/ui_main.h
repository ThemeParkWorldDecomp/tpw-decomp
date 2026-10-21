// ui_main.cpp (main app). PARTIAL: the text conversion (src/ui_main.cpp) and the start of
// the interface with its meshes (src/ui_main_meshes.cpp). UI text is 16-bit (the Mac wchar_t).
#pragma once
#include <cstdint>

#include "Ptr32.h"
#include "TbDynamicStringTemplate.h"
#include "InterfaceWindow.h"
#include "bfl_FileSystem.h"
#include "ui_DrawObject.h"
#include "TbVTag.h"

// 0x10180468 (src/ui_loginpanel.cpp): the interface's string number `id` (the table at gText's resource 6) as Unicode, converted into
// gUIMainConvertBuffer when the language's converters are loaded (the first 0x200 characters; the buffer is left as it was when they are
// not); NULL when the table has no such string.
const char16_t* UIMain_GetString(int32_t id);
class InterfaceWindow;
// 0x1018bd3c (src/ui_loginpanel.cpp): the colour of a title's text: SetFontColour(window, 0xea, 0xef, 0x66, 0xff).
void UI_TITLE_COLOUR(InterfaceWindow* window);
char16_t* UIMain_ConvertMulti(const unsigned char* text);   // 0x101803cc
char16_t* UIMain_ConvertMulti(unsigned char* text);         // 0x10180334
// 0x10380c48 (name invented): 0x4000 characters (the next global is at 0x10388c48, and the island lobby's controller formats its
// texts into it with a limit of 0x3fff); UIMain_ConvertMulti uses the first 0x200.
extern char16_t gUIMainConvertBuffer[0x4000];
void UIMain_GetFullyOutOfFirstPerson();                     // 0x10187434
// ---- the panel helpers (src/ui_main_panel.cpp, unit w101)
enum AdvisorMessage : int32_t;
// 0x101802cc: the sound event `code` of the UI's sound category (gSoundBanks[3]).
void UIMain_UISound(int32_t code);
// 0x10180514: when there is a panel, tells it message 5, forgets it and has the keyboard capture decided again; answers whether there was one.
bool UIMain_CheckPanel();
// 0x101805a0: makes the mouse and its default mode when there is none (and when the current mode answers 9 gives it a fresh default mode),
// and for a panel (not NULL) puts the cursor back, disables the camera, cheat and game key tables and removes the slide-out panel. Then
// (NULL too) the panel is remembered and the keyboard capture decided again.
void UIMain_SetPanel(InterfaceWindow* window);
// 0x10180794: the title text of the child `id` of `window`: made with the title draw object (centred title font, title colour, its
// rectangle widened by half its width each side) when it has none; then the text is set.
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text);
// 0x101821cc and 0x1018227c: queue / cancel an advisor message (CAdvisor of gGameWorld).
void UIMain_AdvisorMessage(AdvisorMessage message);
void UIMain_CancelAdvisorMessage(AdvisorMessage message);
// 0x1018493c: the panels' default message function (keys, removal), the rest to the window's default handler.
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// Untranslated, called by the above: 0x101a83d8 (256 bytes) closes the slide-out panel; 0x101a8c58 (80 bytes) sets the game window's cursor.
void RemoveSlideoutPanel(int16_t which);
void UI_SetGameWindowCursor(int16_t cursor);
// Unit v5313 (src/ui_mainpanel2_window_bumhole_UI_SwitchGameWindow.cpp).
// 0x103949b0: the cursor UI_SetGameWindowCursor last gave the game window (name invented).
extern int32_t gGameWindowCursor;
// 0x101a8cd8 (240 bytes): unless the world's state (mF1DA738) is 4 or the winning animation plays, sends message 6 to the game window
// (a = first) and to the first-person window (a = 1 - first); for the online game type calls ChatGoFirstPerson (first == 0) or
// ChatBackToThirdPerson, then has the keyboard capture decided again.
void UI_SwitchGameWindow(int32_t first);
void ChatGoFirstPerson();       // 0x101e5968 (stand-in)
void ChatBackToThirdPerson();   // 0x101e5ba0 (stand-in)
class InterfaceFont;
// 0x10180270 (src/ui_main_leaves.cpp, unit w40): the interface's font number `index` (0 to 12) of the current resolution group
// (gText.mResolutionGroup, 0 to 3): the address of that entry of the game text object's font table (gText.mFonts, 13 to a group,
// an InterfaceFontWrapper), or NULL for 13 and up. The group is not checked.
InterfaceFont* GetFontTableEntry(uint32_t index);
// 0x1018b8e4 (src/ui_measuretext.cpp, unit w40; the inventory files it under ui_utils.cpp): how large UI_DrawText's text would be. The
// same set-up as UI_DrawText (the formatter, the Hebrew swap of left and right, the justification values, the unwrapped box, the font
// of gText's group), but nothing is coloured or drawn: the text goes through UIMain_ConvertString into the formatter's Measure, and
// `extent` (if not NULL) gets the rectangle it reports. NULL text: nothing happens, and `extent` is not written.
struct TbRect;
void UI_MeasureText(const char16_t* text, const TbRect& rect, int8_t font, int8_t horizontal, int8_t vertical, int32_t wrap,
                    TbRect* extent);

// ---- the start of the interface (src/ui_main_meshes.cpp)
class InterfaceTitleTextObject;
InterfaceDrawEngineResizableFrame* TPResource_AddFrameMesh(bfl::FileSystem::CFileInfo info, bfl::FileSystem::CFileStorage& storage,
                                                           const TbDynamicStringTemplate<char>& folder);   // 0x10184abc
InterfaceDrawEngineResizableSlider* TPResource_AddSliderMesh(bfl::FileSystem::CFileInfo info, bfl::FileSystem::CFileStorage& storage,
                                                             const TbDynamicStringTemplate<char>& folder);  // 0x10184d34
InterfaceDrawEngineMesh* TPResource_AddMeshDrawObject(bfl::FileSystem::CFileInfo info, bfl::FileSystem::CFileStorage& storage,
                                                      TbDynamicStringTemplate<char> folder);                // 0x10184f74
int32_t LoadAllUIMeshes();          // 0x1018515c
int32_t LoadLocalisedUIMeshes();    // 0x10185414
void InitGeneralUI();               // 0x10185b68
// 0x10185d64 (204 bytes, src/ui_main_meshes.cpp): the end of InitGeneralUI's work: the cursor table and the UI system are closed,
// the three windows below deleted (each only when it is not NULL, through the window's virtual destructor with the flag 1) in
// the order A, B, C, then the string heap, the debug font's bitmap and the UI's heap are given back.
void DeInitGeneralUI();
// The cursors (src/ui_cursors.cpp). InitialiseCOLOURCursors loads 22 cursor pictures from "data:UI:cursors" into
// gCursorTable by number (a picture that is not found leaves its number empty).
void InitialiseCOLOURCursors();     // 0x1018575c
InterfaceBFCursor* LoadBFCurs(const char* name);   // 0x10185670: the cursor of the file "data:UI:cursors:<name>", or NULL
void InitLobbyUI();                 // 0x10186324: the lobby's light, and the help text on or off by the player's setting
void DeInitLobbyUI();               // 0x10186434 (src/lobby_exit.cpp): the help text off, the light out of the map's list 0x20
// 0x101874d0 (src/ui_main_leaves.cpp, unit w40): makes the small text window that shows the game's version ("v 2.04b3": the version
// resource's short text) in the bottom right corner of the 2048 x 1536 interface, under the root window when `show` is set and
// under the lobby's window otherwise (gLobby->mWindow; neither is checked for NULL). The text is measured (font 6, left, top, not
// wrapped) and the window is as large as that in the screen's real pixels scaled to the interface's units, ending at (0x7c0, 0x5c0).
void UI_CreateVersionNumber(bool show);
void InitHelpText();                // 0x1018c60c (src/ui_tooltip.cpp, unit w56): builds the tooltip window and makes it the help text handler
// The UI's message filter (src/ui_main_peek.cpp): sound, IME and button highlight before dispatch.
int32_t EffectPeekMsg(InterfaceWindow* window, int32_t a, int32_t b, int32_t c);   // 0x1017fdf8
// 0x1018679c (src/ui_main_pause.cpp, unit w88): with a root window, stop the timers then ShowPauseWindow. UIInterface::Pause calls it.
void PauseUI();
// 0x10186854 (src/ui_main_pause.cpp, unit w88): TogglePauseWindow, then the timers stopped when a pause window is up, started again when not.
void TogglePauseUI();
void UnPauseUI(); // 0x101867f8: with a root window, resume timers then HidePauseWindow
// 0x101a8744 (src/ui_mainpanel2_pause.cpp): closes the pause window (message 4) if there is one, forgets it and has the keyboard capture
// decided again.
void HidePauseWindow();
extern InterfaceWindow* gPauseWindow;   // 0x103949cc (original name unknown): the pause window, set by ShowPauseWindow and TogglePauseWindow
// src/ui_mainpanel2_window.cpp (unit w88): the pause window. 0x101a86d0: makes it (Dialog, modal) when there is none.
void ShowPauseWindow();
// 0x101a87c4: makes the pause window when there is none, else closes it (message 4), forgets it and has the keyboard capture decided again.
void TogglePauseWindow();
// 0x101a8504: the pause window's message function (callback descriptor 0x10267c38): at 0x14 the window is forgotten, at 0x15 (shown) its
// pivot is the middle of its rectangle and the animation gPauseAnim starts, at the key event 0x1000b (with the system key table off) the
// key is looked up there and the game's pause toggled. Everything ends in DefaultDialogCallback, whose answer is returned.
int32_t PauseWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// src/ui_pause_data.cpp (unit w88, names invented): the pause window's script (21 words at main 0x102aacfc) and its animation (five
// frames, 0x102aadbc: four rectangle deltas, then a jump back to frame 0).
extern int16_t gPauseDialogDescription[21];
extern UI_RectDelta gPauseAnim[5];

// The file's globals (original names unknown).
extern InterfaceTitleTextObject* gTitleText;                // 0x1038cc50
extern CursorTable gCursorTable;                            // 0x1038cc64
extern TbDynamicStringTemplate<char> gUIFrameName;          // 0x10380bf4 "!frame"
extern TbDynamicStringTemplate<char> gUISliderName;         // 0x10380bd4 "!slider"
extern TbDynamicStringTemplate<char> gUIPlainFrameName;     // 0x10380bb4 "!f_plain"
extern TbDynamicStringTemplate<char> gUIButtonName;         // (through 0x102638ac) "!cbut"; nothing reads it
extern LbTimeClockTickFunction gUIClockTick;                // 0x10380b7c
// Three windows DeInitGeneralUI deletes if they are set. No translated code sets them (nothing else reads them either, in the
// decompiled text), so their names are made up: the words at 0x10380b88, 0x10380b84 and 0x10380b80, deleted in that order.
extern InterfaceWindow* gUIGeneralWindowA;                  // 0x10380b88
extern InterfaceWindow* gUIGeneralWindowB;                  // 0x10380b84
extern InterfaceWindow* gUIGeneralWindowC;                  // 0x10380b80

// LoadBFCurs's folder: a function-level static of the original, set from the string "data:UI:cursors" on the first
// call (the flag is a byte at 0x10380b8c, the pointer a word at 0x10380b90).
extern char gBFCursorFolderSet;
extern const char* gBFCursorFolder;
// The two result codes InterfaceBFCursor::LoadData compares the picture loader's answers with (-1 each, initialised
// data at 0x102b155c and 0x102b1560).
extern int32_t gTbReadHeaderFailed;
extern int32_t gTbLoadFailed;
// The interface's light (an object at 0x1038cc78 whose CLight part the Mac's word there points at), defined in
// src/ui_cursors.cpp.
class CLight;
extern CLight gUILight;
void DeInitOnlineUI();   // 0x10186620 (124 bytes): src/UI_pump_DeInitOnlineUI.cpp

// ---- the keyboard decision (src/ui_main_kbcapture.cpp, src/ui_coasteredit_kb.cpp; unit w49)
class UIMenu;
// 0x1018116c, 612 bytes (Mac file ui_main.cpp): decides which window has the keyboard, from the main menu, the game's windows, the
// bankrupt window, the panel, the coaster editor and the lobby's window, enabling the camera, game and cheat key tables on the way.
// Every CaptureKb passes 0. The window globals below are NULL or a window; the tree (in the order of the tests):
//   main menu up: the menu's window.
//   no game window: the first-person window, the online panel, the online window (each only when its attribute bit 0 is set; the
//     first also enables the camera table), else the lobby's window when there is a lobby and it has one.
//   a game window: the bankrupt window if there is one; else the first-person window when its bit 0 is set (the camera table is
//     enabled; NO test for NULL: the original reads address 0 when there is none), else the game window when its bit 0 is set (the panel
//     if there is one; else with the coaster editor down the camera, game and cheat tables and the game window, with it up the camera
//     table and the coaster panel), else the bumhole window when it exists and its bit 0 is set (the camera table is enabled).
void UIMain_DecideKbCapture();
// 0x101fc1ec, 80 bytes (Mac file ui_mainmenu.cpp): 1 when there is a main menu and it is shown, else 0.
int32_t MainMenuUp();
// 0x1019cfdc, 12 bytes (Mac file ui_entryPricePanel.cpp): the coaster editor's flag.
int32_t CoasterEditPanelUp();
// 0x1019d014, 48 bytes (Mac file ui_entryPricePanel.cpp): the coaster panel takes the keyboard (CaptureKb(0), no test for NULL).
void CoasterEditPanelGetKb();

// The windows the interface keeps (original names unknown; named here from the function that makes each one).
extern InterfaceWindow* gGameWindow;         // 0x103949e4: InitGameUI 0x10185e58 makes it (full screen, message function Engine_GameWinMsg 0x10183c84, which clears it)
extern InterfaceWindow* gFirstPersonWindow;  // 0x103949e0: InitGameUI makes it (message function Engine_GameFirstPersonMsg 0x10184628, which clears it)
extern InterfaceWindow* gBumholeWindow;      // 0x103949dc: ToggleGameWindow 0x101a8ac0 makes it (full screen, id 0xddf22) while the game window is hidden; its message function is "bumhole" 0x101a886c
extern InterfaceWindow* gOnlinePanel;        // 0x10395e98: CreateOnlinePanel 0x101e8f08 builds it (message function OnlinePanelMsg 0x101e62d0, which clears it)
extern InterfaceWindow* gOnlineWindow;       // 0x10395e94: CreateOnlinePanel makes it (full screen, id 0xfff04d2a, message function OnlinePanelMsg)
extern InterfaceWindow* gBankruptWindow;     // 0x1038cc58: UI_Bankrupt 0x10186f34 makes it (UI_BankruptAfterLoad 0x1018710c and BankruptWindowMsg 0x101868c0 clear it)
extern InterfaceWindow* gUIPanelWindow;      // 0x1038cc5c: UIMain_SetPanel 0x101805a0 sets it (UIMain_CheckPanel 0x10180514 and UIMain_DefaultPanelMsg 0x1018493c clear it)
extern UIMenu* gMainMenu;                    // 0x10396a58: InitialiseMainMenu 0x101fb760 and InitialiseLobbyMenu 0x101fbcd0 make it, HideMainMenu 0x101fc140 deletes it
extern int32_t gCoasterEditMode;             // 0x10394830: CoasterEditMode 0x1019c584 sets it (CoasterEditPanelUp answers it)
extern InterfaceWindow* gCoasterPanel;       // 0x10394834: InitCoasterPanel 0x1019c2dc builds it (CoasterPanelMsg 0x1019b2b8 clears it)

// ---- Unit v5143 (src/ui_mainpanel2_window_bumhole.cpp)
// 0x101a886c: the bumhole window's message function (callback descriptor 0x10267c30): the capture of the keys at 0x10004 / 0x10005, the camera
// messages, the camera key table (0x10004 up, 0x10005 down, 0x1000a down, 0x1000b up); at 0x1000b also the escape key / the game table's
// action 4 toggle the game window and the shortcuts table's action 0xf makes the postcard (the main menu instead of both while the world's
// word +0x1da738 is 4). 0x1000b answers 0; every other message goes to the default handler (so does 0x1000a and 0x10004 / 0x10005).
int32_t bumhole(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// 0x101a8e60: the text window (id 0x1d4c9) of the child `window` of the root gSetOnOffRoot gets the string `text` followed by the string
// on/off/yes/no (0x14c to 0x14f from the two flags: `second` picks the pair, `first` the one of it); when `button` is set the child
// `buttonId` is set to state 0 (first set) or 1 (not), redrawn. The first string is cut to 31 characters without a terminator (kept).
void SetOnOffText(int32_t window, int32_t text, bool first, bool second, bool button, int32_t buttonId);
extern int32_t gGameWindowToggled;           // 0x10394984: 1 while the game window is hidden (ToggleGameWindow flips it)
extern InterfaceWindow* gSetOnOffRoot;       // 0x10394a14 (name invented): the window whose children SetOnOffText looks in

// ---- the park's interface set-up (unit w116)
void InitGameUI();                  // 0x10185e58
void DeInitGameUI();                // 0x1018622c
void UI_ClearSelected();            // 0x10194838
void InitTagPanel();                // 0x10190350
void DeInitTagPanel();              // 0x10190484
void CreateStaticTags();            // 0x10191b28
void DestroyStaticTags();           // 0x10191c40
void InstantRemoveAllTags();        // 0x10192280
void InitCashPanel();               // 0x1019af14
void DeInitCashPanel();             // 0x1019afac
void InitCoasterPanel();            // 0x1019c2dc
void DeInitCoasterPanel();          // 0x1019c378
void InitControlPanel();            // 0x1019d180
void DeInitControlPanel();          // 0x1019d21c
void DeInitStaffPanel();            // 0x101c05d4
void CreateMainPanel();             // 0x101a7878 (src/ui_mainpanel2_create.cpp, unit w118)
void InitStaffPanel();              // 0x101c02fc (src/ui_staffPanel_init.cpp, unit w118)
int32_t Engine_GameWinMsg(InterfaceWindow*, int32_t, int32_t, int32_t);         // 0x10183c84 (src/ui_main_gamewin.cpp, unit w136)
int32_t Engine_GameFirstPersonMsg(InterfaceWindow*, int32_t, int32_t, int32_t); // 0x10184628 (src/ui_main_gamewin.cpp, unit w136)
int32_t TagCallback(InterfaceWindow*, int32_t, int32_t, int32_t);               // 0x10190f04, untranslated
int32_t TagDlgCallback(InterfaceWindow*, int32_t, int32_t, int32_t);            // 0x10191628, untranslated
int32_t CashPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t);              // 0x1019ade8 (unit w131)
int32_t CoasterPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t);           // 0x1019b2b8, untranslated
int32_t ControlPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t);           // 0x1019d070 (unit w131)
extern InterfaceWindow* gCashPanel;     // 0x10394824
extern InterfaceWindow* gControlPanel;  // 0x10394838
extern InterfaceWindow* gTagPanel;      // 0x1038cd78
extern InterfaceWindow* gTagPanelSource; // 0x1038cd80, original role uncertain
extern InterfaceWindow* gStaffPanelA;   // 0x10395a48
extern InterfaceWindow* gStaffPanelB;   // 0x10395a44
extern InterfaceWindow* gStaffPanelC;   // 0x10395a40
// Mac: window +0, signed flag +4, opaque payload +6, stride 0x208. Native pointers on the host.
struct sStaticTag {
    InterfaceWindow* mWindow;
    int16_t mActive;
    uint8_t mPayload[0x202];
};
extern sStaticTag gStaticTags[10];       // 0x1038cd84
// Unit v5001 (src/ui_StatusPanel.cpp). The parameters of a tag; Mac layout in the comments (the text is a native pointer here).
struct CTagParams {
    uint32_t mId;       // 0x00  TagId
    int32_t mF4;        // 0x04  0x17f in LoadTags
    char16_t* mText;    // 0x08
    int32_t mTime;      // 0x0c  display time, 0 for none
    int16_t mFrame;     // 0x10
    int32_t mF14;       // 0x14
    int32_t mF18;       // 0x18
    uint16_t mF1C;      // 0x1c
    int32_t mF20;       // 0x20
    uint8_t mShow;      // 0x24
};
InterfaceWindow* AddATag(CTagParams& params);   // 0x10191ef8
void SaveTags(void* file);                      // 0x1019231c
void LoadTags(void* file);                      // 0x10192704
bool OverrideTag(CTagParams& params);           // 0x10191d7c, untranslated
void ShowTagPanel(InterfaceWindow* window);     // 0x10190d0c, untranslated
// Unit v5249 (src/ui_StatusPanel_FindStaticTag.cpp)
int32_t FindStaticTag(InterfaceWindow* window); // 0x10191cf4
bool AddTag(CTagParams& params);                // 0x101921f4
extern int32_t gMaxTags;                        // 0x102a8464, initially 10
extern int32_t gTagCount;                       // 0x1038cd74
// Unit v5531 (src/ui_StatusPanel_FindStaticTag_OverrideTag.cpp)
extern InterfaceWindow* gTagPanelPending;       // 0x1038cd7c: the window TagDlgCallback makes gTagPanelSource when it opens; message 0x1d (1) clears it
extern UI_RectDelta gTagSlideAnim[7];           // 0x102a83b2: the frames TagDlgCallback slides the panel with (7 frames, the last a goto)
extern InterfaceWindow* gTagFirstWindow;        // 0x103949d0
// Unit v5565 (src/ui_StatusPanel_SetButton_ShowTagPanel.cpp)
extern UI_RectDelta gTagShiftAnim[15];          // 0x102a83ea: the frames TagCallback shifts the other tags with (15 frames, the last an end)
void UIMain_MidWindowParticles(int32_t id, InterfaceWindow* window);   // 0x10182404, stand-in
extern void* gUISelectedThing;          // 0x103947b8, unknown word 0 of the selection
extern void* gUISelectedPeep;           // 0x103947b4
extern void* gUISelectedObject;         // 0x103947b0
extern float gGameWindowCentreX;        // 0x102b2b5c
extern float gGameWindowCentreY;        // 0x102b2b58
extern float gGameWindowHalfWidth;      // 0x10278a28, initially 1.0f
extern float gGameWindowHalfHeight;     // 0x10278a2c, initially 1.0f
extern int16_t gFirstPersonWindowScript[41]; // 0x102a76da
extern int16_t gTagPanelScript[137];         // 0x102a82a0
extern int16_t gCashPanelScript[54];         // 0x102a93cc
extern int16_t gCoasterPanelScript[235];     // 0x102a9438
extern int16_t gControlPanelScript[58];      // 0x102a9634

// ---- the main panel (unit w118). Names invented from the function that makes or reads each one.
extern InterfaceWindow* gMainPanel;          // 0x103949d8: window id 0x1d, the toolbar at the bottom left (CreateMainPanel's guard; MainPanelMsg clears it on message 0x14)
extern InterfaceWindow* gGoldenStuffPanel;   // 0x103949c4: window id 0x33 (message function GoldenStuffMsg)
extern InterfaceWindow* gAerialWindow;       // 0x103949d4: window id 0x2d (message function AerialCallback)
extern InterfaceWindow* gSlideoutTab;        // 0x103949c8: window id 0x23, the tab on the slide-out panel 0x21 (RailsCallback reads it)
extern int32_t gMainPanelFontIndex;          // 0x103949b4: set to 3, then GetFontTableEntry reads it back
extern int32_t gLastPanelLists;              // 0x103949c0: set to 1 (SetLastPanel type 1 writes it: 1 ride list, 2 hire list)
extern int32_t gLastPanelPark;               // 0x103949bc: set to 3 (type 2: 3 park status, 4 all staff, 5 all items, 6 all kids)
extern int32_t gLastPanelFinance;            // 0x103949b8: set to 10 (type 3: 7 finance, 8 loans, 9 staff costs, 10 entry price)
extern int32_t gStaffPanelState;             // 0x10395a38: StaffPanelUp answers it; InitStaffPanel zeroes it last
// The window scripts (src/ui_mainpanel_data.cpp, the image's words).
extern int16_t gMainPanelScript[606];        // 0x102aa840: four top-level windows under the game window (0x1d, 0x2d, 0x2f, 0x33)
extern int16_t gStaffPanelScriptA[195];      // 0x102ae164: the staff panels 0x95c and 0x963
extern int16_t gStaffPanelScriptB[95];       // 0x102ae2ea: the location panel 0x146
// The message functions of the windows the two builders make.
int32_t GoldenStuffMsg(InterfaceWindow*, int32_t, int32_t, int32_t);                // 0x101a6404 (unit w131) (window 0x33)
int32_t AerialCallback(InterfaceWindow*, int32_t, int32_t, int32_t);                // 0x101a68d0, untranslated (window 0x2d)
int32_t BobbleCallback(InterfaceWindow*, int32_t, int32_t, int32_t);                // 0x101a67f4 (unit w131) (window 0x2e)
int32_t CashWindowCallback(InterfaceWindow*, int32_t, int32_t, int32_t);            // 0x101a60c0 (unit w131) (window 0x2f)
int32_t RailsCallback(InterfaceWindow*, int32_t, int32_t, int32_t);                 // 0x101a6f2c, untranslated (window 0x21)
// Unit v5979 (src/ui_mainpanel2_msg_HideIfUnder.cpp)
struct UI_RectDelta;
void HideIfUnder(InterfaceWindow* window, int16_t limit);   // 0x101a6c44: shows each window of the tree (4 levels, then recursion) whose right edge is above the limit
extern UI_RectDelta gAerialSlide[15];                       // 0x102aad44: the bobble window's slide (15 frames, the last an end)
int32_t HappyBarMsg(InterfaceWindow*, int32_t, int32_t, int32_t);                   // 0x101a76f0 (unit w131) (window 0x1f)
int32_t MainPanelButtonsMsg(InterfaceWindow*, int32_t, int32_t, int32_t);           // 0x101a5a2c (unit w131) (window 0x25)
int32_t StaffPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t);                 // 0x101bfdd8 (unit w131) (windows 0x95c and 0x963)
int32_t ModifiedCheckButtonsCallback(InterfaceWindow*, int32_t, int32_t, int32_t);  // 0x101bffb0 (unit w131) (the check button groups 0x95d)
int32_t StaffLocMsg(InterfaceWindow*, int32_t, int32_t, int32_t);                   // 0x101c00f0 (unit w131) (window 0x146)
// src/ui_mainpanel2_msg.cpp: clears gMainPanel on message 0x14, then the default handler's answer.
int32_t MainPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);          // 0x101a5b6c (window 0x1d)
// The default handler's answer, nothing else.
int32_t HappyGaugeCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);    // 0x101a5c44 (window 0x1e)
int32_t DateWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);    // 0x101a61d0 (window 0x20)

// Unit w129 (all untranslated; CParkTime::Update calls them with the month number 1 to 12, after a month change): the three panels that show the date.
void UI_ParkStatusPanelDateChange(int32_t month);    // 0x101ada64
void UI_FinancePanelPanelDateChange(int32_t month);  // 0x101a0cf4
void UI_LoansPanelDateChange(int32_t month);         // 0x101a577c

// ---- Unit w131: the panel message functions (src/ui_mainpanel2_msg.cpp, src/ui_entryPricePanel_msg.cpp, src/ui_staffPanel_msg.cpp).
// Names of the file statics are invented; the addresses are the Mac's.
// CashWindowCallback's statics: a guard byte (tested as a signed byte), the frame counter and a word that only message 0x15 clears.
extern int8_t gCashWindowGuard;        // 0x10394998
extern uint32_t gCashWindowFrames;     // 0x1039499c
extern int32_t gCashWindowWord;        // 0x103949ac
// GoldenStuffMsg's statics: the last spending power shown (-1 when not yet) and the last key count shown, each behind a guard byte.
extern int8_t gGoldenSpendGuard;       // 0x10394990
extern int32_t gGoldenLastSpending;    // 0x10394994
extern int8_t gGoldenKeysGuard;        // 0x10394988
extern int32_t gGoldenLastKeys;        // 0x1039498c
class CThing;
class CParkAnalyser;
extern CThing* gStaffPanelThing;       // 0x10395a3c: the thing the staff panels show (src/ui_staffPanel_msg.cpp); StaffLocMsg reads its kind byte
// The two status panels and the map the panels reach: one object each, only their Show / CreateMapPanel are used (untranslated).
class StaffStatusPanel {
public:
    void Show(CThing* thing);          // 0x101c54f0 (104 bytes)
    // Unit v5107 (src/StaffStatusPanel_FireMe.cpp). The object has the UIStatusPanelFields layout (window at 0x0c, view window at 0x14, table at 0x20).
    void FireMe();                     // 0x101c5400 (192 bytes): sacks the staff member (asks first when the settings confirm deletes), then virtual slot 0x34
    void InitWindow();                 // 0x101c4c34 (136 bytes)
    void UpdateViewWindow();           // 0x101c423c (140 bytes)
    // Unit v5207 (src/StaffStatusPanel_FireMe_StaffStatusPanel_UpdateStatusWindow.cpp).
    void InitStatusWindow();           // 0x101c4304 (1552 bytes): the texts, fonts and bars of the status window (window at 0x10)
    void UpdateStatusWindow();         // 0x101c4990 (616 bytes)
    void UpdateWindow();               // 0x101c4d2c (348 bytes)
    // Unit v5364 (src/StaffStatusPanel_FireMe_StaffStatusPanel_UpdateStatusWindow_StaffStatusMsg.cpp).
    void Remove();                     // 0x101c5668 (36 bytes, stand-in)
    void SetSelected();                // 0x101c5358 (44 bytes, stand-in)
    // Unit v5951 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_sinit_ui_staffRoomStatus_cpp.cpp). +0x8 the edit window, +0xc the parent.
    void ShowEditNamePanel();          // 0x101c5878 (824 bytes)
    // Unit v51051 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_InitViewWindow.cpp)
    void InitViewWindow();             // 0x101c41a0 (4 bytes, nothing)
    void DeInitViewWindow();           // 0x101c41dc (36 bytes): UIMain_DestroySpritePreview
    void DeInitStatusWindow();         // 0x101c4950 (4 bytes, nothing)
    void DeInitWindow();               // 0x101c4cf0 (4 bytes, nothing)
    void ChangeName(char16_t* name);   // 0x101c558c (56 bytes): gStaffRoomPanelThing->SetName when there is one
    const char16_t* GetName();         // 0x101c55f8 (64 bytes): its name, NULL without a thing
};
// Unit v5364: the staff status panel's edit-name window handler and what it calls (untranslated, faked by the test library).
int32_t OneEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101c56bc (388 bytes)
void UIMain_UpdateSpritePreview();                                                    // 0x101817e4 (76 bytes, stand-in)
extern char16_t gStaffEditName[];   // 0x10395ad4 (name invented): the text of the name edit box, SetName's argument
class InterfaceWindow;
// Unit v5207: 0x10180aa0 (stand-in): the text of the staff state in the window.
// Unit v51039: UIMain_SetStaffStateText 0x10180aa0 (160). Callees the host cannot call as the Mac does; weak definitions in
// src/Serialise_basic_std_list_std_basic_string_std_allocator_std_basi_4_callees.cpp, faked by the test library.
namespace staffstate_callee {
const unsigned char* GetConstString(void* table, uint32_t id);   // 0x10238b78 TbITable::GetConstString (the table is gText's resource 10)
void ConvertString(void* converter, char16_t* out, const unsigned char* source, uint16_t size);   // 0x10238b90 TbIMBToUnicode::ConvertString
}
void UIMain_SetStaffStateText(InterfaceWindow* window, int32_t state);
// Unit v5107: functions and globals the staff status panel names (untranslated; faked by the test library).
int32_t fireme(void* context);                                                                  // 0x101c53b8 (the dialog's OK handler)
int32_t StaffStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);           // 0x101c4f8c
int32_t StaffStatusViewWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101c4ec0
extern int16_t gStaffStatusScript[];   // 0x102aedd0 (name invented): the panel's window script
extern CThing* gStaffStatusThing;      // 0x10395b18 (name invented): the thing StaffStatusPanel::Show was given
class VisitorStatusPanel {
public:
    void Show(CThing* thing);          // 0x101433f8 (112 bytes)
    // Unit v5521 (src/WorldSavvy_VisitorStatusPanel_InitStatusWindow.cpp)
    uint8_t mF00[0x10];                // 0x00  (not known)
    InterfaceWindow* mWindow;          // 0x10  the panel's window (a Ptr32 on the Mac, native here)
    // 0x10142238 (2092 bytes): gives the panel's child windows (ids 0x3724 to 0x3733) their draw objects, fonts (table entry 6) and texts.
    void InitStatusWindow();
    // Unit v5929 (src/ActionRecorder_CActionRecorder_CActionRecorder_VisitorStatusMsg_2.cpp). The object has the UIStatusPanelFields layout
    // (Mac offsets: state at 0x00, window at 0x0c, table at 0x20, a word at 0x24); the functions below read it through that struct.
    void InitWindow();          // 0x10142f44 (264 bytes)
    void UpdateWindow();        // 0x101430c0 (276 bytes): three or four virtual calls through the table at 0x20, then the title text
    void Remove();              // 0x1014356c (36 bytes, stand-in)
    void Locate();              // 0x1014350c (44 bytes, stand-in)
    void UpdateStatusWindow();  // 0x10142ae4 (1056 bytes, stand-in)
    uint8_t mF18[0x40];         // the rest of the Mac object (the Mac's size is 0x28; the host's pointer is wider than the Mac's)
    // Unit v51022 (src/ActionRecorder_CActionRecorder_CActionRecorder_VisitorStatusMsg_2_VisitorStatusPanel_UpdateStatusWindow_2.cpp)
    void UpdateViewWindow();    // 0x10142170 (140 bytes): the sprite preview of the thing in the window at Mac +0x14
    // Unit v51037 (untranslated stand-in): a kid left.
    void KidGone(CThing* thing);   // 0x101434a0 (48 bytes)
    // Unit v51072 (stand-ins that do nothing, except DeInitViewWindow)
    void DeInitWindow();           // 0x10143084 (4 bytes)
    void DeInitStatusWindow();     // 0x10142aa0 (4 bytes)
    void DeInitViewWindow();       // 0x10142110 (36 bytes): UIMain_DestroySpritePreview
    void InitViewWindow();         // 0x101420d0 (4 bytes)
};
// Unit v5929: the globals of ui_visitorStatus.cpp (names invented) and its functions.
extern CThing* gVisitorStatusThing;        // 0x1037ec10: the thing Show was given
extern int16_t gVisitorStatusScript[2];    // 0x102a4758: the panel's window script (initial word 1)
extern int32_t gVisitorStatusWordEC3C;     // 0x1037ec3c: 0x10000 after the static initialiser (no reader known)
extern int32_t gVisitorStatusWordEC40;     // 0x1037ec40: 0 after the static initialiser (no reader known)
extern uint32_t* gVisitorStatusTable;      // 0x102a4b5c: the panel's table of virtual slots (an arena block, made by the static initialiser)
int32_t VisitorStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1014320c (432 bytes)
void __sinit_ui_visitorStatus_cpp();   // 0x101435c4 (100 bytes)
// Unit v5521: callees outside the unit that the host cannot call as the Mac does (constructors); weak stand-ins in the src file, faked by the test library.
namespace vsp_callee {
void GradientCtor(void* mem, void* data);   // 0x101898c8 InterfaceDrawEngineHGradient(data)
void TimeCtor(void* mem);                   // 0x1018edf4 InterfaceVisitorTimeTextObject(NULL) (stand-in: the class is not declared)
}  // namespace vsp_callee
// Unit v5058: the map's sprite (0x30 bytes on the Mac; the constructor 0x10230d64 is untranslated and faked by the test library). Mac offsets: the
// x at 0x04 and y at 0x08 (floats), the thing at 0x18 (a Ptr32 on the Mac, native here, the members after it only keep the size).
struct _D3DVECTOR;   // unit v5200
enum EUI2DMapSpriteID : int32_t {};
struct TUI2DMapIndividualShowStatus {   // two words passed by value (r6, r7)
    int32_t mA;
    int32_t mB;
};
class CUI2DMapSprite {
public:
    CUI2DMapSprite(void* image, EUI2DMapSpriteID id, TUI2DMapIndividualShowStatus status, float scale);   // 0x10230d64 (224 bytes, untranslated)
    uint8_t mF00[4];
    float mX;                // 0x04
    float mY;                // 0x08
    uint8_t mF0C[0x18 - 0x0c];
    void* mThing;            // 0x18 (native pointer here)
    uint8_t mF20[0x30 - 0x20];
    // Unit v5200 (untranslated, faked by the test library). Mac offsets: the screen x and y floats at 0x0c and 0x10, the show status int at
    // 0x14 (0 to 2 are shown), the scale float at 0x24.
    void SetPosition(const _D3DVECTOR& position);   // 0x10231014 (64 bytes)
    void SetScale(float scale);                     // 0x10231094 (112 bytes)
    // Unit v5652. The host layout has a native pointer at 0x18, so these fields sit after 0x30 here; Mac offsets in comments.
    int32_t mKind;       // Mac 0x14: show status kind (0 map thing, 1, 3, 5 fixed colours, 4 thing type 6)
    int32_t mMode;       // Mac 0x1c: 1 mask, 2 and 3 equality
    int32_t mMatch;      // Mac 0x20
    void* mSprite;       // Mac 0x28: sprite instance
    int32_t mVisible;    // Mac 0x2c
    // 0x10231134 (860 bytes); the status is passed by value in r4 to r6
    void UpdateVisibilityAndColour(int32_t mask, int32_t second, int32_t third);
    // Unit v5959: not virtual on the host (the Mac's deleting destructor is slot 8, untranslated); a weak empty body is in
    // src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister.cpp.
    // Unit v51047 (src/std_basic_istream_basic_istream.cpp): 0x10230f78 (104 bytes) shows (nonzero) or hides the sprite: on a change the
    // detail block's flag bit 0 is cleared when shown, set when hidden. The destructor 0x10230ea4 (168 bytes) is defined there too.
    void SetVisibility(int32_t visible);
    ~CUI2DMapSprite();
    // Unit v51044: 0x10230f78 (104 bytes, untranslated, faked by the test library). The constructor stores the scale at Mac 0x24,
    // which is mF20 + 4 here (host 0x24) and the Mac's vtable address 0x102b2958 in mF00.
    // (SetVisibility is declared above, unit v51047.)
};
// Unit v51047: the destructor with the Mac's hidden delete flag (a flag above 0 frees the object); answers this.
extern char gCUI2DMapSpriteVt[];   // 0x102b2958 (TOC slot -0x5e64)
CUI2DMapSprite* CUI2DMapSpriteDtor(CUI2DMapSprite* self, int16_t flags);   // 0x10230ea4
void UI2DMapDelete(void* object);   // operator delete 0x10238218 (a fake in the test)
enum EUI2DMapZoom : int32_t {};   // unit v5526: 0 in, 1 out
enum EUI2DMapMoveDirection : int32_t {};   // unit v5083: 0 left, 1 right, 2 down (y up), 3 up (y down)
class CUI2DMap {
public:
    void CreateMapPanel();             // 0x1022c080
    // Unit v5058. The map's fields are read by Mac offset from this (the class has no members yet): width int32 at 0x24, height int32 at 0x28,
    // the sprite images (Ptr32 words) at 0x20068 .. 0x2007c.
    void CreateMapSprites();           // 0x1022fd78 (2628 bytes, src/CUI2DMap_CreateMapSprites.cpp)
    void UpdateSpritePosition(CUI2DMapSprite* sprite);   // 0x10230a0c (388 bytes, untranslated)
    // Unit v5083 (src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre.cpp). More Mac offsets: the image width and height of the map
    // picture ints at 0x48 and 0x4c, the centre x and y floats at 0x54 and 0x58, the zoom float at 0x5c, the sprite list (a
    // TbTList<CUI2DMapSprite*,CUI2DMapSprite*>*, a NATIVE pointer on the host) at 0x20080.
    void MoveCentre(EUI2DMapMoveDirection direction, float amount);   // 0x1022f8a4 (308 bytes)
    void InitSprites();                // 0x1022fad0 (636 bytes)
    void UpdateMapZoom();              // 0x1022f4d4 (928 bytes)
    void UpdateAllSpritePositions();   // 0x10230974 (96 bytes, untranslated)
    // Unit v5194: the object's storage, so the static initialiser can write the Mac offsets 0, 4, 0x10 .. 0x20, 0x5c and 0x20060 of the global.
    alignas(16) uint8_t mBytes[0x20100];
    // Host only: the six sprite images InitSprites loads, as native pointers at 0x200b8 .. 0x200e7 (the Mac's Ptr32 words at
    // 0x20068 .. 0x2007c cannot hold a host heap pointer; 0x200a0 .. 0x200b7 are MapAreaCallback's host-only places). Image k is
    // the one of the Mac word at 0x20068 + 4 * k.
    static constexpr size_t kHostImagesOffset = 0x200b8;
    void*& HostImage(int32_t k) { return reinterpret_cast<void**>(mBytes + kHostImagesOffset)[k]; }
    // Unit v5200 (src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition.cpp). More Mac offsets: the render area
    // (a TbRenderArea) at 0x2c whose depth word at 0x3c gives the bytes per pixel, and the map grid of 128 x 128 eight-byte entries at 0x60
    // (a type int, a thing id halfword at +4).
    void DrawCoasterTracks();          // 0x1022f160 (832 bytes)
    void StoreMapGridDetails();        // 0x1022de40 (1104 bytes)
    // Unit v5526 (src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_MapCallback.cpp). More Mac
    // offsets: window Ptr32 at 4 (the map panel), fog colour at 8, render options at 0xc, mouse x and y ints at 0x10 and 0x14, the
    // layer flag bits at 0x18, the two filter words at 0x1c and 0x20.
    // The window message function; the answer is the default handler's, 0 for the keys it eats.
    int32_t MapCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1022c7e8 (2180 bytes)
    void CloseSprites();               // 0x102307ec (188 bytes, untranslated)
    void UpdateMap();                  // 0x1022db4c (100 bytes, untranslated)
    void Zoom(EUI2DMapZoom zoom, float amount);   // 0x1022fa1c (128 bytes, untranslated): 0 in, 1 out
    void MapAreaCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1022d0ac (2652 bytes, unit v5985)
    int32_t MapAreaCallbackResult(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // v5985: the body of MapAreaCallback, with the answer in r3 (the default handler's, 0 for a click)
    // Unit v5959 (src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister.cpp).
    void UnRegister(CUI2DMapSprite* sprite);   // 0x10230cd4 (84 bytes)
    void DeleteSpriteListEntries();            // 0x10230bd4 (100 bytes)
    void LoadMap(int32_t reload);              // 0x1022dbdc (460 bytes): 0 also sets the map's render area (0x2c) up and allocates it
    void UpdateAllDrawRelated();               // 0x1022e2c4 (3688 bytes, untranslated)
    // Unit v5985: 0x1022ddd0 (64 bytes, untranslated): the entry of the map grid (8 bytes, at 0x60) for a cell, or NULL outside the grid.
    void* GetMapGridCell(int32_t x, int32_t y);
    // Unit v51105 (src/CUI2DMap_..._CUI2DMap_UnRegister_CUI2DMap_Register.cpp): adds the sprite to the list at 0x20080.
    void Register(CUI2DMapSprite* sprite);   // 0x10230c70 (44 bytes)
};
// Unit v51105: the destructor with the Mac's hidden delete flag (a flag above 0 frees the object); answers this. 0x1022bf04 (76 bytes).
CUI2DMap* CUI2DMapDestruct(CUI2DMap* self, int16_t deleteFlag);
// Unit v51105: TUI2DMapMapGridCell::TUI2DMapMapGridCell 0x1022bed0 (4 bytes, blr): the cell is plain data.
struct TUI2DMapMapGridCell;
void TUI2DMapMapGridCellCtor(void* self);
template <class TYPE, class ARG_TYPE> class TbTList;
namespace ui2dmap_callee {
// The list's AddTail 0x102318f0 (weak in the source, a fake in the test).
void ListAddTail(TbTList<CUI2DMapSprite*, CUI2DMapSprite*>* list, CUI2DMapSprite* sprite);
}
// Unit v5985 (defined in src/CUI2DMap_..._CUI2DMap_MapAreaCallback.cpp; names invented): halfword "no thing" ids of the map area window.
extern uint16_t gMapNoThingA;   // 0x102b290c
extern uint16_t gMapNoThingB;   // 0x102b290e
// Unit v5959: 0x102b2910, the character appended to the theme path before the map's file name (a colon).
extern char gUI2DMapSeparator;
void UI_2DMap_MapAreaCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1022bff8
// Unit v5083: the six map images' file names (0x14 bytes an entry on the Mac, the name a Ptr32 at +8; native pointer here). 0x10397010.
struct CUI2DMapImageEntry {
    int32_t mF00;
    int32_t mF04;
    const char* mName;   // 0x08
    int32_t mF0C;
    int32_t mF10;
};
extern CUI2DMapImageEntry gUI2DMapImages[6];   // 0x10397010
// Unit v5083: the end markers of the two lists InitSprites walks (original names unknown, initialised data, 0 here).
extern uint16_t gUI2DMapThingListEnd;    // 0x102b2948
extern uint16_t gUI2DMapObjectListEnd;   // 0x102b294a
// Unit v5058: the thing id lists' end markers (initialised data, original names unknown; CreateMapSprites compares the list ids with them).
extern uint16_t gThingIdNone;    // 0x102b294c
extern uint16_t gObjectIdNone;   // 0x102b294e
extern StaffStatusPanel gStaffStatusPanel;      // 0x10395b1c
extern VisitorStatusPanel gVisitorStatusPanel;  // 0x1037ec14
extern CUI2DMap gUI2DMap;                       // 0x1039742c
// Unit v5194 (src/sinit_ui_2dmap_cpp.cpp): the static initialiser of ui_2dmap.cpp and the globals it sets up (names invented).
extern "C" void __sinit_ui_2dmap_cpp();          // 0x102319d4 (3776 bytes)
struct CUI2DMapColourEntry {                     // 0x24 bytes: a colour and a 32 byte block whose last 16 bytes the original copies from uninitialised stack (zero here)
    uint8_t mColour[4];
    uint32_t mZero[4];
    uint32_t mSpare[4];
};
extern CUI2DMapColourEntry gUI2DMapColours[23];  // 0x103970f0
struct CUI2DMapGlobalNode { uint32_t mWords[3]; };   // the registration record of __register_global_object (12 bytes)
extern CUI2DMapGlobalNode gUI2DMapImageNodes[6];     // 0x10397088, string i uses entry 5 - i
extern CUI2DMapGlobalNode gUI2DMapNameNode;          // 0x103970e4
extern CUI2DMapGlobalNode gUI2DMapNode;              // 0x103b74b4
extern uint8_t gUI2DMapTgaName[0x40];                // 0x103970d0: a TbDynamicStringTemplate<char> ("2dmap.tga"); 0x14 bytes on the Mac
extern uint8_t gUI2DMapNameStrings[6][0x40];         // 0x10397010: six TbDynamicStringTemplate<char>, 0x14 bytes an entry on the Mac
extern uint32_t gUI2DMapCounterA;                    // 0x103b74c4: set to 0
extern uint32_t gUI2DMapCounterB;                    // 0x103b74c0: set to 0x10000
extern uint32_t gUI2DMapVtableSlot;                  // 0x102b296c: its address is the first word of the map
extern uint32_t gUI2DMapDtorVector[2];               // 0x102689f8: TVector of CUI2DMap::~CUI2DMap
extern uint32_t gUI2DMapCtorVector[2];               // 0x10268a00: TVector of TUI2DMapMapGridCell::TUI2DMapMapGridCell
// Untranslated callees of the panels (each answers nothing the panels read).
void ShowControlPanel();                        // 0x1019d28c
void RemoveControlPanel();                      // 0x1019d2f0
void GotoLastUsedPanel(int32_t type);           // 0x101a587c
void CreateResearchPanel();                     // 0x101b4f74
// Unit v5021 (src/CreateResearchPanel.cpp): what the research panel keeps (the Mac's addresses; the names are invented).
extern InterfaceWindow* gResearchPanel;         // 0x10395980: the panel; non-NULL while it is up
extern InterfaceWindow* gResearchEffortBar;     // 0x1039597c: its window 0xd2d10 (the effort gauge scroll bar)
extern int32_t gResearchMaxAbility;             // 0x10395974: CParkAnalyser::MaxResearcherAbility at creation
extern int32_t gResearchCurrentAbility;         // 0x10395970: CParkAnalyser::CurrentResearcherAbility at creation
extern int32_t gResearchGaugeValue;             // 0x1039596c: the value given to the gauge
extern uint8_t gResearchPanelMade;              // 0x10395978: set to 1 when the panel is built
extern int32_t gResearchSoundA;                 // 0x10395968: UIMain_UISound(0x9a)'s answer
extern int32_t gResearchSoundB;                 // 0x10395964: UIMain_UISound(0x9b)'s answer
extern uint32_t gResearchFontIndex;             // 0x102aca94: initialised data, 6 (the font of the five bars)
extern int16_t gResearchPanelScript[];          // 0x102ac35a: the panel's window script (the image's words, generated at configure time)
void InitAll();                                 // 0x101b4000 (516 bytes, untranslated)
// The message functions of the panel and its bars (untranslated; the unit's test library fakes them).
int32_t ResearchPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101b45e0
int32_t StoreTankMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101b495c
int32_t RideMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);            // 0x101b4bb0
int32_t UpgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);             // 0x101b4c70
int32_t ShopMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);            // 0x101b4d30
int32_t ShowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);            // 0x101b4df0
int32_t FeatureMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);         // 0x101b4eb0
void CreateLoansPanel();                        // 0x101a4de4
void CreateStaffCostsPanel();                   // 0x101be440
void CreateYearEndPanel();                      // 0x101c68d0
void UpdateStatus();                            // 0x101bf5c4
void StaffLocationSwitch(int32_t which);        // 0x101bf9f4
void hackfollowmode();                          // 0x101bfb48
// Unit v5076 (src/ui_staffPanel_msg_hackfollowmode.cpp)
void UpdateLocator();                           // 0x101bf798
class CPeep;
enum CELL_MODES : int32_t;
void UIMain_ZoomToPeep(CPeep* peep);            // 0x10181c70 (stand-in)
void GetNextStaffMember();                      // 0x101bf32c (stand-in; sets gStaffPanelThing)
void LLCC_DrawLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY);   // 0x1009c0d0 (768 bytes, src/MapCell_kinds_LLCC_DrawLine_2.cpp)
// The real text of LLCC_DrawLine, answering r3 (0 when a Modify refused, else 1); the void declaration above is what other files use.
int32_t LLCC_DrawLineBody(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY);
int32_t UIMain_GetStaffStateFrame(int32_t state);   // 0x10180a1c (stand-in)
extern int8_t gStaffPanelFrameGuard;            // 0x10395a2c: guard byte of UpdateStatus's frame
extern int32_t gStaffPanelFrame;                // 0x10395a30: the frame last shown (9 at first)
extern int32_t gStaffPanelLocation;             // 0x10395a34: the location switch last chosen (0 none)
void DoCashWin(InterfaceWindow* window);        // 0x101a5ca8 (996 bytes)
void ContextMouse(int32_t context);             // 0x10182548
int32_t StaffPanelUp();                         // 0x101c0cac (untranslated; unit w142)
void RemoveStaffPanel();                        // 0x101c0c5c (unit v51061)
void* UI_GetSelectedObject();                   // 0x10194730 (src/ui_allKidsPanel_select.cpp, unit v5L06; the word is compared with a thing pointer)
// CParkAnalyser::GetHappinessLevel (0x100e4cd8, 424 bytes, unit w154): the analyser's happiness for `which` (HappyBarMsg asks for 4), a float
// in f1. A free forwarder to the member CParkAnalyser::GetHappinessLevel (src/ParkAnalyser_visitors.cpp), which ignores its argument.
float CParkAnalyser_GetHappinessLevel(CParkAnalyser* analyser, int32_t which);
// Unit v5188 (src/ui_mainpanel2_msg_GotoLastUsedPanel.cpp): callees of GotoLastUsedPanel / DoCashWin and the cash window's state.
enum StaffType : int32_t;
void CreateFinancePanel();                      // 0x101a001c (stand-in)
void CreateHireListPanel(StaffType type);       // 0x101a1c88 (stand-in)
int32_t IsCoasterLoftMode();                    // 0x10058b30 (stand-in)
extern int32_t gCashColourIndex;                // 0x103949a0 (name invented): the colour index of the balance, 0..5
extern uint8_t gCashInitDone;                   // 0x103949a4: set to 1 by the first DoCashWin
extern int32_t gCashSessionCost;                // 0x103949a8: the session cost last shown (-999 none)
extern int32_t gCashFontCurrent;                // 0x102aad28: initialised data, 999: the colour index last given to SetFontColour
extern uint8_t gCashFontColours[24];            // 0x102aad2c: initialised data, 6 entries of red, green, blue, spare (red up to white)

// ---- Unit w136 (src/ui_main_gamewin.cpp): the game window's message functions. The names below are invented by w136; the addresses are the
// Mac's (ui_main.cpp BSS). The function statics of Engine_GameWinMsg sit behind guard bytes, kept here as globals so that a test can reset them.
extern int32_t gGameWinRightDown;         // 0x10380c2c (guard 0x10380c28): a right press is being watched
extern int8_t gGameWinRightDownGuard;     // 0x10380c28
extern int32_t gGameWinPressTicks;        // 0x10380c24 (guard 0x10380c20): IveGotTicks() at the press
extern int8_t gGameWinPressTicksGuard;    // 0x10380c20
extern int32_t gGameWinCameraOn;          // 0x10380c1c (guard 0x10380c18, 1 once): set to 1 on every call, 0 for the rest of a right press
extern int8_t gGameWinCameraOnGuard;      // 0x10380c18
extern int16_t gGameWinPressX;            // 0x10380c16: the high half of b at the press
extern int16_t gGameWinPressY;            // 0x10380c14: the low half of b at the press
extern int32_t gGameWinPointerIn;         // 0x10380c30: 1 on message 0x10001, 0 on 0x10002 and 0x15
extern int32_t gGameWinWordC34;           // 0x10380c34: cleared on 0x10001
extern int32_t gGameWinWordC38;           // 0x10380c38: cleared on 0x10001
extern int8_t gGameWinCheatGuard;         // 0x102da940: the static CCheat below is constructed on first use
alignas(4) extern uint8_t gGameWinCheat[12];   // 0x102da934: raw storage of the static CCheat (12 bytes; built behind the guard byte)
extern int32_t gRideHighlightKeep;        // 0x102c6cc8: the frame calls UnhighlightRide when it is 0 (role guessed)
// Callees of the game window's messages no header declared yet.
int32_t ContextMouseClick();              // 0x10183320 (src/ui_main_click.cpp, unit w143)
int16_t CheckShifts();                    // 0x1022a3b4 (ui_worldLobbyController.cpp; untranslated): the modifier keys, passed on as a short
void EndCoasterEditMode();                // 0x1019cad0 (ui_entryPricePanel.cpp; untranslated)
// Unit w143 (callees of ContextMouseClick; the selection ones are src/ui_allKidsPanel_select.cpp, unit v5L06).
void UI_CheckSelected();                  // 0x101947e4: the selection words take gIntrface's three pointer words
CThing* UI_GetSelectedPeep();             // 0x101946f8
void UI_SetSelectedObject(void* object);  // 0x10194768 (the word UI_GetSelectedObject answers)
void UIMain_GotoStatusPanelForThing(void* thing);   // 0x10181e2c
void CreateEntryPricePanel();             // 0x1019d660
void CoasterEditMode(void* object);       // 0x1019c584
// Unit v5246 (src/ui_entryPricePanel_CoasterEditMode.cpp): the coaster edit panel's buttons.
void WhichSpecialButton();                                // 0x1019c3e8 (368 bytes)
void LinkSlideoutPanel(InterfaceWindow* panel, int32_t n);
InterfaceWindow* LinkSlideoutPanelResult(InterfaceWindow* panel, int32_t n);   // same code, keeps r3 (the panel, or NULL)   // 0x101a82ec (src/ui_mainpanel2_window_LinkSlideoutPanel.cpp): answers the panel, or NULL when slot 0x21 already holds one
// Host only: the slide-out slot's message 0xa carries the arriving panel as a 32-bit word (the Mac's pointer); a host window is above
// 4 GB, so LinkSlideoutPanel notes the panel here and RailsCallback's WindowFromWord gets it back from the word's low 32 bits.
inline InterfaceWindow* gSlideoutWordWindows[16] = {};
inline void NoteSlideoutWord(InterfaceWindow* window) {
    const uint32_t word = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(window));
    InterfaceWindow** slot = &gSlideoutWordWindows[(word >> 4) & 15];
    *slot = window;
}
inline InterfaceWindow* SlideoutWordWindow(int32_t word) {
    InterfaceWindow* window = gSlideoutWordWindows[(static_cast<uint32_t>(word) >> 4) & 15];
    if (window != nullptr && static_cast<uint32_t>(reinterpret_cast<uintptr_t>(window)) == static_cast<uint32_t>(word)) return window;
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(word)));
}
int32_t IsTrackClosed(void* coaster);                     // 0x10052850 (stand-in)
void GetCoasterEditAvailableButtons(uint32_t* out);       // 0x10057278 (stand-in): bit mask of the buttons
void GetCoasterEditDefaultButtons(uint32_t* out);         // 0x10057120 (stand-in)
void WhichCheckButtonsAreDown(uint32_t mask);             // 0x1019b120 (stand-in)
extern void* gCoasterEditObject;                          // 0x1039482c: the coaster being edited (name invented); defined in src/ui_entryPricePanel_CoasterEditMode.cpp
extern uint8_t gCoasterEditBusy;                          // 0x10394828: nonzero keeps CoasterEditMode from starting (name invented); defined there too
// Unit v5343 (src/ui_staffPanel_ShowKidLocatorPanel.cpp)
void ShowStaffPanel(CThing* thing);                       // 0x101c0690 (892 bytes)
void ShowKidLocatorPanel();                               // 0x101c0a38 (504 bytes)
// Unit v5538 (src/ui_coasteredit_kb_EndCoasterEditMode.cpp)
void CoasterButtonsEnabled(uint32_t buttons);             // 0x1019cd6c (576 bytes): enables the panel's buttons by the mask bits

// Unit v5986 (src/CUI2DMap_..._CUI2DMap_CreateMapPanel.cpp)
void GetHFieldMaxVertexIndices(uint32_t* width, uint32_t* height);   // 0x1005c250 (stand-in): the heightfield's last vertex indices
int32_t UI_2DMap_CallbackFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1022bf74 (stand-in)
extern int16_t gMapPanelScript[];   // 0x102b2366 (name invented): the map panel's window script (the image's words, generated at configure time)
class InterfaceDrawObject;
namespace mappanel_callee {
InterfaceDrawObject* NewTextSprite();   // operator new(0x19c), then InterfaceTextObjectSprite(NULL) 0x1018d11c; faked by the test library
}

// ---- Unit v5057 (src/ShowEditParkNamePanel.cpp): the panel that edits the two sign names of the park's gates.
class CObject;
int32_t TwoEditsMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1019da6c
void ShowEditParkNamePanel();                  // 0x1019deac
extern InterfaceWindow* gEditParkNamePanel;    // 0x1039483c: the panel, NULL while it is not up (names invented in this block)
extern InterfaceWindow* gParkScreenWindow;     // 0x10394848: the window the panel is built under (its z + 7 is the panel's)
extern int16_t gEditParkNameScript[2];         // 0x102a9834: the panel's window script (initial word 1; the rest of the data is not recovered)

// ---- Unit v5885 (src/ShowEditParkNamePanel_CreateEntryPricePanel_2.cpp): the entry price panel (CreateEntryPricePanel is declared above).
int32_t EntryPricePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);        // 0x1019d350
int32_t EntryPricePanelTitleMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1019d544
void RemoveEntryPricePanel();                  // 0x1019da08 (stand-in)
extern int16_t gEntryPriceScript[];           // 0x102a96a8: the panel's window script (the image's words, generated at configure time)
extern int32_t gEntryPriceClosed;              // 0x10394840: IsParkClosed as the panel was built
extern uint32_t gEntryPriceFee;                // 0x10394844: the admission fee being edited
namespace entry_callee {
void WindowEnable(InterfaceWindow* window, int16_t on);   // the virtual Enable (slot 0x1c of the table at +0x118); faked by the test library
}

// ---- Unit v5071 (src/ui_main_kbcapture_UIMain_ShowRidePreview_UIMain_GotoStatusPanelForThing.cpp): the status panels of the other kinds
// (one object each, only Show is used here; untranslated, the unit's test library fakes them).
class UIStatusPanelOps;
struct IDXTYPEUniqueNameThingId;
template<class Tag, class T> struct tag_IDX_TYPE;
class RideStatusPanel {
public:
    void Show(CThing* thing);                   // 0x101bc0bc (176 bytes)
    // Unit v5074 (stand-ins)
    int32_t Upgrade(int32_t what, bool flag);   // 0x101bbb88
    void UpdateStatusWindow();                  // 0x101b9ac4
    void Remove();                              // 0x101bc1a0
    void UpgradePanelByeBye();                  // 0x101ba930
    // Unit v5191 (src/LCPostFormat_RideStatusPanel_UpdateStatusWindow.cpp)
    void UpdateWhichButtons();                  // 0x101b8cc8 (992 bytes)
    void ShowUpgradePanel();                    // 0x101ba6e8 (528 bytes)
    void UpdateKnobsWindow();                   // 0x101bb494 (588 bytes, stand-in)
    // The fields the three functions use, named from them (the Mac's offsets in the comments; the host's pointers are wider, so its
    // offsets differ).
    uint8_t mF00[4];                // 0x00
    int32_t mState;                 // 0x04  the object's state the panel last saw (UIMain_GetObjectState(object, 2))
    int32_t mF08;                   // 0x08  not 0: ShowUpgradePanel does nothing
    InterfaceWindow* mWindow;       // 0x0c  the panel's window
    InterfaceWindow* mStatusWindow; // 0x10  the window holding the figures (NULL: nothing to update)
    uint8_t mF14[0x18 - 0x14];      // 0x14 (the Mac's 0x14 to 0x1c; the host's pointer below sits at host 0x18, so mOps stays where it was)
    InterfaceWindow* mNameWindow;   // 0x1c  the window that shows the object's name (UpdateWindow; unit v51011)
    UIStatusPanelOps* mOps;         // 0x20  the table whose slot at +0x2c is told the state changed
    InterfaceWindow* mUpgradeWindow;   // 0x24  the upgrade panel's window (NULL while it is not up)
    uint8_t mF28[4];                // 0x28
    int32_t mSpeed;                 // 0x2c  the operating speed asked of the prospective figures (names guessed)
    int32_t mCapacity;              // 0x30
    int32_t mDuration;              // 0x34
    // Unit v5534
    void Edit(int32_t on);          // 0x101bb71c (624 bytes, stand-in)
    void DoDial(int32_t a, int32_t b);   // 0x101bb9b8 (228 bytes, stand-in)
    // Unit v5580 (src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Condemned.cpp)
    // 0x101bc2f0 (156 bytes): with a window, asserts that thing array entry 0 is free, then tells the ops table (slot at +0x1c) when the entry
    // of the id holds the staff room panel's thing.
    void Condemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);
    void BreakDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x101bc1f4 (unit v51061)
    // 0x101b9340 (1804 bytes): gives the status window's children their draw objects, fonts and texts.
    void InitStatusWindow();
    // Unit v5914 (src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade.cpp)
    void ApplyChanges();                // 0x101bbacc (136 bytes): the speed, capacity and (when the ride has a duration unit) duration into gStaffRoomPanelThing
    void UpdateKnobsTitles(int32_t which);   // 0x101baf28 (1328 bytes, stand-in): the knobs' titles (bit mask 1 speed, 2 capacity, 4 duration)
    // Unit v51011 (src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade_RideStatusPanel_InitKnobsWindow.cpp)
    void InitWindow();        // 0x101ba99c (164 bytes)
    void UpdateWindow();      // 0x101baaac (140 bytes)
    void InitKnobsWindow();   // 0x101bab6c (900 bytes)
    // Unit v51121 (src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade_RideStatusPanel_UpgradePanelByeBye.cpp):
    // the view windows are reached by Mac offset (UIStatusPanelFields: +0x14 the preview window, +0x18 the text window).
    void DeInitWindow();         // 0x101baa74 (4 bytes)
    void DeInitStatusWindow();   // 0x101b9a84 (4 bytes)
    void UpdateViewWindow();     // 0x101b9264 (164 bytes)
    void DeInitViewWindow();     // 0x101b9208 (36 bytes)
    void InitViewWindow();       // 0x101b90e4 (236 bytes)
};
// Unit v51011: the string table at 0x1024e460 (the Mac's data; "hilight" at +0x10, the gradient mesh "smallguage.wct" at +0x18).
extern const char gRideStatusMeshNames[0x28];
// Unit v51011: the variable tag UpdateKnobsTitles builds (id 5 speed, 6 capacity, 7 duration; the value without its sign, then 1 for negative).
struct RideKnobTag : TbIntegerVTag {
    RideKnobTag(uint32_t id, int32_t value, int32_t negative) : TbIntegerVTag(id, value), mNegative(negative) {}
    int32_t mNegative;   // Mac +0x10
};
// Unit v5914: 0x10182160 (64 bytes, src/ui_main_preview_tick.cpp, unit v5L06): the "not enough dough" sound, event 0x1d of gSoundBanks[3].
void UIMain_NotEnoughDough();
// Unit v5914: RideStatusPanel::Show's two globals (names invented): the thing the panel was opened for (0x1039599c, bss), and the window
// script it opens (0x102acef0, initial word 1, the rest of the data is not recovered).
extern CThing* gRideStatusShowThing;
extern int16_t gRideStatusScript[2];
// Unit v5191: the variable tag UpdateStatusWindow builds for the age (id 8, the age without its sign, then 1 for a negative age), passed to
// TbITable::FormatString by address (Mac stack +0x44: magic, vtable of TbIntegerVTag, id, value, flag).
struct RideAgeTag : TbIntegerVTag {
    RideAgeTag(int32_t value, int32_t negative) : TbIntegerVTag(8, value), mNegative(negative) {}
    int32_t mNegative;   // Mac +0x10
};
// 0x102ad4f0 (name invented): the upgrade panel's window script (initial word 1; the rest of the data is not recovered).
extern int16_t gUpgradePanelScript[2];
class UpgradeStatusPanel { public: void Show(CThing* thing);   // 0x101c665c (76 bytes)
    void Remove();   // 0x101c66e0 (36 bytes, stand-in; unit v5548)
    // Unit v5905 (src/UpgradeStatusMsg_UpgradeStatusPanel_InitStatusWindow_2.cpp): the panel is a UIStatusPanel; +0xc the parent window
    // and +0x10 the status window (32-bit pointers, read by offset).
    void InitStatusWindow();     // 0x101c5e00 (596 bytes)
    void UpdateStatusWindow();   // 0x101c60d4 (244 bytes)
    void InitWindow();           // 0x101c6208 (136 bytes)
    // Unit v51051 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_InitViewWindow.cpp)
    void InitViewWindow();       // 0x101c5c80 (4 bytes, nothing)
    void DeInitViewWindow();     // 0x101c5cc0 (36 bytes): UIMain_DestroyRidePreview
    void UpdateViewWindow();     // 0x101c5d20 (164 bytes): the ride preview in the view window at +0x14
    // Unit v51111 (src/InterfaceKnob_InterfaceKnob_GetValue.cpp)
    void UpdateWindow();         // 0x101c6304 (120 bytes): the text from slot 0x3c to the window at +0x1c, then slots 0x20 and 0x1c
    void DeInitWindow();         // 0x101c62c8 (4 bytes, nothing)
    void DeInitStatusWindow();   // 0x101c6090 (4 bytes, nothing)
};
// Unit v5905: 0x10395b48 (name invented): the thing the upgrade panel shows (Show stores it, InitWindow lists it).
extern CThing* gUpgradeStatusThing;
// 0x102af2ac (name invented): the upgrade panel's window script (initial word 1; the rest is not kept here).
extern int16_t gUpgradeStatusScript[2];
// Unit v5548 (src/UpgradeStatusMsg.cpp): the upgrade panel's window handler, and the static initialiser of ui_staffStatus.cpp.
int32_t UpgradeStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101c63b4 (620 bytes)
void __sinit_ui_staffStatus_cpp();   // 0x101c5bec (100 bytes)
extern int32_t gStaffStatusF40;      // 0x10395b40 (name invented): 0x10000 after the sinit, no reader known
extern int32_t gStaffStatusF44;      // 0x10395b44 (name invented): 0 after the sinit
// Unit v5085: the table the status panels keep a pointer to at Mac +0x20. A call through it is `ops->slot(panel)`: the Mac's vtable has
// 8 bytes of header and 4 a slot, the one the panel calls is at +0x2c (slot 9). The other slots are not named.
class UIStatusPanelOps {
public:
    virtual void Slot0() {}
    virtual void Slot1() {}
    virtual void Slot2() {}
    virtual void Slot3() {}
    virtual void Slot4() {}
    virtual void Slot5() {}   // Mac vtable +0x1c (the panel is passed in r3 on the Mac; RideStatusPanel::Condemned calls it)
    virtual void Slot6() {}
    virtual void Slot7() {}
    virtual void Slot8() {}
    virtual void Notify(void* panel) {}   // Mac vtable +0x2c: told when the state of the object changed
    // Unit v5534
    virtual void Slot10() {}
    virtual void Slot11(void* panel) {}   // Mac vtable +0x34: RideStatusMsg calls it (__ptr_glue) after GoFirstPerson
    // Unit v5936
    virtual void Slot12() {}              // Mac vtable +0x38
    virtual const char16_t* Slot13(void* panel) { return nullptr; }   // Mac vtable +0x3c: ShopStatusPanel::UpdateWindow hands the answer to the window's SetText
};
// 0x101bdee0 Show (80 bytes), 0x101bd28c InitWindow (1080 bytes) and 0x101bcea0 UpdateStatusWindow (944 bytes) (unit v5085, src/ShopStatusPanel_InitWindow.cpp).
// The fields are named from those two; the Mac's offsets are in the comments (the host's pointers are wider, so its offsets differ).
class ShopStatusPanel {
public:
    void Show(CThing* thing);
    void InitWindow();
    void UpdateStatusWindow();
    uint8_t mF00[4];              // 0x00
    int32_t mState;               // 0x04  the object's state the panel last saw (UIMain_GetObjectState(object, 2))
    uint8_t mF08[4];              // 0x08
    InterfaceWindow* mWindow;     // 0x0c  the panel's window
    InterfaceWindow* mStatusWindow;   // 0x10  the window holding the figures (NULL: nothing to update)
    uint8_t mF14[0x20 - 0x14];    // 0x14
    UIStatusPanelOps* mOps;       // 0x20  the Mac's vtable pointer of the panel's second table
    int32_t mQuality;             // 0x24  times 50 it is the quality asked of GetProspectiveCostOfGoods
    int32_t mIngredient;          // 0x28  the ingredient amount (InitWindow clears it)
    void Remove();                // 0x101bdf64 (36 bytes, stand-in; unit v5152)
    int32_t mF2c;                 // 0x2c  (unit v5152: ShopStatusMsg's message 0x800 / 0xc085 stores b here)
    // Unit v5734 (src/ShopStatusPanel_InitWindow_ShopStatusPanel_ApplyChanges.cpp)
    void ApplyChanges();                      // 0x101bd9bc (108 bytes): the panel's figures onto the shop
    void applytoallcallback(CThing* thing);   // 0x101bdab8 (144 bytes): the same onto a thing of the shop's type
    // Unit v5936 (src/ShopStatusPanel_InitWindow_ShopStatusPanel_ApplyChanges_ShopStatusPanel_InitStatusWindow.cpp)
    // The Mac's window pointer at 0x1c (UpdateWindow) is read from mF14 + 4 here (the native pointer fits the 12 bytes of mF14).
    void InitStatusWindow();   // 0x101bc6c4 (1892 bytes): the figures window's draw objects, fonts and texts
    void UpdateWindow();       // 0x101bd730 (600 bytes): the panel's figures from the shop (gStaffRoomPanelThing)
    // Unit v51121 (src/LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade_RideStatusPanel_UpgradePanelByeBye.cpp)
    void InitViewWindow();       // 0x101bc468 (236 bytes)
    void DeInitViewWindow();     // 0x101bc58c (36 bytes)
    void UpdateViewWindow();     // 0x101bc5e8 (164 bytes)
    void DeInitStatusWindow();   // 0x101bce60 (4 bytes)
    void DeInitWindow();         // 0x101bd6f8 (4 bytes)
};
// 0x1024e580 (unit v5936): "ridestatbar.wct" (16 bytes with the padding), then at +0x10 "happygrad.wct" (the gradients' mesh names)
extern const char gShopStatusMeshNames[];
extern int16_t gShopStatusDescription[2];   // 0x102ad644 (name invented, defined in the unit v5734 file): the window description Show hands OpenWindow (first word 1)
// 0x19c bytes on the Mac, made by operator new(0x19c) and 0x1018de60 (164 bytes, unit v5L16; unit v5085): the draw object of a money figure.
extern CObject* gShopStatusObject;   // 0x103959dc (name invented): the shop the panel shows (defined in src/ShopStatusPanel_InitWindow.cpp)
class ShowStatusPanel { public: void Show(CThing* thing);       // 0x101c2a38 (80 bytes)
    // Unit v5525 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_ShowStatusMsg.cpp). UIStatusPanelFields layout; +0x24, +0x28, +0x2c are ints.
    void Remove();               // 0x101c2abc (36 bytes, stand-in)
    void UpdateStatusWindow();   // 0x101c1848 (1036 bytes, stand-in)
    // Unit v5547 (src/ShowStatusPanel_UpdateStatusWindow.cpp)
    void ApplyChanges();                      // 0x101c2508 (132 bytes)
    void applytoallcallback(CThing* thing);   // 0x101c261c (168 bytes): ApplyChanges for every thing of the same type as gStaffRoomPanelThing
    // Unit v51001 (src/ui_staffPanel_msg_hackfollowmode_GetNextStaffMember.cpp)
    void InitWindow();                        // 0x101c1c90 (1224 bytes)
    // Unit v51007 (src/ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_UpdateWindow_2.cpp)
    void UpdateWindow();                      // 0x101c21c4 (784 bytes): the panel's figures from gStaffRoomPanelThing; +0x1c window, +0x20 table
    // Unit v51018 (src/ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_InitStatusWindow.cpp)
    void InitStatusWindow();                  // 0x101c0f80 (2128 bytes): draw objects and fonts for the children of the window at +0x10
    // Unit v51051 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_InitViewWindow.cpp)
    void DeInitWindow();                      // 0x101c218c (4 bytes, nothing)
    void DeInitStatusWindow();                // 0x101c1808 (4 bytes, nothing)
    void UpdateViewWindow();                  // 0x101c0ea4 (164 bytes): the ride preview in the view window at +0x14
    void DeInitViewWindow();                  // 0x101c0e48 (36 bytes): UIMain_DestroyRidePreview
    void InitViewWindow();                    // 0x101c0d24 (236 bytes): the view window at +0x18 gets a text sprite and its font
};

// Unit v51018: "ridestatbar.wct" at 0x1024e668, "happygrad.wct" 0x10 bytes on.
extern const char gStatusMeshNames[];
// Callees the host cannot call as the Mac does (allocation, constructors, the virtual SetValue at slot 0x24 of the table at +0x118);
// defined in src/ShowStatusPanel_InitStatusWindow_callees.cpp, faked by the test library.
namespace statusinit_callee {
void* OperatorNew(uint32_t size);                      // 0x102384b8
void SpriteCtor(void* mem);                            // 0x1018d11c InterfaceTextObjectSprite(NULL)
void CashCtor(void* mem);                              // 0x1018de60 InterfaceCashTextObject(NULL)
void GradientCtor(void* mem, const char* meshName);    // 0x101898c8 InterfaceDrawEngineHGradient(meshName)
void WindowSetValue(InterfaceWindow* window, int32_t value);   // virtual slot 0x24
}  // namespace statusinit_callee
// Callees the host cannot call as the Mac does (operator new(0x19c) plus the constructor with NULL, 0x1018de60 / 0x1018d11c): weak
// definitions in src/ui_staffPanel_msg_hackfollowmode_GetNextStaffMember.cpp, faked by the test library.
class InterfaceDrawObject;
namespace showstatus_callee {
InterfaceDrawObject* NewCashText();
InterfaceDrawObject* NewTextSprite();
}  // namespace showstatus_callee
// Unit v5547: the variable tag UpdateStatusWindow builds twice for FormatString (Mac 0x14 bytes: magic, vtable, id, value, flag at +0x10).
struct ShowStatusTag : TbIntegerVTag {
    ShowStatusTag(uint32_t id, int32_t value) : TbIntegerVTag(id, value), mFlag(0) {}
    int32_t mFlag;   // Mac +0x10
};
extern CThing* gShowStatusThing;      // 0x10395a54 (name invented): the thing Show was given
extern int16_t gShowStatusScript[2];  // 0x102ae3e8 (name invented): the panel's window script (initial word 1)
int32_t ShowStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101c2708 (760 bytes)

class BogStatusPanel {
public:
    void Show(CThing* thing);   // 0x1019ac5c (140 bytes)
    // Unit v5301 (src/BogStatusPanel_InitViewWindow.cpp). The panel has the UIStatusPanelFields layout (below, in this header).
    void InitViewWindow();      // 0x10199ed0 (236 bytes): the view window (+0x18) one deeper, hidden, with a text sprite and font 8
    void UpdateViewWindow();    // 0x1019a050 (164 bytes): shows the ride preview of gStaffRoomPanelThing in the view window (+0x14)
    void InitWindow();          // 0x1019a7c8 (152 bytes)
    void UpdateWindow();        // 0x1019a8cc (120 bytes): four virtual calls
    // Unit v5519 (src/BogStatusPanel_InitViewWindow_BogStatusPanel_UpdateStatusWindow.cpp)
    void InitStatusWindow();    // 0x1019a12c (864 bytes): the labels, number, bar and cash windows of the status window (+0x10)
    void UpdateStatusWindow();  // 0x1019a504 (648 bytes): refreshes them from gStaffRoomPanelThing and the buttons of the panel window (+0x0c)
    // Unit v5699
    void Remove();              // 0x1019ad1c (36 bytes, stand-in)
    // Unit v51115
    void DeInitViewWindow();    // 0x10199ff4 (36 bytes): UIMain_DestroyRidePreview
    void DeInitStatusWindow();  // 0x1019a4c4 (4 bytes): nothing
    void DeInitWindow();        // 0x1019a894 (4 bytes): nothing
    void ApplyChanges();        // 0x1019a978 (4 bytes): nothing
};
int32_t BogStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1019a9b0 (628 bytes; its TVector is 0x10267aa8)
void __sinit_ui_bogStatus_cpp();   // 0x1019ad70 (76 bytes, unit v5699): gBogStatusPanel's constructor, table 0x102a9384 at +0x20, mode 0x3ea
extern CThing* gBogStatusThing;      // 0x103947fc (name invented): the thing Show was given
extern int16_t gBogStatusScript[];   // 0x102a908c (name invented): the panel's window script (first word 1; the rest not recovered)
class FeatureStatusPanel {
public:
    void Show(CThing* thing);   // 0x1019edc4 (76 bytes)
    // Unit v5311 (src/FeatureStatusPanel_UpdateStatusWindow.cpp). The panel has the UIStatusPanelFields layout (below, in this header).
    void UpdateStatusWindow();   // 0x1019e728 (244 bytes): the park count (0x1586) and the residual value (0x1584) of gStaffRoomPanelThing
    void InitWindow();           // 0x1019e85c (136 bytes)
    void UpdateWindow();         // 0x1019e958 (208 bytes): four virtual calls
    void RideIt();               // 0x1019ea60 (80 bytes)
    void UpdateViewWindow();     // 0x1019e374 (164 bytes): the ride preview of gStaffRoomPanelThing in the view window (+0x14)
    // Unit v5676 (src/FeatureStatusPanel_UpdateStatusWindow_FeatureStatusPanel_InitStatusWindow.cpp)
    void InitStatusWindow();     // 0x1019e454 (596 bytes): the texts, numbers and cash of the windows 0x1587 to 0x1584 of the window at +0x10
    // Unit v5904
    void Remove();               // 0x1019ee48 (36 bytes, stand-in)
    // Unit v51123
    void InitViewWindow();       // 0x1019e2d4 (4 bytes): nothing
    // Unit v51126 (src/std___tree_std_pair_std_map_nocase_string_BuddyI.cpp): both do nothing.
    void DeInitWindow();         // 0x1019e91c (4 bytes)
    void DeInitStatusWindow();   // 0x1019e6e4 (4 bytes)
    // Unit v51125 (src/std_exception_what.cpp)
    void DeInitViewWindow();     // 0x1019e314 (36 bytes): UIMain_DestroyRidePreview
};
void __sinit_ui_FeatureStatus_cpp();   // 0x1019eea0 (76 bytes, unit v5904): gFeatureStatusPanel's constructor, table 0x102a9ba0 at +0x20, mode 3
extern CThing* gFeatureStatusThing;     // 0x1039486c (name invented): the thing Show was given
extern int16_t gFeatureStatusScript[];  // 0x102a9948 (name invented): the panel's window script (first word 1)
int32_t FeatureStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x10267b40 (TVector; untranslated)
extern RideStatusPanel gRideStatusPanel;         // 0x103959a4
extern UpgradeStatusPanel gUpgradeStatusPanel;   // 0x10395b4c
extern ShopStatusPanel gShopStatusPanel;         // 0x103959e0
// Unit v5716
extern UIStatusPanelOps gShopStatusOps;          // 0x102adbd8 (the panel's second table; initial word 0x102adbd0)
void __sinit_ui_shopStatus_cpp();                // 0x101bdfb8 (76 bytes)
extern ShowStatusPanel gShowStatusPanel;         // 0x10395a58
extern BogStatusPanel gBogStatusPanel;           // 0x10394800
extern FeatureStatusPanel gFeatureStatusPanel;   // 0x10394870
// Initialised data (halfwords, 0 at start; names invented): the ride-upgrade ids the status chooser compares with.
extern uint16_t gNoUpgradeParent;                // 0x102a7868: the "no parent" thing id
extern uint16_t gNoUpgradeTarget;                // 0x102a786a: the "upgrades nothing" object id
extern char16_t gEditSignBufA[15];             // 0x10394782: the first edit box's text area (0xe characters)
extern char16_t gEditSignBufB[15];             // 0x10394764: the second one's
extern const char16_t* gCheatSignA;            // 0x102a9940: the cheat text (pointer to a string in the data; initial 0x1024e068)
extern const char16_t* gCheatSignB;            // 0x102a9944: (initial 0x1024e07c)
char16_t* UIMain_GetObjectName(CObject* object);   // 0x101810fc (src/TailSeven.cpp, unit v5L16)
// The call in TwoEditsMsg passes only (object, first text, second text); the 5-argument overload in ui_ErrorDisplay.h is another function.
void RegenerateRideSign(void* object, const char16_t* signA, const char16_t* signB);   // 0x101eeb9c (unit v5934; the callers ignore r3)
int32_t RegenerateRideSignResult(void* object, const char16_t* signA, const char16_t* signB);   // the same, answering r3 (the test compares it)
void RegenerateRideSigns();   // 0x101eeeb4 (unit v5934): regenerates the sign of every ride in the object list that has the 0x8000000 flag
extern uint16_t gRideSignListEnd;   // 0x102b12e6 (name invented, unit v5934): the object list's end marker RegenerateRideSigns compares the link with
int32_t wcscmp(const char16_t* a, const char16_t* b);   // the MSL import (glue 0x10238c98)

// ---- Unit v5004 (src/LCPostFormat.cpp): the staff room panel's list control and message functions.
enum UIObjType : int32_t;
enum StaffType : int32_t;
class CStaff;
// The status panel base: its members are called by the staff room panel's messages (all untranslated).
class UIStatusPanel {
public:
    void GetNextObject();       // 0x10192c28
    void GetPreviousObject();   // 0x10192de0
    void EraseObject();         // 0x10193138
    void Move();                // 0x101938d8
    int32_t FillObjectList(CThing* thing);   // 0x10192a90 (348 bytes, stand-in; unit v5085): the number of objects listed
    void InitEditBox(int32_t id);            // 0x10193dcc (124 bytes, stand-in; unit v5085)
    // Unit v5090 (src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject.cpp)
    void OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t windowA, int32_t windowB, int32_t windowC);   // 0x101935a0 (388 bytes)
    void EnableNameClick(int32_t enable);   // 0x10194608 (52 bytes, stand-in; unit v5191)
    // Unit v5300
    void CloseObject(int32_t flag);   // 0x10193010 (64 bytes, stand-in; unit v5152)
    // Unit v5340: +0x8 the edit-name window (32-bit pointer), +0xc the parent window (32-bit pointer); both read by offset.
    void ShowEditNamePanel();   // 0x10194208 (900 bytes)
    // Unit v5364
    void EditNamePanelGone();         // 0x101945c4 (12 bytes, stand-in)
    // Unit v5534
    void RequestMaintenance(bool on);   // 0x101932a0 (88 bytes, stand-in)
    // Unit v5910
    void DisplayStatusText();         // 0x10193af4 (128 bytes, stand-in)
    // Unit v5951
    void ZoomTo();                    // 0x10193330 (60 bytes, stand-in)
    // Unit v51051
    void CloseWindow();               // 0x10193774 (52 bytes, stand-in, faked by the test library)
    // Unit v51072
    void ObjectGone(CThing* thing);   // 0x10192f9c (60 bytes, stand-in, faked by the test library)
    // Unit v51114 (src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_UIStatusPanel_EnableNameClick.cpp): the empty base versions (blr)
    void ApplyChanges();                 // 0x10194674 (4 bytes)
    char16_t* GetName();                 // 0x1019387c (44 bytes): UIMain_GetObjectName of gStaffRoomPanelThing
    void ChangeName(char16_t* name);     // 0x10193844 (4 bytes)
    void Remove();                       // 0x10193814 (4 bytes)
    void Show(CThing* thing);            // 0x101937dc (4 bytes)
    void DeInitViewWindow();             // 0x10193564 (4 bytes)
    void DeInitStatusWindow();           // 0x10193528 (4 bytes)
    void UpdateViewWindow();             // 0x101934ec (4 bytes)
    void UpdateStatusWindow();           // 0x101934b0 (4 bytes)
    void InitViewWindow();               // 0x10193478 (4 bytes)
    void InitStatusWindow();             // 0x1019343c (4 bytes)
    void DeInitWindow();                 // 0x10193404 (4 bytes)
    void UpdateWindow();                 // 0x101933cc (4 bytes)
    void InitWindow();                   // 0x10193398 (4 bytes)
};
// Unit v51114: the table UIStatusPanel::UIStatusPanel stores at +0x20 (Mac 0x102a8644, name invented; the Mac's initial word before it is 0x102a863c).
// On the host it is a block of the arena (a Ptr32 cannot name a static), made on first use and kept.
uint32_t* UIStatusPanelTable();
enum TagId : uint32_t {};
enum TagButtonAction : int32_t;
// SamsUtilities template instances, all unconditional byte swaps (stwbrx, rlwimi) as the Mac's.
namespace SamsUtilities {
TagId USwap32(TagId value);                           // 0x101926a0 USwap32<TagId>
TagButtonAction USwap32(TagButtonAction value);       // 0x10192544 USwap32<TagButtonAction>
void USwapBlock16(char16_t* words, int32_t count);    // 0x10192644 USwapBlock16<wchar_t>: the platform's _USwapBlock16
void USwapInplace32(int32_t& word);                   // 0x10192600 USwapInplace32<long>
void USwapInplace16(int16_t& half);                   // 0x101925bc USwapInplace16<short>
}  // namespace SamsUtilities
// Unit v5910 (src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_ViewWindowCallback_DefaultStatusPanelMsg.cpp)
// The edit-name panel's handler at 0x10193fac (548 bytes); the Mac's name is TwoEditsMsg too (file-local there), the park-name one at 0x1019da6c keeps the plain name.
int32_t TwoEditsMsgStatus(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
void UIMain_SetObjectStateText(InterfaceWindow* window, int32_t state);   // 0x10181018 (148 bytes, stand-in)
extern char16_t gStatusPanelEmptyText[1];   // 0x1024de12: the text InitEditBox gives UIMain_SetTitleText (an empty string)
// Unit v5534: 0x101bbd78 (780 bytes), the ride status panel's window handler
int32_t RideStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// Unit v5300: UIStatusPanel::UIStatusPanel() (0x101929e8, 40 bytes, stand-in), a free function so that
// the globals of other units need no constructor.
void UIStatusPanel_Construct(UIStatusPanel* self);
// Unit v5300: the table RideStatusPanel::mOps points at (0x102ad5fc, original name unknown)
extern UIStatusPanelOps gRideStatusPanelOps;   // 0x102ad5fc
// Unit v5090: the Mac layout of a UIStatusPanel, which the members above read through raw offsets (the class itself has no members, its
// derived classes lay their own over them). The windows are Ptr32 (arena offsets). The Mac's table at +0x20 is a table of
// TVector pointers, called through __ptr_glue; on the host each entry is the arena offset of a block whose first 8 bytes are a
// void (*)(UIStatusPanel*) (the host's way of a virtual call on a Mac-layout object).
struct UIStatusPanelFields {
    int32_t mMode;                  // 0x00
    int32_t mF04;                   // 0x04
    int32_t mF08;                   // 0x08
    Ptr32<InterfaceWindow> mWindow; // 0x0c the panel's window
    Ptr32<InterfaceWindow> mF10;    // 0x10
    Ptr32<InterfaceWindow> mF14;    // 0x14
    Ptr32<InterfaceWindow> mF18;    // 0x18
    uint32_t mF1C;                  // 0x1c
    Ptr32<uint32_t> mVtable;        // 0x20
};
class CObject;
extern int32_t gStatusPanelListCount;       // 0x103947a0 (name invented): what StatusPanelIteratorCallback counts
extern uint8_t gStatusPanelClickStackDir;   // 0x102e47f0 (name invented)
extern CObject* gStatusPanelMoveObject;     // 0x102e47f4 (name invented)
int32_t StatusPanelIteratorCallback(CThing* thing);   // 0x10192a3c (untranslated)
int32_t delme(void* thing);                            // 0x10193084
void ShowVersionNumberTag();                           // 0x101928dc
extern char16_t gVersionNumberFormat[32];              // 0x1024dde8 (name invented): the wide format of the version tag, "Ve..." at start
int32_t ViewWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x10193bac (untranslated)
class StaffRoomStatusPanel : public UIStatusPanel {
public:
    void FillStaffRoomList(StaffType type, int32_t flag);   // 0x101c31d4
    void Remove();                                          // 0x101c4098
    void Show(CThing* thing);                               // 0x101c4014 (76 bytes, untranslated; unit v5071)
    void UpdateViewSprite(CStaff* staff);                   // 0x101c2b88
    uint8_t mF00[0xc];       // 0x00  not used by the unit
    InterfaceWindow* mWindow;   // 0x0c  the panel's window (a 32-bit pointer on the Mac)
    uint8_t mF18[0x24 - 0x18];  // (Mac 0x10 to 0x23)
    int32_t mScript;         // 0x24  the script handle of the staff view (0 none)
    int32_t mF28;            // 0x28  -1 after the script is killed
    int32_t mF2c;            // 0x2c  -1 after the script is killed
    // Unit v5291 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom.cpp)
    void StaffWentIntoStaffRoom(CThing* staff);   // 0x101c3a80 (240 bytes)
    void UpdateWindow();                          // 0x101c39b8 (140 bytes)
    void StaffLeftStaffRoom(CThing* staff);       // 0x101c3bbc (100 bytes)
    void UpdateStatusWindow();                    // 0x101c3054 (104 bytes)
    void DeInitViewWindow();                      // 0x101c2db4 (80 bytes)
    // Unit v5669 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_FillStaffRoomList.cpp)
    void InitStatusWindow();                      // 0x101c2e88 (328 bytes)
    void InitWindow();                            // 0x101c36f0 (592 bytes)
    // Unit v51051 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_InitViewWindow.cpp)
    void InitViewWindow();                        // 0x101c2d60 (24 bytes): +0x24 0, +0x28 and +0x2c -1
    void UpdateViewWindow();                      // 0x101c2e44 (4 bytes, nothing)
    void DeInitStatusWindow();                    // 0x101c3010 (4 bytes, nothing)
    void DeInitWindow();                          // 0x101c3978 (4 bytes, nothing)
};
// Unit v51051: the staff panel's free functions and the two words its static initialiser sets (names invented).
class CPeep;
int32_t _applytoall(CThing* thing);       // 0x101c25c0 (48 bytes): gShowStatusPanel.applytoallcallback(thing), answers 1
void __sinit_ui_staffPanel_cpp();         // 0x101c0cdc (28 bytes): gStaffPanelWord4c = 0x10000, gStaffPanelWord50 = 0
extern int32_t gStaffPanelWord4c;         // 0x10395a4c
extern int32_t gStaffPanelWord50;         // 0x10395a50
void RemoveStaffPanel();                  // 0x101c0c5c (40 bytes): RemoveSlideoutPanel(0)
void LocatorByeByeKid(CThing* thing);     // 0x101bf98c (56 bytes, src/gameplay_leaves.cpp): RemoveSlideoutPanel(1) when the staff panels show thing
void UI_SetSelectedPeep(CPeep* peep);     // 0x101947a8 (12 bytes, src/ui_allKidsPanel_select.cpp, unit v5L16)
// Unit v5669: globals it defines and the untranslated neighbours it passes to the list (weak stand-ins in its file).
extern int32_t gStaffRoomColumnColour;       // 0x102aed64 (name invented; 8 at start): MAKECOLUMNBUTTON's third argument
class InterfaceListControl2;
extern InterfaceListControl2* gStaffRoomList;   // 0x10395a94 (name invented): the list FillStaffRoomList is filling (the filler adds to it)
extern uint32_t gStaffRoomSpawnBlock[];      // 0x1029ff70 (name invented; data, only the address is used): ESP_SpawnScript's block
int32_t StaffRoomListFiller(CThing* thing);  // 0x101c30fc (untranslated): the iterator's function, adds a staff member to gStaffRoomList
InterfaceWindow* StaffRoomLCFormat();        // 0x101c3360 (untranslated): the list's row maker
extern StaffRoomStatusPanel gStaffRoomStatusPanel;   // 0x10395a9c (the TOC slot holds its address)
// Unit v5291: the Mac layout of a StaffRoomStatusPanel (the members above read it through raw offsets). The table at +0x20 holds
// Ptr32 words, each the arena offset of a block whose first 8 bytes are a uint64_t (*)(void* panel) (the host's virtual call).
struct StaffRoomPanelFields {
    int32_t mF00;                     // 0x00
    int32_t mF04;                     // 0x04
    int32_t mF08;                     // 0x08
    Ptr32<InterfaceWindow> mWindow;   // 0x0c the panel's window (holds the list control 0x1e7b)
    Ptr32<InterfaceWindow> mStatusWindow;   // 0x10 (holds the control 0x1e8a)
    Ptr32<InterfaceWindow> mF14;      // 0x14
    Ptr32<InterfaceWindow> mF18;      // 0x18
    Ptr32<InterfaceWindow> mTextWindow;     // 0x1c gets the text of table slot 0x3c
    Ptr32<uint32_t> mTable;           // 0x20
    int32_t mScript;                  // 0x24
    int32_t mF28;                     // 0x28
    int32_t mF2c;                     // 0x2c
};
struct IDXTYPEUniqueNameThingId;
template<class Tag, class T> struct tag_IDX_TYPE;
extern tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> gStaffRoomId;   // 0x10395a90 (name invented; a word on the Mac, the id in its high half)
extern CThing* gStaffRoomShowThing;                                     // 0x10395a88 (name invented): the thing Show was given
extern int16_t gStaffRoomScript[];                                      // 0x102ae9c8 (name invented): the panel's window script (first word 0, then 1)
extern uint32_t gLCFontIndex;                        // 0x102aed60 (name invented): the font table index LCPostFormat uses (7 at start)
extern int32_t gStaffRoomMsg406;                     // 0x102aed68 (name invented): the last b of message 0x406 (1 at start)
extern CThing* gStaffRoomPanelThing;                 // 0x103947a4 (name invented): the thing CreateAllItemsPanel is given
extern StaffType gStaffRoomStaffType;                // 0x10395a98 (name invented): the staff type CreateAllStaffPanel is given
// Unit v51039: UIMain_IndentClient 0x10180940 (156, stand-in): the window's position (less the animation), moved in by a and b on the
// left/right and top/bottom, becomes its client rectangle. The CoolRect is passed as raw storage (the host's is not the Mac's size).
// Adjust and RectDtor: weak definitions in src/Serialise_basic_std_list_std_basic_string_std_allocator_std_basi_4.cpp; the other three:
// platform/indent_dispatch.cpp (the launcher's). The test library fakes all five.
namespace indent_callee {
void RectCtor(void* rect);                                           // 0x101c7448
int32_t GetPosition(InterfaceWindow* window, void* rect, int16_t subtract);   // 0x101db884
void Adjust(void* rect, int16_t left, int16_t top, int16_t right, int16_t bottom);   // 0x101c7a78
void SetClientRect(InterfaceWindow* window, void* rect);             // 0x101db3fc (InterfaceWindow::SetClientRect, src/InterfaceWindow_z.cpp)
void RectDtor(void* rect, int16_t flag);                             // 0x101c74e0
}
void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b);   // 0x10180940
void CreateAllItemsPanel(UIObjType type, CThing* thing);                   // 0x101983c4 (untranslated)
void CreateAllStaffPanel(StaffType type, CThing* thing);                   // 0x10199630 (untranslated)
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x10193c78 (untranslated)
void LCPostFormat(InterfaceWindow* window);                                // 0x101c346c
int32_t StaffRoomPanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101c35a4
int32_t StaffRoomStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);        // 0x101c3c68
// Unit v5753 (src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_ViewWindowCallback.cpp)
int32_t StatusPanelTitleMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x10193e7c (240 bytes)
// Unit v5951 (src/LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_sinit_ui_staffRoomStatus_cpp.cpp)
void __sinit_ui_staffRoomStatus_cpp();   // 0x101c40f0 (124 bytes)
void __sinit_ui_showStatus_cpp();        // 0x101c2b10 (76 bytes)
extern int32_t gStaffRoomListFormat;     // 0x10395a8c (name invented): the row height StaffRoomLCFormat works out once (0 until then)
extern int32_t gStaffRoomValueA;         // 0x10395acc (name invented): 0x10000 after the static initialiser
extern int32_t gStaffRoomValueB;         // 0x10395ad0 (name invented): 0 after the static initialiser
extern uint32_t* gStaffRoomPanelTable;   // 0x102aed88 (name invented): the table the staff room panel's +0x20 holds (made at the first static init)
extern uint32_t* gShowStatusTable;       // 0x102ae980 (name invented): the table the show status panel's +0x20 holds (the same)
extern int16_t gStaffEditNameScript[2];  // 0x102af16c (name invented): the staff name panel's window script (first word 1)
class InterfaceFont;
class InterfaceDrawObject;
// Callees the host cannot call as the Mac does: weak definitions in src/..._sinit_ui_staffRoomStatus_cpp_callees.cpp, faked by the test library.
namespace staffroom_callee {
InterfaceWindow* NewRowWindow(int16_t bottom);    // operator new(0x138) and the window constructor 0x101d98d8 (no parent, 100 wide)
int16_t FontHeight(InterfaceFont* font);          // the virtual at vtable +0xc of the object at font +0x10 (through __ptr_glue)
InterfaceDrawObject* NewTitleText();              // operator new(0x19c), then 0x1018fbe8 (NULL)
InterfaceDrawObject* NewCursorText();             // operator new(0x19c), then 0x1018f1d0 (NULL)
void WindowSetText(InterfaceWindow* window, const char16_t* text);   // the virtual at slot 0x2c of the table at window +0x118
}  // namespace staffroom_callee

// ---- Unit v5010 (src/CreateRideListPanel.cpp): the ride list panel. Names invented; the addresses are the Mac's.
struct IDXTYPEUniqueNameObjectId;
template<class Tag, class T> struct tag_IDX_TYPE;
// 0x101b8434 (1936 bytes): the ride list (type kUIObjRide to kUIObjFeature, -1 for the type the panel showed last). `selected` is a 16-bit
// id passed in the HIGH half of r4. A panel that is already up is only refilled; a new one is built from the script, filled and announced.
void CreateRideListPanel(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selected);
void FillRideList(UIObjType type, int32_t refresh);                  // 0x101b5c14 (untranslated)
void SetLastPanel(int32_t type, int32_t panel);                      // 0x101a57fc (untranslated; the Mac's UIPanelType, UIPanels)
int32_t RidePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);        // 0x101b7538 (untranslated)
int32_t RideListLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101b7d94 (untranslated)
extern int32_t gRideListType;          // 0x10395994: the type the list showed last (the default for -1)
extern InterfaceWindow* gRideListPanel;   // 0x10395998: the panel, NULL when none (a 32-bit pointer on the Mac)
extern uint16_t gRideListNoneA;        // 0x102aceec: the selected id that means "none" when the panel is already up
extern uint16_t gRideListNoneB;        // 0x102aceee: the same when the panel is new
extern int32_t gRideListButtonFont;    // 0x102acedc: 8 at start; MAKECOLUMNBUTTON's third argument
extern int32_t gRideListSortColumn;    // 0x102acee4: 1 at start
extern int16_t gRideListScript[];      // 0x102aca9c: the panel's window script (the image's words, generated at configure time)
extern int32_t gRideListF90;           // 0x10395990: zeroed by a new panel
extern int16_t gRideListF8C;           // 0x1039598c: zeroed by a new panel
extern int16_t gRideListF8A;           // 0x1039598a: zeroed by a new panel
extern int16_t gRideListF88;           // 0x10395988: zeroed by a new panel
// Unit v51005 (src/ChangeInfoWindow_RidePanelMsg.cpp): two halfwords RidePanelMsg only reads (zero at start; names invented) and the
// panel's remover (0x101b8c28, 56 bytes, stand-in).
extern uint16_t gRideListPending;      // 0x102acee8: the id the timer last refreshed; the tick refreshes the view while f8c differs
extern uint16_t gRideListHelpShown;    // 0x102aceea: the id the help text shows; the tick updates it while f8a differs
void RemoveRideListPanel();            // 0x101b8c28 (src/ui_ridepanel_remove.cpp, unit v5L06): message 4 to gRideListPanel

// ---- Unit v5022 (src/ui_main_panel_UI_Congratulations.cpp): the congratulations and bankrupt windows and ui_main.cpp's static initialiser.
class CObject;
// 0x1018724c (436 bytes): leaves first person and the game window, closes the panel, makes the full-screen congratulations window (script
// gCongratsScript, animation gCongratsAnim, message function CongratsWindowMsg) and gives it the keyboard.
void UI_Congratulations(CObject* object);
// 0x10186f34 (428 bytes): the same for the bankrupt window (script gBankruptScript, animation gBankruptAnim, BankruptWindowMsg); it is remembered in gBankruptWindow.
void UI_Bankrupt(CObject* object);
void UI_BankruptAfterLoad();   // 0x1018710c
// 0x10186b9c (860 bytes): the congratulations window's message function. Message 0x15 keeps the UI clock in gCongratsTicks; the right button
// (0x10006, button 1, with the RMB cancel setting on) more than 2000 ticks later, or the escape key (0x1000b, 0x1b, no modifier), closes it
// (EndWinningSequence, the windows and key tables back, a dialog with string 0x1d7 for the easy game type). Everything else: the default handler.
int32_t CongratsWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
int32_t BankruptWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101868c0, untranslated
// 0x101877b8 (436 bytes): ui_main.cpp's static initialiser: the interface light, the cursor table and the four name strings are built and
// registered for destruction. (The vtable words it stores into gUIClockTick are that object's own construction; not repeated here.)
void __sinit_ui_main_cpp();
// Data of the image (names invented; the scripts are generated at configure time and the animations hold the image's frames).
extern int16_t gCongratsScript[];          // 0x102a772c
extern int16_t gBankruptScript[];          // 0x102a7756
extern UI_RectDelta gCongratsAnim[];       // 0x102a79d4
extern UI_RectDelta gBankruptAnim[];       // 0x102a7914
extern int32_t gCongratsTicks;             // 0x10380b78 (name invented): the UI clock at the congratulations window's message 0x15
extern int32_t gUIMainWordCCF8;            // 0x1038ccf8: 0x10000 after the static initialiser
extern int32_t gUIMainWordCCFC;            // 0x1038ccfc: 0 after the static initialiser
// The records __register_global_object keeps (12 bytes each on the Mac), and the stand-ins for the destructor words it is given.
extern uint8_t gUIMainNodeLight[12];       // 0x1038ccec
extern uint8_t gUIMainNodeCursors[12];     // 0x1038cc6c
extern uint8_t gUIMainNodeFrame[12];       // 0x10380c08
extern uint8_t gUIMainNodeSlider[12];      // 0x10380be8
extern uint8_t gUIMainNodePlain[12];       // 0x10380bc8
extern uint8_t gUIMainNodeButton[12];      // 0x10380ba8
void UIMain_DestroyLight(void* object);    // the light's destructor word 0x102657b8
void UIMain_DestroyCursorTable(void* object);   // the cursor table's destructor word 0x10268138
void UIMain_DestroyString(void* object);   // the string destructor glue 0x103bcd38

// ---- Unit v5060 (src/lobby_exit_InitOnlineUI.cpp; BankruptWindowMsg above is translated there)
// 0x101864b0, 332 bytes: the online lobby's interface: the light's colour and direction, linked into map list 0x20, the online panel,
// the first person window (gFirstPersonWindow), the help text on or off by the tooltips setting.
void InitOnlineUI();

// ---- Unit v5240 (src/ui_StatusPanel_SetButton.cpp): the status panel's button helpers.
struct IDXTYPEUniqueNameThingId;
struct IDXTYPEUniqueNameMapId;
// The Mac's TagButtonAction (an int in r3 / r4): 3 to 27 are the actions the two switches know, anything else does nothing.
enum TagButtonAction : int32_t {};
// 0x101904f8 (1316 bytes): gives `window` the draw object named by the action (strings after "mb_locate") and enables it, and its help text
// number (mHelpText, +0x8c); actions 3 to 27 only (19 sets nothing at all), any other removes the draw object and disables the window.
void SetButton(InterfaceWindow* window, TagButtonAction action);
// 0x10190a60 (588 bytes): runs the action: the panels, the zooms, the maintenance request of the thing `id` (gThingArray) and the challenge
// answers. 2 when a panel was made, 1 for the maintenance request, 0 otherwise. `extra` is the Mac's long (low 16 bits: the ride or map id).
int32_t DoButton(TagButtonAction action, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id, int32_t extra);
// 0x10181d34 (164 bytes, stand-in): zooms the camera to the map cell.
void UIMain_ZoomToMapId(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>& id);
// ---- Unit v5723 (src/ui_main_kbcapture_UIMain_ShowRidePreview_UIMain_GotoStatusPanelForThing_UIMain_ZoomToMapId.cpp)
uint32_t UIMain_GetShiftKeyState();   // 0x10182088 (168 bytes): bits 3 (shift), 0xc (option), 0x30 (command) from the keyboard modifiers
void UIMain_DestroyRidePreview();     // 0x101815d4 (68 bytes): destroys gRidePreview and clears it

// ---- Unit v5079 (src/OnlinePanelMsg.cpp): the online panel's message function. Names invented; the addresses are the Mac's.
// 0x101e62d0, 3,552 bytes: answers what the window's default handler answers.
int32_t OnlinePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
void UpdateAvatarPos();                      // 0x101e5e08 (824 bytes, untranslated)
extern uint16_t gOnlineNoCellId;             // 0x102b0832: the map cell id that means "none" (0)
extern uint16_t gOnlineRideUpgradeId;        // 0x102b0c88: the "upgrades what" id of a ride the panel can pick (0)
extern uint16_t gOnlineNoRideId;             // 0x102b0c8a: LLCR_OverRide's "no ride" answer (0)
extern CObject* gOnlineTargetRide;           // 0x10395e60: the ride under the pointer
extern int32_t gOnlineCursorState;           // 0x10395e64: 0 nothing, 1 key held, 2 ride, 3 first person target mode, 4 both
extern uint8_t gOnlineWindowKeyHeld;         // 0x10395e68
extern uint8_t gOnlinePanelKeyHeld;          // 0x10395e69
extern uint8_t gOnlineBlockToggle;           // 0x10395e79: read only
extern uint8_t gOnlinePanelShown;            // 0x10395e7a
extern InterfaceWindow* gOnlineWindowE8c;    // 0x10395e8c
extern InterfaceWindow* gOnlineWindowE90;    // 0x10395e90
extern char16_t gOnlineEmptyHelp[2];         // 0x1024ef54: the empty help text

// ---- Unit v5088 (src/CreateStaffCostsPanel.cpp). Names invented; the addresses are the Mac's.
extern InterfaceWindow* gStaffCostsPanel;    // 0x10395a20: the panel, NULL while it is not up
extern int32_t gStaffCostsDeposits;          // 0x10395a1c: the deposits history's newest value (CHistory 0x1fc90)
extern int32_t gStaffCostsWithdrawals;       // 0x10395a18: the withdrawals history's newest value (0x1f5a0)
extern int32_t gStaffCostsWord10;            // 0x10395a10: the history at 0x1f7f0's newest value
extern int32_t gStaffCostsWord14;            // 0x10395a14: word 18 less word 10
extern int16_t gStaffCostsScript[];         // 0x102adc20: the panel's window script (the image's words, generated at configure time)
int32_t StaffCostsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101be1f4 (untranslated)

// ---- Unit v5097 (src/DoScroll.cpp): the research panel's scroll handling, InitAll and ResearchPanelMsg.
void DoScroll(int32_t id, int32_t value);       // 0x101b42d0 (752 bytes)
extern int32_t gResearchScrollSound;            // 0x10395960 (name invented): the handle of the scroll sound (0 none)
void NameWindow(int32_t group);                 // 0x101b3da8 (stand-in)
void PercentComplete(int32_t group);            // 0x101b3398 (stand-in)
void ResearchSpeed(int32_t group);              // 0x101b381c (stand-in)
void RemoveResearchPanel();                     // 0x101b5ae8 (stand-in)

// ---- Unit v51043 (src/ui_Postcard_panel_CreateAddressBook_DeleteEntryOK.cpp)
void ResearchPanelCategoryComplete(int32_t category);   // 0x101b5b4c (the category is a ResearchCat)
InterfaceWindow* ResearchListLCFormat();                // 0x101b9ea8 (LCFormat of the research/ride list file)
extern int32_t gResearchRowHeight;                      // 0x103959a0 (name invented): the row height ResearchListLCFormat works out once

// ---- Unit v5102 (src/ChangeInfoWindow.cpp): the ride list's info window and help text.
// 0x101b7108 (1024 bytes): shows or hides the eight windows of the panel's info window (0x1ed) for the type: 0 shows all, 2 a mix, 1 and 3 hide all.
void ChangeInfoWindow(UIObjType type);
// 0x101b6d18 (924 bytes): the help line for the ride id under the mouse inside the panel's window 0x1f8.
void UpdateHelpText(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id);
void UI_GetMouseXY(int16_t& x, int16_t& y);     // 0x101ca900 (src/UI_Base_mouse.cpp, unit v5L06): gUIPointerX, gUIPointerY
extern int32_t gHelpTextState;                  // 0x102acee0 (name invented): -1 at start; 0..3 the ride's state, 4 a negative id

// ---- Unit v5241(src/ChangeInfoWindow_UpdateInfoWindow.cpp): the ride list panel's info window set-up and refresh.
// 0x101b6500 (1332 bytes): gives the info window's (0x1ed) seven text windows their text objects, font 6 and strings, and its three bars their gradients.
void InitInfoWindow();
// 0x101b6a5c (612 bytes): the ride id's count in the park and, for the list type 0 or 2, the bars and text of the info window.
void UpdateInfoWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id);

// ---- Unit v5184 (src/SpeedTimerFudge.cpp): PercentComplete and ResearchSpeed (declared above) and the light's state machine.
void SpeedTimerFudge(InterfaceWindow* window, int32_t speed);   // 0x101b35d4 (528 bytes)
extern uint16_t gResearchProjectNone;           // 0x102aca98 (name invented): the project id meaning none (initial word 0)

// ---- Unit v5200 (src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition.cpp): DrawCoasterTracks and StoreMapGridDetails.
struct TbRenderArea;
struct TbRect;
struct TbPoint;
struct tag_sBezierControl;
namespace bfl { namespace draw { struct CColour; } }
namespace tpw_intensify { template <int N> struct CTraits; }
// 0x102b2912 (initialised data, the halfword 0; name invented): the end marker of the thing id list StoreMapGridDetails walks. Defined in the unit's src file.
extern uint16_t gUI2DMapGridThingListEnd;
// 0x103970f0 (name invented): the map panel's colour table, 0x264 bytes; the CColours at +0x1f8, +0x21c and +0x240 are the coaster colours.
// Defined in the unit's src file (words, so that the CColours inside are aligned).
extern uint32_t gUI2DMapColourTable[0x264 / 4];
// 0x100458e8 (64 bytes, stand-in): the start control of the Nth coaster track; *control gets the track's first control (its point at +0x40).
tag_sBezierControl* GetNthCoasterTrackStart(uint32_t index, void** control);
// 0x1022bb44 (800 bytes, stand-in): the colour of a coaster track for a colour index (0 to 255) between three colours; answers through a hidden pointer.
bfl::draw::CColour UI2DMReturnScaleColour(int32_t index, const bfl::draw::CColour& a, const bfl::draw::CColour& b, const bfl::draw::CColour& c);
// The 3D line blends Antialiased<CTraits<N>> (the Mac's mangled N: 1 __int128, 2 __vec2x32float__, 3, 4; all stand-ins):
// 1 = 0x10006904, 2 = 0x10006394, 3 = 0x10005e30, 4 = 0x100058c0. They draw the line from `from` to `to` into the render area.
namespace tpw_antialias {
template <int N>
void Antialiased(const tpw_intensify::CTraits<N>& traits, const TbRenderArea& area, const TbRect& clip, const bfl::draw::CColour& colour,
                 const TbPoint& from, const TbPoint& to);
// Unit v5940: the instances for 3 and 4 bytes per pixel are defined in src/TPString_Antialiased_3.cpp (specialisations, not declared here).
}

// ---- Unit v5231 (src/CreateRideListPanel_LCHandler.cpp): the ride list's row functions (the LCFormat, LCPostFormat, LCEntryHandler and
// LCHandler of the file at 0x101b7d94; RideListLCHandler is declared above). Names invented; the addresses are the Mac's.
// 0x101b7f30 (432 bytes): message 0x403 colours the row's text by the ride (b is the ride id, negative for none).
int32_t RideListLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
InterfaceWindow* RideListLCFormat();                      // 0x101b811c (320 bytes): the row window (the height is worked out once)
void RideListLCPostFormat(InterfaceWindow* window);       // 0x101b827c (388 bytes): the draw objects and fonts of the row's windows
extern uint32_t gRideListFontIndex;                       // 0x102aced8 (name invented): the font table index of the rows (7 at start)
extern int32_t gRideListRowHeight;                        // 0x10395984 (name invented): the row height RideListLCFormat works out once (0 until then)

// ---- Unit v5279 (src/ContactsLCFormat.cpp): the online panel's contacts and messages lists (row window, draw objects, colours) and the
// contact list helpers. Names invented; the addresses are the Mac's.
int32_t ContactsLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101e7648 (224 bytes): message 0x403 colours the row by b's low three bits
int32_t MessagesLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101e776c (164 bytes): message 0x403 colours the row by b & 7
InterfaceWindow* ContactsLCFormat();                      // 0x101e7a58 (180 bytes): the row window (right 100, bottom from the font height)
void ContactsLCPostFormat(InterfaceWindow* window);       // 0x101e7b34 (168 bytes)
void _OnlineAddContact(char16_t* name, int32_t data);     // 0x101e8068 (152 bytes)
void _OnlineRemoveContact(char16_t* name);                // 0x101e812c (156 bytes)
void _OnlineUpdateContact(char16_t* name, int32_t data);  // 0x101e81f4 (184 bytes)
extern uint32_t gContactsFontIndex;        // 0x102b0c4c: the font table index of the contacts rows (7 at start)
extern uint8_t gMessagesLCColours[32];     // 0x102b0c58: 8 colours of red, green, blue, spare (the first word 0x00e20600)
extern uint32_t gContactsLCColours[4];     // 0x102b0c78: 4 colour words 0xRRGGBB00 (the first 0xffffff00)
extern int32_t gOnlineContactsBusy;        // 0x10395e7c: not 0 keeps the contact helpers from touching the list

// ---- Unit UpdateStaffCost (src/CreateStaffCostsPanel_UpdateStaffCost.cpp)
void UpdateStaffCost(int32_t id, int32_t value);   // 0x101be030 (412 bytes): the staff HQ's cost word for the row id, clamped at 0; a positive one sets the flag at 0x14
int32_t ShopStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101bdb8c (796 bytes)
void UIMain_TickRidePreview();     // 0x10181570 (src/ui_main_preview_tick.cpp, unit v5L06): TickRidePreview(gRidePreview) when there is one
void RemoveStaffCostsPanel();      // 0x101bf278 (stand-in)
// Unit v51121: the static initialisers of ui_ridepanel.cpp and ui_StaffCostsPanel.cpp, the two words of the latter (names invented) and the
// shop's _applytoall (0x101bda5c; the staff room's 0x101c25c0 owns the name _applytoall).
void __sinit_ui_ridepanel_cpp();          // 0x101b8c8c
void __sinit_ui_StaffCostsPanel_cpp();    // 0x101bf2dc
extern int32_t gStaffCostsF24;            // 0x10395a24: 0x10000 after the sinit
extern int32_t gStaffCostsF28;            // 0x10395a28: 0 after the sinit
int32_t _applytoall_shop(CThing* thing);  // 0x101bda5c: gShopStatusPanel.applytoallcallback(thing), answers 1
void CreateFinancePanel();         // 0x101a001c (stand-in)
extern int16_t gFinanceScript[];   // 0x102a9bec: the finance panel's window script (unit v51026, src/CreateFinancePanel_2.cpp)
int32_t _applytoall(CThing* thing);   // 0x101bda5c (not translated; ShopStatusMsg hands its TVector 0x10267e20 to UI_IterateThingList)

// ---- Unit v5363 (src/FillRideList.cpp): FillRideList (declared above) and the ride list panel's view window.
// 0x101b5f1c (1420 bytes): the picture (0x1eb) and the label (0x1ec) of the panel's view window (0x1ea) for the ride id (the Mac passes the
// address of the word; negative ids -1 and -2 are the two special rows, a positive one is a locked ride). A ride that is open gets the grid
// of its profile drawn into the picture and its type string as the label; otherwise the picture is black and the label is string 0x86, 0x87 or 0x89.
void UpdateViewWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id);
// 0x1008f880 (280 bytes, stand-in): the ride's profile: a grid of 16 rows of 16 words (a cell is 0 for empty, 1 to 4 for a kind of cell),
// then the row count at word 256 and the column count at word 257.
int32_t* LLCC_GetRideProfileInfo(void* ride);

// ---- Unit v5541 (src/OnlineParkFindPanelMsg.cpp): the online panel's builder and the park find panel's message function. Names invented.
template<class T> class TbPtrList;
class CFSOnlinePark;
int32_t OnlineParkFindPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101e93b0 (700 bytes)
void CreateOnlinePanel();                                    // 0x101e8f08 (916 bytes)
InterfaceWindow* MessagesLCFormat();                         // 0x101e7c18
void MessagesLCPostFormat(InterfaceWindow* window);          // 0x101e7cf4
int32_t OnlineEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);          // 0x101e71a8
int32_t OnlinePanelSubPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b); // 0x101e70ec
void ContactMenu(int32_t a);   // 0x101e8b98 (844 bytes, stand-in; unit v5658 only calls it)
int32_t ContactListHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);     // 0x101e7854
int32_t ButtonsMenuMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);         // 0x101e87b8
int32_t ButtonsUnderlayMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);     // 0x101e86c4
void FindParksPostGetPlayerParksCallbackFn(int32_t result, const void* context, TbPtrList<CFSOnlinePark>* list);   // 0x101e9324
void RemoveOnlineParkFindPanel();                            // 0x101ea468 (stand-in)
void ShowFindListSearching();                                // 0x101e9f28 (stand-in)
extern int16_t gOnlinePanelScript[2];        // 0x102b0834 (initial word 1; the rest is not kept here)
extern int16_t gOnlineUnderlayScriptA[2];    // 0x102b0a22 (initial word 1)
extern int16_t gOnlineUnderlayScriptB[2];    // 0x102b0b5e (initial word 1)
extern uint32_t gOnlineEditFontIndex;        // 0x102b0c50: the font table index of the panel's edit box (7 at start)
extern uint8_t gOnlineByte78;                // 0x10395e78: CreateOnlinePanel clears it
extern int32_t gOnlineWord88;                // 0x10395e88: CreateOnlinePanel clears it
extern char16_t gOnlineEditText[256];        // 0x10395e9c: the edit box's static text area
extern char16_t gFindPanelPlayerName[64];    // 0x103960a4: the player name the park find panel looks for
extern void (*gFindPanelCloseCallback)();    // 0x103960c4: called when the find panel closes
extern int32_t gFindPanelWord;               // 0x103960c8: cleared by message 0x14
// Unit v5660 (src/OnlineParkFindPanelMsg_ContactMenu.cpp). Names invented.
void FillBuddyList();            // 0x101e8608 (152 bytes): empties the contacts list and has the online control refill it
void ContactMenu(int32_t position);   // 0x101e8b98 (844 bytes): pops the buttons menu up at the mouse for the contact at `position`
void HideButtonsMenu();          // 0x101e8b18 (88 bytes, stand-in)

// ---- Unit v5913 (src/UpgradeStatusMsg_UpgradeStatusPanel_InitStatusWindow_2_CreateYearEndPanel.cpp). Names invented.
extern InterfaceWindow* gYearEndPanel;   // 0x10395b70: the year end panel, NULL while it is not up (a 32-bit pointer on the Mac)
extern int16_t gYearEndScript[];         // 0x102af524: the panel's window script, 478 words (the image's words, generated at configure time)
int32_t YearEndPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101c67b4 (TVector 0x10268078; unit v51007 translates it in src/ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_UpdateWindow_2.cpp)
// Unit v51007
void RemoveYearEndPanel();   // 0x101c73e4 (56 bytes, stand-in)
void __sinit_ui_upgradeStatus_cpp();   // 0x101c6738 (76 bytes): gUpgradeStatusPanel's constructor, table 0x102af4dc at +0x20, mode 0x3ec

// ---- Unit v5965 (src/CUI2DMap_..._UpdateAllDrawRelated_2.cpp): CUI2DMap::UpdateAllDrawRelated draws through the render context a CDraw holds
// (the object at CDraw+0, whose Mac vtable slots 0x10, 0x14, 0x24 and 0x28 it calls with the CDraw's view port (CDraw+0x30), its clip (CDraw+0x58)
// and the flag word 8 or 0xc). The host has no such dispatch yet, so mRenderContext is read as this interface (a native vtable); the test
// library installs a recording implementation. Slot 0x10/0x14: a line of `length` from (x, y) (0x10 is horizontal, 0x14 vertical); slots
// 0x24 and 0x28: a rectangle (left, top, right, bottom) in a colour (a 36 byte CColour).
struct CUI2DMapDrawOps {
    virtual void Slot10(const void* viewPort, const void* clip, int32_t flags, int32_t x, int32_t y, int32_t length, const void* colour) = 0;
    virtual void Slot14(const void* viewPort, const void* clip, int32_t flags, int32_t x, int32_t y, int32_t length, const void* colour) = 0;
    virtual void Slot24(const void* viewPort, const void* clip, int32_t flags, const int32_t* rect, const void* colour) = 0;
    virtual void Slot28(const void* viewPort, const void* clip, int32_t flags, const int32_t* rect, const void* colour) = 0;
};
// Host seam: the draw ops of CDraw's render context. On the Mac the context is one of Draw.cpp's four objects and the slots
// are its vtable's HorizontalLine (+0x10), VerticalLine (+0x14), RectangleOutline (+0x24) and RectangleFilled (+0x28); the
// host's contexts have no vtable, so src/host_ui2dmap_draw_ops.cpp answers an adapter onto CRenderClass8 .. 32 for them
// (any other context as it is). The unit test's library defines it as the identity.
CUI2DMapDrawOps* UI2DMapDrawOps(void* context);

// ---- Unit v5991 (src/ContactsLCFormat_OnlineReceiveString_2.cpp)
class COnlineControl;
extern int32_t gContactsMaxRows;   // 0x102b0c54: the rows the contacts list keeps (20 at start; name invented)
void FillContactList();            // 0x101e82dc (152 bytes, stand-in)
void OnlineReceiveString(char16_t* text, int32_t data);   // 0x101e7dd8 (612 bytes)
void _OnlineAddBuddy(char16_t* name, int32_t data);       // 0x101e839c
void _OnlineRemoveBuddy(char16_t* name);                  // 0x101e845c
void _OnlineUpdateBuddy(char16_t* name, int32_t data);    // 0x101e8524
// Callees the host cannot call as the Mac does: weak definitions in the unit's file, faked by the test library.
namespace contacts_callee {
int32_t FontCharWidth(InterfaceFont* font, char16_t character);   // the font's virtual at slot +0x14 of the table at +0x10 (via __ptr_glue)
void WindowSlot1c(InterfaceWindow* window, int32_t value);        // the window's virtual at slot +0x1c of the table at +0x118
COnlineControl* NewOnlineControl();                                // operator new(0xe8c), then the constructor
}  // namespace contacts_callee

// ---- Unit v5995 (src/ui_mainpanel2_msg_UI_MainPanelDateChange_2.cpp)
// 0x101a6234 (416 bytes): shows the date (day, month, year since 1900) in the toolbar's title window and sets the frame of the cash window's gauge.
void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year);
