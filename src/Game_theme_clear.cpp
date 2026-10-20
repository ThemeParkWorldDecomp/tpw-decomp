// CGame::ClearThemeResources (main app, SimTheme Park 2.04b3, Game.cpp; unit w81).
#include "Game.h"

#include "TPGlobals.h"
#include "TPResource.h"

// 0x10149334 (680 bytes). Takes the theme's resources out of the game's resource manager: the current theme's own resource (by the
// theme's name), then thirteen resources the game always has, by the names TPGlobals keeps ("SharedTex", "Small_SharedTex", "Rides",
// "Features", "Sideshow", "Upgrades", "Shops", "LocalAdvisor", "Terrain", "Dynamic", "Hoarding", "MiscMesh", "Queue"), in that order:
// FindResource, and when it answers 1, UnregisterResource of what it found. The Mac works on gGame (0x1037f134, through the TOC), not on
// `this`, for the theme's name and the manager; the manager pointer is read again before every call (the first read is the load-with-update
// of the address gGame + 0x34, the others plain loads through it), so a call that replaces it is honoured.
void CGame::ClearThemeResources() {
    CTPBaseResource* resource;
    const TbDynamicStringTemplate<char>& theme = gGame.GetCurrentThemeName();   // (before the manager is read)
    if (gGame.mResourceManager->FindResource(theme, &resource) == 1) {
        gGame.mResourceManager->UnregisterResource(resource);
    }
    const TbDynamicStringTemplate<char>* const names[] = {
        &gTPGlobals.mF4614, &gTPGlobals.mF4628, &gTPGlobals.mF4650, &gTPGlobals.mF4740, &gTPGlobals.mF472C,
        &gTPGlobals.mF4704, &gTPGlobals.mF4718, &gTPGlobals.mF47CC, &gTPGlobals.mF46C8, &gTPGlobals.mF47E0,
        &gTPGlobals.mF47F4, &gTPGlobals.mF4808, &gTPGlobals.mF481C,
    };
    for (const TbDynamicStringTemplate<char>* name : names) {
        if (gGame.mResourceManager->FindResource(*name, &resource) == 1) {
            gGame.mResourceManager->UnregisterResource(resource);
        }
    }
}
