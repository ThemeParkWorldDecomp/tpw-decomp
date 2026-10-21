// RideDB.cpp: name proven by __sinit_RideDB_cpp in the main application.

#include "analyser.h"
#include "bfl_FileSystem.h"
#include "CoasterBalance.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "Game.h"
#include "InterfaceFontWrapper.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Main.h"
#include "MainBalance.h"
#include "MslSubscriptionTree.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "RideDB.h"
#include "rsse.h"
#include "SkyObjects.h"
#include "tb_sky.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "Text.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPSound.h"
#include "TPString.h"
#include "v51029_decls.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ios>
#include <new>

// RideDB.cpp, main app: RideInfo's text, custom effects and destruction.

// The original text parser helpers (untranslated).
int32_t find_string_id(const char16_t* text); // 0x10155604
const char16_t* find_line(const char16_t* text); // 0x101556a0
void copy_line(const char16_t** destination, const char16_t* text); // 0x10155170

// 0x1015457c, 192 bytes. The caller's delete supplies the Mac's positive destruction flag.
RideInfo::~RideInfo() {
    for (int32_t i = 0; i < 4; ++i) {
        if (mEmitters[i] != 0) PAR_KillLibEmitter(mEmitters[i]);
    }
    if (mSoundCategory != 0) SOUND_FreeCatagory(mSoundCategory);
    mBalance.UnInit();
}

// CRideDatabase::ClearDatabase (main app, SimTheme Park 2.04b3, RideDB.cpp; unit w81).

// 0x101546e8 (152 bytes). The custom effects first, then every ride record in order: its strings given back, the record destroyed with
// the flag 1 (which frees it), the slot cleared. The count (signed compare) and the record pointer are read again each time round, as on
// the Mac, since DeallocStrings could change them. Then the count and the lookup cache (id and record) are reset.
void CRideDatabase::ClearDatabase() {
    DeleteCustomEffects();
    for (int32_t i = 0; i < mRideCount; ++i) {
        mRides[i]->DeallocStrings();
        delete mRides[i];
        mRides[i] = nullptr;
    }
    mRideCount = 0;
    mCachedId = 0;
    mCachedRide = nullptr;
}

// RideDB.cpp (main app 0x101547b4 and 0x10154800, unit w114): the ride database's walk over its ride records.

// Puts the position (mF72C) on slot 0 and answers that slot (NULL for an empty database; the count is not looked at).
RideInfo* CRideDatabase::GetFirstRide() {
    mF72C = 0;
    return mRides[mF72C];
}

// While the position is below the count less one (signed; the subtraction wraps in 32 bits) moves it on by one and answers that slot,
// else answers NULL and leaves the position where it is. A NULL slot inside the count is answered as it is. (CRideDatabase::
// DeleteCustomEffects does the same walk in line.)
RideInfo* CRideDatabase::GetNextRide() {
    if (mF72C < static_cast<int32_t>(static_cast<uint32_t>(mRideCount) - 1u)) {
        mF72C = mF72C + 1;
        return mRides[mF72C];
    }
    return nullptr;
}

// CRideDatabase::GetRideInfo (main app, RideDB.cpp range; unit w200): 0x10154868, 104 bytes. The ride record of an object type.

// The Mac keeps the id (the HIGH halfword of its register argument) on its stack and compares the zero-extended halfwords. The cached type answers its
// cached record. Otherwise the slots are walked from 0 while the index is below the count (a signed compare, so a negative count walks nothing): the first
// record whose first halfword is the id becomes the cache and is the answer; no record answers NULL and leaves the cache as it was.
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    if (id.mIndex == mCachedId) return mCachedRide;
    for (int32_t i = 0; i < mRideCount; ++i) {
        RideInfo* ride = mRides[i];
        if (ride->mId == id.mIndex) {
            mCachedId = id.mIndex;
            mCachedRide = ride;
            return ride;
        }
    }
    return nullptr;
}

// RideDB.cpp range of the main app (SimTheme Park 2.04b3): the ride types' names. The name database (CRideNameDB, which the
// inventory files under SaveLoad.cpp) builds a ride type's name from two strings of the object names string table; the ride
// database's three-argument GetTypeName gives the name the ride's own file named, or the built one.

const char16_t gRideMissingName[] = u"MISSING STRING";   // 0x102a5ce2

