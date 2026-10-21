// ui_loadSavePanel.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "Game.h"
#include "History.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "LoadPanelMsg.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "StaffDB.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "UI_pump.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <new>

// ui_mainpanel2.cpp (main app), unit v5183: LoadPanelMsg 0x101a2bdc, UI_HirePanelStateChange 0x101a2a14, SavePanelMsg 0x101a2df4.

char16_t gSavePanelName[34];        // 0x1039492c
int32_t gSavePanelOpen;             // 0x10394970
int32_t gLoadPanelOpen;             // 0x10394974

int32_t LoadPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == -2) {
            RemoveLoadPanel();
            return 0;
        }
    } else if (message < 0x100) {
        if (message == 0x14) {
            gLoadPanelOpen = 0;
            UIMain_DecideKbCapture();
            gGame.mF3C = 1;
            gGame.Unpause();
        } else if (message < 0x14 && message == 5) {
            RemoveLoadPanel();
            return 1;
        }
    } else if (message == 0x400) {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t count;
            list->GetData(b, &count);
            TFSLoadSaveSaveFileDetails* details = gLoadSave.GetFirstSaveFileDetails();
            for (int32_t i = 0; i < count; ++i) details = gLoadSave.GetNextSaveFileDetails(details);
            gLoadSave.Load(details->mName);   // the Mac passes the entry itself (its name is the first member)
            RemoveLoadPanel();
            return 0;
        }
    }
    return DefaultDialogCallback(window, message, a, b);
}

// Unit v5702: the save panel's row maker LCFormat (0x101a3154) and the save dialog's OK handler SaveCallback_OK (0x101a2d74).

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

int32_t SavePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x400) {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t count;
            list->GetData(b, &count);
            TFSLoadSaveSaveFileDetails* details = gLoadSave.GetFirstSaveFileDetails();
            for (; count != 0; --count) details = gLoadSave.GetNextSaveFileDetails(details);
            char16_t* text = details->mName.mData;
            window->GetWindow(0x23bce2)->SetText(text);   // the virtual at slot 0x2c of the table at +0x118
        }
    } else if (message < 0x400) {
        if (message == 0x14) {
            gSavePanelOpen = 0;
            UIMain_EnableAllKeyTables();
            UIMain_DecideKbCapture();
            gGame.mF3C = 1;
            gGame.Unpause();
        } else if (message < 0x14) {
            if (message == 5) {
                RemoveSavePanel();
                return 1;
            }
        } else if (message == 0x100) {
            if (a == -1) {
                bool found = false;
                for (TFSLoadSaveSaveFileDetails* d = gLoadSave.GetFirstSaveFileDetails(); d != nullptr;
                     d = gLoadSave.GetNextSaveFileDetails(d)) {
                    if (wcscmp(d->mName.mData, gSavePanelName) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    TbDynamicStringTemplate<char16_t> name(gSavePanelName);
                    gLoadSave.Save(name);
                    RemoveSavePanel();
                    return 0;
                }
                TbUniStringVTag tag(2, gSavePanelName);
                char16_t text[128];
                static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(text, 0x7f, 0xcd, &tag);
                dlgboxparams params;
                params.mText = text;
                params.mRed = 0xff;
                params.mGreen = 0xff;
                params.mBlue = 0xff;
                params.mValue8 = 4;
                UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(1), SaveCallback_OK,
                             static_cast<dlgbutton>(0), nullptr, true);
            } else if (a == -2) {
                RemoveSavePanel();
                return 0;
            }
        }
    } else if (message != 0x803) {
        if (message < 0x803) {
            if (message > 0x801) UI_PostMessage(window, 0x100, -1, 0);
        } else if (message < 0x805) {
            UI_PostMessage(window, 0x100, -2, 0);
        }
    }
    return DefaultDialogCallback(window, message, a, b);
}

// ui_mainpanel2.cpp (main app), unit v5969: the load and save panels' openers CreateLoadPanel 0x101a37ac and CreateSavePanel 0x101a3994,
// the two list fills FillLoadList 0x101a33c0 and FillSaveList 0x101a35a8, and the lists' attach function LCPostFormat 0x101a3258 (here
// LoadSaveLCPostFormat: the name LCPostFormat exists at other addresses).

int16_t gLoadSaveScript[] = {TPW_WINDOW_SCRIPT_102AA330};   // 0x102aa330: the load and save panels' window script (the image's words, generated)
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

namespace {

// The window a 32-bit global holds (the panel globals typed as int in the headers).
InterfaceWindow* WindowOf(int32_t word) {
    Ptr32<InterfaceWindow> pointer;
    pointer.mOffset = static_cast<uint32_t>(word);
    return pointer.get();
}

}  // namespace

// 0x101a3934 (56 bytes): the global is typed as an int in the header; it holds the window.
void RemoveLoadPanel() { UI_SendMessage(WindowOf(gLoadPanelOpen), 4, 0, 0); }

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

// 0x101a3be4 (56 bytes): the same for the save panel.
void RemoveSavePanel() { UI_SendMessage(WindowOf(gSavePanelOpen), 4, 0, 0); }
