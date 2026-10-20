// Game.cpp (main app 0x10149ba8..0x1014a44c): the setters of the current theme. The themes are the folder names in
// gGame.mThemes (FS_ScanThemes fills them); the current one is an index into that list and a copy of its name. Every setter ends
// in CFSGlobalMasterSave::ChangeCurrentTheme, so the master save always follows the game's theme.
#include "FS_MasterSave.h"
#include "Game.h"
#include "platform/macdoze.h"

namespace {

// The master save is made on first use; every setter carries this sequence inline. (A failed allocation would leave
// gMasterSave NULL, as the Mac's operator new answers NULL; nothing here tests for it.)
CFSGlobalMasterSave* TheMasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}

}  // namespace

// Makes `theme` the current theme's index (the name follows only when the index changes) and tells the master save.
void CGame::SetCurrentTheme(int32_t theme) {
    if (mCurrentTheme != theme) {
        mCurrentTheme = theme;
        SetCurrentThemeName();
    }
    CFSGlobalMasterSave* save = TheMasterSave();
    save->ChangeCurrentTheme(GetCurrentThemeName());
}

// Finds the theme called `name` (ignoring case). Every theme is made current on the way, in the master save too (it is told each
// time), and the comparison is made on a COPY of the current name. A name that is not found puts the original theme back
// (and tells the master save once more).
void CGame::SetCurrentTheme(const TbDynamicStringTemplate<char>& name) {
    const int32_t original = mCurrentTheme;
    bool found = false;
    for (uint32_t index = 0; index < static_cast<uint32_t>(mThemes.mCount); ++index) {
        if (mCurrentTheme != static_cast<int32_t>(index)) {
            mCurrentTheme = static_cast<int32_t>(index);
            SetCurrentThemeName();
        }
        CFSGlobalMasterSave* save = TheMasterSave();
        save->ChangeCurrentTheme(GetCurrentThemeName());
        int32_t difference;
        {
            TbDynamicStringTemplate<char> copy(GetCurrentThemeName());
            difference = NS_MacDoze::_stricmp(copy.mData, name.mData);
        }
        if (difference == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (mCurrentTheme != original) {
            mCurrentTheme = original;
            SetCurrentThemeName();
        }
        CFSGlobalMasterSave* save = TheMasterSave();
        save->ChangeCurrentTheme(GetCurrentThemeName());
    }
}

// Copies the name of theme number mCurrentTheme into mCurrentThemeName (the string's virtual assignment, slot 0x1c). The walk
// has no end test: an index past the list, or any index of an empty list, reads through a NULL node (a negative index takes the
// first name).
void CGame::SetCurrentThemeName() {
    auto* node = mThemes.mHead;
    for (int32_t i = 0; i < mCurrentTheme; ++i) node = node->mNext;
    mCurrentThemeName = *node->mData;
}

// The old theme numbers of the settings file (CURRENTTheme) to the folder order: 3 is the first folder, 1 the third, 2 the second,
// 4 the fourth, anything else (0, negatives, 5 and more) the third. The Mac has the name copy and the master save inline.
void ConverttoCurrentTheme(int32_t theme) {
    int32_t index;
    switch (theme) {
    case 3: index = 0; break;
    case 1: index = 2; break;
    case 2: index = 1; break;
    case 4: index = 3; break;
    default: index = 2; break;
    }
    if (gGame.mCurrentTheme != index) {
        gGame.mCurrentTheme = index;
        gGame.SetCurrentThemeName();
    }
    CFSGlobalMasterSave* save = TheMasterSave();
    save->ChangeCurrentTheme(gGame.mCurrentThemeName);
}
