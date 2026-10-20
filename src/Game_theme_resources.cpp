// Game.cpp range of the main app (SimTheme Park 2.04b3): InitThemeResources (0x10148c3c, 1,708 bytes), which registers the
// resources of the theme a park is built in (src/Game.cpp has InitResources, the ones the game always has).
#include "Game.h"

#include "FS_System.h"
#include "TPGlobals.h"
#include "TPResource.h"

// Fourteen resources, each a name and a folder. The folder is the park's path (`path`, handed in by the caller) for the first
// one; for the others it is the path, a directory separator and one folder word of gTPGlobals. Every resource is given its
// local storages (the folder and path.wad) and registered with the game's resource manager (this object's, not gGame's;
// the first resource is named after the CURRENT THEME of gGame, whichever object this is called on). The shared texture
// resources go through the manager's second RegisterResource, which keeps them in the manager's shared texture slot.
void CGame::InitThemeResources(const TbDynamicStringTemplate<char>& path) {
    const TPGlobals& g = gTPGlobals;
    TbDynamicStringTemplate<char> folder;
    folder = path;

    // The theme's own textures: the folder is the path as it is (no separator is added).
    CTPBaseResource* resource = new CTPTextureResource(gGame.GetCurrentThemeName(), folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4B78;
    CTPSharedTextureResource* shared = new CTPSharedTextureResource(g.mF4614, folder);
    shared->AddLocalStorage(folder);
    mResourceManager->RegisterResource(shared);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4B8C;
    shared = new CTPSharedTextureResource(g.mF4628, folder);
    shared->AddLocalStorage(folder);
    mResourceManager->RegisterResource(shared);

    // The ride database's lists: rides, features, sideshows, upgrades and shops.
    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4BB4;
    resource = new CTPItemListResource(g.mF4650, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4BF0;
    resource = new CTPItemListResource(g.mF4740, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4BC8;
    resource = new CTPItemListResource(g.mF472C, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4C04;
    resource = new CTPItemListResource(g.mF4704, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4BDC;
    resource = new CTPItemListResource(g.mF4718, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4B28;
    resource = new CTPAdvisorResource(g.mF47CC, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF49AC;
    resource = new CTPTerrainResource(g.mF46C8, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF49D4;
    resource = new CTPTextureResource(g.mF47E0, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF49FC;
    resource = new CTPTextureResource(g.mF47F4, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4A24;
    resource = new CTPTextureResource(g.mF4808, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);

    folder = path;
    FS_AddDirectorySeparator(folder);
    folder += g.mF4A4C;
    resource = new CTPTextureResource(g.mF481C, folder);
    resource->AddLocalStorage(folder);
    mResourceManager->RegisterResource(resource);
}
