// ui_mainmenu.cpp (main app): menu callbacks. PARTIAL; untranslated.
#pragma once
#include <cstdint>

int32_t MainLobbyMenuConfirmQuit_OK(void* context);   // 0x101fb1f4
int32_t MainGameMenuConfirmQuit_OK(void* context);    // 0x101fab10


// Unit v5522 (src/game/ui/ui_mainmenu.cpp).
class InterfaceWindow;
class CFSOnlineSystem;
class CFSUserInfo;
class CLBScreenComponent;
struct OnlineUploadFile;

int32_t MainGameMenuConfirmPublish_OK(void* context);      // 0x101fabd8 (316): saves the park to the upload file and starts publishing
int32_t MainLobbyMenuConfirmLogout_OK(void* context);      // 0x101fb258 (128): the user info logs out, the login panel opens
// Their neighbours, untranslated (the dialogs hold their TVectors: 0x10268638, 0x10268630, 0x10268610).
int32_t MainGameMenuConfirmRestart_OK(void* context);      // 0x101fad4c
int32_t BankruptGameMenuConfirmRestart_OK(void* context);  // 0x101fadb8
void MainLobbyMenuPostOnlineLoginCallbackFn(int32_t error, const void* context);   // 0x101fb310
// Panels and helpers the menu callbacks call (untranslated here).
void CreateLoadPanel(int32_t mode);                        // 0x101a37ac
void CreateSavePanel(int32_t mode);                        // 0x101a3994
void CreateOptionsPanel();                                 // 0x101a9fb8
void UI_ByeByeBankrupt();                                  // 0x10187220
int32_t UIMenu_DefaultCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1018ab78
// Unit v5575
int32_t UIMenu_DefaultItemCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1018aa48

// Callees the host cannot call as the Mac does (src/ui_iterators_MainGameMenuConfirmPublish_OK_callees.cpp for the program, fakes in
// the test library).
namespace mainmenu_callee {
CFSOnlineSystem* NewOnlineSystem();                        // operator new(0x404) and CFSOnlineSystem::CFSOnlineSystem 0x1016f7d0
CFSUserInfo* NewUserInfo();                                // operator new(0xd4) and CFSUserInfo::CFSUserInfo 0x1017de54
// CFSOnlineSystem::ReturnUploadFilename 0x10175608 into `result` (a TbDynamicStringTemplate<wchar_t>, 0x40 bytes of room).
void ReturnUploadFilename(void* result, CFSOnlineSystem* system);
// UniStringToString 0x10005670 into the global (its word at 0x08 becomes the path of the file).
void UniStringToUploadPath(OnlineUploadFile* out, const void* wide);
void WideStringDtor(void* string, int16_t flag);           // 0x10238710
// The virtual calls on a lobby screen: slot 0x24, 0x28, 0x2c, and slot 0x34 of the publish globe (identity unknown).
// (void*: the screens are different classes; the callee takes the object as the Mac does, `this` unadjusted)
void ScreenShow(void* screen);
void ScreenHide(void* screen);
int32_t ScreenIsVisible(void* screen);
void PublishGlobeSlot34(void* screen);
}  // namespace mainmenu_callee

// Unit v5634 (src/game/ui/ui_optionsPanel.cpp): CreateOptionsPanel 0x101a9fb8 opens the options panel (gSetOnOffRoot, ui_main.h). The
// globals of its neighbourhood (ui_parkStatus.cpp on the Mac; names invented):
extern uint8_t gOptionsPanelGameRunning;    // 0x10394a08  0 when opening the panel paused the game (gGame.mF3C was 1), else 1
extern int32_t gOptionsPanelF0C;            // 0x10394a0c  cleared when the panel opens
extern int32_t gOptionsPanelRenderOptions;  // 0x10394a10  the map's render options from before the panel set 0x21
extern uint32_t gOptionsPanelFontIndex;     // 0x102ab668  the font table index of the panel's texts (6 at start)
// 0x102aade4: the panel's window script, 1090 words (the image's words, generated at configure time; src/game/ui/ui_parkStatus.cpp).
extern int16_t gOptionsPanelScript[];
// 0x101a9d94 (untranslated; its TVector is 0x10267c90): the panel's message function.
int32_t OptionsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// The options panel's own DoScroll and DoButton (static in the Mac's file; the research panel has another DoScroll(int, int),
// include/ui_main.h), untranslated.
namespace options_panel {
void DoScroll(int32_t id, int32_t value);   // 0x101a8fb8 (1824 bytes)
void DoButton(int32_t id, int32_t on);      // 0x101a9808 (1388 bytes); the Mac's answer in r3 is not read here
// Unit v5895 (src/game/ui/ui_parkStatus.cpp):
void StartScroll(int32_t id, int32_t value);   // 0x101a96f8 (236 bytes)
}  // namespace options_panel
// 0x1024e168: the panel's number formats (wide string data; the number format starts 5 characters in).
extern char16_t gOptionsPanelNumberFormats[];