// Unit w210: the ride record's name (mItemName) of a ride type, a wide string; CObject::GetFileName asks for it. NULL when no record has the id.
const char16_t* CRideDatabase::GetFileName(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    RideInfo* ride;
    if (id.mIndex == mCachedId) {
        ride = mCachedRide;
    } else {
        ride = nullptr;
        for (int32_t i = 0; i < mRideCount; ++i) {
            if (mRides[i]->mId == id.mIndex) {
                mCachedId = id.mIndex;
                mCachedRide = mRides[i];
                ride = mRides[i];
                break;
            }
        }
    }
    return ride != nullptr ? ride->mItemName : nullptr;
}
// The ride type's name. The database remembers the last type it found (its id and record) and answers it without a search:
// the cache starts as id 0 with no record, so the first question for type 0 finds nothing. A record whose file named the
// ride (a name that is neither missing nor the "MISSING STRING" text the loader leaves) gives that name; anything else, the
// name builder's.
void CRideDatabase::GetTypeName(uint16_t id, char16_t* buffer, uint32_t size) {
    RideInfo* ride;
    if (id == mCachedId) {
        ride = mCachedRide;
    } else {
        ride = nullptr;
        for (int32_t i = 0; i < mRideCount; ++i) {
            if (mRides[i]->mId == id) {
                mCachedId = id;
                mCachedRide = mRides[i];
                ride = mRides[i];
                break;
            }
        }
    }
    if (ride == nullptr || ride->mName == nullptr || ride->mName == gRideMissingName) {
        mNames.GetName(id, buffer, static_cast<int32_t>(size));
    } else {
        wcsncpy(buffer, ride->mName, size);
    }
}

// The ride type's own name: the text inside its record (RideInfo::mTypeName), or NULL when the database has no such type.
// The lookup is the one above (the cache first, a hit by search is remembered).
const char16_t* CRideDatabase::GetTypeName(uint16_t id) {
    RideInfo* ride;
    if (id == mCachedId) {
        ride = mCachedRide;
    } else {
        ride = nullptr;
        for (int32_t i = 0; i < mRideCount; ++i) {
            if (mRides[i]->mId == id) {
                mCachedId = id;
                mCachedRide = mRides[i];
                ride = mRides[i];
                break;
            }
        }
    }
    if (ride == nullptr) return nullptr;
    return ride->mTypeName;
}

// The first part of the ride's sign text. The name builder's table is asked first (without the hyphen trim); a type it does
// not have gets the sign text of its record. A type the database does not have reads the record at address 0 on the Mac
// (mSignA is at 0x7f4), so the translation dereferences NULL as well.
void CRideDatabase::GetSignNameA(uint16_t id, char16_t* buffer, uint32_t size) {
    if (!mNames.GetNameA(id, buffer, static_cast<int32_t>(size), false)) {
        RideInfo* ride;
        if (id == mCachedId) {
            ride = mCachedRide;
        } else {
            ride = nullptr;
            for (int32_t i = 0; i < mRideCount; ++i) {
                if (mRides[i]->mId == id) {
                    mCachedId = id;
                    mCachedRide = mRides[i];
                    ride = mRides[i];
                    break;
                }
            }
        }
        wcsncpy(buffer, ride->mSignA, size);
    }
}

// The second part, the same way (mSignB is at 0x7f8).
void CRideDatabase::GetSignNameB(uint16_t id, char16_t* buffer, uint32_t size) {
    if (!mNames.GetNameB(id, buffer, static_cast<int32_t>(size))) {
        RideInfo* ride;
        if (id == mCachedId) {
            ride = mCachedRide;
        } else {
            ride = nullptr;
            for (int32_t i = 0; i < mRideCount; ++i) {
                if (mRides[i]->mId == id) {
                    mCachedId = id;
                    mCachedRide = mRides[i];
                    ride = mRides[i];
                    break;
                }
            }
        }
        wcsncpy(buffer, ride->mSignB, size);
    }
}

// RideDB.cpp, main app: LoadTheme 0x10154ea8 (480), ScanDirectory 0x10155c00 (964).

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

// 0x101550bc, 132 bytes. The placeholder is retained and owned blocks are cleared after Free.
void RideInfo::DeallocStrings() {
    for (int32_t i = 0; i < 4; ++i) {
        if (mStrings[i] != nullptr && mStrings[i] != gRideMissingName) {
            LbMem_Free(const_cast<char16_t*>(mStrings[i]));
            mStrings[i] = nullptr;
        }
    }
}

// RideDB.cpp, main app: UTF-16 token/line helpers and custom effect getters.

