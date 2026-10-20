// FS_MasterSave.cpp range of the main app (SimTheme Park 2.04b3), second part (unit w32): the game-wide save's constructor, its
// reset, the keys the player holds and the theme records, and the park record's constructor, its balance file and its name.
// The ticket bookkeeping and the sign names are in FS_MasterSave_tickets.cpp and FS_MasterSave_signs.cpp (unit w89); the loader and
// the saver of the same range are not translated yet.
#include "FS_MasterSave.h"

#include <cstdio>
#include <utility>

#include "Debug.h"
#include "TPEngine.h"
#include "platform/msl.h"

// The rows of the park balance table's description (0x10294614): CanEarnTicketsInTheme, CanSpendTicketsInTheme and
// CostToEnter are integers, GateObjectId an integer from 0; "Tickets", "Keys" and "ParkName" are the group headings that follow
// their members. The row of type 12 ends the table.
const BalanceDescription gGlobalParkDataBalanceDescriptions[11] = {
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CanEarnTicketsInTheme", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CanSpendTicketsInTheme", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "Tickets", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "CostToEnter", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "Keys", 0, 0, 0.0f, 0.0f, 0, 0u},
    {0, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {5, "GateObjectId", 0, 0, 0.0f, 0.0f, 0, 0u},
    {1, "ParkName", 0, 0, 0.0f, 0.0f, 0, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CGlobalParkDataBalance::GetBalanceDescription(int32_t row) {
    return &gGlobalParkDataBalanceDescriptions[row];
}
BalanceValue* CGlobalParkDataBalance::GetBalanceValuesStart() { return mValues; }

namespace {
// The text part of the sign names' work area (0x10380a4a): GetFullName copies the first sign name here to cut its hyphen off.
// The Mac's array is 0x21 characters, the size of a sign name.
char16_t gParkNameScratch[0x21];   // 0x10380a4a
}  // namespace

// ---- CFSGlobalParkData

// The record of a theme with nothing won and no balance table; the name is copied. (The Mac's constructor sets the tickets,
// the awards, the sign flag and the first character of the sign names; the awards' scores are left as they are.)
CFSGlobalParkData::CFSGlobalParkData(const TbDynamicStringTemplate<char>& name) : mBalance(nullptr) {
    for (int32_t i = 0; i < 6; ++i) mTickets[i] = 0;
    for (int32_t i = 0; i < 4; ++i) mAwards[i] = 0;
    mName = name;
    mHasSignNames = 0;
    mAllResearchCompleted = 0;
    mSignNameA[0] = 0;
    mSignNameB[0] = 0;
}

int32_t CFSGlobalParkData::HowManyTickets() const {
    int32_t count = 0;
    for (int32_t i = 0; i < 6; ++i) count += mTickets[i] != 0 ? 1 : 0;
    return count;
}

int32_t CFSGlobalParkData::KeyCostToEnter() {
    if (!CheckBalanceFile()) return -1;
    return mBalance->mValues[3].mInteger;
}

// The balance table is made and read the first time anyone asks. When the file cannot be read the table is taken down again
// (so the next question tries the file again) and a trace line says so.
bool CFSGlobalParkData::CheckBalanceFile() {
    if (mBalance != nullptr) return true;
    mBalance = new CGlobalParkDataBalance;
    char path[260];
    std::sprintf(path, "data:levels:%s:global.sam", mName.mData);
    if (mBalance->Load(path, CBalanceTable::kLoadFresh) != 0) return true;
    delete mBalance;
    mBalance = nullptr;
    CTRACE(3, "!!!!!\nLoading the global.sam went horribly wrong! Shout at Bjarne!\n!!!!!");
    return false;
}

// The sign names, when the player gave the park some: both lines joined with a blank, or without when the first ends with a
// hyphen (the hyphen is dropped, and the line is cut to 31 characters first); one line alone as it is. Otherwise the park's name
// is that of its gate object in the ride database. When the balance file cannot be read nothing is written (the Mac formats an
// empty text into a buffer of size 0).
void CFSGlobalParkData::GetFullName(char16_t* buffer, uint32_t size) {
    if (mHasSignNames != 0) {
        const uint32_t length = wcslen(mSignNameA);
        if (length == 0) {
            wcsncpy(buffer, mSignNameB, size);
        } else if (mSignNameA[length - 1] == u'-') {
            gParkNameScratch[0x20] = 0;
            swprintf(gParkNameScratch, 0x20, u"%ls", mSignNameA);
            gParkNameScratch[length - 1] = 0;
            swprintf(buffer, size, u"%ls%ls", gParkNameScratch, mSignNameB);
        } else if (wcslen(mSignNameB) == 0) {
            wcsncpy(buffer, mSignNameA, size);
        } else {
            swprintf(buffer, size, u"%ls %ls", mSignNameA, mSignNameB);
        }
        return;
    }
    if (CheckBalanceFile()) gRideDatabase.GetTypeName(static_cast<uint16_t>(mBalance->mValues[4].mInteger), buffer, size);
    else swprintf(buffer, 0, u"");
}

// ---- CFSGlobalMasterSave

int32_t CFSGlobalMasterSave::HowManyKeys() const {
    int32_t tickets = 0;
    for (int32_t i = 0; i < 4; ++i) tickets += mGlobalTickets[i] != 0 ? 1 : 0;
    for (const auto& theme : mThemes) {
        if (theme.second->CheckBalanceFile()) tickets += theme.second->HowManyTickets();
    }
    for (int32_t i = 0; i < 2; ++i) tickets += mSecretTickets[i] != 0 ? 1 : 0;
    return mExtraKeys + tickets / 3;
}

CFSGlobalParkData* CFSGlobalMasterSave::GetInfoOn(const TbDynamicStringTemplate<char>& name) {
    const auto found = mThemes.find(name);
    if (found == mThemes.end()) {
        CTRACE(3, "Trying to get info from a theme (%s) which doesn't exist, better create one then \n", name.mData);
        CFSGlobalParkData* park = CreateNewTheme(name);
        return park->CheckBalanceFile() ? park : nullptr;
    }
    return found->second->CheckBalanceFile() ? found->second : nullptr;
}

CFSGlobalParkData* CFSGlobalMasterSave::CreateNewTheme(const TbDynamicStringTemplate<char>& name) {
    if (mThemes.find(name) != mThemes.end()) {
        CTRACE(3, "Trying to create Global save info for a theme (%s) which is already in the map (FS_LoadSave.cpp)\n", name.mData);
    }
    CFSGlobalParkData* park = new CFSGlobalParkData(name);
    std::pair<const TbDynamicStringTemplate<char>, CFSGlobalParkData*> entry(name, park);
    mThemes.insert(entry);
    return park;
}
