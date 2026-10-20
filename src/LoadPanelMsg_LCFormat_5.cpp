// Unit v5702: the save panel's row maker LCFormat (0x101a3154) and the save dialog's OK handler SaveCallback_OK (0x101a2d74).
#include "LoadPanelMsg.h"
#include "FS_LoadSave.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "StaffDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

#include <new>

int32_t gSavePanelListFormat;   // 0x10394928 (name invented): the row height SavePanelLCFormat works out once (0 until then)

InterfaceWindow* SavePanelLCFormat() {
    if (gSavePanelListFormat == 0) {
        // The Mac calls a virtual through the font entry's table here (__ptr_glue); the height getter stands in.
        InterfaceFont* font = GetFontTableEntry(7);
        int32_t height = static_cast<int16_t>(font->GetHeight());
        gSavePanelListFormat = height * 0x600 / gUIRealScreenHeight + 6;
    }
    void* memory = ::operator new(sizeof(InterfaceWindow));   // 0x138 bytes on the Mac; the host window is larger
    InterfaceWindow* window = new (memory) InterfaceWindow(nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(gSavePanelListFormat));
    window->CreateObject(1, 1, 1, 0, 0, 100, static_cast<int16_t>(gSavePanelListFormat));
    return window;
}

int32_t SaveCallback_OK(void* unused) {
    TbDynamicStringTemplate<char16_t> name(gSavePanelName);
    gLoadSave.Save(name);
    RemoveSavePanel();
    return 1;
}