const char16_t* const gRideStringTags[4] = {u"NAME", u"SIGNA", u"SIGNB", nullptr}; // 0x102a5d00

// 0x10155170, 172 bytes. The old destination is overwritten without being freed.
void copy_line(const char16_t** destination, const char16_t* text) {
    uint32_t count = 0;
    const char16_t* scan = text;
    while (*scan != 0 && *scan != 10 && *scan != 13) {
        ++scan;
        ++count;
    }
    *destination = static_cast<char16_t*>(LbMem_Alloc((count + 1u) << 1));
    auto* output = const_cast<char16_t*>(*destination);
    while (count != 0) {
        const char16_t value = *text++;
        --count;
        *output++ = value;
    }
    *output = 0;
}

// 0x10155244, 928 bytes. Simultaneous delimiters end a matching token.
int32_t comp(const char16_t* left, const char16_t* right) {
    for (;;) {
        const char16_t a = *left;
        const bool endA = a == 0 || a == 32 || a == 9 || a == 10 || a == 13;
        const char16_t b = *right;
        const bool endB = b == 0 || b == 32 || b == 9 || b == 10 || b == 13;
        if (endA && endB) return 1;
        if (a == 0 || b == 0) return 0;
        if (a != b) return 0;
        ++left;
        ++right;
    }
}

// 0x10155604, 116 bytes. The table is terminated by a null pointer.
int32_t find_string_id(const char16_t* text) {
    const char16_t* const* tags = gRideStringTags;
    uint32_t index = 0;
    for (;;) {
        const char16_t* tag = tags[index];
        if (tag == nullptr) return -1;
        if (comp(text, tag) != 0) return static_cast<int32_t>(index);
        ++index;
    }
}

// 0x101556a0, 224 bytes. Return the next nonblank line's start, or the final NUL.
const char16_t* find_line(const char16_t* text) {
    while (*text != 0 && *text != 10 && *text != 13) ++text;
    if (*text == 0) return text;
    if ((*text == 10 && text[1] == 13) || (*text == 13 && text[1] == 10)) text += 2;
    else ++text;
    const char16_t* scan = text;
    while (*scan == 32 || *scan == 9) ++scan;
    if (*scan == 0 || *scan == 10 || *scan == 13) return find_line(text);
    return text;
}

// RideDB.cpp, main app: ProcessShape 0x101557a4 (536 bytes).

void CRideDatabase::ProcessShape(RideInfo** ride) {
    auto* shape = static_cast<RideShape*>((*ride)->Value(kRideSlot_Shape).mShape);
    int32_t entryX = 0;
    int32_t entryY = 0;
    uint8_t entryFlag = 0;
    bool entrance = false;
    for (int32_t x = 0; x < shape->mWidth && !entrance; ++x) {
        for (int32_t y = 0; y < shape->mHeight; ++y) {
            if (shape->mCells[y][x].mValue == 9) {
                entryX = x;
                entryY = y;
                entryFlag = shape->mCells[y][x].mFlag;
                entrance = true;
                break;
            }
        }
    }
    if (entrance) {
        (*ride)->mEntryX = entryX;
        (*ride)->mEntryY = entryY;
        (*ride)->mEntryFlag = entryFlag;
        shape = static_cast<RideShape*>((*ride)->Value(kRideSlot_Shape).mShape);
        bool exit = false;
        int32_t exitX = 0;
        int32_t exitY = 0;
        uint8_t exitFlag = 0;
        for (int32_t x = 0; x < shape->mWidth && !exit; ++x) {
            for (int32_t y = 0; y < shape->mHeight; ++y) {
                if (shape->mCells[y][x].mValue == 10) {
                    exitX = x;
                    exitY = y;
                    exitFlag = shape->mCells[y][x].mFlag;
                    exit = true;
                    break;
                }
            }
        }
        (*ride)->mExitX = exit ? exitX : entryX;
        (*ride)->mExitY = exit ? exitY : entryY;
        (*ride)->mExitFlag = exit ? exitFlag : entryFlag;
    } else {
        (*ride)->mEntryX = 0;
        (*ride)->mEntryY = 0;
        (*ride)->mEntryFlag = 1;
        (*ride)->mExitX = 0;
        (*ride)->mExitY = 0;
        (*ride)->mExitFlag = 0x10;
    }
    const int32_t offsetY = (*ride)->Value(kRideSlot_MapOffsetY).mInteger;
    const int32_t offsetX = (*ride)->Value(kRideSlot_MapOffsetX).mInteger;
    (*ride)->mMapOffsetX = offsetX;
    (*ride)->mMapOffsetY = offsetY;
    (*ride)->mId = static_cast<uint16_t>((*ride)->Value(kRideSlot_Id).mInteger);
    (*ride)->mUIType = (*ride)->Value(kRideSlot_WhichUIType).mInteger;
}

