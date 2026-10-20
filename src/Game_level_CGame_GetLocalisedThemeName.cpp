// CGame::GetLocalisedThemeName (0x1014a570): the name of a theme in the player's language.
#include "Game.h"
#include "TPString.h"
#include "TbITable.h"
#include "Text.h"
#include "ui_main.h"

void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char16_t>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[22]);   // gText + 0x5a4
    TbDynamicStringTemplate<char> unused;   // built and destroyed, never used
    if (TPUniStrEq(theme, gGameThemeFantasy)) {
        out = UIMain_ConvertMulti(table->GetConstString(2));
    } else if (TPUniStrEq(theme, gGameThemeHallow)) {
        out = UIMain_ConvertMulti(table->GetConstString(1));
    } else if (TPUniStrEq(theme, gGameThemeJungle)) {
        out = UIMain_ConvertMulti(table->GetConstString(0));
    } else if (TPUniStrEq(theme, gGameThemeSpace)) {
        out = UIMain_ConvertMulti(table->GetConstString(3));
    }
}
