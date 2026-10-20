// RideDB.cpp, main app: LoadTheme 0x10154ea8 (480), ScanDirectory 0x10155c00 (964).
#include "TPEngine.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "Game.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include <new>

int32_t CRideDatabase::LoadTheme(const char*) {
    CTPBaseResource* resource = nullptr;
    DeleteCustomEffects();
    for (int32_t i = 0; i < mRideCount; ++i) {
        mRides[i]->DeallocStrings();
        delete mRides[i];
        mRides[i] = nullptr;
    }
    mRideCount = 0;
    mCachedId = 0;
    mCachedRide = nullptr;
    const TbDynamicStringTemplate<char>* names[] = {
        &gTPGlobals.mF4650, &gTPGlobals.mF4740, &gTPGlobals.mF4704,
        &gTPGlobals.mF4718, &gTPGlobals.mF472C
    };
    for (const auto* name : names) {
        if (gGame.mResourceManager->FindResource(*name, &resource) == 1 &&
            ScanDirectory(*static_cast<CTPItemListResource*>(resource)) == 0) return 0;
    }
    return 1;
}

int32_t CRideDatabase::ScanDirectory(CTPItemListResource& list) {
    if (list.mStorages == nullptr) return 0;
    bfl::FileSystem::CFileInfo info;
    bfl::FileSystem::CFile file;
    TbDynamicStringTemplate<char> unused;
    POSITION position = list.mStorages->GetHeadPosition();
    while (position != nullptr) {
        auto* storage = list.mStorages->GetNext(position);
        if (!storage->FindFirst(gTPGlobals.mAllFiles.mData, info).Succeeded()) continue;
        do {
            if (FS_IsSubDirectory(info) || FS_IsWadFile(info)) {
                TbDynamicStringTemplate<char> full;
                full = list.mPath;
                full += info.mName;
                auto* item = new (std::nothrow) CTPItemResource(info, full);
                if (item == nullptr) return 0;
                if (item->GetSamFile(file) == 1) {
                    file.Close();
                    if (TheGameType().mType == kGameTypeEasy) {
                        if (item->GetEasySamFile(file) == 1) {
                            file.Close();
                            if (list.FindItem(item->mName) == 0) {
                                list.RegisterItem(*item);
                                if (LoadItem(list, *storage, *item) == 0) return 0;
                            }
                            // The easy-game duplicate is left allocated by the Mac.
                        } else {
                            delete item;
                        }
                    } else {
                        if (list.FindItem(item->mName) == 0) {
                            list.RegisterItem(*item);
                            if (LoadItem(list, *storage, *item) == 0) return 0;
                        } else {
                            delete item;
                        }
                    }
                } else {
                    file.Close();
                    delete item;
                }
            }
        } while (storage->FindNext(info).Succeeded());
    }
    return 1;
}