// 0x101559f8, 452 bytes. Swap remains a platform call, as in the other file readers.
void RideInfo::LoadStrings(bfl::FileSystem::CFile& file) {
    for (int32_t i = 0; i < 4; ++i) mStrings[i] = gRideMissingName;
    const int32_t size = file.GetSize();
    if (size % 2 != 0) {
        GAME_ASSERTMSG(0, "Object text file's size is not divis. by 2 - so I don't believe you that it's UNICODE!");
        return;
    }
    auto* buffer = static_cast<char16_t*>(LbMem_Alloc(static_cast<uint32_t>(size) + 2));
    const int32_t read = file.Read(buffer, static_cast<uint32_t>(size));
    if (buffer == nullptr || read == 0) return;
    const int32_t count = size / 2;
    buffer[count] = 0;
    const char16_t* cursor = buffer;
    bool swap = true;
    if (buffer[0] == 0xfffe || buffer[0] == 0xfeff) {
        cursor += 1;
        swap = buffer[0] != 0xfeff;
    }
    if (swap) SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(buffer), count);
    while (*cursor != 0) {
        const int32_t id = find_string_id(cursor);
        if (id == -1) {
            LbMem_Free(buffer);
            return;
        }
        cursor = find_line(cursor);
        copy_line(&mStrings[id], cursor);
        CTRACE(1, "Successfully copied field: %d", id);
        cursor = find_line(cursor);
    }
    LbMem_Free(buffer);
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

// RideDB.cpp, main app: LoadItem 0x1015600c (2724 bytes).

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

// 0x10156b28, 80 bytes.
void RideInfo::LoadSounds(bfl::FileSystem::CFileStorage& storage) {
    mSoundCategory = SOUND_RegisterCatagory(storage.GetRootPath(), "cat_ride");
}

// 0x10156bc4, 412 bytes. The final character selects a word without a range check.
void RideInfo::LoadParticles(bfl::FileSystem::CFileStorage& storage) {
    bfl::FileSystem::CFileInfo info;
    if (!storage.FindFirst(gTPGlobals.mF45B0.mData, info).Succeeded()) return;
    bfl::FileSystem::CFile file;
    TbDynamicStringTemplate<char> name;
    do {
        if (storage.OpenFile(file, info.mName, 0x80000001, nullptr).Succeeded()) {
            name = info.mName;
            name = RemoveExtension(gTPGlobals.mF43A8, name);
            name = name.Right(1);
            char digit[4];
            std::strcpy(digit, name.mData);
            const int32_t index = static_cast<int32_t>(std::atol(digit));
            const uint32_t emitter = static_cast<uint32_t>(PAR_LoadLibEmitter(&file));
            // Digits 6 to 9 overwrite the four string pointers on the Mac. Native
            // pointers need a whole pointer assignment rather than a four-byte stride.
            if (index < 6) mEmitterWords[index] = emitter;
            else mStrings[index - 6] = reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(emitter));
        }
        file.Close();
    } while (storage.FindNext(info).Succeeded());
}

uint32_t RideInfo::GetCustomParticleEffect(int32_t index) const { // 0x10156db0, 16 bytes
    return mEmitterWords[static_cast<uint32_t>(index) + 1u];
}

int32_t RideInfo::GetCustomSoundBank() const { // 0x10156df8, 8 bytes
    return mSoundCategory;
}

// RideDB.cpp (main app 0x10156e34, unit w88): the ride database's custom-effect clean-up.

// 0x10156e34 (208 bytes). The position (mF72C, GetFirstRide and GetNextRide's) starts at the first slot. Each ride found frees its four
// emitters (the non-zero words) and its sound category (when not 0), and nothing is cleared afterwards: the words stay as they were. The
// next ride is the next slot while the position is below the count less one (signed; the Mac's subtraction wraps), else the walk ends;
// a NULL slot ends it too, and the first slot is visited whatever the count is. The position and the count are read again every time.
void CRideDatabase::DeleteCustomEffects() {
    mF72C = 0;
    RideInfo* ride = mRides[mF72C];
    while (ride != nullptr) {
        for (int32_t i = 0; i < 4; ++i) {
            const uint32_t handle = ride->mEmitters[i];
            if (handle != 0) PAR_KillLibEmitter(handle);
        }
        if (ride->mSoundCategory != 0) SOUND_FreeCatagory(ride->mSoundCategory);
        if (mF72C < static_cast<int32_t>(static_cast<uint32_t>(mRideCount) - 1u)) {
            mF72C = mF72C + 1;
            ride = mRides[mF72C];
        } else {
            ride = nullptr;
        }
    }
}

