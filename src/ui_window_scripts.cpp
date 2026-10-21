// Window scripts that InterfaceWindow::Build reads and that no translated file defined (unit v5L42): the launcher had all-zero
// stand-ins for them, so opening the panel failed the lpNew assertion in ProcessLevel. A file of their own because each building
// function's unit test defines its array with its fakes; the words are the image's, generated at configure time
// (devtools/gen_game_tables.py, WindowScripts.inc).
#include "FS_Online.h"
#include "MainWindowCallback.h"
#include "WindowScripts.inc"
#include "lobby.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

int16_t gAllShowsScript[] = {TPW_WINDOW_SCRIPT_102A8B42};          // 0x102a8b42, CreateAllShowsPanel 0x10196ed0
int16_t gOptionsPanelScript[] = {TPW_WINDOW_SCRIPT_102AADE4};      // 0x102aade4, CreateOptionsPanel 0x101a9fb8
int16_t gProgBarScript[] = {TPW_WINDOW_SCRIPT_102AFE24};           // 0x102afe24, CreateUIProgressBar 0x101dffd8
int16_t gCityInfoOverlayScript[] = {TPW_WINDOW_SCRIPT_102B0288};   // 0x102b0288, CreateOnlineCityInfoOverlay 0x101e20f0
int16_t gOutboxPanelData[] = {TPW_WINDOW_SCRIPT_102B05C4};         // 0x102b05c4, CreateOnlineOutboxPanel 0x101e4f28
int16_t gOnlineParkInfoScript[] = {TPW_WINDOW_SCRIPT_102B0E58};    // 0x102b0e58, CreateOnlineParkInfoPanel 0x101eabb4
