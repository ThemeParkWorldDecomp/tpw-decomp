// Unit v5L09: the advisor load, BMP_Load (main app 0x10023850, 8432 B, TPAdvisor.cpp on the Mac).
// It reads the chunks BMP_Save writes (src/host_leaves_Write_BumpToChunk_BMP_Save.cpp) with the chunk reader of unit v51017: the save
// chunk (tag 1), then chunk after chunk until the save's physical size is used up: 2 the build time (skipped), 3 a ride record
// (BMP_CreateRide and its settings), 4 a track section, 5 a bump of a ride (its block, collision object, ride instances and buoy),
// 6 the ride's buoy sequence, 7 and 8 the ride's leaving and joining peeps, 9 a node of the bump's list from the world's free list.
// Each chunk is closed by seeking past it and freeing it. Tags 0 and 1 and anything above 9 stop the load with 0 (the open chunks
// are not freed). The answer is the save chunk's physical size.
// The file is little-endian: the Mac swaps every word it reads; on the host the words are already in order.
#include <cstdint>
#include <cstring>
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "LbFile.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "TPEngine.h"
#include "analyser.h"
#include "rsse.h"
#include "platform/macdoze.h"

namespace MathLib {
int __fpclassifyf(float x);   // glue 0x10239268
}

namespace {

const uint32_t kWorldMagic = 0x4a454647;   // 'JEFG'

// A word of a record by its Mac offset.
uint32_t Word(const void* base, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

void SetWord(void* base, uint32_t offset, uint32_t value) { std::memcpy(static_cast<uint8_t*>(base) + offset, &value, 4); }

// A 32-bit pointer field by its Mac offset.
template <class T>
T* Follow(const void* base, uint32_t offset) {
    Ptr32<T> pointer{Word(base, offset)};
    return pointer.get();
}

void SetPointer(void* base, uint32_t offset, const void* target) {
    Ptr32<const void> pointer;
    pointer.set(target);
    SetWord(base, offset, pointer.mOffset);
}

tagBUMPSAVEWORLD* World() { return reinterpret_cast<tagBUMPSAVEWORLD*>(&gColList); }

// The ride record an id names (its low byte is the index); NULL when the world is not live.
uint8_t* RideFromId(uint32_t id) {
    tagBUMPSAVEWORLD* world = World();
    if (world->magic != kWorldMagic) return nullptr;
    return reinterpret_cast<uint8_t*>(world->rides.get()) + (id & 0xff) * 0xd0;
}

// The id of a ride record: its index in the world's records (a signed division by 0xd0) or'd with its kind shifted by 8;
// 0 when the world is not live.
uint32_t RideId(const uint8_t* ride) {
    tagBUMPSAVEWORLD* world = World();
    if (world->magic != kWorldMagic) return 0;
    int32_t diff = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ride)) -
                                        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(world->rides.get())));
    return (Word(ride, 0) << 8) | static_cast<uint32_t>(diff / 0xd0);
}

// The inlined word read: the word when all four bytes came, else 0.
uint32_t ReadWord(tabBMPCHUNK* chunk) {
    uint32_t word;
    if (Read_DataFromChunk(chunk, &word, 4) != 4) return 0;
    return word;
}

// The inlined pair read: the answer is not looked at. (On a failed read the Mac's pair is what its stack held; here it is 0.)
void ReadPair(tabBMPCHUNK* chunk, uint32_t pair[2]) {
    pair[0] = 0;
    pair[1] = 0;
    Read_DataFromChunk(chunk, pair, 8);
}

// The inlined chunk close: past the chunk in the file, the block freed; answers the chunk's physical size.
int32_t CloseChunk(tabBMPCHUNK* chunk) {
    TRACE("Close Chunk %d bytes unread\n", chunk->dataSize - static_cast<int32_t>(chunk->pad08));
    LbFile_Seek(chunk->file.get(), chunk->start + chunk->physicalSize, 0);
    int32_t size = chunk->physicalSize;
    NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(chunk));
    return size;
}

