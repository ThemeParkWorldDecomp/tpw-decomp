// ui_mainpanel2.cpp (main app), unit v5183: LoadPanelMsg 0x101a2bdc, UI_HirePanelStateChange 0x101a2a14, SavePanelMsg 0x101a2df4.
#include "LoadPanelMsg.h"

#include "FS_LoadSave.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "StaffDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "World.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

// Original names unknown.
int32_t gHirePanelType;             // 0x10394920
InterfaceWindow* gHirePanelWindow;  // 0x10394924
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

static void HireEntry(InterfaceListControl2* list, CStaffDB* db, int slot) {
    CStaffInfo* staff = db->GetStaff(static_cast<uint32_t>(slot));
    char16_t name[40];
    LCWord item[2];
    staff->GetName(name, 0x28);
    item[0].mText = name;
    item[1].mInt = static_cast<int32_t>(staff->GetSalary());
    list->AddItem(item, slot, -1, 0);
}

void UI_HirePanelStateChange(int slot) {
    if (gHirePanelWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gHirePanelWindow->GetWindow(0x248e));
    int32_t position = list->FindData(slot);
    CStaffInfo* staff = reinterpret_cast<CStaffDB*>(gGameWorld)->GetStaff(static_cast<uint32_t>(slot));
    if (position > -1) {
        if (staff == nullptr) {
            list->RemoveItem(position);
            return;
        }
        list->RemoveItem(position);
        HireEntry(list, reinterpret_cast<CStaffDB*>(gGameWorld), slot);
        return;
    }
    if (staff == nullptr) return;
    if (gHirePanelType != static_cast<int32_t>(staff->mType)) return;
    HireEntry(list, reinterpret_cast<CStaffDB*>(gGameWorld), slot);
    list->UpdateDisplay();
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
