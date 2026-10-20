// CGame::ExitLevel (main app, SimTheme Park 2.04b3, Game.cpp; unit w75).
#include "CMessageCentre.h"
#include "Game.h"
#include "SkyObjects.h"
#include "StaffDB.h"
#include "TPEngine.h"
#include "World.h"
#include "tb_resource_display.h"
#include "tb_sky.h"

// 0x101496ec (292 bytes). The end of a level, in the Mac's order:
//  1. the world, when there is one (gGameWorld is read again before each step): CWorld::ExitLevel, CWorld::Free, then the block is
//     destroyed as a CStaffDB (a direct call of 0x10128f68 with the flag -1, which does not free the block) and given back with the
//     plain operator delete; gGameWorld is cleared even when the world's own functions replaced it;
//  2. the ride database is cleared and the sky closed;
//  3. with display flag 0x400000 and a sky object set, the sky objects are destroyed and deleted and the member cleared;
//  4. the message centre, when there is one, is destroyed (__destroy_arr over its 29 headers of 0x10 bytes, last first, the
//     destructor 0x101522e0 with the flag -1) and its block freed; the global is cleared even when it was NULL;
//  5. the theme's resources are cleared.
// The Mac answers 1 in r3 (the callers ignore it).
void CGame::ExitLevel() {
    if (gGameWorld != nullptr) {
        gGameWorld->ExitLevel();
        gGameWorld->Free();
        CWorld* world = gGameWorld;
        if (world != nullptr) {
            reinterpret_cast<CStaffDB*>(world)->~CStaffDB();
            ::operator delete(world);
        }
        gGameWorld = nullptr;
    }
    gRideDatabase.ClearDatabase();
    gei_Sky.Close();
    if ((gei_Display.mFlags & 0x400000) != 0 && mSkyObjects != nullptr) {
        mSkyObjects->DestroySkyObjects();
        delete mSkyObjects;
        mSkyObjects = nullptr;
    }
    // (CMessageCentre holds the 29 headers by value: its implicit destructor is __destroy_arr, last element first.)
    if (gMessageCentre != nullptr) delete gMessageCentre;
    gMessageCentre = nullptr;
    ClearThemeResources();
}