RideInfo* Info(const uint8_t* ride) {
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(Word(ride, 0x10))});
}

// Tag 3: the ride record.
void LoadRide(tabBMPCHUNK* chunk) {
    uint32_t id = ReadWord(chunk);
    uint32_t position[2];
    ReadPair(chunk, position);
    uint32_t rotation = ReadWord(chunk);
    uint32_t f10 = ReadWord(chunk);
    if (static_cast<uint32_t>(BMP_CreateRide(0, static_cast<int32_t>(position[0]), static_cast<int32_t>(position[1]),
                                             static_cast<int32_t>(rotation), static_cast<int32_t>(id),
                                             static_cast<int32_t>(f10))) != id)
        return;
    uint8_t* ride = RideFromId(id);
    BMP_SetRidePerformance(static_cast<int32_t>(id), static_cast<int32_t>(ReadWord(chunk)));
    uint32_t a = ReadWord(chunk);
    uint32_t b = ReadWord(chunk);
    BMP_SetRideModelSelection(static_cast<int32_t>(id), static_cast<int32_t>(a), static_cast<int32_t>(b));
    BMP_SetRideDuration(static_cast<int32_t>(id), static_cast<int32_t>(ReadWord(chunk)));
    SetWord(ride, 0x50, ReadWord(chunk));
}

// Tag 6: the ride's buoy sequence, for the kinds that have buoys.
void LoadBuoys(tabBMPCHUNK* chunk) {
    uint32_t id = ReadWord(chunk);
    uint8_t* ride = RideFromId(id);
    if (id != RideId(ride)) return;
    switch (static_cast<int32_t>(Word(ride, 0))) {
    case -4: case -5: case -7: case -8: case -9: case -10: case -12: case -13:
        SetBuoySequence(reinterpret_cast<tagBUMPRIDE*>(ride));
        break;
    default:
        TRACE("###WARNING### Ride not properly initialised after load\n");
        break;
    }
}

// Tag 5: a bump of the ride.
void LoadBump(tabBMPCHUNK* chunk) {
    uint32_t id = ReadWord(chunk);
    uint8_t* ride = RideFromId(id);
    if (id != RideId(ride)) return;
    tagBUMP* bump = BMP_StartBump(0, 0, 0);
    if (bump == nullptr) return;
    Read_BumpFromChunk(chunk, *bump);
    SetWord(bump, 0x94, 0);
    SetPointer(bump, 0x9c, ride);
    SetWord(ride, 0x5c, Word(ride, 0x5c) + 1);
    SetPointer(ride, 0xcc, bump);
    SetWord(bump, 0x30, 0);
    float heading;
    std::memcpy(&heading, reinterpret_cast<uint8_t*>(bump) + 0xa0, 4);
    if (MathLib::__fpclassifyf(heading) == 1) {
        heading = 0.0f;
        std::memcpy(reinterpret_cast<uint8_t*>(bump) + 0xa0, &heading, 4);
    }
    bump->col = PointInAnyColObject(bump->x, bump->y);
    uint32_t x = ReadWord(chunk);
    uint32_t y = ReadWord(chunk);
    if (bump->col == nullptr) bump->col = PointInAnyColObject(static_cast<int32_t>(x), static_cast<int32_t>(y));

    // The ride type of the bump's sub-ride (its word 4, not bounded) and, for the kinds with a second instance, of sub-ride 0.
    SetWord(bump, 8, RestoreRideInstance(Word(bump, 8), Info(ride)->mSubRideTypes[Word(bump, 4)], 0x101, nullptr, 0, nullptr, 0, 0, 0));
    if ((bump->flags & 0x02002000) != 0)
        SetWord(bump, 0xc, RestoreRideInstance(Word(bump, 0xc), Info(ride)->mSubRideTypes[0], 0x101, nullptr, 0, nullptr, 0, 0, 0));
    if ((bump->flags & 0x00400000) != 0) SetPointer(ride, 0x58, bump);

    uint32_t target = ReadWord(chunk);
    if (target != 0) {
        uint8_t* targetRide = RideFromId(target);
        uint32_t bx = ReadWord(chunk);
        uint32_t by = ReadWord(chunk);
        SetWord(bump, 0x7c, static_cast<uint32_t>(FindBuoy(reinterpret_cast<tagBUMPRIDE*>(targetRide), static_cast<int32_t>(bx),
                                                           static_cast<int32_t>(by))));
    } else {
        ReadWord(chunk);
        ReadWord(chunk);
        SetWord(bump, 0x7c, 0xffffffffu);
    }
    if ((bump->flags & 4) != 0) SetWord(bump, 0x8c, 0);
}

