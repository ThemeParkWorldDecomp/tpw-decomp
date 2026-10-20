// ui_mainpanel2.cpp (main app), unit v5969: the load and save panels' openers CreateLoadPanel 0x101a37ac and CreateSavePanel 0x101a3994,
// the two list fills FillLoadList 0x101a33c0 and FillSaveList 0x101a35a8, and the lists' attach function LCPostFormat 0x101a3258 (here
// LoadSaveLCPostFormat: the name LCPostFormat exists at other addresses).
#include <dlfcn.h>
#include "FS_LoadSave.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ui_ErrorDisplay.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

// 0x102aa330: the load and save panels' window script (initial word 1; the rest is not kept here).
int16_t gLoadSaveScript[2] = {1, 0};
// 0x1024e0db: the string "hilight" (the selection bar's and the cursor's draw object).
char gLoadSaveHilight[8] = "hilight";
// 0x1024e0e4: the list rows' time format (its first characters, "%l", are what the binary shows; the rest is a guess), and at +0x24 the
// characters the name box does not accept (also a guess).
char16_t gLoadSaveFormat[0x40] = {'%', 'l', 's', ' ', '%', '0', '2', 'd', ':', '%', '0', '2', 'd', 0, 0, 0, 0, 0,
                                  '\\', '/', ':', '*', '?', '"', '<', '>', '|', 0};

// The real constructor, named by its symbol and weakly so the unit's test library (which fakes CursorPosCtor) links without it.
// (looked up at run time).
// Weak: the test library defines the recording fakes.
__attribute__((weak)) void loadsave_callee::CursorPosCtor(void* mem) {
    using Ctor = void (*)(void*, void*);
    static Ctor ctor = reinterpret_cast<Ctor>(dlsym(RTLD_DEFAULT, "_ZN34InterfaceCursorPosTextObjectSpriteC1EPv"));
    if (ctor != nullptr) ctor(mem, nullptr);
}
__attribute__((weak)) void loadsave_callee::FileTimeToLocal(const void* in, void* out) {
    // The Mac's glue converts to local time; no time zone is modelled here, so the stamp is copied.
    *static_cast<NS_MacDoze::tagFILETIME*>(out) = *static_cast<const NS_MacDoze::tagFILETIME*>(in);
}

namespace {

// The Mac stores the panel windows' pointers in these two int words.
int32_t WindowWord(InterfaceWindow* window) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(window)); }

// The rows of both lists: name, then "<date text> hh:mm". The Mac builds this inline in both fills.
void FillList(InterfaceListControl2* list, bool withString) {
    alignas(16) unsigned char stringStorage[0x40];
    if (withString) onlineparkfind_callee::WStrCtor(stringStorage);
    TFSLoadSaveSaveFileDetails* details = gLoadSave.GetFirstSaveFileDetails();
    TbTimeStamp stamp;
    stamp.mTicks = 0;
    int32_t index = 0;
    while (details != nullptr) {
        UI_LCAdd item[2];
        item[0].mRaw = 0;
        item[1].mRaw = 0;
        item[0].mText = details->mName.mData;
        NS_MacDoze::tagFILETIME local;
        loadsave_callee::FileTimeToLocal(&details->mStampHigh, &local);
        stamp = local;
        int32_t year, month, day, hour, minute;
        stamp.GetTime(&year, &month, &day, nullptr, &hour, &minute, nullptr, nullptr);
        char16_t text[0x20];
        TbIntegerVTag dayTag(0x10, day);
        TbIntegerVTag monthTag(0x11, month);
        TbIntegerVTag yearTag(0x12, year);
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(text, 0x20, 0x1c0, &dayTag, &monthTag, &yearTag);
        swprintf(gUIMainConvertBuffer, 0x3fff, gLoadSaveFormat, text, hour, minute);
        item[1].mText = gUIMainConvertBuffer;
        list->AddItem(item, index, -1, 1);
        details = gLoadSave.GetNextSaveFileDetails(details);
        ++index;
    }
    if (withString) onlineparkfind_callee::WStrDtor(stringStorage);
}

}  // namespace

