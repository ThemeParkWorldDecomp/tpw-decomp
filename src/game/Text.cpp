// Text.cpp: name proven by __sinit_Text_cpp in the main application.

#include "Debug.h"
#include "ExitErr.h"
#include "FS_Registry.h"
#include "platform/samsutils.h"
#include "ResolutionBalance.h"
#include "TbITable.h"
#include "TbResource.h"
#include "Text.h"
#include "TPSettings.h"
#include <cstdio>

// Text.cpp: CText methods at 0x1015be6c..0x1015ea58 and __sinit_Text_cpp at 0x1015ea80.
// CSkyObjects and CSaveLoadHeader::Serialise belong to other files and are left out.
// External ltms calls remain imports.

// __sinit_Text_cpp (0x1015ea80): strings, then CText. The native runtime supplies
// the global destructor registration performed explicitly by CodeWarrior.
TbDynamicStringTemplate<char> gTextLanguages[16] = {
    TbDynamicStringTemplate<char>("English"), TbDynamicStringTemplate<char>("French"),
    TbDynamicStringTemplate<char>("German"), TbDynamicStringTemplate<char>("Spanish"),
    TbDynamicStringTemplate<char>("Swedish"), TbDynamicStringTemplate<char>("Italian"),
    TbDynamicStringTemplate<char>("Dutch"), TbDynamicStringTemplate<char>("Polish"),
    TbDynamicStringTemplate<char>("Danish"), TbDynamicStringTemplate<char>("Japanese"),
    TbDynamicStringTemplate<char>("TraditionalChinese"), TbDynamicStringTemplate<char>("SimplifiedChinese"),
    TbDynamicStringTemplate<char>("Thai"), TbDynamicStringTemplate<char>("Korean"),
    TbDynamicStringTemplate<char>("Hebrew"), TbDynamicStringTemplate<char>("American")
};
TbDynamicStringTemplate<char> gTextTitle("Theme Park World");
TbDynamicStringTemplate<char> gTextLanguage("Language");
CText gText; // 0x1037ff18

namespace {
struct TableLoad { uint32_t id; const char* member; const char* table; };
constexpr TableLoad kTables[] = {
    {1020, "mShortcutKeys", "KEYBOARD::TableID"},
    {1000, "mErrorMsgs", "ERRORMSG::TableID"},
    {1001, "mIngredients", "INGREDIENT::TableID"},
    {1002, "mTagSystem", "TAG_SYSTEM::TableID"},
    {1006, "mUIText", "UITEXT::TableID"},
    {1007, "mUIHelpText", "UIHELPTEXT::TableID"},
    {1003, "mUIThoughts", "THOUGHTS::TableID"},
    {1004, "mUIKidStates", "KIDSTATES::TableID"},
    {1005, "mUIStaffStates", "STAFFSTATES::TableID"},
    {1011, "mHandymanNames", "HANDYMAN_NAMES::TableID"},
    {1012, "mMechanicNames", "MECHANIC_NAMES::TableID"},
    {1013, "mEntertainerNames", "ENTERTAINER_NAMES::TableID"},
    {1014, "mGuardNames", "GUARD_NAMES::TableID"},
    {1015, "mResearcherNames", "RESEARCHER_NAMES::TableID"},
    {1016, "mFemaleNames", "FEMALE_NAMES::TableID"},
    {1019, "mStaffTypes", "STAFF_TYPES::TableID"},
    {1018, "mLenderNames", "LOANNAMES::TableID"},
    {1008, "mChatCommands", "CHAT_COMMANDS::TableID"},
    {1010, "mObjectNames", "OBJECT_NAMES::TableID"},
    {1017, "mObjectTypeNames", "ITEMTYPES::TableID"},
    {1009, "mThemeNames", "THEMENAMES::TableID"}
};
constexpr uint32_t kFonts[4][13] = {
    {1045,1039,1040,1048,1023,1036,1021,1023,1021,1021,1034,1023,1052},
    {1046,1041,1042,1049,1024,1037,1024,1026,1024,1024,1034,1024,1052},
    {1047,1043,1044,1050,1031,1038,1026,1028,1024,1026,1034,1026,1052},
    {1047,1043,1044,1051,1032,1038,1028,1030,1032,1028,1034,1028,1052}
};
void LoadError(const char* text) {
    gExitError.SetError(static_cast<CExitError::ErrorType>(3), 0, text);
}
}

