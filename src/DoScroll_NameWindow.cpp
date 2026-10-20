// NameWindow 0x101b3da8 (564 bytes): the research panel's text for one category's current project.
#include <cstdint>
#include "AdvisorScoring.h"
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"
#include "TPEngine.h"
#include "Text.h"
#include "TbITable.h"
#include "TbVTag.h"

// TankButton 0x101b4224 (120 bytes): step the category's project (flag: back), then redraw its name and percentage.
void TankButton(int32_t which, InterfaceWindow* window, int32_t flag) {
    CResearchLab* lab = gGameWorld->GetResearchLab();
    if (flag != 0)
        lab->PreviousProject(static_cast<ResearchCat>(which));
    else
        lab->NextProject(static_cast<ResearchCat>(which));
    NameWindow(which);
    PercentComplete(which);
}

void NameWindow(int32_t group) {
    CResearchLab* lab = gGameWorld->GetResearchLab();
    ResearchCat category = static_cast<ResearchCat>(group);
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> project = lab->GetCurrentProject(category);
    InterfaceWindow* window;
    int32_t id;
    switch (group) {
        case 0: id = 0xd2d01; break;
        case 1: id = 0xd2d08; break;
        case 2: id = 0xd2d09; break;
        case 3: id = 0xd2d0a; break;
        case 4: id = 0xd2d07; break;
        default: id = 0; break;
    }
    if (id != 0)
        window = gResearchPanel->GetWindow(id)->GetWindow(0xd2d05);
    else
        window = nullptr;
    if (gRideDatabase.GetRideInfo(project) != nullptr) {
        char16_t name[0x42];
        char16_t text[0x80];
        gRideDatabase.GetTypeName(project.mIndex, name, 0x42);
        int32_t cost = lab->GetEstimatedCostForCurrentProject(category);
        TbIntegerVTag costTag(13, cost);
        TbUniStringVTag nameTag(14, name);
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(text, 0x7f, 0x1be, &nameTag, &costTag);
        window->SetClientDrawText(text);
    } else {
        window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0xb9)));
        ResearchSpeed(group);
    }
}
