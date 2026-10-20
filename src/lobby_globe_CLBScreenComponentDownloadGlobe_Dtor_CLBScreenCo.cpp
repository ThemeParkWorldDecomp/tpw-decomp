#include <new>
#include "lobby.h"
#include "tb_resource_display.h"

// 0x100af330: the destructor takes the online globe's lists apart itself (the Mac's compiler inlined the base destructor's body):
// every globe and every city flag is removed and deleted, then each list is deleted and cleared. QUIRK: a NULL list is read
// unconditionally by the loops. The base destructor of CLBScreenComponent follows (0x100b6c2c, the C++ chain), the delete flag's
// operator delete is the deleting form.
CLBScreenComponentDownloadGlobe::~CLBScreenComponentDownloadGlobe() {
    while (mGlobes->mCount != 0) {
        CLBGlobe* globe = mGlobes->RemoveHead();
        if (globe != nullptr) delete globe;
    }
    delete mGlobes;
    mGlobes = nullptr;
    while (mCityFlags->mCount != 0) {
        CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
        if (flag != nullptr) delete flag;
    }
    delete mCityFlags;
    mCityFlags = nullptr;
}

// 0x100ae63c: a visible screen (asked twice through slot 0x2c) is hidden: both lists emptied (the globes and flags deleted), the
// meshes' instances deleted, display flag 0x2000000 cleared, both sounds faded; the city info overlay goes either way.
void CLBScreenComponentDownloadGlobe::Hide() {
    if (IsVisible() != 0) {
        if (IsVisible() != 0) {
            mVisible = 0;
            RemoveWorldLobbyController();
            while (mGlobes->mCount != 0) {
                CLBGlobe* globe = mGlobes->RemoveHead();
                if (globe != nullptr) delete globe;
            }
            while (mCityFlags->mCount != 0) {
                CLBGlobeCityFlag* flag = mCityFlags->RemoveHead();
                if (flag != nullptr) delete flag;
            }
            gLobbyMeshes->DeleteMeshInstanceListEntries();
            gei_Display.mFlags &= ~0x02000000u;
            SOUND_FadeSoundEffect(mF74);
            SOUND_FadeSoundEffect(mF78);
            mF78 = 0;
            mF74 = 0;
        }
        RemoveOnlineCityInfoOverlay();
    }
}