// 0x101a33c0
void FillLoadList(InterfaceListControl2* list) { FillList(list, false); }

// 0x101a35a8: the same with an unused wide string made first and destroyed last.
void FillSaveList(InterfaceListControl2* list) { FillList(list, true); }

// 0x101a3258: the row gets a text draw object, font 7 and white; its first child the same, centred (2, 1).
void LoadSaveLCPostFormat(InterfaceWindow* window) {
    using namespace lender_callee;
    void* object = OperatorNew(0x19c);
    if (object != nullptr) SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    window->SetFont(GetFontTableEntry(7));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    InterfaceWindow* child = window->GetChild(0);
    object = OperatorNew(0x19c);
    if (object != nullptr) SpriteCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(object)));
    child->SetFont(GetFontTableEntry(7));
    child->SetFontColour(0xff, 0xff, 0xff, 0xff);
    child->SetFontJust(2, 1);
}

// 0x101a37ac
void CreateLoadPanel(int32_t mode) {
    (void)mode;
    gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
    gGame.mF3C = 0;
    UIMain_CheckPanel();
    InterfaceWindow* panel = Dialog(gLoadSaveScript, LoadPanelMsg, 1);
    gLoadPanelOpen = WindowWord(panel);
    if (panel != nullptr) {
        panel->CaptureKb(0);
        UIMain_SetTitleText(panel, 0x23bce1, const_cast<char16_t*>(UIMain_GetString(0xca)));
        panel->GetWindow(-1)->Show(0);
        panel->GetWindow(0x23bce2)->Show(0);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(panel->GetWindow(0x23bce0));
        if (list != nullptr) {
            list->SetColumnFormatsAll(SavePanelLCFormat, LoadSaveLCPostFormat);
            list->SetColumnType(0, static_cast<LCDataType>(0));
            list->SetColumnType(1, static_cast<LCDataType>(0));
            list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(gLoadSaveHilight));
            FillLoadList(list);
        }
    }
}

// 0x101a3994
void CreateSavePanel(int32_t mode) {
    (void)mode;
    gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
    gGame.mF3C = 0;
    UIMain_CheckPanel();
    InterfaceWindow* panel = Dialog(gLoadSaveScript, SavePanelMsg, 1);
    gSavePanelOpen = WindowWord(panel);
    if (panel != nullptr) {
        panel->CaptureKb(0);
        UIMain_SetTitleText(panel, 0x23bce1, const_cast<char16_t*>(UIMain_GetString(0xc9)));
        InterfaceWindow* box = panel->GetWindow(0x23bce2);
        void* object = lender_callee::OperatorNew(0x19c);
        if (object != nullptr) loadsave_callee::CursorPosCtor(object);
        box->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceCursorPosTextObjectSprite*>(object)));
        box->SetFont(GetFontTableEntry(7));
        static_cast<InterfaceEditBox*>(box)->SetStaticTextArea(gSavePanelName, 0xf);
        static_cast<InterfaceEditBox*>(box)->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(gLoadSaveHilight));
        hirelist_callee::WindowSetText(box, const_cast<char16_t*>(UIMain_GetString(0xce)));
        static_cast<InterfaceEditBox*>(box)->SetUnAcceptedChars(gLoadSaveFormat + 0x12);
        box->CaptureKb(0);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(panel->GetWindow(0x23bce0));
        if (list != nullptr) {
            list->SetColumnFormatsAll(SavePanelLCFormat, LoadSaveLCPostFormat);
            list->SetColumnType(0, static_cast<LCDataType>(0));
            list->SetColumnType(1, static_cast<LCDataType>(0));
            list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(gLoadSaveHilight));
            FillSaveList(list);
        }
        UIMain_DisableAllKeyTables();
    }
}
