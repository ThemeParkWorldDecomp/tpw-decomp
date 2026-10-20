// ui_StatusPanel_SaveTags.cpp (main): AddATag 0x10191ef8 (720 B), SaveTags 0x1019231c (520 B), LoadTags 0x10192704
// (440 B), the status panel's tags, translated apart from ui_StatusPanel.cpp so that the unit libraries compiling that
// file need none of these callees.
#include <new>
#include "ui_main.h"

// Unit v5001: AddATag 0x10191ef8 (720 B), SaveTags 0x1019231c (520 B), LoadTags 0x10192704 (440 B).
#include <cstring>
#include "LbFile.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "platform/msl.h"

int32_t gMaxTags = 10;                 // 0x102a8464
int32_t gTagCount;                     // 0x1038cd74
InterfaceWindow* gTagFirstWindow;      // 0x103949d0

namespace {
// The file's record of one tag, 0x21c bytes (the file is little-endian: the Mac swaps every field with SamsUtilities::USwap*,
// which are no-ops on this host).
struct TagFileRecord {
    uint32_t mId;          // 0x000
    char16_t mText[256];   // 0x004
    int32_t mF204;         // 0x204
    int16_t mF208;         // 0x208
    int32_t mF20C;         // 0x20c
    int32_t mF210;         // 0x210
    uint16_t mF214;        // 0x214
    int32_t mF218;         // 0x218
};
static_assert(sizeof(TagFileRecord) == 0x21c, "tag record");
int32_t Low32(void* p) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(p)); }
void* Extend(int32_t v) { return reinterpret_cast<void*>(static_cast<intptr_t>(v)); }
}  // namespace

InterfaceWindow* AddATag(CTagParams& params) {
    InterfaceWindow* result = nullptr;
    if (gGameWindow == nullptr) return nullptr;
    if (!OverrideTag(params) || gTagCount >= gMaxTags || gAerialWindow == nullptr) return result;
    int16_t width;   // never read back
    gAerialWindow->GetDims(&width, nullptr, 1);
    InterfaceWindow* window = nullptr;
    int16_t frame = params.mFrame;
    bool found = false;
    for (int32_t i = 0; i < 10 && !found; ++i) {
        if (gStaticTags[i].mActive == 0) {
            found = true;
            gStaticTags[i].mActive = 1;
            window = gStaticTags[i].mWindow;
            window->SetFrame(frame);
        }
    }
    if (window == nullptr) return result;
    if (gTagFirstWindow == nullptr) gTagFirstWindow = window;
    char16_t* dest = static_cast<char16_t*>(window->getExtraParam(2));
    if (params.mText != nullptr && wcslen(params.mText) != 0 && dest != nullptr) {
        wcsncpy(dest, params.mText, 0xff);
        dest[0xff] = 0;
    }
    if (params.mTime > 0) {
        window->setAnimTimer(0x80080, params.mTime);
        window->startAnimTimer(0x80080);
        window->mF134 = 1;
        window->setAnimTimer(0x80081, params.mTime - 5000);
        window->startAnimTimer(0x80081);
    }
    window->setExtraParam(1, Extend(static_cast<int32_t>(params.mId)));
    window->setExtraParam(3, Extend(params.mF1C));
    window->setExtraParam(5, Extend(params.mF14));
    window->setExtraParam(6, Extend(params.mF18));
    window->setExtraParam(4, Extend(params.mFrame));
    window->setExtraParam(8, Extend(params.mF20));
    window->setExtraParam(7, Extend(params.mTime));
    window->setExtraParam(9, Extend(-1));
    window->setExtraParam(10, nullptr);
    UI_PostMessage(gAerialWindow, 0x100001, Low32(window), 0);
    ++gTagCount;
    SOUND_SendEvent(0, gSoundBanks[3], 0x1e, 0, 0, 0);
    return window;
}

void SaveTags(void* file) {
    int32_t count = 0;
    for (int32_t i = 0; i < 10; ++i) {
        if (gStaticTags[i].mActive != 0) ++count;
    }
    uint32_t transferred = 0;
    LbFile_Write(file, &count, 4, &transferred);
    for (int32_t i = 0; i < 10; ++i) {
        sStaticTag& tag = gStaticTags[i];
        if (tag.mActive == 0) continue;
        InterfaceWindow* window = tag.mWindow;
        TagFileRecord record;
        std::memset(&record, 0, sizeof record);
        record.mId = static_cast<uint32_t>(Low32(window->getExtraParam(1)));
        record.mF204 = Low32(window->getExtraParam(7));
        record.mF208 = static_cast<int16_t>(Low32(window->getExtraParam(4)));
        record.mF20C = Low32(window->getExtraParam(5));
        record.mF210 = Low32(window->getExtraParam(6));
        record.mF214 = static_cast<uint16_t>(Low32(window->getExtraParam(3)));
        record.mF218 = Low32(window->getExtraParam(8));
        wcscpy(record.mText, reinterpret_cast<char16_t*>(tag.mPayload));
        LbFile_Write(file, &record, 0x21c, &transferred);
    }
}

void LoadTags(void* file) {
    CTagParams params;
    params.mId = 0x17f;
    params.mF4 = 0x17f;
    params.mText = nullptr;
    params.mTime = 0;
    params.mFrame = 0;
    params.mF14 = 0;
    params.mF18 = 0;
    params.mF1C = 0;
    params.mF20 = 0;
    params.mShow = 0;
    int32_t count = 0;
    uint32_t transferred = 0;
    LbFile_Read(file, &count, 4, &transferred);
    while (count != 0) {
        TagFileRecord record;
        LbFile_Read(file, &record, 0x21c, &transferred);
        params.mId = record.mId;
        params.mText = record.mText;
        params.mTime = record.mF204;
        params.mFrame = record.mF208;
        params.mF14 = record.mF20C;
        params.mF18 = record.mF210;
        params.mF1C = record.mF214;
        params.mF20 = record.mF218;
        if (params.mShow == 1) {   // never true: the byte is set to 0 above and nothing writes it
            InterfaceWindow* added = AddATag(params);
            if (added != nullptr) ShowTagPanel(added);
        } else {
            AddATag(params);
        }
        --count;
    }
}