// Tag 9: a node of the ride's bump's list, taken from the world's free list (which is not tested for NULL).
void LoadLink(tabBMPCHUNK* chunk) {
    uint32_t id = ReadWord(chunk);
    uint8_t* ride = RideFromId(id);
    if (id != RideId(ride)) return;
    tagBUMPSAVEWORLD* world = World();
    uint8_t* link = Follow<uint8_t>(world, 0x34);
    uint8_t* bump = Follow<uint8_t>(ride, 0xcc);
    SetWord(world, 0x34, Word(link, 0x10));
    SetWord(link, 0x10, Word(bump, 0x30));
    SetPointer(bump, 0x30, link);
    SetWord(link, 0, ReadWord(chunk));
    SetWord(link, 4, id);
    uint8_t* owner = Follow<uint8_t>(bump, 0x9c);
    SetWord(owner, 0x60, Word(owner, 0x60) + 1);
    SetWord(link, 0xc, ReadWord(chunk));
    SetWord(link, 8, FindDataNodeIndexByID(gRideAnimSets[Word(bump, 8)], 0x80, Word(link, 0xc)));
}

// Tag 4: a track section.
void LoadTrack(tabBMPCHUNK* chunk) {
    uint32_t id = ReadWord(chunk);
    uint32_t type = ReadWord(chunk);
    uint32_t position[2];
    ReadPair(chunk, position);
    BMP_TrackSectionPlaceSection(static_cast<int32_t>(id), static_cast<int32_t>(position[0]), static_cast<int32_t>(position[1]),
                                 static_cast<int32_t>(type), 0, 0, 0);
}

}  // namespace

int32_t BMP_Load(void* file) {
    if (World()->magic != kWorldMagic) return 0;
    Ptr32<tabBMPCHUNK> save{};
    if (Read_OpenChunk(file, &save) == 1) {
        int32_t consumed = 0;
        do {
            Ptr32<tabBMPCHUNK> slot{};
            uint32_t tag = Read_OpenChunk(file, &slot);
            tabBMPCHUNK* chunk = slot.get();
            switch (tag) {
            case 2:
                break;
            case 3:
                LoadRide(chunk);
                break;
            case 4:
                LoadTrack(chunk);
                break;
            case 5:
                LoadBump(chunk);
                break;
            case 6:
                LoadBuoys(chunk);
                break;
            case 7: {
                uint32_t id = ReadWord(chunk);
                BMP_AddPeepToRideLeavingList(static_cast<int32_t>(id), ReadWord(chunk));
                break;
            }
            case 8: {
                uint32_t id = ReadWord(chunk);
                BMP_AddPeepToRideJoiningList(static_cast<int32_t>(id), ReadWord(chunk));
                break;
            }
            case 9:
                LoadLink(chunk);
                break;
            default:   // 0, 1 and above 9
                TRACE("####BMP_Load - UNKNOWN CHUNK####\n");
                return 0;
            }
            consumed += CloseChunk(chunk);
        } while (static_cast<uint32_t>(save.get()->physicalSize) != save.get()->pad08 + static_cast<uint32_t>(consumed));
    }
    // A save chunk that did not open leaves the slot as it was (on the Mac whatever the stack held).
    return CloseChunk(save.get());
}
