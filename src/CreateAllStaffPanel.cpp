// Unit v5019: the all-staff panel (ui_allStaffPanel.cpp), CreateAllStaffPanel 0x10199630 (1972 bytes).
#include "MainWindowCallback.h"

#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

// ---- globals (no other file names them; names invented)
int32_t gStaffColumnFont = 8;          // 0x102a9084 the font number of the column buttons
int32_t gStaffSort = 1;                // 0x102a9088 the sort column of the staff list
InterfaceWindow* gStaffPanel = nullptr;   // 0x103947f0 the open all-staff panel
int32_t gStaffType = 0;                // 0x103947ec the kind of staff the panel lists

// 0x10199630 (1972 bytes): builds the all-staff panel from its script, sets up the list (columns, sort, selection bar, the kind of
// staff, the current entry), the kind check buttons, the texts and the happiness bars, starts the refresh timer and says the advice.
// QUIRK: the model for the happiness average (r30) is only set for the kinds 0 to 4; for another kind the original passes whatever
// its caller left in r30 (here 0).
void CreateAllStaffPanel(StaffType type, CThing* thing) {
    UIMain_CheckPanel();
    SetLastPanel(2, 4);
    InterfaceWindow* built = gGameWindow->Build(gAllStaffScript, AllStaffPanelMsg);
    if (built == nullptr) return;
    gStaffPanel = built;
    UIMain_SetPanel(gStaffPanel);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gStaffPanel->GetWindow(0x321));
    if (list != nullptr) {
        list->SetColumnFormatsAll(AllStaffLCFormat, AllStaffLCPostFormat);
        list->SetColumnType(0, kLCText);
        list->SetColumnType(1, kLCText);
        list->SetColumnType(2, kLCNumber);
        list->SetColumnType(3, kLCNumber);
        list->SetColumnType(4, kLCNumber);
        list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        list->SetCallbackFunction(AllStaffPanelLCHandler);
        list->SetSort(gStaffSort, false);
        InterfaceWindow* button = list->GetWindow(0x10);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x65)), gStaffColumnFont, 0xff, 0xef, 0, 0x77);
        button = list->GetWindow(0x11);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x66)), gStaffColumnFont, 0xff, 0xef, 0, 0x78);
        button = list->GetWindow(0x12);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x67)), gStaffColumnFont, 0xff, 0xef, 0, 0x79);
        button = list->GetWindow(0x13);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x68)), gStaffColumnFont, 0xff, 0xef, 0, 0x7a);
        button = list->GetWindow(0x14);
        MAKECOLUMNBUTTON(button, const_cast<char16_t*>(UIMain_GetString(0x69)), gStaffColumnFont, 0xff, 0xef, 0, 0x7b);
        int32_t kind = static_cast<int32_t>(type);
        if (kind == -1) kind = gStaffType;
        gStaffType = kind;
        FillAllStaffList(static_cast<StaffType>(gStaffType), 0);
        if (thing != nullptr) {
            int32_t position = list->FindData(static_cast<int32_t>(reinterpret_cast<intptr_t>(thing)));
            if (position >= 0) list->SetCurrent(position);
        }
        InterfaceCheckButtons* checks = static_cast<InterfaceCheckButtons*>(list->GetWindow(0x322));
        if (checks != nullptr) {
            int32_t now = gStaffType;
            if (now == 2) {
                checks->SetCurrent(0x325);
            } else if (now < 2) {
                if (now == 0) {
                    checks->SetCurrent(0x323);
                } else if (now >= 0) {
                    checks->SetCurrent(0x326);
                }
            } else if (now == 4) {
                checks->SetCurrent(0x324);
            } else if (now < 4) {
                checks->SetCurrent(0x327);
            }
        }
    }
    InterfaceWindow* box = gStaffPanel->GetWindow(0x329);
    InterfaceWindow* sub = box->GetWindow(0x32a);
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    sub->SetClientDrawObject(sprite);
    sub->SetFont(GetFontTableEntry(6));
    sub->SetClientDrawText(nullptr);
    sub = box->GetWindow(0x32d);
    UIMain_IndentClient(sub, 0, 8);
    InterfaceDrawEngineHGradient* gradient = new InterfaceDrawEngineHGradient(const_cast<char*>("happygrad.wct"));
    sub->SetClientDrawObject(gradient);
    sub = box->GetWindow(0x32b);
    sprite = new InterfaceTextObjectSprite(nullptr);
    sub->SetClientDrawObject(sprite);
    sub->SetFont(GetFontTableEntry(6));
    sub->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x6a)));
    sub = box->GetWindow(0x32c);
    UIMain_IndentClient(sub, 0, 8);
    gradient = new InterfaceDrawEngineHGradient(const_cast<char*>("happygrad.wct"));
    sub->SetClientDrawObject(gradient);
    float level = gGameWorld->GetParkAnalyser()->GetStaffHappinessLevel(0);
    float scaled = 1024.0f * level;
    float percent = scaled / 100.0f;
    sub->SetValue(ppc_fctiwz(static_cast<double>(percent)));

    int32_t now = gStaffType;
    box = gStaffPanel->GetWindow(0x329);
    sub = box->GetWindow(0x32a);
    if (now == 2) {
        sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6e)));
    } else if (now < 2) {
        if (now == 0) {
            sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6b)));
        } else if (now >= 0) {
            sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6c)));
        }
    } else if (now == 4) {
        sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6f)));
    } else if (now < 4) {
        sub->SetText(const_cast<char16_t*>(UIMain_GetString(0x6d)));
    }

    now = gStaffType;
    box = gStaffPanel->GetWindow(0x329);
    sub = box->GetWindow(0x32d);
    int32_t model = 0;
    if (now == 2) {
        model = 6;
    } else if (now < 2) {
        if (now == 0) {
            model = 5;
        } else if (now >= 0) {
            model = 4;
        }
    } else if (now == 4) {
        model = 8;
    } else if (now < 4) {
        model = 7;
    }
    int32_t average = gGameWorld->GetParkAnalyser()->AverageHappinessOfStaffType(static_cast<CThing::ThingModel>(model));
    // the average times 1024, divided by 100 by the multiply-high trick (0x51eb851f)
    int32_t shifted = static_cast<int32_t>(static_cast<uint32_t>(average) << 10);
    int32_t quotient = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * shifted) >> 32) >> 5;
    quotient += static_cast<int32_t>(static_cast<uint32_t>(quotient) >> 31);
    sub->SetValue(quotient);
    if (gStaffPanel->setAnimTimer(0x80083, 2000) != 0) gStaffPanel->startAnimTimer(0x80083);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11d));
}
