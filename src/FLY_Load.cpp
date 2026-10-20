// Unit v5209: FLY_Load 0x10079aa8, FLY_RestoreMeshes 0x10079f8c, OBJECT_Load 0x1007a0b8 (the HLSprite.cpp range).
//
// The Mac swaps every word it reads from a file (SamsUtilities::_USwapBlock32, USwapInplace32<long>, tag_IDX_TYPE::Byteswap,
// USwapBlockStruct<FLYBOY>): the files are little-endian, so on this little-endian host there is nothing to do and the swaps are
// left out (the same as the particle loader's helpers in rsse.h).
#include <cmath>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// Reads one word of the file into `word`, as the original does (the byte count goes to a local).
void ReadWord(void* file, void* word) {
    uint32_t transferred;
    LbFile_Read(file, word, 4, &transferred);
}

// The original's x / 1024 (srawi, addze: rounds toward zero).
int32_t Div1024(int32_t value) { return value / 1024; }

}  // namespace

// 0x10079aa8: loads the fly file from the open `file`; 1 when the magic is wrong.
int32_t FLY_Load(void* file) {
    uint32_t word;
    if (gFlyInitialised != 0) FLY_Close();
    ReadWord(file, &word);
    if (word != 0x5f594c46u) {
        GAME_ASSERTMSG(0, "FLY: Load Fail ");
        return 1;
    }
    ReadWord(file, &word);
    if (word == 0) return 0;
    if (gFlyInitialised == 0) {
        gFlyInitialised = 1;
        gFlyActive = 1;
        for (int32_t i = 0; i < 100; ++i) gFlyBases[i] = nullptr;
        gFlySinTable = LbMem_Alloc(0x4000);
        for (int32_t i = 0; i < 0x1000; ++i) {
            double angle = 3.14159 * static_cast<double>(i * 2) / 4096.0;
            static_cast<int32_t*>(gFlySinTable)[i] = ppc_fctiwz(ppc_fmadd(1024.0, std::sin(angle), 0.5));
        }
    }
    ReadWord(file, &word);
    // The file's bytes go straight into the table of bases (the Mac's entries are 32-bit words; the host's are pointers, so the
    // words go through a copy and only the entries the file changed are replaced).
    uint32_t marks[100];
    for (int32_t i = 0; i < 100; ++i) marks[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gFlyBases[i]));
    uint32_t before[100];
    std::memcpy(before, marks, sizeof(marks));
    LbFile_Read(file, marks, word, nullptr);
    for (int32_t i = 0; i < 100; ++i)
        if (marks[i] != before[i]) gFlyBases[i] = reinterpret_cast<void*>(static_cast<uintptr_t>(marks[i]));
    ReadWord(file, &gFlyZoneD);
    ReadWord(file, &gFlyZoneB);
    ReadWord(file, &gFlyZoneA);
    ReadWord(file, &gFlyZoneC);
    for (int32_t i = 0; i < 6; ++i) ReadWord(file, &word);   // the sixth is the size of a base
    for (int32_t i = 1; i < 100; ++i) {
        if (gFlyBases[i] != nullptr) {
            gFlyBases[i] = LbMem_Alloc(word);
            LbFile_Read(file, gFlyBases[i], word, nullptr);
        }
    }
    return 0;
}

// 0x10079f8c: gives every active slot of every fly base a new ride instance.
void FLY_RestoreMeshes() {
    for (int32_t i = 1; i < 100; ++i) {
        sFlyBase* base = static_cast<sFlyBase*>(gFlyBases[i]);
        if (base == nullptr) continue;
        for (int32_t j = 0; j < kFlySlotCount; ++j) {
            sFlySlot* slot = base->Slot(j);
            if (slot->mActive == 0) continue;
            uint16_t thingId = *reinterpret_cast<uint16_t*>(reinterpret_cast<char*>(base) + 0x58);
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0,
                           "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            uint16_t type = *reinterpret_cast<uint16_t*>(static_cast<char*>(gThingArray[thingId].mThing) + 0xe);
            RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{type});
            slot->mRide = RestoreRideInstance(slot->mRide, info->mSubRideTypes[0], 0x101, nullptr, 0, nullptr, 0, 0, 0);
        }
    }
}

// 0x1007a0b8: loads the level's object file (a NULL path does nothing).
void OBJECT_Load(char* path) {
    if (path == nullptr) return;
    void* file = nullptr;
    LbFile_Open(file, path, 0x80000000u);
    if (file == nullptr) return;
    // This file is the Mac's own (big-endian) and the original reads its words as they are: on the little-endian host every word
    // read is byte-reversed to give the value the Mac saw.
    uint32_t magic;
    int32_t count;
    uint32_t recordSize;
    LbFile_Read(file, &magic, 4, nullptr);
    if (__builtin_bswap32(magic) != 0x5f4a424fu) return;
    LbFile_Read(file, &count, 4, nullptr);
    count = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(count)));
    LbFile_Read(file, &recordSize, 4, nullptr);
    recordSize = __builtin_bswap32(recordSize);
    for (int32_t n = 0; n < count; ++n) {
        int32_t rec[15];
        std::memset(rec, 0, sizeof(rec));
        if (recordSize > 0x3c) {
            LbFile_Read(file, rec, 0x3c, nullptr);
            LbFile_Seek(file, static_cast<int32_t>(recordSize - 0x3c), 1);
        } else {
            LbFile_Read(file, rec, recordSize, nullptr);
        }
        for (int32_t& word : rec) word = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(word)));
        // rec[0] 0x58 type, [1] 0x5c bank flag, [2] 0x60 event, [3] 0x64 x, [4] 0x68 y, [5] 0x6c z, [6] 0x70, [7] 0x74, [8] 0x78
        switch (rec[0]) {
        case 1:
            SOUND_SendEventExt(0, rec[1] != 0 ? gSoundBanks[6] : gSoundBanks[0], rec[2], Div1024(rec[3]), Div1024(rec[4]),
                               Div1024(rec[5]), Div1024(static_cast<int32_t>(static_cast<uint32_t>(rec[6]) << 1)));
            break;
        case 2:
            PAR_InitEmitter(rec[1], rec[3], rec[4], rec[5]);
            break;
        case 3: {
            int32_t handle = SOUND_SendEvent(0, rec[1] != 0 ? gSoundBanks[6] : gSoundBanks[0], rec[2], Div1024(rec[3]),
                                             Div1024(rec[4]), Div1024(rec[5]));
            SOUND_SetRect(handle, Div1024(rec[3]), Div1024(rec[5]), Div1024(rec[7]), Div1024(rec[8]));
            break;
        }
        default:
            break;
        }
    }
    LbFile_Close(file);
}
