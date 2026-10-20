// RideDB.cpp, main app: LoadItem 0x1015600c (2724 bytes).
#include "TPEngine.h"
#include "CoasterBalance.h"
#include "ExitErr.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "Debug.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPString.h"
#include "platform/msl.h"
#include <cstdio>
#include <cstring>

void LoadCoasterType(uint32_t type, char* folder, CCoasterBalance& balance); // 0x1003ab7c

int32_t CRideDatabase::LoadItem(const CTPItemListResource& list, bfl::FileSystem::CFileStorage& storage, CTPItemResource& item) {
    TbDynamicStringTemplate<char> folder;
    bfl::FileSystem::CFile file;
    folder = item.mPath;
    bool same;
    {
        TbDynamicStringTemplate<char> suffix(folder.Right(gTPGlobals.mWadFolderPattern.Length()));
        same = TPStrEq(suffix, gTPGlobals.mWadFolderPattern) == true;
    }
    if (same) {
        TbDynamicStringTemplate<char> left(folder.Left(folder.Length() - gTPGlobals.mWadFolderPattern.Length()));
        folder = left;
        FS_AddDirectorySeparator(folder);
    }
    if (mRideCount == 0x14a) {
        GAME_ASSERTMSG(0, "Too many rides loaded");
        return 0;
    }
    RideInfo* allocated = new RideInfo;
    allocated->mBalance.Init();
    allocated->mEntryX = 0;
    allocated->mEntryY = 0;
    allocated->mExitX = 0;
    allocated->mExitY = 0;
    allocated->mMapOffsetX = 0;
    allocated->mMapOffsetY = 0;
    for (int32_t i = 0; i < 4; ++i) allocated->mStrings[i] = nullptr;
    std::memset(allocated->mEmitters, 0, sizeof allocated->mEmitters);
    allocated->mSoundCategory = 0;
    mRides[mRideCount] = allocated;
    RideInfo* ride = mRides[mRideCount];
    ++mRideCount;
    std::strcpy(ride->mFolder, folder.mData);
    swprintf(ride->mItemName, 0x40, u"%s", item.mName.mData);
    char label[1024];
    auto makeLabel = [&](const char* prefix) {
        const char* name = item.mName.mData;
        const char* root = item.mStorage.GetRootPath();
        std::sprintf(label, "%s:%s%s", root, prefix, name);
    };
    auto overlay = [&]() {
        if (ride->mBalance.Load(label, file, CBalanceTable::kLoadOverlay) == 0) {
            gExitError.SetError(CExitError::kType3, 0, label);
            return false;
        }
        file.Close();
        return true;
    };
    if (list.GetSamFile(storage, file) != 0) {
        makeLabel("");
        if (ride->mBalance.Load(label, file, CBalanceTable::kLoadFresh) == 0) return 0;
        file.Close();
        if (TheGameType().mType == kGameTypeOnline && list.GetOnlineSamFile(storage, file) != 0) {
            makeLabel("");
            if (!overlay()) return 0;
        }
    }
    makeLabel("");
    if (item.GetSamFile(file) != 0 && !overlay()) return 0;
    if (TheGameType().mType == kGameTypeOnline) {
        makeLabel(gTPGlobals.mOnlinePrefix.mData);
        if (item.GetOnlineSamFile(file) != 0 && !overlay()) return 0;
    } else if (TheGameType().mType == kGameTypeEasy) {
        makeLabel(gTPGlobals.mF427C.mData);
        if (item.GetEasySamFile(file) != 0 && !overlay()) return 0;
    }
    if (item.HasSounds() != 0) ride->LoadSounds(item.mStorage);
    if (item.HasParticles() != 0) ride->LoadParticles(item.mStorage);
    ProcessShape(&ride);
    if (item.GetTextFile(file) != 0) {
        ride->SetIsCustomRide();
        ride->LoadStrings(file);
    }
    file.Close();
    const uint16_t id = static_cast<uint16_t>(ride->Value(kRideSlot_Id).mInteger);
    RideInfo* found = nullptr;
    if (id == mCachedId) {
        found = mCachedRide;
    } else {
        for (int32_t i = 0; i < mRideCount; ++i) {
            RideInfo* candidate = mRides[i];
            if (candidate->mId == id) {
                mCachedId = id;
                mCachedRide = candidate;
                found = candidate;
                break;
            }
        }
    }
    if (found == nullptr || found->mName == nullptr || found->mName == gRideMissingName) {
        mNames.GetName(id, ride->mTypeName, 0x40);
    } else {
        wcsncpy(ride->mTypeName, found->mName, 0x40);
    }
    const bool dontApplyOffset = ride->Value(kRideSlot_DontApplyOffset).mInteger != 0;
    const uint32_t headFlags = ride->Value(kRideSlot_DoHeadProcessing).mInteger != 0 ? 0x400000 : 0;
    if (dontApplyOffset) {
        ride->mRideTypeNumber = LoadRideType(folder.mData, item.mName.mData,
            ride->Value(kRideSlot_Shape).mShape, ride->Value(kRideSlot_Hoarding).mShape,
            0x50c00 | headFlags, ride->mId, ride->Value(kRideSlot_NumSimultAnims).mInteger,
            nullptr, nullptr, ride->Value(kRideSlot_EngineFootprintWidthOverride).mInteger,
            ride->Value(kRideSlot_EngineFootprintHeightOverride).mInteger);
    } else {
        ride->mRideTypeNumber = LoadRideType(folder.mData, item.mName.mData,
            ride->Value(kRideSlot_Shape).mShape, ride->Value(kRideSlot_Hoarding).mShape,
            0xb24a9 | headFlags, ride->mId, ride->Value(kRideSlot_NumSimultAnims).mInteger,
            nullptr, nullptr, 0, 0);
        if (ride->mRideTypeNumber != 0 && ride->Value(kRideSlot_WhichTrackType).mInteger == 3) {
            CCoasterBalance coaster;
            coaster.Init();
            const char* name = gTPGlobals.mF42A4.mData;
            const char* root = item.mStorage.GetRootPath();
            std::sprintf(label, "%s:%s", root, name);
            if (item.GetCoasterSamFile(file) != 0) {
                if (coaster.Load(label, file, CBalanceTable::kLoadFresh) == 0) {
                    gExitError.SetError(CExitError::kType3, 0, label);
                    // The destructor (0x10039b30) runs the UnInit on this return.
                    return 0;
                }
                file.Close();
            }
            LoadCoasterType(ride->mRideTypeNumber, folder.mData, coaster);
            // The scope end runs ~CCoasterBalance (0x10039b30), which is the UnInit.
        }
    }
    ride->mLoadFailed = ride->mRideTypeNumber == 0 ? 1 : 0;
    if (ride->mLoadFailed == 0) {
        ride->mSubRideCount = ride->Value(kRideSlot_SupplementalMeshes_Count).mInteger;
        for (int32_t i = 0; i < ride->mSubRideCount; ++i) {
            ride->mSubRideTypes[i] = LoadRideType(folder.mData,
                ride->mBalance.mValues[kRideSlot_SupplementalMeshes_FileName + i].mString,
                nullptr, nullptr, 0xc0, static_cast<uint32_t>(ride->mId) + i + 1, 1,
                nullptr, nullptr, 0, 0);
        }
    }
    return 1;
}
