// Unit v51002: the save-file instances for CStaffHQ::StrikeType (SAVEVARSTRUCT 0x1012c3d4, LOADVARSTRUCT 0x1012c27c) and CPerson::PersonState
// (SAVEVAR 0x1011b90c), and CPerson::PayPricePerUseForObject (0x1011a4ac). LOADVAR<ResearchCat> (0x10124554) is in
// src/ResearchLab_projects_CResearchProject_Serialise.cpp.
#include "ParkMessages.h"
#include "ThingReceivers.h"
#include "Serialise.h"
#include "TPSound.h"

// A strike record is three words; the first is saved as it is, the other two are byte-reversed (the file is little-endian).
static void SwapForFile(CStaffHQ::StrikeType& value) {
    value.mF04 = serialise::SwapWord(value.mF04);
    value.mF08 = serialise::SwapWord(value.mF08);
}

static void UnswapForFile(CStaffHQ::StrikeType& value) {
    value.mF04 = serialise::UnswapWord(value.mF04);
    value.mF08 = serialise::UnswapWord(value.mF08);
}

template<> int32_t SAVEVARSTRUCT<CStaffHQ::StrikeType>(void* file, CStaffHQ::StrikeType* value, const char* name, int32_t noLog) {   // 0x1012c3d4
    return serialise::SaveStruct(file, value, name, noLog);
}

template<> int32_t LOADVARSTRUCT<CStaffHQ::StrikeType>(void* file, CStaffHQ::StrikeType* value, const char* name, int32_t noLog) {   // 0x1012c27c
    return serialise::LoadStruct(file, value, name, noLog);
}

template<> int32_t SAVEVAR<CPerson::PersonState>(void* file, CPerson::PersonState* value, const char* name, int32_t noLog) {   // 0x1011b90c
    return serialise::SaveVar(file, value, name, noLog);
}

template<> int32_t LOADVAR<ResearchCat>(void* file, ResearchCat* value, const char* name, int32_t noLog) {   // 0x10124554
    return serialise::LoadVar(file, value, name, noLog);
}

void CPerson::PayPricePerUseForObject(CObject* object) {   // 0x1011a4ac
    uint32_t price = object->GetPricePerUse();
    if (price != 0) {
        object->RecordNewTakings(price);
        reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0xd0);
        uint32_t* money = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x1a0);
        *money = *money - price;
    }
}