void RideInfo::SetIsCustomRide() { mFlags |= 1; } // 0x10156f40, 16 bytes

// CRideBalance: main app balance descriptions at 0x10299854.

const BalanceDescription gRideBalanceDescriptions[132] = {
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "Id", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "Name", 0, 0, 0.0f, 0.0f, 0, 0u},
    {11, "Shape", 0, 0, 0.0f, 0.0f, 0, 1u},
    {11, "Hoarding", 0, 0, 0.0f, 0.0f, 0, 2u},
    {5, "EngineMapOffsetOverrideX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "EngineMapOffsetOverrideY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "EngineFootprintWidthOverride", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "EngineFootprintHeightOverride", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "MapOffsetX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "MapOffsetY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "DontApplyOffset", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "IsChoosable", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "HasQueue", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "CanBreakDown", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "RunsContinuously", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "WhichUIType", 0, 5, 0.0f, 0.0f, 0, 0u},
    {6, "DontDeformBase", 0, 2, 0.0f, 0.0f, 0, 0u},
    {5, "OverwritePriority", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "AttractionValue", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "NewAttractionDecayTime", 1, 1000, 0.0f, 0.0f, 0, 0u},
    {6, "CreateParticleEffect", 0, 500, 0.0f, 0.0f, 0, 0u},
    {6, "DestroyParticleEffect", 0, 500, 0.0f, 0.0f, 0, 0u},
    {6, "RepairParticleEffect", 0, 500, 0.0f, 0.0f, 0, 0u},
    {6, "UpgradeParticleEffect", 0, 500, 0.0f, 0.0f, 0, 0u},
    {5, "DurationUnit", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "UsesCars", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "PreviewAnimType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "PreviewAnimNum", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "FirstPersonSprites", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "DoHeadProcessing", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "TurnOffAnimatingTextures", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "TurnOffScrollingTextures", 0, 2, 0.0f, 0.0f, 0, 0u},
    {5, "RideTypeStringIndex", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "DownloadedRideNumber", 0, 256, 0.0f, 0.0f, 0, 0u},
    {6, "IsPreviewRide", 0, 2, 0.0f, 0.0f, 0, 0u},
    {1, "Info", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "WhichTrackType", 0, 4, 0.0f, 0.0f, 0, 0u},
    {4, "BumperType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "NorthXAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "NorthYAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "EastXAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "EastYAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "SouthXAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "SouthYAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "WestXAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "WestYAdjust", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "Bumper", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "GoldenTicketCost", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "Length", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "Width", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "Height", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "EntryCellStandPosX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "EntryCellStandPosY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "ExitCellAppearPosX", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "ExitCellAppearPosY", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "InitPricePerUse", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "FatigueEffect", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "InitChanceOfLoosing", 0, 100, 0.0f, 0.0f, 0, 0u},
    {6, "ProvidesSecurity", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "ProvidesRelief", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "HoldsLitter", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "ChillsYouOut", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "RequiresTeleport", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "RideHandlesSprite", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "RequiresWatering", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "NonWinnableSideshow", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "IsFireworks", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "ISIndoors", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "CannotRide", 0, 2, 0.0f, 0.0f, 0, 0u},
    {6, "ConstrainCamera", 0, 2, 0.0f, 0.0f, 0, 0u},
    {5, "PukeReductionLevel", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "MinCapacity", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "MaxCapacity", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "MinDuration", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "MaxDuration", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "MinSpeed", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "MaxSpeed", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "ExcitementLevel", 0, 101, 0.0f, 0.0f, 0, 0u},
    {5, "InitCostOfGoods", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "ThirstEffect", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "HungerEffect", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "VomitEffect", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "HappinessEffect", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "LitterEffect", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "SpecialIngredient", 0, 6, 0.0f, 0.0f, 0, 0u},
    {6, "AppearanceEffect", 0, 3, 0.0f, 0.0f, 0, 0u},
    {5, "InitPrizeValue", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "ShopType", 0, 7, 0.0f, 0.0f, 0, 0u},
    {6, "ExciteFactor", 50, 200, 0.0f, 0.0f, 0, 0u},
    {5, "RipOffOK", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "NumSimultAnims", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "UsageInfo", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "Cost", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "Group", 0, 255, 0.0f, 0.0f, 0, 0u},
    {6, "Category", 0, 5, 0.0f, 0.0f, 0, 0u},
    {1, "Research", 0, 0, 0.0f, 0.0f, 0, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "CostOfResearch", 0, 0, 0.0f, 0.0f, 0, 0u},
    {6, "EstimatedPriceVariance", 0, 100, 0.0f, 0.0f, 0, 0u},
    {5, "ScrapValueYear1", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "ScrapValueYear2", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "ScrapValueYear3", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "ScrapValueYear4", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "InitCapacity", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "RedLineCapacity", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "InitDuration", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "RedLineDuration", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "InitSpeed", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "RedLineSpeed", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "WearRate", 0, 0, 0.0f, 0.0f, 0, 0u},
    {7, "QueueWaitTimeConstant", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "CostOfUpgrade", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "DurationOfUpgrade", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "Upgrades", 0, 0, 0.0f, 0.0f, 3, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "FileName", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "SupplementalMeshes", 0, 0, 0.0f, 0.0f, 8, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "NewBonus", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "Attraction", 0, 0, 0.0f, 0.0f, 3, 0u},
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {10, "FileName", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "SignTextures", 0, 0, 0.0f, 0.0f, 2, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "UpgradesId", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "UpgradeType", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "AddOn", 0, 0, 0.0f, 0.0f, 0, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CRideBalance::GetBalanceDescription(int32_t row) {
    return &gRideBalanceDescriptions[row];
}
BalanceValue* CRideBalance::GetBalanceValuesStart() { return mValues; }

// Unit v51029: seven small functions of the main app.
//   CSkyObjects::InitSkyObjects 0x1015b688, CText::~CText 0x1015ee20, __sinit_RideDB_cpp 0x10157010,
//   CSubscription::Save 0x10152b08, __sinit_keyboard_cpp 0x101512fc,
//   InterfaceFontWrapper::GetWidth 0x10168c7c, InterfaceFontWrapper::LastCharIn 0x10168d38.

// ---------------------------------------------------------------------------------------------------------------- RideDB.cpp initialiser

uint8_t gRideDBIosInit[16];
uint8_t gRideDBIosNode[32];
uint8_t gRideDBNode[32];

__attribute__((weak)) void IosBaseInitConstruct(void* object) { new (object) std::ios_base::Init(); }
__attribute__((weak)) void IosBaseInitDestroy(void* object) { static_cast<std::ios_base::Init*>(object)->~Init(); }
void RideDatabaseDestroy(void* object) { static_cast<CRideDatabase*>(object)->~CRideDatabase(); }

// 0x10157010 (140 bytes). The iostream initialiser object is built and registered; then the ride database global: the count is
// cleared, the ride table (0x528 bytes on the Mac: the 0x14a ride pointers) is zeroed, the cached id and ride cleared, and the
// database is registered for destruction. The other fields (the name builder) are left alone.
void __sinit_RideDB_cpp() {
    IosBaseInitConstruct(gRideDBIosInit);
    __register_global_object(gRideDBIosInit, reinterpret_cast<void*>(&IosBaseInitDestroy), gRideDBIosNode);
    gRideDatabase.mRideCount = 0;
    std::memset(&gRideDatabase.mRides, 0, sizeof(gRideDatabase.mRides));
    gRideDatabase.mCachedId = 0;
    gRideDatabase.mCachedRide = nullptr;
    __register_global_object(&gRideDatabase, reinterpret_cast<void*>(&RideDatabaseDestroy), gRideDBNode);
}

// ---------------------------------------------------------------------------------------------------------------- CSubscription::Save

// 0x10152b08 (124 bytes). The id is written byte-swapped (2 bytes) to the file the reference holds. A transfer count other than 2
// shows the "Warning! Something went hideously wrong ..." message and answers -1; else 0. The count is not set before the call.
int32_t CSubscription::SaveResult(void*& file) const {
    // The Mac swaps the big-endian id before the write, so the file holds it little-endian: on the host that is the value as it is.
    uint16_t swapped = mThingId;
    uint32_t written = 0;
    LbFile_Write(file, &swapped, 2, &written);
    if (written == 2) return 0;
    DEBUG_MESSAGE("Warning! Something went hideously wrong while saving the message subscriptions!", 0);
    return -1;
}