int32_t CText::Init() {
    constexpr const char* path = "data:Language:";
    if (!mResources.Open(path).Succeeded()) {
        TRACE("Error: Unable to open text resource at %s\n", path);
        LoadError(path);
        return 0;
    }
    if (!mResources.SetLanguage(gLanguageName).Succeeded()) {
        TRACE("Error: Unable to set language to %s\n", gLanguageName);
        LoadError(gLanguageName);
        return 0;
    }
    auto* toMB = static_cast<TbIUnicodeToMB*>(mResourcePointers[1]);
    const auto toMBResult = mResources.GetResource(&toMB, 2);
    mResourcePointers[1] = toMB;
    if (!toMBResult.Succeeded()) {
        TRACE("Unable to get Unicode to MB resource");
        return -1;
    }
    auto* toUnicode = static_cast<TbIMBToUnicode*>(mResourcePointers[0]);
    const auto toUnicodeResult = mResources.GetResource(&toUnicode, 1);
    mResourcePointers[0] = toUnicode;
    if (!toUnicodeResult.Succeeded()) {
        TRACE("Unable to get MB to Unicode resource ");
        return -1;
    }
    // Originally unrolled, one load/failure path per table and font; member/table names are from its messages.
    for (uint32_t i = 0; i < 21; ++i) {
        auto* table = static_cast<TbITable*>(mResourcePointers[i + 2]);
        const auto result = mResources.GetResource(&table, kTables[i].id);
        mResourcePointers[i + 2] = table;
        if (!result.Succeeded()) {
            char message[256];
            std::sprintf(message, "Unable to load string table ( %s, %s )", kTables[i].member, kTables[i].table);
            SamsUtilities::UDebug(message);
            LoadError(kTables[i].table);
            return 0;
        }
    }
    int32_t group;
    const int32_t override = gResolutionBalance.mValues[1].mInteger;
    if (override > 0) group = override == 1 ? 0 : override == 2 ? 1 : override == 3 ? 2 : 3;
    else {
        const int32_t saved = static_cast<int32_t>(gPlayerSettings.mCurrent.mResolution);
        group = saved == 0 ? 0 : saved == 1 ? 1 : saved == 2 ? 2 : 3;
    }
    // The original also has a separate load and failure path for each font.
    for (int32_t i = 0; i < 13; ++i) {
        if (!mResources.GetResource(&mFonts[group * 13 + i].mFont, kFonts[group][i]).Succeeded()) {
            LoadError("Font");
            TRACE(" Error: Unable to load font\n");
            return 0;
        }
    }
    mRenderParams.UseAlphaWriteMode(); // the result is ignored
    mInitialised = 1;
    return 1;
}

void CText::SetRez(int32_t width) {
    mResolutionGroup = width <= 512 ? 0 : width <= 640 ? 1 : width <= 800 ? 2 : 3;
}
void CText::UnInit() {
    mInitialised = 0;
    for (int32_t i = 0; i < 13; ++i) {
        for (int32_t group = 0; group < 4; ++group) {
            auto*& font = mFonts[group * 13 + i].mFont;
            if (font) { font->Release(); font = nullptr; }
        }
    }
    constexpr int32_t order[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,16,19,20,21,22};
    for (const auto i : order) {
        auto*& resource = mResourcePointers[i];
        if (resource) { static_cast<TbResource*>(resource)->Release(); resource = nullptr; }
    }
    mResources.Close();
}
char* CText::GetLanguageName() { return gLanguageName; }
void CText::InitThemeSpecific() {}
void CText::DeInitThemeSpecific() {}

uint32_t CText::GetLanguageId() const { return mLanguageId; }
void CText::SetLanguageId(uint32_t id) { mLanguageId = id; }
bool CText::IsHebrew() const { return mLanguageId == 0x40d; }