// Unit v5969 (src/game/ui/ui_loadSavePanel.cpp): the list fills and the lists' attach function.
class InterfaceListControl2;
void FillLoadList(InterfaceListControl2* list);          // 0x101a33c0 (428)
void FillSaveList(InterfaceListControl2* list);          // 0x101a35a8 (456): the same with an unused wide string made and destroyed around it
void LoadSaveLCPostFormat(InterfaceWindow* window);      // 0x101a3258 (LCPostFormat of ui_mainpanel2.cpp's save/load list)
extern int16_t gLoadSaveScript[];      // 0x102aa330 (name invented): the panels' window script, 186 words (the image's words, generated at configure time)
extern char gLoadSaveHilight[8];       // 0x1024e0db: "hilight"
extern char16_t gLoadSaveFormat[0x40]; // 0x1024e0e4 (name invented): the row time format; at +0x24 the characters the name box refuses
namespace loadsave_callee {
void CursorPosCtor(void* mem);                           // 0x1018f1d0 InterfaceCursorPosTextObjectSprite(NULL)
void FileTimeToLocal(const void* in, void* out);         // glue 0x10239fe8 NS_MacDoze::FileTimeToLocalFileTime(const tagFILETIME*, tagFILETIME*)
}  // namespace loadsave_callee

// Unit v5978 (src/game/ui/ui_optionsPanel.cpp): the virtual calls on a window the host cannot make as the
// Mac does (the table at window +0x118): weak definitions in that file, faked by the test library.
namespace options_callee {
int32_t CallSetValue(InterfaceWindow* window, int32_t value);   // slot 0x24; the Mac reads the answer as a short
void CallSetText(InterfaceWindow* window, const char16_t* text);   // slot 0x2c
}  // namespace options_callee

// Unit v51056: __sinit_ui_mainmenu_cpp 0x101fc260 (60): builds a wide string global from a literal and registers its destructor.
void __sinit_ui_mainmenu_cpp();
extern uint8_t gMainMenuText[];                  // 0x10396a5c (a TbDynamicStringTemplate<wchar_t>, 0x14 bytes on the Mac, sizeof(...) on the host; name invented)
extern uint8_t gMainMenuTextNode[12];            // 0x10396a70 (the registration record)
extern const char16_t gMainMenuLiteral[];        // 0x1024f694 ("upload")
namespace mainmenu_callee {
void WideStringCtor(void* string, const char16_t* text);   // glue 0x10238800 TbDynamicStringTemplate<wchar_t>(const wchar_t*)
}

// Unit v51122: __sinit_ui_mainpanel2_cpp 0x101a8df4 (64): the same, for the string at 0x103949e8 built from the literal at 0x1024e12e + 0x2a.
void __sinit_ui_mainpanel2_cpp();
extern uint8_t gMainPanel2Text[];                // 0x103949e8 (a TbDynamicStringTemplate<wchar_t>, 0x14 bytes on the Mac, sizeof(...) on the host; name invented)
extern uint8_t gMainPanel2TextNode[12];          // 0x103949fc (the registration record)
extern const char16_t gMainPanel2Literal[];      // 0x1024e158 (string pool 0x1024e12e + 0x2a)
namespace mainpanel2_callee {
void WideStringCtor(void* string, const char16_t* text);   // glue 0x10238800
}
