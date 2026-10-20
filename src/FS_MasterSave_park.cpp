// CFSGlobalParkData's loader constructor (0x1016bb04, 104 bytes) and Serialise (0x1016bd9c, 780 bytes), main app, unit w89b: one
// theme's record inside the master save file. The other members of the record are in src/FS_MasterSave_global.cpp and
// src/FS_MasterSave_tickets.cpp.
#include "FS_MasterSave.h"
#include "Serialise.h"

// 0x1016bb04: sets only the balance table (none) and the name, which the Mac assigns through the string's virtual assignment. Every
// other member is left as it is (unlike the other constructor, 0x1016bbc4, which clears the tickets and the flags): the loader
// reads them all with Serialise right after. The CSerialise argument is not read.
CFSGlobalParkData::CFSGlobalParkData(CSerialise& serialise, const TbDynamicStringTemplate<char>& name) : mBalance(nullptr) {
    (void)serialise;
    mName = name;
}

// 0x1016bd9c. The file holds, in this order: the six local tickets, the four awards each followed by its score, the 0x21 units of both
// sign names (interleaved: a unit of the first name, then the same index of the second), the "name changed" flag and the "all research
// completed" flag. The booleans are bytes here (the members are uint8_t); the serialiser instances take them as bool.
int32_t CFSGlobalParkData::Serialise(void* file, int32_t save, uint32_t version) {
    (void)version;
    if (save == 0) {
        for (int32_t i = 0; i < 6; ++i) {
            if (LOADVAR(file, reinterpret_cast<bool*>(&mTickets[i]), "mEarnedLocalTicket[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 4; ++i) {
            if (LOADVAR(file, reinterpret_cast<bool*>(&mAwards[i]), "mAward[i]", 1) != 0) return 1;
            if (LOADVAR(file, &mAwardScores[i], "mAwardScore[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 0x21; ++i) {
            if (LOADVAR(file, &mSignNameA[i], "mSignNameA[i]", 1) != 0) return 1;
            if (LOADVAR(file, &mSignNameB[i], "mSignNameB[i]", 1) != 0) return 1;
        }
        if (LOADVAR(file, reinterpret_cast<bool*>(&mHasSignNames), "mNameChanged", 1) != 0) return 1;
        if (LOADVAR(file, reinterpret_cast<bool*>(&mAllResearchCompleted), "mAllResearchCompleted", 1) != 0) return 1;
    } else {
        for (int32_t i = 0; i < 6; ++i) {
            if (SAVEVAR(file, reinterpret_cast<bool*>(&mTickets[i]), "mEarnedLocalTicket[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 4; ++i) {
            if (SAVEVAR(file, reinterpret_cast<bool*>(&mAwards[i]), "mAward[i]", 1) != 0) return 1;
            if (SAVEVAR(file, &mAwardScores[i], "mAwardScore[i]", 1) != 0) return 1;
        }
        for (int32_t i = 0; i < 0x21; ++i) {
            if (SAVEVAR(file, &mSignNameA[i], "mSignNameA[i]", 1) != 0) return 1;
            if (SAVEVAR(file, &mSignNameB[i], "mSignNameB[i]", 1) != 0) return 1;
        }
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mHasSignNames), "mNameChanged", 1) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mAllResearchCompleted), "mAllResearchCompleted", 1) != 0) return 1;
    }
    return 0;
}
