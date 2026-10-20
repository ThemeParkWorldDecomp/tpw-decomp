// Theme-island ResetCurrentIsland 0x100b3760 (168 bytes) and UpdateSigns 0x100b38d4 (528); existing bodies, main, unit w43.
#include "lobby.h"
#include "FS_UserInfo.h"
#include "FS_MasterSave.h"
#include "Game.h"
#include "TPString.h"
#include "platform/macdoze.h"

void CLBScreenComponentThemeIslands::ResetCurrentIsland(int32_t a) {
    mCurrent = mIslands->mHead->mData;
    if (a == 0) {
        for (auto* node = mIslands->mHead; node != nullptr;) {
            CLBIsland* island = node->mData;
            node = node->mNext;
            if (NS_MacDoze::_stricmp(island->mName.mData, gLobbyIslandTheme.mData) == 0) mCurrent = island;
        }
    } else {
        gLobbyIslandTheme = "";
    }
}

void CLBScreenComponentThemeIslands::UpdateSigns() {
    for (auto* node = mIslands->mHead; node != nullptr;) {
        CLBIsland* island = node->mData;
        node = node->mNext;
        TbDynamicStringTemplate<char16_t> sign1;
        TbDynamicStringTemplate<char16_t> sign2;
        if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
        if (gUserInfo->mGameUser != -1) {
            CFSGlobalParkData* park;
            {
                const TbDynamicStringTemplate<char> name(island->mName.mData);
                if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
                park = gMasterSave->GetInfoOn(name);
            }
            char16_t textA[0x100];
            park->GetSignNameA(textA, 0x100);
            sign1 = textA;
            char16_t textB[0x100];
            park->GetSignNameB(textB, 0x100);
            sign2 = textB;
        } else {
            TbDynamicStringTemplate<char16_t> unicodeName;
            {
                const TbDynamicStringTemplate<char> name(island->mName.mData);
                StringToUniString(unicodeName, name);
            }
            gGame.GetLocalisedThemeGateNames(unicodeName, sign1, sign2);
        }
        island->mMeshA->SetRideSigns(sign1.mData, sign2.mData);
        island->mMeshB->SetRideSigns(sign1.mData, sign2.mData);
    }
}
