// BumperRide.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Advisor.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "CBalanceTable.h"
#include "CellBase.h"
#include "Clock.h"
#include "Debug.h"
#include "effect_manager.h"
#include "FS_Online.h"
#include "helpers/AdvisorHelpers.h"
#include "helpers/BumperRideHelpers.h"
#include "helpers/TPCoasterHelpers.h"
#include "HoardingsBalance.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "LevelSystems.h"
#include "m3d2.h"
#include "MapCell.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TbSoundBank.h"
#include "ThingReceivers.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ui_main.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Four small leaves of the main app (SimTheme Park 2.04b3) that the effect manager (src/effect_manager.cpp) calls
// every pass: the real-time clock that stands still while the game is paused (RealTimePauseAdjusted,
// 0x10007c68, the inventory's Advisor.cpp range) and the table of sines (HOST_Sin and HOST_Cos, 0x100218f4 and
// 0x10021928, and BMP_GenerateSinTab, 0x100259d0, the inventory's TPAdvisor.cpp range). See
// include/effect_manager.h.

int32_t gHostSineTable[512];   // main app data 0x102b4ec0 (not exported; the name is invented)

namespace {

// BMP_GenerateSinTab's constants (main app data 0x102645a8..0x102645c8). The "pi" is the decimal 3.14159, not
// the double nearest to pi, so the table's quarter-turn entries come out one short of 256 (255 and -255).
constexpr double kPi = 3.14159;        // 0x102645c0
constexpr double kEntryAngle = 0.001953125;   // 0x102645a8: 2^-9, so that 2 i entries make 2 pi i / 512 turns
constexpr double kAmplitude = 256.0;   // 0x102645c8

}  // namespace

// The angle is in 512ths of a turn (only the low nine bits count, so negative angles and any multiple of a turn
// work); the answers are 1024 times the sine and cosine, rounded as the table's 8-bit steps are.
int32_t HOST_Sin(int32_t angle) {
    return static_cast<int32_t>(static_cast<uint32_t>(gHostSineTable[angle & 0x1ff]) << 2);
}

// The cosine is the sine a quarter turn (128 entries) on.
int32_t HOST_Cos(int32_t angle) {
    return static_cast<int32_t>(static_cast<uint32_t>(gHostSineTable[(static_cast<uint32_t>(angle) + 0x80) & 0x1ff]) << 2);
}

// Unit v5247: Write_BumpToChunk 0x10021960 and Write_CloseChunk 0x10021e44 (the advisor save's chunk writer), main app.

namespace {
// The file is little-endian: the Mac swaps each big-endian word before the write; on the host the word is already in file
// order, so the swap does nothing (as the reader's USwap32 calls, src/host_leaves_Write_BumpToChunk_BMP_Save_Read_BumpFromChunk.cpp).
uint32_t Swap(uint32_t v) { return v; }
}  // namespace

// The original copies 0xac bytes of the bump (the struct is 0xa8: the last word is read past it), swaps every word
// (the USwap calls) and writes them.
uint32_t Write_BumpToChunk(tabBMPCHUNK* chunk, tagBUMP& bump) {
    uint32_t words[0x2b];
    std::memcpy(words, &bump, 0xac);
    for (uint32_t& word : words) word = Swap(word);
    uint32_t transferred;   // uninitialised in the original too
    TbError result = LbFile_Write(chunk->file.get(), words, 0xac, &transferred);
    if (result.mValue < 0) return 0;
    chunk->dataSize += 0xac;
    return transferred;
}

int32_t Write_CloseChunk(tabBMPCHUNK* chunk) {
    int32_t position = LbFile_GetPosition(chunk->file.get());
    chunk->physicalSize = position - chunk->start;
    TRACE("CloseChunk: DataSize %d  PhysicalSize %d\n", chunk->dataSize, chunk->physicalSize);
    LbFile_Seek(chunk->file.get(), chunk->start, 0);
    uint32_t header[3];
    header[0] = Swap(chunk->tag);
    header[1] = Swap(static_cast<uint32_t>(chunk->dataSize));
    header[2] = Swap(static_cast<uint32_t>(chunk->physicalSize));
    uint32_t transferred;
    TbError result = LbFile_Write(chunk->file.get(), header, 0xc, &transferred);
    if (result.mValue >= 0) chunk->dataSize += 0xc;
    LbFile_Seek(chunk->file.get(), position, 0);
    int32_t answer = chunk->physicalSize;
    NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(chunk));
    return answer;
}

// The advisor save's chunk reader: Read_OpenChunk 0x10021fac, Read_DataFromChunk 0x100220bc, Read_BumpFromChunk 0x100221d4.
// The file is little-endian: the Mac swaps every word after reading it; on the host the words are already in order, so the
// swaps (SamsUtilities::USwap32 of include/TbSoundBank.h) do nothing.

// 0x100220bc: reads up to `size` bytes of the chunk's data into buffer (never past the chunk's end), zero fills what is missing,
// answers the bytes read, 0 when the read failed.
uint32_t Read_DataFromChunk(tabBMPCHUNK* chunk, void* buffer, uint32_t size) {
    uint32_t remaining = static_cast<uint32_t>(chunk->dataSize - static_cast<int32_t>(chunk->pad08));
    const uint32_t* count = remaining < size ? &remaining : &size;
    uint32_t transferred;
    TbError result = BMPFile_Read(chunk->file.get(), buffer, *count, &transferred);
    if (result.mValue < 0) {
        TRACE("#####Read_DataFromChunk - ReadFailed####\n");
        return 0;
    }
    chunk->pad08 += transferred;
    if (transferred < size) std::memset(static_cast<uint8_t*>(buffer) + transferred, 0, size - transferred);
    return transferred;
}

// 0x10021fac: makes a chunk at the file's position, reads its header (tag, data size, physical size) and answers the tag, 0 when
// the allocation fails.
uint32_t Read_OpenChunk(void* file, Ptr32<tabBMPCHUNK>* out) {
    tabBMPCHUNK* chunk = reinterpret_cast<tabBMPCHUNK*>(NS_MacDoze::GlobalAlloc(0x40, 0x18));
    if (chunk == nullptr) return 0;
    chunk->start = LbFile_GetPosition(file);
    chunk->file.set(static_cast<uint8_t*>(file));
    chunk->pad08 = 0;
    chunk->dataSize = 0xc;
    Read_DataFromChunk(chunk, &chunk->tag, 0xc);
    chunk->tag = SamsUtilities::USwap32(chunk->tag);
    chunk->dataSize = SamsUtilities::USwap32(chunk->dataSize);
    chunk->physicalSize = SamsUtilities::USwap32(chunk->physicalSize);
    out->set(chunk);
    return chunk->tag;
}

// 0x100221d4: reads the bump's 0xac bytes (43 words, the last one past tagBUMP), then swaps each word (a no-op here).
uint32_t Read_BumpFromChunk(tabBMPCHUNK* chunk, tagBUMP& bump) {
    uint32_t size = 0xac;
    uint32_t remaining = static_cast<uint32_t>(chunk->dataSize - static_cast<int32_t>(chunk->pad08));
    const uint32_t* count = remaining < 0xac ? &remaining : &size;
    uint32_t transferred;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&bump);
    TbError result = BMPFile_Read(chunk->file.get(), bytes, *count, &transferred);
    uint32_t answer;
    if (result.mValue >= 0) {
        chunk->pad08 += transferred;
        if (transferred < size) std::memset(bytes + transferred, 0, size - transferred);
        answer = transferred;
    } else {
        TRACE("#####Read_DataFromChunk - ReadFailed####\n");
        answer = 0;
    }
    for (uint32_t offset = 0; offset <= 0xa8; offset += 4) {
        uint32_t word;
        std::memcpy(&word, bytes + offset, 4);
        word = SamsUtilities::USwap32(word);
        std::memcpy(bytes + offset, &word, 4);
    }
    return answer;
}

// Unit v5630: the advisor save, BMP_Save (main app 0x10022464, 5068 B, TPAdvisor.cpp on the Mac).
// It writes the bump world as chunks: 1 the save, 2 the build time and date, then per live ride record 3 (its words), 4 per
// node of its 0xbc list, 5 per live bump of the ride (the bump, its collision object, its table entry) with 9 per node of the
// bump's 0x30 list, 7 per node of the ride's 0xc8 list, 8 per node of its 0xc4 list and 6 (the ride id alone).
// Every value is put in file order before the write (the file is little-endian); a chunk's header is written raw when it opens and
// again, swapped, by Write_CloseChunk.

// 0x1023cbfa and 0x1023cc03: the build's __TIME__ and __DATE__ (the original measures a second copy of each, at 0x1023c9c3
// and 0x1023c9cc, with strlen; the contents are the same).
extern const char gBMPSaveTime[] = "17:56:39";
extern const char gBMPSaveDate[] = "Jan  8 2001";

namespace {

const uint32_t kWorldMagic = 0x4a454647;   // 'JEFG'

// The file is little-endian: the Mac swaps each big-endian value before the write; on the host the value is already in file
// order, so the swap does nothing (a byte swap here wrote big-endian words that BMP_Load read back swapped; v5L15).
uint32_t Swap_2(uint32_t value) { return value; }

// A word of a record by its Mac offset.
uint32_t Word(const void* base, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

template <class T>
T* Follow(uint32_t ptr32) {
    Ptr32<T> pointer{ptr32};
    return pointer.get();
}

tagBUMPSAVEWORLD* World() { return reinterpret_cast<tagBUMPSAVEWORLD*>(&gColList); }

// The inlined chunk opener: a zeroed 0x18-byte block, the header written raw (12 bytes from the tag).
tabBMPCHUNK* OpenChunk(void* file, uint32_t tag) {
    tabBMPCHUNK* chunk = reinterpret_cast<tabBMPCHUNK*>(NS_MacDoze::GlobalAlloc(0x40, 0x18));
    if (chunk == nullptr) return nullptr;
    chunk->start = LbFile_GetPosition(file);
    chunk->file = static_cast<uint8_t*>(file);
    chunk->pad08 = 0;
    chunk->tag = tag;
    chunk->dataSize = 0;
    chunk->physicalSize = 0;
    uint32_t transferred;
    TbError result = LbFile_Write(chunk->file.get(), &chunk->tag, 0xc, &transferred);
    if (result.mValue >= 0) chunk->dataSize += 0xc;
    return chunk;
}

void WriteBytes(tabBMPCHUNK* chunk, const void* buffer, uint32_t length) {
    uint32_t transferred;
    TbError result = LbFile_Write(chunk->file.get(), buffer, length, &transferred);
    if (result.mValue >= 0) chunk->dataSize += static_cast<int32_t>(length);
}

void WriteWord(tabBMPCHUNK* chunk, uint32_t value) {
    uint32_t buffer = Swap_2(value);
    WriteBytes(chunk, &buffer, 4);
}

void WritePair(tabBMPCHUNK* chunk, uint32_t first, uint32_t second) {
    uint32_t buffer[2] = {Swap_2(first), Swap_2(second)};
    WriteBytes(chunk, buffer, 8);
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

}  // namespace

int32_t BMP_Save(void* file) {
    tagBUMPSAVEWORLD* world = World();
    if (world->magic != kWorldMagic) return 0;
    tabBMPCHUNK* save = OpenChunk(file, 1);
    if (save == nullptr) return 0;

    tabBMPCHUNK* chunk = OpenChunk(file, 2);
    if (chunk != nullptr) {
        WriteBytes(chunk, gBMPSaveTime, static_cast<uint32_t>(std::strlen(gBMPSaveTime)));
        WriteBytes(chunk, gBMPSaveDate, static_cast<uint32_t>(std::strlen(gBMPSaveDate) + 1));
        Write_CloseChunk(chunk);

        uint8_t* ride = reinterpret_cast<uint8_t*>(world->rides.get());
        if (ride != nullptr) {
            int32_t left = world->rideCount;
            while (left != 0) {
                if (Word(ride, 0) != 0) {
                    left--;
                    chunk = OpenChunk(file, 3);
                    WriteWord(chunk, RideId(ride));
                    WritePair(chunk, Word(ride, 0xb4), Word(ride, 0xb8));
                    WriteWord(chunk, Word(ride, 0xb0));
                    WriteWord(chunk, Word(ride, 0x10));
                    WriteWord(chunk, Word(ride, 0x1c));
                    WriteWord(chunk, Word(ride, 0x14));
                    WriteWord(chunk, Word(ride, 0x18));
                    WriteWord(chunk, Word(ride, 0x04));
                    WriteWord(chunk, Word(ride, 0x50));
                    Write_CloseChunk(chunk);

                    for (tagBUMPSAVETRACK* node = Follow<tagBUMPSAVETRACK>(Word(ride, 0xbc)); node != nullptr;
                         node = node->next.get()) {
                        chunk = OpenChunk(file, 4);
                        WriteWord(chunk, RideId(ride));
                        WriteWord(chunk, static_cast<uint32_t>(node->f04));
                        WritePair(chunk, static_cast<uint32_t>(node->f14), static_cast<uint32_t>(node->f18));
                        Write_CloseChunk(chunk);
                    }

                    uint8_t* slot = world->bumps.get();
                    int32_t bumpsLeft = world->bumpCount;
                    while (bumpsLeft != 0) {
                        tagBUMP* bump = reinterpret_cast<tagBUMP*>(slot);
                        if ((bump->flags & 1) != 0) {
                            bumpsLeft--;
                            if (bump->owner.mOffset == tpw::ArenaOffset(ride)) {
                                chunk = OpenChunk(file, 5);
                                WriteWord(chunk, RideId(ride));
                                Write_BumpToChunk(chunk, *bump);
                                WriteWord(chunk, static_cast<uint32_t>(bump->col->x));
                                WriteWord(chunk, static_cast<uint32_t>(bump->col->y));
                                int32_t index = static_cast<int32_t>(Word(slot, 0x7c));
                                if (index != -1) {
                                    uint32_t id = 0;
                                    if (world->magic == kWorldMagic) {
                                        tagBUMPSAVEENTRY* entry =
                                            Follow<tagBUMPSAVEENTRY>(world->table.mOffset + static_cast<uint32_t>(index * 0x1c));
                                        id = RideId(reinterpret_cast<uint8_t*>(entry->ride.get()));
                                    }
                                    WriteWord(chunk, id);
                                } else {
                                    WriteWord(chunk, 0);
                                }
                                // The table entry is read for index -1 as well (the word before the table's first entry).
                                index = static_cast<int32_t>(Word(slot, 0x7c));
                                tagBUMPSAVEENTRY* entry =
                                    Follow<tagBUMPSAVEENTRY>(world->table.mOffset + static_cast<uint32_t>(index * 0x1c));
                                WriteWord(chunk, static_cast<uint32_t>(entry->f08));
                                index = static_cast<int32_t>(Word(slot, 0x7c));
                                entry = Follow<tagBUMPSAVEENTRY>(world->table.mOffset + static_cast<uint32_t>(index * 0x1c));
                                WriteWord(chunk, static_cast<uint32_t>(entry->f0c));
                                Write_CloseChunk(chunk);

                                for (tagBUMPSAVELINK* link = Follow<tagBUMPSAVELINK>(Word(slot, 0x30)); link != nullptr;
                                     link = link->next.get()) {
                                    chunk = OpenChunk(file, 9);
                                    WriteWord(chunk, RideId(ride));
                                    WriteWord(chunk, static_cast<uint32_t>(link->value));
                                    WriteWord(chunk, static_cast<uint32_t>(link->value0c));
                                    Write_CloseChunk(chunk);
                                }
                            }
                        }
                        slot += 0xac;
                    }

                    for (tagBUMPSAVELINK* link = Follow<tagBUMPSAVELINK>(Word(ride, 0xc8)); link != nullptr;
                         link = link->next.get()) {
                        chunk = OpenChunk(file, 7);
                        WriteWord(chunk, RideId(ride));
                        WriteWord(chunk, static_cast<uint32_t>(link->value));
                        Write_CloseChunk(chunk);
                    }
                    for (tagBUMPSAVELINK* link = Follow<tagBUMPSAVELINK>(Word(ride, 0xc4)); link != nullptr;
                         link = link->next.get()) {
                        chunk = OpenChunk(file, 8);
                        WriteWord(chunk, RideId(ride));
                        WriteWord(chunk, static_cast<uint32_t>(link->value));
                        Write_CloseChunk(chunk);
                    }

                    chunk = OpenChunk(file, 6);
                    WriteWord(chunk, RideId(ride));
                    Write_CloseChunk(chunk);
                }
                ride += 0xd0;
            }
        }
    }
    return Write_CloseChunk(save);
}

// Unit v5L09: the advisor load, BMP_Load (main app 0x10023850, 8432 B, TPAdvisor.cpp on the Mac).
// It reads the chunks BMP_Save writes (src/host_leaves_Write_BumpToChunk_BMP_Save.cpp) with the chunk reader of unit v51017: the save
// chunk (tag 1), then chunk after chunk until the save's physical size is used up: 2 the build time (skipped), 3 a ride record
// (BMP_CreateRide and its settings), 4 a track section, 5 a bump of a ride (its block, collision object, ride instances and buoy),
// 6 the ride's buoy sequence, 7 and 8 the ride's leaving and joining peeps, 9 a node of the bump's list from the world's free list.
// Each chunk is closed by seeking past it and freeing it. Tags 0 and 1 and anything above 9 stop the load with 0 (the open chunks
// are not freed). The answer is the save chunk's physical size.
// The file is little-endian: the Mac swaps every word it reads; on the host the words are already in order.

namespace MathLib {
int __fpclassifyf(float x);   // glue 0x10239268
}

namespace {

void SetWord(void* base, uint32_t offset, uint32_t value) { std::memcpy(static_cast<uint8_t*>(base) + offset, &value, 4); }

// A 32-bit pointer field by its Mac offset.
template <class T>
T* Follow_2(const void* base, uint32_t offset) {
    Ptr32<T> pointer{Word(base, offset)};
    return pointer.get();
}

void SetPointer(void* base, uint32_t offset, const void* target) {
    Ptr32<const void> pointer;
    pointer.set(target);
    SetWord(base, offset, pointer.mOffset);
}

// The ride record an id names (its low byte is the index); NULL when the world is not live.
uint8_t* RideFromId(uint32_t id) {
    tagBUMPSAVEWORLD* world = World();
    if (world->magic != kWorldMagic) return nullptr;
    return reinterpret_cast<uint8_t*>(world->rides.get()) + (id & 0xff) * 0xd0;
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
    uint8_t* link = Follow_2<uint8_t>(world, 0x34);
    uint8_t* bump = Follow_2<uint8_t>(ride, 0xcc);
    SetWord(world, 0x34, Word(link, 0x10));
    SetWord(link, 0x10, Word(bump, 0x30));
    SetPointer(bump, 0x30, link);
    SetWord(link, 0, ReadWord(chunk));
    SetWord(link, 4, id);
    uint8_t* owner = Follow_2<uint8_t>(bump, 0x9c);
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

// Unit v51069: Advisor.cpp's static initialiser and save templates, and small leaves of the bump rides (TPAdvisor.cpp), the camera,
// the coasters and the lobby (main app).

namespace {

// A Ptr32 field of a structure read by Mac offset.
template <class T>
T* PtrAt(const void* base, size_t offset) {
    return reinterpret_cast<const Ptr32<T>*>(static_cast<const uint8_t*>(base) + offset)->get();
}

}  // namespace

// SAVEVAR<AdvisorMessage> 0x1000f820 and LOADVAR<AdvisorMessage> 0x1000f250 are lead unit v5L05's
// (src/Advisor_AdvisorMessage_LOADVAR_SAVEVAR.cpp, test_AdvisorMessageVar).

// ---- the bump rides
// 0x10025960: the index of the ride's buoy at (x, y), 0 when there is none.
int32_t FindBuoy(tagBUMPRIDE* ride, int32_t x, int32_t y) {
    for (tabBUOY* buoy = PtrAt<tabBUOY>(ride, 0xa8); buoy != nullptr; buoy = buoy->next.get()) {
        if (buoy->x == x && buoy->y == y) return buoy->index;
    }
    return 0;
}

// Fills the table: entry i is 256 * sin(3.14159 * 2i * 2^-9) cut toward zero. (The double product is taken in
// the listing's order: pi times 2i first, then the scale.)
void BMP_GenerateSinTab() {
    for (int32_t i = 0; i < 0x200; ++i) {
        const double angle = kPi * static_cast<double>(i << 1) * kEntryAngle;
        gHostSineTable[i] = ppc_fctiwz(kAmplitude * std::sin(angle));
    }
}

// Unit v5659: the advisor's bump world (main app, TPAdvisor.cpp): BMP_InitSystem 0x10025a94, BMP_CloseSystem 0x10025cb0, BMP_FreeAll 0x10025db8,
// BMP_OpenRide 0x10025ee4, BMP_CloseRide 0x10026118. The world is the global at 0x102b56c0 (gColList in BMP_InsideArea.h); its 0x3c bytes are
// viewed here by Mac offset (pointer fields are Ptr32).

int32_t Rand();   // 0x10239148 rand (ui_menu.h)

namespace {

constexpr uint32_t kMagic = 0x4a454647;

struct Link {                // 0x14 bytes
    uint32_t pad00[4];       // 0x00
    Ptr32<Link> next;        // 0x10
};

struct BumpWorld {
    uint32_t magic;          // 0x00
    uint32_t tick;           // 0x04
    Ptr32<uint8_t> rides;    // 0x08 0xd0 bytes each
    int32_t rideCount;       // 0x0c
    Ptr32<uint8_t> bumps;    // 0x10 0xac bytes each
    int32_t bumpCount;       // 0x14
    Ptr32<uint8_t> block18;  // 0x18
    uint32_t pad1c;          // 0x1c
    Ptr32<uint8_t> block20;  // 0x20
    uint32_t pad24;          // 0x24
    Ptr32<uint8_t> block28;  // 0x28
    uint32_t pad2c;          // 0x2c
    Ptr32<Link> links;       // 0x30 1024 records of 0x14 bytes
    Ptr32<Link> freeHead;    // 0x34
    int32_t seed;            // 0x38
};
static_assert(sizeof(BumpWorld) == 0x3c, "BumpWorld");

BumpWorld& World_2() { return *reinterpret_cast<BumpWorld*>(&gColList); }

template <class T>
T& At_2(void* base, uint32_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

// The ride record at the id's low byte, NULL when the world is not live.
uint8_t* RideOf(int32_t ride) {
    BumpWorld& world = World_2();
    if (world.magic != kMagic) return nullptr;
    return world.rides.get() + static_cast<uint32_t>(ride & 0xff) * 0xd0;
}

// The id of a ride record: kind << 8 | index, 0 when the world is not live.
int32_t IdOf(uint8_t* record) {
    BumpWorld& world = World_2();
    if (world.magic != kMagic) return 0;
    int32_t index = static_cast<int32_t>(record - world.rides.get()) / 0xd0;
    return (At_2<int32_t>(record, 0) << 8) | index;
}

// Links the 1024 records into a chain, each pointing at the next, the last at NULL.
void RebuildLinks() {
    BumpWorld& world = World_2();
    Link* previous = nullptr;
    for (int32_t i = 0x3ff; i >= 0; --i) {
        Link* link = reinterpret_cast<Link*>(reinterpret_cast<uint8_t*>(world.links.get()) + i * 0x14);
        link->next = previous;
        previous = link;
    }
}

Ptr32<uint8_t> Allocate(uint32_t size) {
    Ptr32<uint8_t> result;
    result.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalAlloc(0x40, size)));
    return result;
}

// The jump tables (0x1029c98c, 0x1029c9b4): what each kind does.
// Open, by kind + 13 (kinds -13 to -4): 0x10026014 sets state 2, 0x10025fa4 state 1 (both only from state 0, after SetBuoySequence), 0x10026084 state 1.
enum OpenAction : uint8_t { kOpenStart1, kOpenStart2, kOpenDefault };
const OpenAction kOpenTable[10] = {
    kOpenStart2, kOpenStart2, kOpenDefault, kOpenStart1, kOpenStart1,
    kOpenStart2, kOpenStart1, kOpenDefault, kOpenStart2, kOpenStart1,
};
// Close, by kind + 14 (kinds -14 to -1): true for the kinds -14, -11, -6, -3 and -1, which release the ride's bumps.
const bool kCloseTable[14] = {true, false, false, true, false, false, false, false, true, false, false, true, false, true};

}  // namespace

void BMP_InitSystem() {
    BumpWorld& world = World_2();
    for (int32_t i = 0; i < 0x200; ++i) {
        const double angle = 3.14159 * static_cast<double>(i << 1) * 0.001953125;
        gHostSineTable[i] = ppc_fctiwz(256.0 * std::sin(angle));
    }
    std::memset(&world, 0, 0x3c);
    world.magic = kMagic;
    world.tick = 0;
    world.rides = Allocate(0x3400);
    world.bumps = Allocate(0xac00);
    world.block18 = Allocate(0x10000);
    world.block20 = Allocate(0x7000);
    world.block28 = Allocate(0xa000);
    world.links.set(reinterpret_cast<Link*>(Allocate(0x5000).get()));
    RebuildLinks();
    world.freeHead = world.links;
    world.seed = Rand();
}

void BMP_FreeAll() {
    BumpWorld& world = World_2();
    if (world.magic != kMagic) return;
    int32_t remaining = world.rideCount;
    uint8_t* record = world.rides.get();
    while (remaining != 0) {
        if (At_2<int32_t>(record, 0) != 0) {
            --remaining;
            At_2<int32_t>(record, 0x50) = 4;
            BMP_DestroyRide(IdOf(record));
        }
        record += 0xd0;
    }
    RebuildLinks();
    world.freeHead = world.links;
}

void BMP_CloseSystem() {
    BumpWorld& world = World_2();
    if (world.magic != kMagic) return;
    BMP_FreeAll();
    world.block28.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.block28.get()))));
    world.block20.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.block20.get()))));
    world.block18.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.block18.get()))));
    world.bumps.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.bumps.get()))));
    world.rides.set(reinterpret_cast<uint8_t*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.rides.get()))));
    world.links.set(reinterpret_cast<Link*>(NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(world.links.get()))));
    std::memset(&world, 0, 0x3c);
}

int32_t BMP_OpenRide(int32_t ride) {
    BumpWorld& world = World_2();
    uint8_t* record = RideOf(ride);
    if (ride != IdOf(record)) return 0;
    TRACE("Open ride %d\n", ride);
    uint32_t index = static_cast<uint32_t>(At_2<int32_t>(record, 0) + 13);
    OpenAction action = index <= 9 ? kOpenTable[index] : kOpenDefault;
    if (action == kOpenDefault) {
        At_2<int32_t>(record, 0x50) = 1;
    } else {
        if (At_2<int32_t>(record, 0x50) != 0) return 0;
        SetBuoySequence(reinterpret_cast<tagBUMPRIDE*>(record));
        At_2<int32_t>(record, 0x50) = action == kOpenStart1 ? 1 : 2;
    }
    BMP_SetRidePerformance(IdOf(record), At_2<int32_t>(record, 0x1c));
    (void)world;
    return 0;
}

int32_t BMP_CloseRide(int32_t ride) {
    BumpWorld& world = World_2();
    uint8_t* record = RideOf(ride);
    if (ride != IdOf(record)) return 0;
    if (At_2<int32_t>(record, 0x50) == 0) return 0;
    At_2<int32_t>(record, 0x50) = 0;
    TRACE("Close ride %d\n", ride);
    Link* tail = reinterpret_cast<Ptr32<Link>*>(record + 0xc8)->get();
    Ptr32<Link>& source = At_2<Ptr32<Link>>(record, 0xc4);
    if (tail == nullptr) {
        At_2<Ptr32<Link>>(record, 0xc8) = source;
    } else {
        while (tail->next.get() != nullptr) tail = tail->next.get();
        tail->next = source;
    }
    source.mOffset = 0;
    uint32_t index = static_cast<uint32_t>(At_2<int32_t>(record, 0) + 14);
    if (index <= 13 && kCloseTable[index]) {
        uint8_t* bump = world.bumps.get();
        if (bump != nullptr) {
            int32_t remaining = world.bumpCount;
            while (remaining != 0) {
                uint32_t& flags = At_2<uint32_t>(bump, 0);
                if ((flags & 1) != 0) {
                    --remaining;
                    if (At_2<Ptr32<uint8_t>>(bump, 0x9c).get() == record) {
                        flags &= ~0x4000u;
                        flags &= 0xffffff81u;
                        flags |= 0x10000u;
                        flags |= 0x20u;
                        At_2<int32_t>(bump, 0x88) = 0;
                    }
                }
                bump += 0xac;
            }
        }
    }
    return 0;
}

// Unit v5220: the ride commands of the bump world (halt, break, fix, begin), main app.

namespace {

tagBUMPWORLD* World_3() { return reinterpret_cast<tagBUMPWORLD*>(&gColList); }

// The ride record of a ride id, or null when the world is not live; answers the id the record holds for it (kind << 8 | index).
tagBUMPRIDE* FindRide(int32_t ride, int32_t* id) {
    tagBUMPWORLD* world = World_3();
    if (world->magic != 0x4a454647u) {
        *id = 0;
        return nullptr;
    }
    uint32_t index = static_cast<uint32_t>(ride) & 0xffu;
    tagBUMPRIDE* record = reinterpret_cast<tagBUMPRIDE*>(reinterpret_cast<uint8_t*>(world->rides.get()) + index * 0xd0u);
    *id = static_cast<int32_t>((static_cast<uint32_t>(record->kind) << 8) | index);
    return record;
}

// The kinds the break and fix commands act on: -14 to -3 and -1.
bool KindActs(int32_t kind) {
    if (kind == -2) return false;
    if (kind < -2) return kind >= -0xe;
    return kind < 0;
}

// Sets the bump's animation state, flushes its set and starts the animation.
void StartAnim(tagBUMPSLOT* slot, uint32_t state, uint32_t loop) {
    slot->animState = state;
    if (slot->animSet != 0) FlushDeferredAnim(gRideAnimSets[slot->animSet], 0);
    if (slot->animSet != 0) SetCurRideAnim(gRideAnimSets[slot->animSet], state, 0, loop, 1.0f, 0);
}

}  // namespace

int32_t BMP_HaltRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = FindRide(ride, &id);
    if (ride == id) {
        tagBUMPWORLD* world = World_3();
        tagBUMPSLOT* slot = world->bumps.get();
        if (slot != nullptr) {
            int32_t count = world->bumpCount;
            while (count != 0) {
                if (slot->flags & 1) {
                    --count;
                    if (slot->owner.get() == record) BMP_DOCK(reinterpret_cast<tagBUMP*>(slot));
                }
                slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
            }
        }
        BMP_CloseRide(ride);
    }
    return 0;
}

// Unit v51036: ride commands on the bump world (main app): BMP_SetRideDuration, BMP_RideWorn, BMP_GetCameraMesh, BMP_StartBump,
// BMP_StartBuoy, BMP_AddBuoyToRide, and the static initialiser of HoardingsBalance.cpp.

namespace {

constexpr uint32_t kWorldMagic_2 = 0x4a454647;

uint8_t* World_4() { return reinterpret_cast<uint8_t*>(&gColList); }

uint32_t& W(void* base, uint32_t offset) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(base) + offset); }

Ptr32<uint8_t>& P(void* base, uint32_t offset) {
    return *reinterpret_cast<Ptr32<uint8_t>*>(static_cast<uint8_t*>(base) + offset);
}

// The ride record the id names (NULL when the world is not live) and the id it holds: kind << 8 | index.
uint8_t* LookupRide(int32_t ride, int32_t* id) {
    uint8_t* world = World_4();
    uint8_t* record = nullptr;
    if (W(world, 0) == kWorldMagic_2) record = P(world, 8).get() + static_cast<uint32_t>(ride & 0xff) * 0xd0;
    if (W(world, 0) == kWorldMagic_2) {
        int32_t index = static_cast<int32_t>(record - P(world, 8).get()) / 0xd0;
        *id = static_cast<int32_t>((W(record, 0) << 8) | static_cast<uint32_t>(index));
    } else {
        *id = 0;
    }
    return record;
}

}  // namespace

// 0x100263f4
int32_t BMP_RideWorn(int32_t ride) {
    int32_t id;
    uint8_t* record = LookupRide(ride, &id);
    if (ride == id) {
        W(record, 0x54) = 1;
        TRACE("Ride worn out\n");
    }
    return 0;
}

int32_t BMP_BreakRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = FindRide(ride, &id);
    if (ride == id) {
        record->broken = 2;
        TRACE("Ride broken\n");
        if (!KindActs(record->kind)) return 0;
        tagBUMPWORLD* world = World_3();
        tagBUMPSLOT* slot = world->bumps.get();
        int32_t count = world->bumpCount;
        while (count != 0) {
            if (slot->flags & 1) {
                --count;
                if (slot->owner.get() == record && (slot->flags & 0x4000) && slot->animIndex != -1 && slot->emitter == -1) {
                    tag_sRideAnimSet* set = gRideAnimSets[slot->animSet];
                    tagBumpHeadTable* table = static_cast<tagBumpHeadTable*>(set->mHeads->mF28);
                    tagBumpHeadNode* node = table->entries[slot->animIndex].node;
                    float x = 0.0f, y = 0.0f, z = 0.0f;
                    if (node != nullptr) {
                        x = node->x;
                        y = node->y;
                        z = node->z;
                    }
                    float sx = 1024.0f * x;
                    float sy = 1024.0f * y;
                    float sz = 1024.0f * z;
                    slot->emitter = static_cast<int32_t>(PAR_InitEmitter(2, ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz)));
                    if (slot->owner->kind == -1) StartAnim(slot, 0xc, 2);
                }
            }
            slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
        }
    }
    return 0;
}

int32_t BMP_FixRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = FindRide(ride, &id);
    if (ride == id) {
        if (record->broken != 2) {
            record->broken = 0;
            return 0;
        }
        record->broken = 0;
        TRACE("Ride Fixed\n");
        if (!KindActs(record->kind)) return 0;
        tagBUMPWORLD* world = World_3();
        tagBUMPSLOT* slot = world->bumps.get();
        int32_t count = world->bumpCount;
        while (count != 0) {
            if (slot->flags & 1) {
                --count;
                if (slot->owner.get() == record && slot->emitter != -1) {
                    PAR_KillEmitter(slot->emitter);
                    slot->emitter = -1;
                    if (slot->owner->kind == -1 && (slot->flags & 0x4000)) StartAnim(slot, 5, 1);
                }
            }
            slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
        }
    }
    return 0;
}

int32_t BMP_BeginRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = FindRide(ride, &id);
    // The jump table at 0x1029c9ec: kinds -14 to -1; -13, -12, -8, -5 and -2 go to the return, the rest to the body.
    constexpr uint32_t kBodyKinds = (1u << 0) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 10) | (1u << 11) | (1u << 13);
    uint32_t tableIndex = ride == id ? static_cast<uint32_t>(record->kind + 0xe) : 0xffu;
    if (tableIndex < 0xe && ((kBodyKinds >> tableIndex) & 1u)) {
        if (record->state == 1) {
            record->state = 2;
            tagBUMPWORLD* world = World_3();
            tagBUMPSLOT* slot = world->bumps.get();
            int32_t count = world->bumpCount;
            while (count != 0) {
                if (slot->flags & 1) {
                    --count;
                    if (slot->owner.get() == record) {
                        slot->flags &= 0xfff7ffbfu;
                        slot->f88 = record->f04;
                        SetControlType(record, reinterpret_cast<tagBUMP*>(slot));
                        if (record->kind == -1 && (slot->flags & 0x4000)) StartAnim(slot, 5, 1);
                    }
                }
                slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
            }
        }
        TRACE("Start Bump Ride\n");
    }
    return 0;
}

// 0x10026b14
int32_t BMP_SetRideDuration(int32_t ride, int32_t duration) {
    int32_t id;
    uint8_t* record = LookupRide(ride, &id);
    if (ride == id) {
        W(record, 4) = static_cast<uint32_t>(duration);
        TRACE("Set Ride Duration to %d\n", duration);   // the original leaves the duration in r4
    }
    return 0;
}

namespace {

uint8_t* WorldBytes() { return reinterpret_cast<uint8_t*>(&gColList); }
uint32_t& Wd(uint8_t* base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }

// The id of a ride record (kind << 8 | index), index = (record - records) / 0xd0 by the compiler's multiply.
uint32_t IdOfRide(uint8_t* world, uint32_t rideOffset) {
    Ptr32<uint8_t> recs{Wd(world, 8)};
    int32_t diff = static_cast<int32_t>(rideOffset - recs.mOffset);
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(0x4ec4ec4f) * diff) >> 32) >> 6;
    q += static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
    Ptr32<uint8_t> ride{rideOffset};
    return (Wd(ride.get(), 0) << 8) | static_cast<uint32_t>(q);
}

// The record slot of an id (null when the world is dead) and its recomputed id (0 when dead).
uint8_t* RideOf_2(int32_t id, uint32_t* outId) {
    uint8_t* world = WorldBytes();
    if (Wd(world, 0) == 0x4a454647u) {
        Ptr32<uint8_t> recs{Wd(world, 8)};
        uint8_t* ride = recs.get() + (static_cast<uint32_t>(id) & 0xff) * 0xd0;
        *outId = IdOfRide(world, recs.mOffset + (static_cast<uint32_t>(id) & 0xff) * 0xd0);
        return ride;
    }
    *outId = 0;
    return nullptr;
}

int32_t RideEventHandle(uint32_t rideId) {
    uint16_t thing;
    TrackRideIdToThingId(&thing, static_cast<int32_t>(rideId));
    if (thing == gAdvisorThingId) return 0;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    return static_cast<int32_t>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(gThingArray[thing].mThing) + 0x20));
}

uint32_t RideIdOfBump(uint8_t* bump) {
    uint8_t* world = WorldBytes();
    if (Wd(world, 0) == 0x4a454647u) return IdOfRide(world, Wd(bump, 0x9c));
    return 0;
}

}  // namespace

void BMP_SetRideModelSelection(int32_t ride, int32_t a, int32_t b) {
    uint32_t id;
    uint8_t* rec = RideOf_2(ride, &id);
    if (static_cast<uint32_t>(ride) != id) return;
    Wd(rec, 0x14) = static_cast<uint32_t>(a);
    Wd(rec, 0x18) = static_cast<uint32_t>(b);
}

// Unit v5344: the ride track counting, performance and teardown of the advisor world, main app.
//   BMP_CountTrackSections 0x10026f20, BMP_FreeTrack 0x100270e0, BMP_SetRidePerformance 0x10026c98, BMP_DestroyRide 0x10027274

int32_t gRidePerfMax = 100;   // 0x1029ca24
int32_t gRidePerfMin = 0;     // 0x1029ca28

namespace {

constexpr uint32_t kWorldMagic_3 = 0x4a454647u;

tagBUMPWORLD* WorldOf() { return reinterpret_cast<tagBUMPWORLD*>(&gColList); }

// The record of a ride id and the id the record holds (kind << 8 | index); NULL and 0 when the world is not live.
tagBUMPRIDE* LookupRide_2(int32_t ride, int32_t* id) {
    tagBUMPWORLD* world = WorldOf();
    if (world->magic == kWorldMagic_3) {
        tagBUMPRIDE* rides = world->rides.get();
        tagBUMPRIDE* record = rides + (static_cast<uint32_t>(ride) & 0xffu);
        int32_t diff = static_cast<int32_t>(reinterpret_cast<uint8_t*>(record) - reinterpret_cast<uint8_t*>(rides));
        *id = static_cast<int32_t>((static_cast<uint32_t>(record->kind) << 8) | static_cast<uint32_t>(diff / 0xd0));
        return record;
    }
    *id = 0;
    return nullptr;
}

// Docks every live bump owned by the record, then closes the ride.
void DockAndClose(int32_t ride, tagBUMPRIDE* owner) {
    tagBUMPWORLD* world = WorldOf();
    tagBUMPSLOT* slot = world->bumps.get();
    if (slot != nullptr) {
        int32_t count = world->bumpCount;
        while (count != 0) {
            if (slot->flags & 1) {
                --count;
                if (slot->owner.get() == owner) BMP_DOCK(reinterpret_cast<tagBUMP*>(slot));
            }
            slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
        }
    }
    BMP_CloseRide(ride);
}

uint32_t Word_2(const void* base, uint32_t offset) { return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(base) + offset); }

// A pointer word of a track node (an arena offset, 0 null).
const uint8_t* NodeAt(const void* base, uint32_t offset) { return Ptr32<const uint8_t>{Word_2(base, offset)}.get(); }

}  // namespace

void BMP_SetRidePerformance(int32_t ride, int32_t speed) {
    int32_t id;
    tagBUMPRIDE* record = LookupRide_2(ride, &id);
    if (ride != id) return;
    uint8_t* r = reinterpret_cast<uint8_t*>(record);
    int32_t v = gRidePerfMax < speed ? gRidePerfMax : speed;
    v = gRidePerfMin < v ? v : gRidePerfMin;
    *reinterpret_cast<int32_t*>(r + 0x1c) = v;
    TRACE("Set ride performance to %d\n", *reinterpret_cast<int32_t*>(r + 0x1c));
    for (uint32_t i = 0; i < 4; ++i) {
        int32_t lo = *reinterpret_cast<int32_t*>(r + 0x30 + 4 * i);
        int32_t hi = *reinterpret_cast<int32_t*>(r + 0x40 + 4 * i);
        *reinterpret_cast<int32_t*>(r + 0x20 + 4 * i) = lo + static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(hi - lo)) / 100;
    }
}

int32_t BMP_GetCarsOnRide(int32_t ride) {
    uint32_t id;
    uint8_t* rec = RideOf_2(ride, &id);
    if (static_cast<uint32_t>(ride) != id) return 0;
    return static_cast<int32_t>(Wd(rec, 0x5c));
}

int32_t BMP_CountTrackSections(int32_t ride, int32_t* a, int32_t* b, int32_t* c, int32_t* d) {
    int32_t id;
    tagBUMPRIDE* record = LookupRide_2(ride, &id);
    if (ride != id) return 0;
    int32_t run = 0, ends = 0, longest = 0, crossings = 0, plain = 0, total = 0;
    for (int32_t pass = 2; pass != 0; --pass) {
        for (const uint8_t* node = NodeAt(record, 0xbc); node != nullptr; node = NodeAt(node, 0x24)) {
            uint32_t type = Word_2(node, 4) & 0xffffu;
            if (type == 11) {
                ++run;
                ++crossings;
            } else if (type < 11 && type >= 9) {
                ++run;
            } else if (type > 11 && type < 14) {
                if (Word_2(node, 8) == 0) ++plain;
                ++run;
            } else {
                if (run > longest) longest = run;
                run = 0;
                ++ends;
            }
            ++total;
        }
    }
    if (a != nullptr) *a = ends / 2;
    if (b != nullptr) *b = longest;
    if (c != nullptr) *c = crossings / 2;
    if (d != nullptr) *d = plain;
    return total / 2;
}

void BMP_FreeTrack(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = LookupRide_2(ride, &id);
    if (ride == id) {
        tagBUMPRIDE* owner = LookupRide_2(ride, &id);
        if (ride == id) DockAndClose(ride, owner);
        FreeBuoysForRide(record, 6);
        FreeTrackForRide(record);
    }
}

void BMP_DestroyRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = LookupRide_2(ride, &id);
    if (ride == id) {
        tagBUMPRIDE* owner = LookupRide_2(ride, &id);
        if (ride == id) DockAndClose(ride, owner);
        FreeBuoysForRide(record, 0);
        FreeTrackForRide(record);
        FreeColObjectsForRide(record);
        record->kind = 0;
        uint32_t* words = reinterpret_cast<uint32_t*>(&gColList);
        words[3] = words[3] - 1;
    }
}

// 0x10027424: the ride's collision objects are freed (flags cleared, the list's count lowered).
void FreeColObjectsForRide(tagBUMPRIDE* ride) {
    tabCOLOBJECT* object = PtrAt<tabCOLOBJECT>(ride, 0xc0);
    if (object == nullptr) return;
    do {
        gColList.count = gColList.count - 1;
        object->flags = 0;
        object = PtrAt<tabCOLOBJECT>(object, 0x14);
    } while (object != nullptr);
}

void FreeBuoysForRide(tagBUMPRIDE* r, int32_t n) {
    uint8_t* ride = reinterpret_cast<uint8_t*>(r);
    uint8_t* world = WorldBytes();
    Ptr32<uint8_t> headp{Wd(ride, 0xa8)};
    uint8_t* cur = headp.get();
    if (cur == nullptr) return;
    uint8_t* keep = nullptr;
    while (cur != nullptr) {
        if (n == 0 || (Wd(cur, 0) & static_cast<uint32_t>(n)) == 0) {
            Wd(ride, 0xac) -= 1;
            Wd(world, 0x24) -= 1;
            Wd(cur, 0) &= 0xfffffffeu;
        } else if (keep == nullptr) {
            keep = cur;
        } else {
            uint8_t* p = keep;
            for (;;) {
                Ptr32<uint8_t> nx{Wd(p, 0x14)};
                if (nx.get() == nullptr) break;
                p = nx.get();
            }
            Ptr32<uint8_t> link;
            link.set(cur);
            Wd(p, 0x14) = link.mOffset;
        }
        Ptr32<uint8_t> next{Wd(cur, 0x14)};
        Wd(cur, 0x14) = 0;
        cur = next.get();
    }
    Ptr32<uint8_t> nh;
    nh.set(keep);
    Wd(ride, 0xa8) = nh.mOffset;
}

// BMP_CreateRide 0x1002756c: takes a ride record (a free one, or the one an id names), copies the kind's template into it, sets
// it up by the kind (collision objects, buoys, performance) and answers its id (kind << 8 | index), 0 on a dead world.
// The ride record is read by Mac offset (words). The listing's jump tables (0x1029ca64 on the kind, 0x1029ca2c on the kind again)
// are kTop and kSub below.

extern int32_t gHostSineTable[512];   // 0x102b4ec0

namespace {

enum Target { kEnd = 0, kShip, kSub, kTrack, kSingle, kS1, kS2, kS3 };

// The jump tables of the listing, indexed by kind + 14.
const uint8_t kTop[14] = { kSub, kSingle, kSingle, kSub, kTrack, kTrack, kSingle, kTrack, kSub, kSingle, kTrack, kSub, kShip, kSub };
const uint8_t kSubTable[14] = { kS3, kS3, kS3, kS2, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS1 };

int32_t MulHw(int32_t a, int32_t b) { return static_cast<int32_t>((static_cast<int64_t>(a) * b) >> 32); }

// srawi n, 8 then addze: a division by 256 cut toward zero.
int32_t Div256(int32_t n) {
    int32_t q = n >> 8;
    if (n < 0 && (n & 0xff) != 0) q++;
    return q;
}

// The quarter turn step of the buoy rings: (a, b) rotated by the sine table's c and s.
void Rotate(int32_t& a, int32_t& b, int32_t c, int32_t s) {
    uint32_t ua = static_cast<uint32_t>(a), ub = static_cast<uint32_t>(b);
    uint32_t uc = static_cast<uint32_t>(c), us = static_cast<uint32_t>(s);
    int32_t na = Div256(static_cast<int32_t>(ua * uc + ub * us));
    int32_t nb = Div256(static_cast<int32_t>(ub * uc - ua * us));
    a = na;
    b = nb;
}

uint32_t* Words(void* p) { return reinterpret_cast<uint32_t*>(p); }
uint32_t* World_5() { return Words(&gColList); }
uint32_t* Rides() { return reinterpret_cast<uint32_t*>(reinterpret_cast<tagBUMPWORLD*>(&gColList)->rides.get()); }

// kind << 8 | index of the ride in the list, 0 when the world is dead (the listing leaves a stale value there).
int32_t IdOf_2(uint32_t* ride) {
    if (World_5()[0] != 0x4a454647u) return 0;
    int32_t diff = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ride)) - static_cast<uint32_t>(reinterpret_cast<uintptr_t>(Rides())));
    int32_t q = MulHw(0x4ec4ec4f, diff) >> 6;
    q += static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
    return static_cast<int32_t>((ride[0] << 8) | static_cast<uint32_t>(q));
}

uint32_t* RideOfId(int32_t id) {
    if (World_5()[0] != 0x4a454647u) return nullptr;
    return reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(Rides()) + (static_cast<uint32_t>(id) & 0xff) * 0xd0);
}

// The performance step at the end of every kind: the ride's word 0x1c clamped, traced, and the four blends 0x20.. = lo + v * (hi - lo) / 100.
void SetPerformance(uint32_t* ride) {
    int32_t v = static_cast<int32_t>(ride[0x1c / 4]);
    int32_t id = IdOf_2(ride);
    uint32_t* r = RideOfId(id);
    if (id != IdOf_2(r)) return;
    const int32_t* pick = &gRidePerfMax;
    if (!(gRidePerfMax < v)) pick = &v;
    if (!(gRidePerfMin < *pick)) pick = &gRidePerfMin;
    v = *pick;
    r[0x1c / 4] = static_cast<uint32_t>(v);
    TRACE("Set ride performance to %d\n", static_cast<int32_t>(r[0x1c / 4]));
    for (int k = 0; k < 4; k++) {
        int32_t lo = static_cast<int32_t>(r[0x30 / 4 + k]);
        int32_t hi = static_cast<int32_t>(r[0x40 / 4 + k]);
        int32_t prod = static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(hi - lo));
        int32_t h = MulHw(0x51eb851f, prod) >> 5;
        h += static_cast<int32_t>(static_cast<uint32_t>(h) >> 31);
        r[0x20 / 4 + k] = static_cast<uint32_t>(lo + h);
    }
}

// The ring of eight buoys around (x, y) shared by the three sub kinds.
void RingOfBuoys(uint32_t* ride, int32_t x, int32_t y, int32_t dx, int32_t dy) {
    ride[0x90 / 4] = static_cast<uint32_t>(x);
    ride[0x74 / 4] = static_cast<uint32_t>(x);
    ride[0x94 / 4] = static_cast<uint32_t>(y);
    ride[0x78 / 4] = static_cast<uint32_t>(y);
    for (int i = 8; i != 0; i--) {
        BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), x + dx, y + dy);
        Rotate(dx, dy, gHostSineTable[0x300 / 4], gHostSineTable[0x100 / 4]);
    }
    ride[0x50 / 4] = 0;
    SetPerformance(ride);
}

// The corner offsets the track kinds use from the ride's rotation word (bit 0 swaps, bit 1 negates the first, bit 2 the second).
void Orient(uint32_t rot, int32_t& a, int32_t& b) {
    if (rot & 1) { a = 0; b = -0xc00; } else { a = -0xc00; b = 0; }
    if (rot & 2) a = -a;
    if (rot & 4) b = -b;
}

void Dispatch(int target, uint32_t* ride, int32_t x, int32_t y);

void Ship(uint32_t* ride) {
    const int32_t base = 0x10000;
    BMP_StartColObject(base - 0x4c00, base - 0x6a00, 0x1e00);
    tabCOLOBJECT* o = BMP_StartColObject(base - 0x4c00, base - 0x6a00, 0x1400);
    o->flags |= 0x40;
    BMP_StartColObject(base - 0x7000, 0x7200, 0x2600);
    int32_t a = 0, b = -0x1800;
    for (int i = 7; i != 0; i--) {
        BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), a + base - 0x4c00, b + base - 0x6a00);
        Rotate(a, b, gHostSineTable[0x100 / 4], gHostSineTable[0x700 / 4]);
    }
    a = 0;
    b = 0x2000;
    for (int i = 7; i != 0; i--) {
        BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), a + base - 0x7000, b + 0x7200);
        Rotate(a, b, gHostSineTable[0x100 / 4], gHostSineTable[0x700 / 4]);
    }
    ride[0x50 / 4] = 2;
}

void Track(uint32_t* ride, int32_t x, int32_t y) {
    ride[0x90 / 4] = static_cast<uint32_t>(x);
    ride[0x74 / 4] = static_cast<uint32_t>(x);
    ride[0x94 / 4] = static_cast<uint32_t>(y);
    ride[0x78 / 4] = static_cast<uint32_t>(y);
    for (int32_t i = 0; i < static_cast<int32_t>(ride[0x64 / 4]); i++) {
        int32_t n = static_cast<int32_t>(ride[0x64 / 4]);
        uint32_t rot = ride[0xb0 / 4];
        int32_t a = (n - i) * 1024 - 0x800;
        int32_t b = 0x400 - ((i << 11) & 0x800);
        if (rot & 1) { int32_t t = a; a = b; b = t; }
        if (rot & 2) a = -a;
        if (rot & 4) b = -b;
        uint32_t* buoy = Words(BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), x + a, y + b));
        buoy[0] |= 2;
    }
    int32_t a, b;
    Orient(ride[0xb0 / 4], a, b);
    uint32_t* buoy = Words(BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), x + a, y + b));
    buoy[0] |= 4;
    ride[0x50 / 4] = 0;
    SetPerformance(ride);
}

void Single(uint32_t* ride, int32_t x, int32_t y) {
    ride[0x74 / 4] = static_cast<uint32_t>(x);
    ride[0x78 / 4] = static_cast<uint32_t>(y);
    int32_t a, b;
    Orient(ride[0xb0 / 4], a, b);
    uint32_t* buoy = Words(BMP_AddBuoyToRide(reinterpret_cast<tagBUMPRIDE*>(ride), x + a, y + b));
    buoy[0] |= 4;
    ride[0x90 / 4] = ride[0x74 / 4] + static_cast<uint32_t>(MulHw(0x55555556, a) + static_cast<int32_t>(static_cast<uint32_t>(MulHw(0x55555556, a)) >> 31));
    ride[0x94 / 4] = ride[0x78 / 4] + static_cast<uint32_t>(MulHw(0x55555556, b) + static_cast<int32_t>(static_cast<uint32_t>(MulHw(0x55555556, b)) >> 31));
    ride[0x50 / 4] = 0;
    SetPerformance(ride);
}

void Dispatch(int target, uint32_t* ride, int32_t x, int32_t y) {
    switch (target) {
    case kShip: Ship(ride); break;
    case kSub: {
        uint32_t idx = ride[0] + 14;
        Dispatch(idx > 13 ? kS3 : kSubTable[idx], ride, x, y);
        break;
    }
    case kTrack: Track(ride, x, y); break;
    case kSingle: Single(ride, x, y); break;
    case kS1:
    case kS2:
    case kS3: {
        x += 0x600;
        y += 0x600;
        int32_t shape = target == kS1 ? 0x1200 : target == kS2 ? 0x1100 : 0x1600;
        int32_t dy = target == kS1 ? 0xc00 : target == kS2 ? 0xb00 : 0x1000;
        BMP_StartColObjectForRide(reinterpret_cast<tagBUMPRIDE*>(ride), x, y, shape);
        RingOfBuoys(ride, x, y, 0, dy);
        break;
    }
    default: break;
    }
}

}  // namespace

int32_t BMP_CreateRide(int32_t kind, int32_t x, int32_t y, int32_t rot, int32_t id, int32_t f10) {
    uint32_t* world = World_5();
    if (world[0] != 0x4a454647u) return 0;
    uint32_t* ride;
    if (id != 0) {
        ride = RideOfId(id);
        kind = id >> 8;
    } else {
        ride = nullptr;
        uint32_t* p = Rides();
        if (p != nullptr) {
            for (int i = 0; i < 0x40; i++, p += 0x34) {
                if (p[0] == 0) { ride = p; break; }
            }
        }
    }
    // A NULL ride (no free record) is read at the end like the original does: the Mac reads low memory, the host would fault.
    if (ride != nullptr) {
        const uint32_t* src = reinterpret_cast<const uint32_t*>(reinterpret_cast<uintptr_t>(gRideTemplates) + static_cast<uint32_t>(-kind) * 0xd0u - 0xd0u);   // the listing copies from base + (-kind) * 0xd0 - 0xd0
        for (int i = 0; i < 0x34; i++) ride[i] = src[i];
        world[3] = world[3] + 1;
        ride[0x5c / 4] = 0;
        ride[0xac / 4] = 0;
        ride[0xa8 / 4] = 0;
        ride[0xc0 / 4] = 0;
        ride[0xbc / 4] = 0;
        ride[0xb4 / 4] = static_cast<uint32_t>(x);
        ride[0xb8 / 4] = static_cast<uint32_t>(y);
        ride[0xb0 / 4] = static_cast<uint32_t>(rot);
        ride[0xcc / 4] = 0;
        ride[0x58 / 4] = 0;
        ride[0x10 / 4] = static_cast<uint32_t>(f10);
        uint32_t idx = ride[0] + 14;
        if (idx <= 13) Dispatch(kTop[idx], ride, x, y);
    }
    return IdOf_2(ride);
}

// Unit v5672: BMP_DrawCarsForRide (0x10028030, 3,156 bytes): places the bumper cars of one ride.
// Pass 1 collects the ride's live bumps (four corner points each), pass 2 looks up the ground height under every corner
// (one SetRideSpritePosnFocus per distinct mesh), pass 3 tilts the cars along the ground and sets each car's matrices.

float gDrawCarsAngles[3];          // 0x1029ca9c (initial 0): the rotation angles, copied to the stack and overwritten before each use
float gDrawCarsClampLo = -1.0f;    // 0x1029caa8
float gDrawCarsClampHi = 1.0f;     // 0x1029caac
int8_t gDrawCarsInit;              // 0x102b4eb8 (a char): set once the scale below holds its value
float gDrawCarsScale;              // 0x102b4ebc: 49143.0 once gDrawCarsInit is set

namespace {

// One corner point of a car: the Mac keeps 0x20 bytes per point (+0 the bump of the first point of a car, +4 the mesh, +8 the angle,
// +0xc and +0x10 the car's centre, +0x14, +0x18 and +0x1c the corner's x, z and height).
struct Corner {
    uint8_t* bump;
    tag_sRideAnimSet* mesh;
    uint32_t angle;
    float cx, cz;
    float x, z, y;
};

inline int32_t& W_2(void* p, uint32_t off) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(p) + off); }
inline float& Fl(void* p, uint32_t off) { return *reinterpret_cast<float*>(static_cast<uint8_t*>(p) + off); }

inline float Sine(int32_t i) { return static_cast<float>(gHostSineTable[i & 0x1ff]); }
inline float Sine4(int32_t i) {   // the table word times four, as the listing shifts it
    return static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(gHostSineTable[i & 0x1ff]) << 2));
}

// cur - (cur - prev) * (-step), the listing's fsubs + fnmsubs
inline float Slope(float cur, float prev, float negStep) { return ppc_fnmsubs(cur - prev, negStep, cur); }

inline sMatrix* MatrixOf(uint32_t index) {
    return &gRideAnimSets[index]->mHeads->mHeader->mF78.get()->mMatrix;
}

inline int32_t Idx(int32_t v, int32_t mul) { return static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(mul)); }

}  // namespace

void BMP_DrawCarsForRide(int32_t ride, float step) {
    const float kPi = 3.1415927f;
    const float kHalfPi = 1.5707964f;
    tagBUMPWORLD* world = reinterpret_cast<tagBUMPWORLD*>(&gColList);

    float angles[3];
    angles[0] = gDrawCarsAngles[0];
    angles[1] = gDrawCarsAngles[1];
    angles[2] = gDrawCarsAngles[2];

    const float* pick = &gDrawCarsClampLo;
    if (*pick < step) pick = &step;
    if (!(*pick < gDrawCarsClampHi)) pick = &gDrawCarsClampHi;
    const float s = *pick;
    const float negStep = -s;

    int32_t n = 0;
    tagBUMPRIDE* rec = nullptr;
    uint32_t id = 0;
    if (world->magic == 0x4a454647u) {
        rec = world->rides.get() + (ride & 0xff);
        id = (static_cast<uint32_t>(rec->kind) << 8) | static_cast<uint32_t>(ride & 0xff);
    }
    if (ride != static_cast<int32_t>(id)) return;
    uint8_t* bump = reinterpret_cast<uint8_t*>(world->bumps.get());
    if (bump == nullptr) return;
    int32_t count = world->bumpCount;

    static Corner cars[1024];
    Corner* c = cars;

    // Pass 1: four corner points per live bump of this ride.
    while (count != 0) {
        if ((W_2(bump, 0) & 1) != 0) {
            count--;
            if (reinterpret_cast<tagBUMPRIDE*>(reinterpret_cast<tagBUMPSLOT*>(bump)->owner.get()) == rec) {
                const int32_t shift = ppc_fctiwz(static_cast<double>(static_cast<float>(W_2(bump, 0x5c)) * negStep));
                const uint32_t angle = (static_cast<uint32_t>(W_2(bump, 0x54)) - static_cast<uint32_t>(shift)) & 0x1ffu;
                const float rideW = static_cast<float>(W_2(rec, 8));
                const float cx = ppc_fnmsubs(static_cast<float>(W_2(bump, 0x3c)), negStep, static_cast<float>(W_2(bump, 0x34))) / 307.2f;
                const float cz = ppc_fnmsubs(static_cast<float>(W_2(bump, 0x40)), negStep, static_cast<float>(W_2(bump, 0x38))) / 307.2f;
                const float a = ((rideW * Sine4(static_cast<int32_t>(angle))) * 0.0009765625f) / 307.2f;
                const float b = ((rideW * Sine4(static_cast<int32_t>(angle) + 0x80)) * 0.0009765625f) / 307.2f;
                c[0].bump = bump;
                c[0].angle = angle;
                c[0].cx = cx;
                c[0].cz = cz;
                const float xs[4] = {cx + a, cx - a, cx - b, cx + b};
                const float zs[4] = {cz + b, cz - b, cz + a, cz - a};
                for (int k = 0; k < 4; k++) {
                    c[k].x = xs[k];
                    c[k].z = zs[k];
                    c[k].y = Fl(bump, 0xa0);
                    c[k].mesh = GetMeshPtrAtHFieldCell(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(c[k].x / 10.0f))),
                                                       static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(c[k].z / 10.0f))));
                }
                c += 4;
                n += 4;
            }
        }
        bump += 0xac;
    }

    // Pass 2: the height under every corner, one mesh at a time.
    int32_t first = 0;
    while (first != -1 && first < n) {
        const int32_t start = first;
        tag_sRideAnimSet* mesh = cars[start].mesh;
        first = -1;
        if (mesh != nullptr) SetRideSpritePosnFocus(mesh->mHeads);
        for (int32_t k = start; k < n; k++) {
            tag_sRideAnimSet* m = cars[k].mesh;
            if (m == nullptr) continue;
            if (m == mesh) {
                GetRideHeightAtPoint(cars[k].x, cars[k].z, cars[k].y);
                if (-100.0f == cars[k].y) cars[k].y = Fl(cars[k & ~3].bump, 0xa0);
                cars[k].mesh = nullptr;
            } else if (first == -1) {
                first = k;
            }
        }
    }

    // Pass 3: the cars.
    for (int32_t g = 0; g < n; g += 4) {
        Corner* q = &cars[g];
        uint8_t* bp = q->bump;
        const float f27 = kPi * (2.0f * (static_cast<float>(static_cast<int32_t>((q->angle + 0x100u) & 0x1ffu)) * 0.001953125f));
        if ((static_cast<uint32_t>(W_2(bp, 0)) & 0x1000000u) != 0) {
            const int32_t idx = W_2(bp, 0x90);
            q[0].y = q[0].y + Slope(Sine(Idx(idx, 6)), Sine(Idx(idx - 1, 6)), negStep) / 800.0f;
            q[1].y = q[1].y + Slope(Sine(((static_cast<uint32_t>(idx) * 16u) & 0x7f0u) >> 2),
                                    Sine(((static_cast<uint32_t>(idx - 1) * 16u) & 0x7f0u) >> 2), negStep) / 800.0f;
            q[2].y = q[2].y + Slope(Sine(Idx(idx, 3)), Sine(Idx(idx - 1, 3)), negStep) / 800.0f;
            q[3].y = q[3].y + Slope(Sine(Idx(idx, 5)), Sine(Idx(idx - 1, 5)), negStep) / 800.0f;
        }
        const float h0 = q[0].y, h1 = q[1].y, h2 = q[2].y, h3 = q[3].y;
        const float rideW = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(W_2(rec, 8)) << 1));
        const float atanA = static_cast<float>(std::atan2(static_cast<double>(rideW / 307.2f), static_cast<double>(h1 - h0)));
        const float atanB = static_cast<float>(std::atan2(static_cast<double>(rideW / 307.2f), static_cast<double>(h3 - h2)));
        float f30, f28;
        if ((static_cast<uint32_t>(W_2(bp, 0)) & 0x800u) != 0) {
            f30 = (h0 + h1) * 0.5f;
            f28 = kHalfPi;
        } else {
            f28 = atanB;
            f30 = (h3 + (h2 + (h0 + h1))) * 0.25f;
        }
        Fl(bp, 0xa0) = f30;
        const uint32_t flags = static_cast<uint32_t>(W_2(bp, 0));
        if ((flags & 0x400u) != 0) {
            const int32_t idx = W_2(bp, 0x90);
            const float cur = Sine(Idx(idx, 6)), prev = Sine(Idx(idx - 1, 6));
            f30 = f30 + (1.6f + Slope(cur, prev, negStep) / 600.0f);
        }
        if ((flags & 0x1000u) != 0) {
            const float t = static_cast<float>(W_2(bp, 0x60)) * negStep;
            const float u = static_cast<float>(W_2(bp, 0x5c)) - t;
            f28 = ppc_fmadds(kHalfPi, u / 13.0f, f28);
        }

        sMatrix* m = MatrixOf(static_cast<uint32_t>(W_2(bp, 8)));
        SetLMatrixIdentityNoTrans(m);
        m = MatrixOf(static_cast<uint32_t>(W_2(bp, 8)));
        m->m[3][0] = q->cx;
        m->m[3][1] = f30;
        m->m[3][2] = q->cz;
        angles[0] = f28 - kHalfPi;
        angles[2] = f27;
        angles[1] = atanA - kHalfPi;
        RotateLMatrixXYZNoTrans(MatrixOf(static_cast<uint32_t>(W_2(bp, 8))), static_cast<eRotateParams>(0xe), angles);

        if ((static_cast<uint32_t>(W_2(bp, 0)) & 0x4000000u) != 0) {
            W_2(bp, 0) = static_cast<int32_t>(static_cast<uint32_t>(W_2(bp, 0)) & 0xfbffffffu);
            const int32_t ex = ppc_fctiwz(static_cast<double>(1024.0f * q->cx));
            const int32_t ey = ppc_fctiwz(static_cast<double>(1024.0f * f30));
            const int32_t ez = ppc_fctiwz(static_cast<double>(1024.0f * q->cz));
            PAR_InitEmitter(0xf, ex, ey, ez);
        }
        if ((static_cast<uint32_t>(W_2(bp, 0)) & 0x2000u) != 0) {
            SetLMatrixIdentityNoTrans(MatrixOf(static_cast<uint32_t>(W_2(bp, 0xc))));
            sMatrix* m2 = MatrixOf(static_cast<uint32_t>(W_2(bp, 0xc)));
            const float avg = (h3 + (h2 + (h0 + h1))) * 0.25f;
            m2->m[3][0] = q->cx;
            m2->m[3][1] = avg;
            m2->m[3][2] = q->cz;
            angles[0] = atanB - kHalfPi;
            angles[2] = f27;
            angles[1] = atanA - kHalfPi;
            RotateLMatrixXYZNoTrans(MatrixOf(static_cast<uint32_t>(W_2(bp, 0xc))), static_cast<eRotateParams>(0xe), angles);
        }
        if ((static_cast<uint32_t>(W_2(bp, 0)) & 0x2000000u) != 0) {
            if (gDrawCarsInit == 0) {
                gDrawCarsScale = 49143.0f;
                gDrawCarsInit = 1;
            }
            SetLMatrixIdentityNoTrans(MatrixOf(static_cast<uint32_t>(W_2(bp, 0xc))));
            const int32_t sa = static_cast<int32_t>(static_cast<uint32_t>(W_2(bp, 0x4c)) *
                                                    (static_cast<uint32_t>(gHostSineTable[q->angle & 0x1ff]) << 2));
            q->cx = q->cx - static_cast<float>(sa) / gDrawCarsScale;
            const int32_t sb = static_cast<int32_t>(static_cast<uint32_t>(W_2(bp, 0x4c)) *
                                                    (static_cast<uint32_t>(gHostSineTable[(q->angle + 0x80) & 0x1ff]) << 2));
            q->cz = q->cz - static_cast<float>(sb) / gDrawCarsScale;
            sMatrix* m3 = MatrixOf(static_cast<uint32_t>(W_2(bp, 0xc)));
            m3->m[3][0] = q->cx;
            m3->m[3][1] = 0.7f + f30;
            m3->m[3][2] = q->cz;
            angles[0] = 0.0f;
            angles[2] = f27;
            angles[1] = 0.0f;
            RotateLMatrixXYZNoTrans(MatrixOf(static_cast<uint32_t>(W_2(bp, 0xc))), static_cast<eRotateParams>(0xe), angles);
        }
    }
}

// 0x10028cb0, 352 bytes: one tick of the bumper cars (AI and move, then collision and areas).

namespace {
uint32_t& W_3(void* p, uint32_t off) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(p) + off); }
}  // namespace

void BMP_Tick() {
    tagBUMPWORLD* world = reinterpret_cast<tagBUMPWORLD*>(&gColList);
    if (world->magic != 0x4a454647u) return;
    world->pad04 = world->pad04 + 1;
    tagBUMPSLOT* slot = world->bumps.get();
    int32_t left = world->bumpCount;
    while (left != 0) {
        if (slot->flags & 1) {
            slot->flags &= 0xffdfffffu;
            left--;
            BMP_AI(reinterpret_cast<tagBUMP*>(slot));
            if (slot->flags & 1) BMP_Move(reinterpret_cast<tagBUMP*>(slot));
        }
        slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
    }
    slot = world->bumps.get();
    left = world->bumpCount;
    while (left != 0) {
        if (slot->flags & 1) {
            tagBUMP* b = reinterpret_cast<tagBUMP*>(slot);
            left--;
            BMP_Collision(b);
            if (static_cast<int32_t>(W_3(slot, 0xa4)) > 0) {
                int32_t n = static_cast<int32_t>(W_3(slot, 0xa8));
                W_3(slot, 0xa8) = n + 1;
                if (n >= 0x20) {
                    if (static_cast<int32_t>(W_3(slot, 0xa4)) > 0x10) {
                        Ptr32<uint8_t> col{W_3(slot, 0x98)};
                        Ptr32<tabBUOY> buoy{W_3(col.get(), 0x1c)};
                        tabBUOY* target = buoy.get();
                        if (target == nullptr) {
                            Ptr32<tagBUMPRIDE> ride{W_3(slot, 0x9c)};
                            target = GetFinishLine(ride.get());
                        }
                        SetNextBuoyTarget(target, b);
                        slot->flags |= 0x10000u;
                    }
                    W_3(slot, 0xa4) = 0;
                    W_3(slot, 0xa8) = 0;
                }
            }
            BMP_InsideArea(b);
            BMP_OutsideArea(b);
            BMP_ControlSound(b);
        }
        slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
    }
}

void BMP_ControlSound(tagBUMP* b) {
    uint8_t* bump = reinterpret_cast<uint8_t*>(b);
    uint8_t* world = WorldBytes();
    uint32_t pending = Wd(bump, 0x14);
    if (pending != 0) {
        if (Wd(bump, 0x18) < pending) Wd(bump, 0x18) = pending;
        Wd(bump, 0x14) = 0;
    }
    int32_t state = static_cast<int32_t>(Wd(bump, 0x18));
    int32_t pt[2];
    switch (state) {
        case 3:
            SingleSoundAtBump(b, 0xc);
            TRACE("CRUNCH\n");
            Wd(bump, 0x18) = 0;
            break;
        case 1:
            SingleSoundAtBump(b, 0x18);
            Wd(bump, 0x18) = 0;
            break;
        case 2: {
            int32_t seed = static_cast<int32_t>(Wd(world, 0x38)) * 0x343fd + 0x269ec3;
            Wd(world, 0x38) = static_cast<uint32_t>(seed);
            if (((seed >> 16) & 7) == 0) {
                MineToSystemCords(static_cast<int32_t>(Wd(bump, 0x34)), static_cast<int32_t>(Wd(bump, 0x38)),
                                  reinterpret_cast<NS_MacDoze::tagPOINT*>(pt));
                int32_t handle = RideEventHandle(RideIdOfBump(bump));
                SND_SendIndirectedEvent(handle, 0, gSoundBanks[7], 3, pt[0], 0, pt[1]);
                TRACE("TOOT!\n");
            }
            Wd(bump, 0x18) = 0;
            break;
        }
        case 4: {
            MineToSystemCords(static_cast<int32_t>(Wd(bump, 0x34)), static_cast<int32_t>(Wd(bump, 0x38)),
                              reinterpret_cast<NS_MacDoze::tagPOINT*>(pt));
            int32_t handle = RideEventHandle(RideIdOfBump(bump));
            SND_SendIndirectedEvent(handle, 0, gSoundBanks[7], 4, pt[0], 0, pt[1]);
            Wd(bump, 0x18) = 0;
            break;
        }
        case 5:
            SingleSoundAtBump(b, 0xc);
            Wd(bump, 0x18) = 0;
            break;
        default:
            break;
    }
}

// Unit v5926: the bump AI and its small helpers, main app.
//   BMP_MaskCar 0x1002ae74, BMP_Thrust 0x10029ac8, SetNextBuoyTarget 0x100296f4, BMP_AI 0x10029128
// The bump (0xac bytes), its ride record (the owner) and the buoys are read by Mac offset.

int32_t gBumpAiMinDist = 1;      // 0x1029cab0 (name invented): the lower clamp of the chase distance
int32_t gBumpAiMaxStep = 0x18;   // 0x1029cab4 (name invented): the upper clamp of the chase step

namespace {

inline uint32_t& Wu(const void* p, uint32_t off) {
    return *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(static_cast<const uint8_t*>(p)) + off);
}
inline int32_t& Wd_2(const void* p, uint32_t off) {
    return *reinterpret_cast<int32_t*>(const_cast<uint8_t*>(static_cast<const uint8_t*>(p)) + off);
}
// A Ptr32 word of a record.
inline uint8_t* Pw(const void* p, uint32_t off) {
    Ptr32<uint8_t> q;
    q.mOffset = Wu(p, off);
    return q.get();
}
inline uint8_t* OwnerOf(tagBUMP* b) { return Pw(b, 0x9c); }

// The jump tables at 0x1029caf0 and 0x1029cab8, by the owner kind + 14.
// Kinds -14, -11, -6, -3 and -1 take the first target of each table, every other kind the default.
const bool kClearAnim[14] = {true, false, false, true, false, false, false, false, true, false, false, true, false, true};
const bool kLeave[14] = {true, false, false, true, false, false, false, false, true, false, false, true, false, true};

void DisableSet(uint32_t index) {
    tag_sRideAnimSet* set = gRideAnimSets[index];
    if (set->mFlags & 0x4000)
        DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else
        DisableMeshDraw(set->mHeads);
}
void EnableSet(uint32_t index) {
    tag_sRideAnimSet* set = gRideAnimSets[index];
    if (set->mFlags & 0x4000)
        EnableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else
        EnableMeshDraw(set->mHeads);
}

}  // namespace

// 0x100296f4
void SetNextBuoyTarget(tabBUOY* buoy, tagBUMP* b) {
    if (buoy == nullptr) return;
    uint8_t* cur = reinterpret_cast<uint8_t*>(buoy);
    do {
        cur = Pw(cur, 0x14);
        if (cur == nullptr) cur = Pw(OwnerOf(b), 0xa8);
    } while (Wu(cur, 0) & 2);
    Wd_2(b, 0x7c) = Wd_2(cur, 4);
    Wd_2(b, 0x78) = Wd_2(b, 0x74);
    uint32_t m = Wu(OwnerOf(b), 0x68);
    uint32_t r = gGameWorld->GetRand();
    int32_t half = static_cast<int32_t>(m) / 2;
    Wd_2(b, 0x74) = half - static_cast<int32_t>(r - (r / m) * m);
}

// 0x10029128
void BMP_AI(tagBUMP* b) {
    uint8_t* list = reinterpret_cast<uint8_t*>(&gColList);
    Wd_2(b, 0x90) = Wd_2(b, 0x90) + 1;
    int32_t c = Wd_2(b, 0x88);
    if (c >= 0 && Wd_2(OwnerOf(b), 0x50) == 2 && Wd_2(OwnerOf(b), 0x54) != 2) {
        c = c - 1;
        Wd_2(b, 0x88) = c;
        if (c == 0) {
            uint32_t index = static_cast<uint32_t>(Wd_2(OwnerOf(b), 0)) + 14;
            if (index <= 13 && kClearAnim[index]) b->flags &= ~0x4000u;
            b->flags &= 0xffffff81u;
            b->flags |= 0x10020;
        }
    }

    uint32_t f = b->flags;
    if (f & 4) {
        uint8_t* t = Pw(list, 0x10) + static_cast<int32_t>(Wd_2(b, 0x80) * 0xac);
        int32_t count = Wd_2(b, 0x8c) - 1;
        Wd_2(b, 0x8c) = count;
        if (count <= 0 || (Wu(t, 0) & 0x40021) != 1) {
            SetControlType(reinterpret_cast<tagBUMPRIDE*>(OwnerOf(b)), b);
            return;
        }
        b->flags |= 0x10000;
        int32_t d1 = BMP_Distance(Wd_2(b, 0x3c), Wd_2(b, 0x40));
        int32_t m = gBumpAiMinDist < d1 ? d1 : gBumpAiMinDist;
        int32_t d2 = BMP_Distance(Wd_2(b, 0x34) - Wd_2(t, 0x34), Wd_2(b, 0x38) - Wd_2(t, 0x38));
        int32_t q = d2 / m;
        int32_t step = q < gBumpAiMaxStep ? q : gBumpAiMaxStep;
        int32_t lx = Wd_2(t, 0x34) + step * Wd_2(t, 0x3c);
        int32_t ly = Wd_2(t, 0x38) + step * Wd_2(t, 0x40);
        tabCOLOBJECT* col = reinterpret_cast<tabCOLOBJECT*>(Pw(t, 0x98));
        if (PointInColObject(lx, ly, col) == 0 && PointInAnyColObject(lx, ly) == nullptr) {
            tabCOLOBJECT* cc = ClosestIntersectingColObjectToPoint(col, lx, ly, 1);
            int32_t dx = (lx - cc->x) / 8;
            int32_t dy = (ly - cc->y) / 8;
            int32_t dist = BMP_Distance(dx, dy);
            int32_t cx = cc->x;
            int32_t cy = cc->y;
            int32_t rr = cc->radiusX;
            lx = cx + (dx * rr) / (dist + 0x20);
            ly = cy + (dy * rr) / (dist + 0x20);
        }
        int32_t rc = SetNavRoute(b, lx, ly);
        if ((b->flags & 0x20000) && rc <= Wd_2(OwnerOf(b), 0x68)) b->flags &= ~0x20000u;
        return;
    }

    if (f & 0x20) {
        uint8_t* own = OwnerOf(b);
        uint32_t index = static_cast<uint32_t>(Wd_2(own, 0)) + 14;
        if (index <= 13 && kLeave[index]) {
            BMP_LeaveBump(b);
            int32_t handle = Wd_2(b, 0x20);
            if (handle != 0) SOUND_FadeSoundEffect(handle);
            return;
        }
        int32_t rc = SetNavRoute(b, Wd_2(own, 0x90), Wd_2(own, 0x94));
        if ((b->flags & 0x20000) && rc <= Wd_2(OwnerOf(b), 0x68)) {
            b->flags &= ~0x20000u;
            b->flags |= 0x10000;
            return;
        }
        if (rc < Wd_2(OwnerOf(b), 0x68)) BMP_DOCK(b);
        return;
    }

    if ((f & 0x58) == 0) return;
    uint8_t* buoys = Pw(list, 0x20);
    if (buoys == nullptr) return;
    uint8_t* buoy = buoys + static_cast<int32_t>(Wd_2(b, 0x7c) * 0x1c);
    if (!(Wu(buoy, 0) & 1)) return;
    int32_t rc = SetNavRoute(b, Wd_2(buoy, 8), Wd_2(buoy, 0xc));
    uint32_t g = b->flags;
    if ((g & 0x20000) && rc <= Wd_2(OwnerOf(b), 0x68)) {
        b->flags = g & ~0x20000u;
        b->flags |= 0x10000;
        return;
    }
    if ((g & 0x48000) == 0x48000) {
        SetControlType(reinterpret_cast<tagBUMPRIDE*>(OwnerOf(b)), b);
        b->flags |= 0x10000;
        return;
    }
    if (rc > Wd_2(OwnerOf(b), 0x68)) return;
    int32_t left = Wd_2(b, 0x8c) - 1;
    Wd_2(b, 0x8c) = left;
    if (left == 0) {
        SetControlType(reinterpret_cast<tagBUMPRIDE*>(OwnerOf(b)), b);
        b->flags |= 0x10000;
        return;
    }
    g = b->flags;
    if (!(g & 8)) {
        if (g & 0x10) {
            uint32_t r = gGameWorld->GetRand();
            uint32_t n = Wu(list, 0x24);
            Wd_2(b, 0x7c) = static_cast<int32_t>(r - (r / n) * n);
        }
        b->flags |= 0x10000;
        return;
    }
    if (g & 0x40) {
        if (rc <= Wd_2(OwnerOf(b), 0x68) / 4) {
            b->flags = g | 0x80000;
            Wd_2(b, 0x7c) = Wd_2(Pw(OwnerOf(b), 0xa4), 4);
        }
        b->flags |= 0x10000;
        return;
    }
    if (Wu(buoy, 0) & 4) {
        uint8_t* own = OwnerOf(b);
        bool reset = false;
        if (Wd_2(own, 0x50) == 3 || Wd_2(own, 0x50) == 0) {
            reset = true;
        } else {
            int32_t v = Wd_2(b, 0x88) + 1;
            Wd_2(b, 0x88) = v;
            reset = (v == 0);
        }
        if (reset) {
            int32_t kind = Wd_2(own, 0);
            if (kind == -7 || kind == -9 || kind == -10 || kind == -4) {
                if (Wd_2(own, 0x50) == 2) {
                    Wd_2(own, 0x50) = 3;
                    TRACE("GoKart Race Over\n");
                }
            }
            Wd_2(b, 0x74) = 0;
            Wd_2(b, 0x78) = 0;
            b->flags &= 0xffffff81u;
            b->flags |= 0x10020;
        } else {
            SetControlType(reinterpret_cast<tagBUMPRIDE*>(own), b);
        }
    }
    if (Wu(buoy, 0x18) == 0) {
        SetNextBuoyTarget(reinterpret_cast<tabBUOY*>(buoy), b);
    } else {
        if (PointInColObject(Wd_2(b, 0x34), Wd_2(b, 0x38), reinterpret_cast<tabCOLOBJECT*>(Pw(buoy, 0x18))) != 0) {
            Wu(b, 0x98) = Wu(buoy, 0x18);
            SetNextBuoyTarget(reinterpret_cast<tabBUOY*>(buoy), b);
        } else {
            Wd_2(b, 0x78) = 0;
            Wd_2(b, 0x74) = 0;
        }
    }
    b->flags |= 0x10000;
}

// The two thrust limits BumpToPoint reads (initial words 0x30 and 0x16 on the Mac).
int32_t gBumpThrustLimitA = 0x30;  // 0x1029cb28
int32_t gBumpThrustLimitB = 0x16;  // 0x1029cb2c

namespace {
inline int32_t& W_4(void* base, uint32_t offset) {
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(base) + offset);
}
}  // namespace

// 0x1002992c, 368 bytes: steers toward the point, answers the distance.
int32_t BumpToPoint(tagBUMP* b, int32_t px, int32_t py) {
    int32_t dx = px - b->x;
    int32_t dy = py - b->y;
    double a = std::atan2(static_cast<double>(dx), static_cast<double>(-dy));
    double q = a / 3.14159;
    double t = ppc_fnmsub(256.0, q, 256.0);
    int32_t heading = ppc_fctiwz(t);
    int32_t w = (heading - W_4(b, 0x50)) & 0x1ff;
    int32_t dist = BMP_Distance(dx, dy);    void* owner = b->owner.get();
    int32_t turn;
    if (w > 0x100) {
        w = 0x200 - w;
        turn = -W_4(owner, 0x28);
    } else {
        turn = W_4(owner, 0x28);
    }
    int32_t rate = W_4(owner, 0x28);
    if (w > rate / 2 && W_4(owner, 0x54) != 2 && (b->flags & 0x4000)) {
        W_4(b, 0x50) = (W_4(b, 0x50) + turn) & 0x1ff;
    }
    int32_t quotient = dist / 512;
    const int32_t* p = &gBumpThrustLimitA;
    if (quotient < *p) p = &quotient;
    if (gBumpThrustLimitB < *p) {
    } else {
        p = &gBumpThrustLimitB;
    }
    if (w < *p) BMP_Thrust(b, W_4(owner, 0x20));
    return dist;
}

// 0x100297cc, 308 bytes: picks the point to steer to, then bumps toward it.
int32_t SetNavRoute(tagBUMP* b, int32_t x, int32_t y) {
    if ((b->flags & 0x10000) && b->col.mOffset != 0) {
        tabCOLOBJECT* hit = PointInAnyColObject(x, y);
        if (hit == nullptr) {
            W_4(b, 0x6c) = x;
            W_4(b, 0x70) = y;
            return -1;
        }
        if (DoAreasIntersect(b->col.get(), hit) != 0) {
            W_4(b, 0x6c) = W_4(b, 0x74) + x;
            W_4(b, 0x70) = W_4(b, 0x78) + y;
        } else {
            tabCOLOBJECT* near = ClosestIntersectingColObjectToPoint(b->col.get(), x, y, 0);
            if (near == nullptr) {
                TRACE("The impossible has happened\n");
                b->vy = 0;
                b->vx = 0;
                return -1;
            }
            int32_t pt[2] = {0, 0};
            NavPointInArea(near, reinterpret_cast<NS_MacDoze::tagPOINT*>(pt));
            W_4(b, 0x6c) = pt[0];
            W_4(b, 0x70) = pt[1];
            b->flags |= 0x20000;
        }
        b->flags &= ~0x10000u;
    }
    return BumpToPoint(b, W_4(b, 0x6c), W_4(b, 0x70));
}

// 0x10029ac8
void BMP_Thrust(tagBUMP* b, int32_t amount) {
    if ((b->flags & 0x84000) != 0x4000) return;
    if (Wd_2(OwnerOf(b), 0x54) == 2) return;
    uint8_t* col = Pw(b, 0x98);
    int32_t a = amount + Wd_2(b, 0x68);
    if ((Wu(col, 0) & 0x1e) != 0x1e) a = (a * 3) / 4;
    uint32_t angle = Wu(b, 0x50);
    int32_t sx = gHostSineTable[angle & 0x1ff];
    int32_t sy = gHostSineTable[(angle + 0x80) & 0x1ff];
    int32_t dx = (a * (sx * 4)) / 1024;
    int32_t dy = (a * (sy * 4)) / 1024;
    Wd_2(b, 0x3c) = Wd_2(b, 0x3c) + dx;
    Wd_2(b, 0x40) = Wd_2(b, 0x40) + dy;
}

// Unit v5627: BMP_Move (main 0x10029b9c, 4464 bytes, TPAdvisor.cpp): one step of a bump car. Scales the velocity by the owner's speed,
// moves the car, then by the owner's kind (a jump table on kind + 14) steers it, starts and moves its particle emitters and updates its
// sound; finally turns the heading and opens or closes the gate of the collision object the car is in.
// The bump, its owner and the list at 0x102b56c0 are read by Mac offset (see include/BMP_InsideArea.h, unit v5627).

int32_t gBumpTurnMax = 16;    // 0x1029cb30
int32_t gBumpTurnMin = -16;   // 0x1029cb34

namespace {

const uint32_t kBumpWorldMagic = 0x4a454647;

int32_t& Word_3(void* base, uint32_t offset) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + offset); }

int32_t& ListWord(uint32_t offset) { return Word_3(&gColList, offset); }

// The list's random number (MSVC's rand step on the seed at +0x38).
int32_t NextRandom() {
    uint32_t seed = static_cast<uint32_t>(ListWord(0x38)) * 0x343fdu + 0x269ec3u;
    ListWord(0x38) = static_cast<int32_t>(seed);
    return static_cast<int32_t>(seed) >> 16;
}

bool TickGate() { return (ListWord(4) & 0x1f) == 0; }

tagBumpHeadEntry& HeadEntry(tag_sRideAnimSet* set, int32_t index) {
    return static_cast<tagBumpHeadTable*>(set->mHeads->mF28)->entries[index];
}

void HeadPosition(tag_sRideAnimSet* set, int32_t index, float& x, float& y, float& z) {
    tagBumpHeadNode* node = HeadEntry(set, index).node;
    if (node != nullptr) {
        x = node->x;
        y = node->y;
        z = node->z;
    } else {
        x = y = z = 0.0f;
    }
}

// The sound block the three steering cases share: places the sound and sets its parameter from the speed.
void UpdateSound(tagBUMP* b, tagBUMPRIDE* owner) {
    PossitionSound1(b);
    int32_t rideId;
    if (static_cast<uint32_t>(ListWord(0)) == kBumpWorldMagic) {
        int32_t diff = static_cast<int32_t>(b->owner.mOffset - static_cast<uint32_t>(ListWord(8)));
        rideId = static_cast<int32_t>((static_cast<uint32_t>(owner->kind) << 8) | static_cast<uint32_t>(diff / 0xd0));
    } else {
        rideId = 0;
    }
    uint16_t thing;
    TrackRideIdToThingId(&thing, rideId);
    int32_t value;
    if (thing != gAdvisorThingId) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        value = Word_3(gThingArray[thing].mThing, 0x20);   // the thing's word at Mac offset 0x20
    } else {
        value = 0;
    }
    int32_t code = RSSE_GetSoundEventCode(value, 10);
    Word_3(b, 0x20) = SOUND_SetParamValue(Word_3(b, 0x20), static_cast<uint32_t>(code), static_cast<uint32_t>(Word_3(b, 0x4c) / 3));
}

// The turn toward the target heading at 0x58, scaled by the speed.
int32_t Turn(tagBUMP* b) {
    int32_t d = (Word_3(b, 0x58) - Word_3(b, 0x54)) & 0x1ff;
    if (d > 0x100) d = -(0x200 - d);
    return (d * Word_3(b, 0x4c)) / 1000;
}

// The heading of the velocity: 256 - 256 * atan2(vx, -vy) / 3.14159, in 1/512 turns.
void AimAlongVelocity(tagBUMP* b) {
    double a = std::atan2(static_cast<double>(b->vx), static_cast<double>(static_cast<int32_t>(0u - static_cast<uint32_t>(b->vy))));
    Word_3(b, 0x58) = ppc_fctiwz(ppc_fnmsub(256.0, a / 3.14159, 256.0));
}

// Starts an emitter of `type` at the head entry named by the bump's word `field` (0x24 or 0x28), launched along the node's z axis.
void LaunchFromHead(tagBUMP* b, int32_t type, uint32_t field) {
    uint32_t setIndex = static_cast<uint32_t>(Word_3(b, 8));
    float x, y, z;
    HeadPosition(gRideAnimSets[setIndex], Word_3(b, field), x, y, z);
    uint32_t id = setIndex;
    tag_sRideAnimSet* ride = *GetRideFromID(id);
    float* matrix = GetDataNodeWorldMatrix(ride->mHeads, reinterpret_cast<uint32_t&>(Word_3(b, field)));
    s3DPoint dir;
    if (matrix != nullptr) {
        if (HeadEntry(ride, Word_3(b, field)).f00 & 0x10) {
            dir.x = -matrix[8];
            dir.y = -matrix[9];
            dir.z = -matrix[10];
        } else {
            dir.x = matrix[8];
            dir.y = matrix[9];
            dir.z = matrix[10];
        }
        NormaliseVector(&dir);
    } else {
        dir.x = 0.0f;
        dir.y = 0.0f;
        dir.z = 1.0f;
    }
    float sx = 1024.0f * x, sy = 1024.0f * y, sz = 1024.0f * z;
    float dx = 1024.0f * dir.x, dy = 1024.0f * dir.y, dz = 1024.0f * dir.z;
    PAR_InitEmitterLaunch(type, ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz), ppc_fctiwz(dx), ppc_fctiwz(dy), ppc_fctiwz(dz));
}

// Moves the emitter at 0x2c to the head entry at 0x24 when both are set.
void FollowHead(tagBUMP* b) {
    if (Word_3(b, 0x2c) == -1 || Word_3(b, 0x24) == -1) return;
    float x, y, z;
    HeadPosition(gRideAnimSets[static_cast<uint32_t>(Word_3(b, 8))], Word_3(b, 0x24), x, y, z);
    float sx = 1024.0f * x, sy = 1024.0f * y, sz = 1024.0f * z;
    PAR_SetEmitterPos(Word_3(b, 0x2c), ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz));
}

void SetCarAnim(tagBUMP* b, uint32_t anim, uint32_t loop) {
    if (Word_3(b, 8) != 0) FlushDeferredAnim(gRideAnimSets[static_cast<uint32_t>(Word_3(b, 8))], 0);
    if (Word_3(b, 8) != 0) SetCurRideAnim(gRideAnimSets[static_cast<uint32_t>(Word_3(b, 8))], anim, 0, loop, 1.0f, 0);
}

// Case 0x10029ce0: the push decays and steers.
void SteerByPush(tagBUMP* b, tagBUMPRIDE* owner) {
    if (static_cast<int32_t>(std::labs(b->push)) > 0x30) {
        float push = static_cast<float>(b->push);
        float scaled = 99.8f * push;
        b->push = ppc_fctiwz(scaled / 100.0f);
    }
    int32_t wanted = b->push / 128;
    const int32_t* p = &gBumpTurnMax;
    if (wanted < *p) p = &wanted;
    if (!(*p > gBumpTurnMin)) p = &gBumpTurnMin;
    Word_3(b, 0x5c) = *p;
    if (owner->state == 2 && TickGate()) {
        if ((NextRandom() & 7) == 2) {
            tag_sNode* node = gRideAnimSets[static_cast<uint32_t>(Word_3(b, 8))]->mHeads->mHeader->mF78.get();
            float sx = 1024.0f * node->mMatrix.m[3][0];
            float sy = 1024.0f * node->mMatrix.m[3][1];
            float sz = 1024.0f * node->mMatrix.m[3][2];
            PAR_InitEmitter(0x60, ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz));
        }
    }
    if (Word_3(b, 0x20) != 0) UpdateSound(b, owner);
}

// Case 0x10029f1c: steers toward the velocity (kind -1: toward the target at 0x50) and launches the head's emitter now and then.
void SteerByVelocity(tagBUMP* b, tagBUMPRIDE* owner) {
    if (owner->state == 2) {
        if (owner->kind != -1) {
            AimAlongVelocity(b);
        } else {
            Word_3(b, 0x58) = Word_3(b, 0x50);
        }
        Word_3(b, 0x5c) = Turn(b);
    } else {
        Word_3(b, 0x5c) = 0;
    }
    owner = reinterpret_cast<tagBUMPRIDE*>(b->owner.get());
    if (owner->state == 2 && (b->flags & 0x4000) && TickGate() && Word_3(b, 0x28) != -1) {
        if ((NextRandom() & 3) == 2) {
            if (reinterpret_cast<tagBUMPRIDE*>(b->owner.get())->kind != -1 && Word_3(b, 0x14) == 0) {
                LaunchFromHead(b, 1, 0x28);
                Word_3(b, 0x14) = 4;
            }
        }
    }
    if (Word_3(b, 0x20) != 0) UpdateSound(b, reinterpret_cast<tagBUMPRIDE*>(b->owner.get()));
    FollowHead(b);
}

// Case 0x1002a3c0: the car's anim by speed, the head's emitters, then the default steering.
void Drive(tagBUMP* b, tagBUMPRIDE* owner) {
    if (owner->state == 2 && TickGate() && Word_3(b, 0x4c) > 0xe6 && Word_3(b, 0x14) == 0) Word_3(b, 0x14) = 2;
    if (Word_3(b, 0x4c) > 0x28) {
        if (Word_3(b, 0x10) == 0xc) {
            Word_3(b, 0x10) = 5;
            SetCarAnim(b, 5, 1);
        }
    } else if (Word_3(b, 0x10) == 5 && (b->flags & 0x400) == 0) {
        Word_3(b, 0x10) = 0xc;
        SetCarAnim(b, 0xc, 2);
    }
    owner = reinterpret_cast<tagBUMPRIDE*>(b->owner.get());
    if (owner->state == 2 && owner->broken == 1 && TickGate() && Word_3(b, 0x4c) > 0x50 && Word_3(b, 0x24) != -1) {
        int32_t roll = NextRandom() & 7;
        if (roll == 2) {
            if (Word_3(b, 0x14) == 0) {
                LaunchFromHead(b, 10, 0x24);
                Word_3(b, 0x14) = 1;
            }
        } else if (roll < 2 && roll >= 0) {
            LaunchFromHead(b, 1, 0x24);
        }
    }
    if (Word_3(b, 0x20) != 0) UpdateSound(b, reinterpret_cast<tagBUMPRIDE*>(b->owner.get()));
    FollowHead(b);
}

// Case default (0x1002a9f4): steers toward the velocity and keeps the turn's change.
void SteerDefault(tagBUMP* b) {
    AimAlongVelocity(b);
    int32_t d = (Word_3(b, 0x58) - Word_3(b, 0x54)) & 0x1ff;
    if (d > 0x100) d = -(0x200 - d);
    Word_3(b, 0x60) = Word_3(b, 0x5c);
    Word_3(b, 0x5c) = (d * Word_3(b, 0x4c)) / 1000;
    Word_3(b, 0x60) = Word_3(b, 0x5c) - Word_3(b, 0x60);
}

}  // namespace

void BMP_Move(tagBUMP* b) {
    tagBUMPRIDE* owner = reinterpret_cast<tagBUMPRIDE*>(b->owner.get());
    int32_t scale = Word_3(owner, 0x24);
    if (b->flags & 0x80000) {
        scale = (scale * 7) / 10;
    } else if (owner->broken == 2) {
        scale = 1000;
    }
    b->vx = (b->vx * scale) / 1024;
    b->dirX = b->vx;
    b->vy = (b->vy * scale) / 1024;
    b->dirY = b->vy;
    int32_t square = b->vx * b->vx + b->vy * b->vy;
    Word_3(b, 0x4c) = ppc_fctiwz(std::sqrt(static_cast<double>(square)));
    b->x = b->x + b->vx;
    b->y = b->y + b->vy;

    owner = reinterpret_cast<tagBUMPRIDE*>(b->owner.get());
    switch (owner->kind) {   // the table at 0x1029cb38 on kind + 14; -2 and the kinds outside -14..-1 take the default
    case -14:
    case -11:
    case -6:
    case -3:
    case -1:
        SteerByVelocity(b, owner);
        break;
    case -13:
    case -12:
    case -8:
    case -5:
        SteerByPush(b, owner);
        break;
    case -10:
    case -9:
    case -7:
    case -4:
        Drive(b, owner);
        SteerDefault(b);
        break;
    default:
        SteerDefault(b);
        break;
    }

    Word_3(b, 0x54) = (Word_3(b, 0x54) + Word_3(b, 0x5c)) & 0x1ff;
    tabCOLOBJECT* col = b->col.get();
    if (col == nullptr) return;
    if ((col->flags & 0x100) == 0) {
        if (b->flags & 0x200) BMP_MaskCar(b, 0);
        return;
    }
    Ptr32<tagBUMPTRIGGER> link;
    link.mOffset = col->pad14[1];   // the object's word 0x18
    tagBUMPTRIGGER* trigger = link.get();
    if (trigger == nullptr || trigger->magic != 0xdac) return;
    int32_t d = trigger->axis == 0xc ? b->x - col->x : b->y - col->y;
    uint32_t kind = 0;   // the Mac reads its stack slot unset when no anim set is named (index 0)
    uint32_t index = 0;
    if (trigger->open == 0) {
        uint32_t f = b->flags;
        if ((f & 0x200) == 0 && d < 0x4b0 && d > -0x4b0) {
            if (d < 0) b->flags = f | 0x800000;
            else b->flags = f & ~0x800000u;
            BMP_MaskCar(b, 1);
            if (trigger->animSet != 0) GetCurrentAnim(gRideAnimSets[trigger->animSet], &kind, &index, 0);
            if (kind != 5) {
                if (trigger->animSet != 0) FlushDeferredAnim(gRideAnimSets[trigger->animSet], 0);
                if (trigger->animSet != 0) SetCurRideAnim(gRideAnimSets[trigger->animSet], 5, 0, 0, 1.0f, 0);
                trigger->flags |= 2;
            }
        }
    } else {
        uint32_t f = b->flags;
        if (f & 0x200) {
            uint32_t right = f & 0x800000;
            if ((right != 0 && d > 0x4b0) || (right == 0 && d < -0x4b0)) BMP_MaskCar(b, 0);
        }
    }
    if (trigger->flags & 2) {
        if (trigger->animSet != 0) GetDeferredAnim(gRideAnimSets[trigger->animSet], &kind, &index, 0);
        if (kind == 0xc) {
            if (trigger->animSet != 0) SetCurRideAnim(gRideAnimSets[trigger->animSet], 2, 0, 1, 1.0f, 0);
            trigger->flags &= ~2u;
        }
    }
}

// 0x1002ad34
void NormaliseVector(s3DPoint* v) {
    double dx = static_cast<double>(v->x) * static_cast<double>(v->x);
    float yy = v->y * v->y;
    float zz = v->z * v->z;
    double sum = static_cast<double>(zz) + (dx + static_cast<double>(yy));
    float len = static_cast<float>(std::sqrt(sum));
    v->x = v->x / len;
    v->y = v->y / len;
    v->z = v->z / len;
}

// ---- the lobby leaves
// 0x1002addc: the matrix of the data node `index` of the mesh (entries of 0x14 bytes on the Mac).
float* GetDataNodeWorldMatrix(tag_sMesh*& mesh, uint32_t& index) {
    tagDataNodeTable* table = static_cast<tagDataNodeTable*>(mesh->mF28);
    return table->mEntries[index].mMatrix;
}

// 0x1002ae38: the slot of the ride in gRideAnimSets.
tag_sRideAnimSet** GetRideFromID(uint32_t& id) {
    return gRideAnimSets + id;
}

// 0x1002ae74
void BMP_MaskCar(tagBUMP* b, int32_t mask) {
    if (mask != 0) {
        if (b->flags & 0x200) return;
        b->flags |= 0x200;
        DisableSet(Wu(b, 8));
        if ((b->flags & 0x2002000) && Wu(b, 0xc) != 0) DisableSet(Wu(b, 0xc));
        for (uint8_t* node = Pw(b, 0x30); node != nullptr; node = Pw(node, 0x10)) {
            if (Wu(node, 8) != 0xffffffffu) DisableRideHead(gRideAnimSets[Wu(b, 8)]->mHeads, Wu(node, 8));
        }
    } else {
        if (!(b->flags & 0x200)) return;
        b->flags &= ~0x200u;
        EnableSet(Wu(b, 8));
        if ((b->flags & 0x2002000) && Wu(b, 0xc) != 0) EnableSet(Wu(b, 0xc));
        for (uint8_t* node = Pw(b, 0x30); node != nullptr; node = Pw(node, 0x10)) {
            if (Wu(node, 8) != 0xffffffffu) EnableRideHead(gRideAnimSets[Wu(b, 8)]->mHeads, Wu(node, 8), 0);
        }
    }
}

// 0x1002b0dc: out = (x, y) / 302.7 truncated.
void MineToSystemCords(int32_t x, int32_t y, NS_MacDoze::tagPOINT* out) {
    float fx = static_cast<float>(x) / 302.7f;
    float fy = static_cast<float>(y) / 302.7f;
    int32_t* pt = reinterpret_cast<int32_t*>(out);
    pt[0] = static_cast<int32_t>(fx);
    pt[1] = static_cast<int32_t>(fy);
}

// 0x1002b180: one event on the sound category [2] at the bump's position.
void SingleSoundAtBump(tagBUMP* bump, int32_t event) {
    float fy = static_cast<float>(bump->y) / 302.7f;
    float fx = static_cast<float>(bump->x) / 302.7f;
    SOUND_SendEvent(0, gSoundBanks[2], event, static_cast<int32_t>(fx), 0, static_cast<int32_t>(fy));
}

// 0x1002b248: moves the bump's sound (handle at 0x20) to its position over 302.7.
void PossitionSound1(tagBUMP* bump) {
    int32_t* handle = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(bump) + 0x20);
    if (*handle != 0) {
        float fy = static_cast<float>(bump->y) / 302.7f;
        float fx = static_cast<float>(bump->x) / 302.7f;
        *handle = SOUND_MoveSoundEffect(*handle, static_cast<int32_t>(fx), 0, static_cast<int32_t>(fy));
    }
}

// 0x1002b318: the length of (dx, dy) in integers; both halves are shifted right until they fit, the root is shifted back.
int32_t BMP_Distance(int32_t dx, int32_t dy) {
    int32_t a = dx < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(dx)) : dx;   // labs
    int32_t b = dy < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(dy)) : dy;
    uint32_t shift = 0;
    while (a > 0x8000 || b > 0x7fff) {
        a >>= 1;
        shift++;
        b >>= 1;
    }
    int32_t sum = a * a + b * b;
    double root = std::sqrt(static_cast<double>(sum));
    int32_t r = static_cast<int32_t>(root);
    return static_cast<int32_t>(static_cast<uint32_t>(r) << (shift & 0x3f));
}

// Unit v5112: point tests over the collision list (PointOutAllInverse, PointInAnyColObject, NavPointInArea).

NS_MacDoze::tagPOINT* NavPointInArea(tabCOLOBJECT* object, NS_MacDoze::tagPOINT* out) {
    int32_t* pt = reinterpret_cast<int32_t*>(out);   // x at +0, y at +4
    pt[0] = object->x;
    pt[1] = object->y;
    uint32_t f = object->flags;
    if ((f & 8) == 0) pt[1] = pt[1] - 0x87c;
    else if ((f & 0x10) == 0) pt[1] = pt[1] + 0x87c;
    if ((f & 2) == 0) pt[0] = pt[0] + 0x87c;
    else if ((f & 4) == 0) pt[0] = pt[0] - 0x87c;
    return out;
}

// Unit v5367: the collision object tests of the advisor world, main app.
//   PointInColObject 0x1002bc34, DoAreasIntersect 0x1002b6c0, ClosestIntersectingColObjectToPoint 0x1002b4cc

namespace {

// The Mac's labs on a 4-byte long (wraps for INT_MIN).
inline int32_t Labs32(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }

// Distance of (dx, dy) scaled down until both fit, as the original: sqrt of the squares, then shifted back up.
int32_t ScaledDistance(int32_t dx, int32_t dy) {
    int32_t a = Labs32(dx);
    int32_t b = Labs32(dy);
    uint32_t shift = 0;
    while (a > 0x8000 || b > 0x7fff) {
        a >>= 1;
        b >>= 1;
        shift++;
    }
    int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(a) +
                                       static_cast<uint32_t>(b) * static_cast<uint32_t>(b));
    double root = std::sqrt(static_cast<double>(sum));
    int32_t q = (root >= 0.0 && root < 2147483648.0) ? static_cast<int32_t>(root) : INT32_MIN;
    return static_cast<int32_t>(shift >= 32 ? 0u : (static_cast<uint32_t>(q) << shift));
}

inline int32_t BigRadius(const tabCOLOBJECT* o) { return o->radiusX < o->radiusY ? o->radiusY : o->radiusX; }

}  // namespace

int32_t DoAreasIntersect(tabCOLOBJECT* a, tabCOLOBJECT* b) {
    if (a == b) return 1;
    uint32_t fa = a->flags & 0x1e;
    if (fa == 0x1e && (b->flags & 0x1e) == 0x1e) {
        int32_t d = ScaledDistance(a->x - b->x, a->y - b->y);
        int32_t sum = BigRadius(b) + BigRadius(a);
        return sum > d ? 1 : 0;
    }
    if (((a->flags | b->flags) & 0x1e) == 0x1e) {
        bool bothPartial;
        tabCOLOBJECT* other;
        tabCOLOBJECT* full;
        if (fa == 0x1e) {
            bothPartial = false;
            other = b;
            full = a;
        } else {
            other = a;
            full = b;
            bothPartial = (b->flags & 0x1e) != 0x1e;
        }
        uint32_t f = other->flags;
        if (((f & 2) == 0 || other->x <= full->x) && ((f & 4) == 0 || full->x <= other->x) &&
            ((f & 8) == 0 || full->y <= other->y) && ((f & 0x10) == 0 || other->y <= full->y)) {
            return 0;
        }
        if (bothPartial) {
            int32_t dy = Labs32(other->y - full->y);
            int32_t dx = Labs32(other->x - full->x);
            return dx + dy == 0x1800 ? 1 : 0;
        }
        int32_t d = ScaledDistance(other->x - full->x, other->y - full->y);
        int32_t sum = BigRadius(other) + BigRadius(full);
        return sum > d ? 1 : 0;
    }
    return (a->y == b->y && a->x == b->x) ? 1 : 0;
}

tabCOLOBJECT* ClosestIntersectingColObjectToPoint(tabCOLOBJECT* area, int32_t px, int32_t py, int32_t includeSelf) {
    int32_t best = INT32_MAX;
    tabCOLOBJECT* found = nullptr;
    tabCOLOBJECT* p = gColList.first.get();
    if (p != nullptr) {
        int32_t n = gColList.count;
        while (n != 0) {
            uint32_t f = p->flags;
            if ((f & 1) != 0) {
                int32_t x = p->x;
                int32_t y = p->y;
                n--;
                if ((f & 8) == 0) {
                    y -= 0x87c;
                } else if ((f & 0x10) == 0) {
                    y += 0x87c;
                }
                if ((f & 2) == 0) {
                    x += 0x87c;
                } else if ((f & 4) == 0) {
                    x -= 0x87c;
                }
                if ((f & 0x40) == 0 && (includeSelf != 0 || area != p)) {
                    if (DoAreasIntersect(area, p) != 0 && PointOutAllInverse(x, y) != 0) {
                        int32_t d = ScaledDistance(p->x - px, p->y - py);
                        if (d < best) {
                            best = d;
                            found = p;
                        }
                    }
                }
            }
            p++;
        }
    }
    return found;
}

tabCOLOBJECT* PointInAnyColObject(int32_t x, int32_t y) {
    tabCOLOBJECT* p = gColList.first.get();
    if (p != nullptr) {
        int32_t n = gColList.count;
        while (n != 0) {
            uint32_t f = p->flags;
            if ((f & 1) != 0) {
                n--;
                if ((f & 0x40) == 0 && PointInColObject(x, y, p) != 0) return p;
            }
            p++;
        }
    }
    return nullptr;
}

int32_t PointOutAllInverse(int32_t x, int32_t y) {
    tabCOLOBJECT* p = gColList.first.get();
    if (p != nullptr) {
        int32_t n = gColList.count;
        while (n != 0) {
            uint32_t f = p->flags;
            if ((f & 1) != 0) {
                n--;
                if ((f & 0x40) != 0 && PointInColObject(x, y, p) != 0) return 0;
            }
            p++;
        }
    }
    return 1;
}

int32_t PointInColObject(int32_t x, int32_t y, tabCOLOBJECT* object) {
    if (object == nullptr) return 0;
    int32_t dx = object->x - x;
    uint32_t sides = dx < 0 ? 4 : 2;
    int32_t dy = object->y - y;
    sides |= dy < 0 ? 8 : 0x10;
    if (dx == 0 && dy == 0) return 1;
    uint32_t flags = object->flags;
    if (sides != (sides & flags)) return 0;
    if ((flags & 0x20) != 0) {
        if (Labs32(dx) <= object->radiusX && Labs32(dy) <= object->radiusY) return 1;
        return 0;
    }
    int32_t d = ScaledDistance(dx, dy);
    return object->radiusX >= d ? 1 : 0;
}

// BMP_InsideArea 0x1002c260 and BMP_OutsideArea 0x1002be04 (TPAdvisor.cpp): keep a moving bump inside its current collision
// object (circle or box) and push it out of the "outside area" objects, bouncing its velocity and nudging mPush.

namespace {

inline int32_t Labs(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }

// divw: the emulator answers the dividend for a zero divisor (the hardware is undefined).
inline int32_t DivW(int32_t a, int32_t b) {
    if (b == 0) return a;
    if (b == -1) return static_cast<int32_t>(0u - static_cast<uint32_t>(a));
    return a / b;
}

// (a * b) >> 10 rounded toward zero (mullw, srawi 10, addze).
inline int32_t Mul10(int32_t a, int32_t b) {
    int32_t p = static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
    return (p >> 10) + ((p < 0 && (p & 0x3ff) != 0) ? 1 : 0);
}

// neg then convert (the original negates the integer, so 0 stays +0.0).
inline double NegD(int32_t v) { return static_cast<double>(static_cast<int32_t>(0u - static_cast<uint32_t>(v))); }

// 256 - 256 * atan2(y, x) / 3.14159 as a truncated int: the angle in 512ths of a turn.
inline int32_t Angle(double y, double x) {
    double a = std::atan2(y, x);
    return static_cast<int32_t>(ppc_fnmsub(256.0, a / 3.14159, 256.0));
}

inline int32_t Table(int32_t i) { return gHostSineTable[i & 0x1ff]; }

inline int32_t IntSqrt(int32_t a, int32_t b) {
    int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(a) +
                                       static_cast<uint32_t>(b) * static_cast<uint32_t>(b));
    return static_cast<int32_t>(std::sqrt(static_cast<double>(sum)));
}

// The length of (dx, dy) as sqrt scaled down by halving until it fits, then shifted back up.
inline int32_t Length(int32_t dx, int32_t dy) {
    int32_t a = Labs(dx), b = Labs(dy);
    uint32_t shift = 0;
    while (a > 0x8000 || b > 0x7fff) {
        a >>= 1;
        b >>= 1;
        shift++;
    }
    int32_t root = IntSqrt(a, b);
    return (shift & 0x3f) >= 32 ? 0 : static_cast<int32_t>(static_cast<uint32_t>(root) << shift);
}

// The tail both functions share: nudge mPush by the distance moved, toward the direction the velocity turned.
inline void Nudge(tagBUMP* b, int32_t moved, int32_t angleBefore) {
    int32_t angleNow = Angle(static_cast<double>(b->vx), NegD(b->vy));
    int32_t m;
    int32_t v;
    if (((angleNow - angleBefore) & 0x1ff) < 0x100) {
        m = static_cast<int32_t>(0u - static_cast<uint32_t>(moved)) * 0x30;
        v = b->push + m;
        b->push = (m >= v) ? m : v;
    } else {
        m = moved * 0x30;
        v = b->push + m;
        b->push = (v >= m) ? m : v;
    }
}

inline bool KindPushes(int32_t kind) { return kind == -5 || kind == -8 || kind == -0xc || kind == -0xd; }

}  // namespace

void BMP_OutsideArea(tagBUMP* b) {
    tabCOLOBJECT* p = gColList.first.get();
    if (p == nullptr) return;
    int32_t n = gColList.count;
    while (n != 0) {
        uint32_t f = p->flags;
        if ((f & 1) != 0) {
            n--;
            if ((f & 0x40) != 0 && PointInColObject(b->x, b->y, p) != 0) {
                int32_t dx = b->x - p->x;
                int32_t dy = b->y - p->y;
                int32_t ang27 = Angle(NegD(b->dirX), static_cast<double>(b->dirY));
                int32_t ang28 = Angle(static_cast<double>(dx), NegD(dy));
                int32_t rel = (ang27 - ang28) & 0x1ff;
                if (rel < 0x80 || rel > 0x180) {
                    int32_t mag = IntSqrt(b->dirX, b->dirY);
                    tagBUMPOwner* owner = b->owner.get();
                    int32_t t = Mul10(mag, Table(rel + 0x80) * 4);
                    t = Mul10(t, owner->scale + 0x400);
                    b->vx = b->vx + Mul10(t, Table(ang28) * 4);
                    b->vy = b->vy + Mul10(t, Table(ang28 + 0x80) * 4);
                    b->flags |= 0x50000;
                    int32_t len = Length(dx, dy);
                    b->x = p->x + DivW(static_cast<int32_t>(static_cast<uint32_t>(dx) * static_cast<uint32_t>(p->radiusX)), len);
                    b->y = p->y + DivW(static_cast<int32_t>(static_cast<uint32_t>(dy) * static_cast<uint32_t>(p->radiusX)), len);
                    if (KindPushes(b->owner->kind)) {
                        int32_t moved = Labs(len - p->radiusX);
                        // the push uses the angle of (-vx, vy) against ang27
                        int32_t angleNow = Angle(NegD(b->vx), static_cast<double>(b->vy));
                        int32_t m;
                        int32_t v;
                        if (((angleNow - ang27) & 0x1ff) < 0x100) {
                            m = static_cast<int32_t>(0u - static_cast<uint32_t>(moved)) * 0x30;
                            v = b->push + m;
                            b->push = (m >= v) ? m : v;
                        } else {
                            m = moved * 0x30;
                            v = b->push + m;
                            b->push = (v >= m) ? m : v;
                        }
                    }
                }
            }
        }
        p++;
    }
}
void BMP_InsideArea(tagBUMP* b) {
    if (PointInColObject(b->x, b->y, b->col.get()) != 0) return;

    tabCOLOBJECT* p = gColList.first.get();
    int32_t y = b->y;
    int32_t x = b->x;
    bool found = false;
    if (p != nullptr) {
        int32_t n = gColList.count;
        while (n != 0) {
            uint32_t f = p->flags;
            if ((f & 1) != 0) {
                n--;
                if ((f & 0x40) == 0 && PointInColObject(x, y, p) != 0) {
                    found = true;
                    break;
                }
            }
            p++;
        }
    }
    if (found) {
        b->col = p;
        b->flags |= 0x10000;
        return;
    }

    tabCOLOBJECT* cur = b->col.get();
    if (cur == nullptr) return;
    if (b->counter >= 0) b->counter++;

    int32_t dx = b->x - cur->x;
    int32_t dy = b->y - cur->y;
    int32_t ang = Angle(static_cast<double>(b->dirX), NegD(b->dirY));
    int32_t moved = 0;   // r23: the caller's value when nothing moves (the original leaves it unset)
    tagBUMPOwner* owner = b->owner.get();

    if ((cur->flags & 0x20) != 0) {
        if (Labs(dx) > cur->radiusX) {
            b->vx = Mul10(0 - b->vx, owner->scale);
            b->x = cur->x + (dx < 0 ? -cur->radiusX : cur->radiusX);
            moved = Labs(cur->radiusX - Labs(dx));
        }
        if (Labs(dy) > cur->radiusY) {
            b->vy = Mul10(0 - b->vy, owner->scale);
            b->y = cur->y + (dy < 0 ? -cur->radiusY : cur->radiusY);
            moved = Labs(cur->radiusY - Labs(dy));
        }
    } else {
        int32_t ang2 = Angle(static_cast<double>(dx), NegD(dy));
        int32_t a = (ang + 0x100) & 0x1ff;
        int32_t bb = (ang2 - 0x80) & 0x1ff;
        int32_t c = ((2 * bb - a) + 0x100) & 0x1ff;
        int32_t d = c - ang;
        int32_t e = d & 0x1ff;
        int32_t s = Table(d);
        int32_t co = Table(e + 0x80);
        int32_t vyScaled = Mul10(b->vy, owner->scale);
        int32_t vxScaled = Mul10(b->vx, owner->scale);
        int32_t newVx = Mul10(vyScaled, s * 4) ;
        // vx' = (vyScaled * s * 4 + vxScaled * co * 4) >> 10 rounded toward zero
        {
            int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(vxScaled) * static_cast<uint32_t>(co * 4) +
                                               static_cast<uint32_t>(vyScaled) * static_cast<uint32_t>(s * 4));
            newVx = (sum >> 10) + ((sum < 0 && (sum & 0x3ff) != 0) ? 1 : 0);
        }
        b->vx = newVx;
        {
            int32_t diff = static_cast<int32_t>(static_cast<uint32_t>(vyScaled) * static_cast<uint32_t>(co * 4) -
                                                static_cast<uint32_t>(vxScaled) * static_cast<uint32_t>(Table(e) * 4));
            b->vy = (diff >> 10) + ((diff < 0 && (diff & 0x3ff) != 0) ? 1 : 0);
        }
        int32_t len = Length(dx, dy);
        moved = Labs(cur->radiusX - len);
        b->x = cur->x + DivW(static_cast<int32_t>(static_cast<uint32_t>(dx) * static_cast<uint32_t>(cur->radiusX)), len);
        b->y = cur->y + DivW(static_cast<int32_t>(static_cast<uint32_t>(dy) * static_cast<uint32_t>(cur->radiusX)), len);
    }

    if (KindPushes(owner->kind)) Nudge(b, moved, ang);
}

// Unit v5062: the advisor's bump (ride launch) functions, main app:
//   SetupHeadIDs 0x1002cddc, BMP_AddJoiningListToBump 0x1002cc70, BMP_LAUNCH 0x1002cff4, BMP_Collision 0x1002c7ec.
//
// The bump (0xac bytes apart in the list at 0x102b56c0 + 0x10), the ride record (0xd0 bytes apart, the list's +8) and the head
// nodes (0x14 bytes, a chain) are structures of the Mac's 32-bit layout, read here by Mac offset; the words that hold addresses
// hold host addresses that fit in 32 bits (the arena). The ride anim sets and meshes use the host's own layout.

namespace {

uint32_t& R(uintptr_t base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }
int32_t& S(uintptr_t base, uint32_t off) { return *reinterpret_cast<int32_t*>(base + off); }

// A word that holds an address is a Ptr32 offset from the arena base (0 is null).
uintptr_t Deref(uint32_t offset) {
    Ptr32<char> pointer;
    pointer.mOffset = offset;
    return reinterpret_cast<uintptr_t>(pointer.get());
}
uint32_t Offset(uintptr_t address) {
    Ptr32<char> pointer;
    pointer.set(reinterpret_cast<char*>(address));
    return pointer.mOffset;
}
uintptr_t P_2(uintptr_t base, uint32_t off) { return Deref(R(base, off)); }

uintptr_t ListBase() { return reinterpret_cast<uintptr_t>(&gColList); }   // 0x102b56c0

// fctiwz: round toward zero, a NaN or an out-of-range value gives 0x80000000 (a positive overflow saturates).
int32_t Cvt(double d) {
    if (d != d) return INT32_MIN;
    if (d >= 2147483648.0) return INT32_MAX;
    if (d <= -2147483649.0) return INT32_MIN;
    return static_cast<int32_t>(d);
}

int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t Sub(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

// srawi 10 then addze: division by 1024 toward zero.
int32_t Q(int32_t v) { return v / 1024; }

// divw (the emulator answers the dividend for 0 and INT_MIN / -1).
int32_t Div(int32_t a, int32_t b) {
    if (b == 0 || (a == INT32_MIN && b == -1)) return a;
    return a / b;
}

// The ride record the id names: the list's magic, the record array at +8, the id = (record[0] << 8) | index.
bool ListIsLive() { return R(ListBase(), 0) == 0x4a454647u; }

// The jump table at 0x1029cb70 (12 words, kind + 14): 3 and 5 are the two targets that mark both bumps and set the +0x14 word.
const int kKindAction[12] = {5, 0, 0, 5, 3, 3, 0, 3, 5, 0, 3, 5};

}  // namespace

// 0x1002c7ec, 1112 bytes: pushes every live bump that overlaps this one apart. The direction angles of both decide: a head-on
// meeting (the angle between 0x80 and 0x180 of 512) moves the positions back, otherwise the velocities are exchanged along
// the line, and for two kinds of owner a hard hit marks both bumps (flag 0x200000, +0x14 = 3 or 5).
void BMP_Collision(tagBUMP* self) {
    const uintptr_t p = reinterpret_cast<uintptr_t>(self);
    const uintptr_t g = ListBase();
    uintptr_t q = P_2(g, 0x10);
    if ((R(P_2(p, 0x98), 0) & 0x80) != 0) return;
    if ((R(p, 0) & 0x200) != 0) return;
    int32_t left = S(g, 0x14);
    for (; left != 0; q += 0xac) {
        if ((R(q, 0) & 1) == 0) continue;
        left--;
        if (q == p) continue;
        if ((R(P_2(q, 0x98), 0) & 0x80) != 0) continue;
        if ((R(q, 0) & 0x200) != 0) continue;

        const int32_t dx = Sub(S(q, 0x34), S(p, 0x34));
        const int32_t dy = Sub(S(q, 0x38), S(p, 0x38));
        const int32_t radii = Add(S(q, 0x64), S(p, 0x64));
        int32_t ax = static_cast<int32_t>(std::labs(static_cast<long>(dx)));
        int32_t ay = static_cast<int32_t>(std::labs(static_cast<long>(dy)));
        uint32_t shifts = 0;
        while (ax > 0x8000 || ay > 0x7fff) {
            ax >>= 1;
            ay >>= 1;
            shifts++;
        }
        const int32_t sum = Add(Mul(ax, ax), Mul(ay, ay));
        const uint32_t scaled = static_cast<uint32_t>(Cvt(std::sqrt(static_cast<double>(sum)))) << (shifts & 0x3f);
        const int32_t dist = (shifts & 0x3f) >= 32 ? 0 : static_cast<int32_t>(scaled);
        if (dist == 0) continue;
        if (dist >= radii) continue;

        const int32_t ey = Sub(S(p, 0x48), S(q, 0x48));
        const int32_t ex = Sub(S(p, 0x44), S(q, 0x44));
        const double a1 = std::atan2(static_cast<double>(ex), static_cast<double>(Sub(0, ey)));
        const int32_t angleDir = Cvt(std::fma(-256.0, a1 / 3.14159, 256.0));
        const double a2 = std::atan2(static_cast<double>(dx), static_cast<double>(Sub(0, dy)));
        const int32_t angleLine = Cvt(std::fma(-256.0, a2 / 3.14159, 256.0));
        const int32_t rel = Sub(angleDir, angleLine) & 0x1ff;

        if (rel < 0x80 || rel > 0x180) {
            const int32_t len = Cvt(std::sqrt(static_cast<double>(Add(Mul(ex, ex), Mul(ey, ey)))));
            const int32_t t8 = gHostSineTable[(rel + 0x80) & 0x1ff] * 4;
            const int32_t t7 = gHostSineTable[(angleLine + 0x80) & 0x1ff] * 4;
            const int32_t t6 = gHostSineTable[angleLine & 0x1ff] * 4;
            const int32_t scale = S(P_2(q, 0x9c), 0x2c);
            const int32_t a = Q(Mul(len, t8));
            const int32_t part = Q(Mul(a, scale));
            S(q, 0x3c) = Add(S(q, 0x3c), Q(Mul(t6, part)));
            S(q, 0x40) = Add(S(q, 0x40), Q(Mul(t7, part)));
            const int32_t rest = Sub(a, part);
            R(q, 0) = R(q, 0) | 0x50000;
            S(p, 0x3c) = Sub(S(p, 0x3c), Q(Mul(t6, rest)));
            S(p, 0x40) = Sub(S(p, 0x40), Q(Mul(t7, rest)));
            const uint32_t idx = static_cast<uint32_t>(S(P_2(p, 0x9c), 0) + 0xe);
            if (idx <= 0xb) {
                const int action = kKindAction[idx];
                if (action != 0 && part > 0x82) {
                    if (((R(p, 0) | R(q, 0)) & 0x200000u) == 0) {
                        R(p, 0) = R(p, 0) | 0x200000;
                        R(q, 0) = R(q, 0) | 0x200000;
                        S(p, 0x14) = action;
                    }
                }
            }
        } else {
            const int32_t scale = S(P_2(q, 0x9c), 0x2c);
            const int32_t oldX = S(q, 0x34);
            const int32_t nx = Sub(dx, Div(Mul(dx, radii), dist));
            const int32_t ny = Sub(dy, Div(Mul(dy, radii), dist));
            const int32_t qx = Q(Mul(nx, scale));
            S(q, 0x34) = Sub(oldX, qx);
            const int32_t qy = Q(Mul(ny, scale));
            S(q, 0x38) = Sub(S(q, 0x38), qy);
            S(p, 0x34) = Add(S(p, 0x34), Sub(nx, qx));
            S(p, 0x38) = Add(S(p, 0x38), Sub(ny, qy));
        }
    }
}
// 0x1002cddc, 296 bytes: numbers the bump's chain of head nodes from 1, finds each head's data node in the ride's anim set,
// enables it and (flag set) starts an emitter at the head node's position.
void SetupHeadIDs(tagBUMP* bump, int32_t flag) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(bump);
    uintptr_t node = P_2(b, 0x30);
    if (node == 0) return;
    uint32_t id = 1;
    do {
        tag_sRideAnimSet* set = gRideAnimSets[S(b, 8)];
        const uint32_t index = FindDataNodeIndexByID(set, 0x80, id);
        R(node, 8) = index;
        R(node, 0xc) = id;
        id++;
        if (R(node, 8) != 0xffffffffu) {
            set = gRideAnimSets[S(b, 8)];
            EnableRideHead(set->mHeads, R(node, 8), R(node, 0));
            if (flag != 0) {
                set = gRideAnimSets[S(b, 8)];
                const uintptr_t fnode = Deref(set->mHeads->mHeader->mF78.mOffset);
                float f0, f1, f2;
                std::memcpy(&f0, reinterpret_cast<const void*>(fnode + 0x40), 4);
                std::memcpy(&f1, reinterpret_cast<const void*>(fnode + 0x44), 4);
                std::memcpy(&f2, reinterpret_cast<const void*>(fnode + 0x48), 4);
                const float scale = 1024.0f;
                const float x = scale * f0;
                const float y = scale * f1;
                const float z = scale * f2;
                PAR_InitEmitter(0x13, Cvt(static_cast<double>(x)), Cvt(static_cast<double>(y)), Cvt(static_cast<double>(z)));
            }
        }
        const uintptr_t ride = P_2(b, 0x9c);
        R(ride, 0x60) = R(ride, 0x60) + 1;
        node = P_2(node, 0x10);
    } while (node != 0);
}

// 0x1002cc70, 316 bytes: gives the first live bump of the ride that has no head chain the ride's waiting chain (+0xc4).
// With a list that is not live and the id 0 the original reads through a null ride (not reproduced as a case).
int32_t BMP_AddJoiningListToBump(int32_t rideId) {
    const uintptr_t g = ListBase();
    uintptr_t ride = 0;
    int32_t id = 0;
    if (ListIsLive()) {
        ride = P_2(g, 8) + static_cast<uint32_t>(rideId & 0xff) * 0xd0;
        const int32_t index = static_cast<int32_t>((ride - P_2(g, 8)) / 0xd0);
        id = static_cast<int32_t>((R(ride, 0) << 8) | static_cast<uint32_t>(index));
    }
    if (rideId == id && S(ride, 0x50) != 0) {
        uintptr_t bump = P_2(g, 0x10);
        int32_t count = S(g, 0x14);
        while (count != 0) {
            if ((R(bump, 0) & 1) != 0) {
                count--;
                if (P_2(bump, 0x9c) == ride && R(bump, 0x30) == 0) {
                    R(bump, 0x30) = R(ride, 0xc4);
                    R(ride, 0xc4) = 0;
                    R(bump, 0) = R(bump, 0) | 0x84000;
                    R(bump, 0x88) = R(ride, 4);
                    SetupHeadIDs(reinterpret_cast<tagBUMP*>(bump), 1);
                    SetControlType(reinterpret_cast<tagBUMPRIDE*>(ride), reinterpret_cast<tagBUMP*>(bump));
                    return 1;
                }
            }
            bump += 0xac;
        }
    }
    return 0;
}

// 0x1002cf30
int32_t BMP_GetCameraMesh(int32_t ride) {
    int32_t id;
    uint8_t* record = LookupRide(ride, &id);
    if (ride == id) {
        uint8_t* mesh = P(record, 0x58).get();
        if (mesh != nullptr && (W(mesh, 0) & 0x400000) != 0) return static_cast<int32_t>(W(mesh, 8));
    }
    return 0;
}

// 0x1002cff4, 808 bytes: launches a rider bump from the ride; with flags 0 it also instances the sub ride and sets up the heads.
// The answer is the new bump (a 32-bit address) or 0.
int32_t BMP_LAUNCH(int32_t rideId, int32_t flags) {
    const uintptr_t g = ListBase();
    uintptr_t ride = 0;
    int32_t id = 0;
    if (ListIsLive()) {
        ride = P_2(g, 8) + static_cast<uint32_t>(rideId & 0xff) * 0xd0;
        const int32_t index = static_cast<int32_t>((ride - P_2(g, 8)) / 0xd0);
        id = static_cast<int32_t>((R(ride, 0) << 8) | static_cast<uint32_t>(index));
    }
    if (rideId != id) return 0;
    if (!(S(ride, 0x5c) < S(ride, 0x64))) return 0;
    if (S(ride, 0x5c) >= 0x40) return 0;
    if (S(ride, 0x50) == 0) return 0;
    tagBUMP* started = BMP_StartBump(S(ride, 0x74), S(ride, 0x78), S(ride, 8));
    if (started == nullptr) return 0;
    const uintptr_t bump = reinterpret_cast<uintptr_t>(started);

    R(bump, 0) = R(bump, 0) | 0x4000;
    R(bump, 0x9c) = Offset(ride);
    R(ride, 0x5c) = R(ride, 0x5c) + 1;
    R(bump, 0x88) = R(ride, 4);

    uintptr_t col = P_2(g, 0x18);
    const int32_t y = S(ride, 0x78);
    const int32_t x = S(ride, 0x74);
    if (col != 0) {
        int32_t count = S(g, 0x1c);
        bool found = false;
        while (count != 0) {
            const uint32_t f = R(col, 0);
            if ((f & 1) != 0) {
                count--;
                if ((f & 0x40) == 0 && PointInColObject(x, y, reinterpret_cast<tabCOLOBJECT*>(col)) != 0) {
                    found = true;
                    break;
                }
            }
            col += 0x20;
        }
        if (!found) col = 0;
    }
    R(bump, 0x98) = Offset(col);
    R(bump, 0x30) = R(ride, 0xc4);
    R(ride, 0xc4) = 0;

    if (flags == 0) {
        R(bump, 0) = R(bump, 0) | 0x100000;
        const int32_t count = S(ride, 0x5c);
        const int32_t size = S(ride, 0x18);
        S(bump, 4) = Add(S(ride, 0x14), Sub(count, Mul(Div(count, size), size)));
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> rideType;
        rideType.mIndex = static_cast<uint16_t>(R(ride, 0x10) & 0xffff);
        RideInfo* info = gRideDatabase.GetRideInfo(rideType);
        S(bump, 8) = static_cast<int32_t>(InstanceRide(info->mSubRideTypes[S(bump, 4)], 0x101, nullptr, 0, 0, nullptr, 0, 0, 0xffffffffu, 0));
        if (R(bump, 8) != 0) SetCurRideAnim(gRideAnimSets[R(bump, 8)], 0xc, 0, 2, 1.0f, 0);
        R(bump, 0x10) = 0xc;
        if (R(ride, 0x58) == 0) {
            R(ride, 0x58) = Offset(bump);
            R(bump, 0) = R(bump, 0) | 0x400000;
        }
        uintptr_t node = P_2(bump, 0x30);
        if (node != 0) {
            uint32_t headId = 1;
            do {
                tag_sRideAnimSet* set = gRideAnimSets[S(bump, 8)];
                R(node, 8) = FindDataNodeIndexByID(set, 0x80, headId);
                R(node, 0xc) = headId;
                headId++;
                if (R(node, 8) != 0xffffffffu) {
                    set = gRideAnimSets[S(bump, 8)];
                    EnableRideHead(set->mHeads, R(node, 8), R(node, 0));
                }
                const uintptr_t owner = P_2(bump, 0x9c);
                R(owner, 0x60) = R(owner, 0x60) + 1;
                node = P_2(node, 0x10);
            } while (node != 0);
        }
        SetControlType(reinterpret_cast<tagBUMPRIDE*>(ride), started);
        R(bump, 0) = R(bump, 0) | 0x4000000;
    } else {
        R(bump, 0) = R(bump, 0) | static_cast<uint32_t>(flags);
        R(bump, 0x80) = 0;
        R(bump, 0x7c) = 0;
    }
    return static_cast<int32_t>(Offset(bump));
}

// 0x1002d340: the first buoy of the ride with flag 4 (no end test: the original would run off the list).
tabBUOY* GetFinishLine(tagBUMPRIDE* ride) {
    tabBUOY* buoy = PtrAt<tabBUOY>(ride, 0xa8);
    while ((buoy->flags & 4) == 0) buoy = buoy->next.get();
    return buoy;
}

// 0x1002d390: the ride's word 0xa4 becomes the first buoy after the finish line (wrapping to the first) without flag 2.
void SetBuoySequence(tagBUMPRIDE* ride) {
    tabBUOY* first = PtrAt<tabBUOY>(ride, 0xa8);
    tabBUOY* buoy = first;
    while ((buoy->flags & 4) == 0) buoy = buoy->next.get();
    do {
        buoy = buoy->next.get();
        if (buoy == nullptr) buoy = first;
    } while ((buoy->flags & 2) != 0);
    reinterpret_cast<Ptr32<tabBUOY>*>(reinterpret_cast<uint8_t*>(ride) + 0xa4)->set(buoy);
}

// 0x1002d404, 2768 bytes: sets up the bump's behaviour for the kind of its ride (the kind -14 to -1 go through a jump table at
// 0x1029cba0, the rest take the default). The ride record (0xd0 bytes) and the bump (0xac bytes) are read by Mac offset; the
// list is the one at 0x102b56c0 (+0 magic, +8 rides, +0x10 bumps, +0x14 their count, +0x20 the table of 0x1c bytes, +0x24 its
// count). Pointer words inside the list, the rides and the bumps are Ptr32 offsets from the arena base.
namespace {

inline uint32_t& R_2(uintptr_t base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }
inline int32_t& S_2(uintptr_t base, uint32_t off) { return *reinterpret_cast<int32_t*>(base + off); }
inline uintptr_t P_3(uintptr_t base, uint32_t off) {
    const uint32_t v = R_2(base, off);
    return v != 0 ? reinterpret_cast<uintptr_t>(tpw::gArenaBase) + v : 0;
}
inline uint32_t Off(uintptr_t p) { return p != 0 ? tpw::ArenaOffset(reinterpret_cast<const void*>(p)) : 0; }

inline int32_t Labs_2(int32_t x) { return x < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(x)) : x; }
inline uint32_t Mod(uint32_t a, uint32_t b) { return b != 0 ? a % b : a; }   // divwu by 0 answers 0 in the emulator
// srawi 8 then addze: the division by 256 toward zero
inline int32_t Div256_2(int32_t v) { return (v >> 8) + (v < 0 && (v & 0xff) != 0 ? 1 : 0); }

}  // namespace

void SetControlType(tagBUMPRIDE* rideArg, tagBUMP* bumpArg) {
    const uintptr_t ride = reinterpret_cast<uintptr_t>(rideArg);
    const uintptr_t bump = reinterpret_cast<uintptr_t>(bumpArg);
    const uintptr_t list = reinterpret_cast<uintptr_t>(&gColList);

    auto rideIdOf = [&](uintptr_t r) -> int32_t {
        if (R_2(list, 0) != 0x4a454647u) return 0;
        const int32_t index = static_cast<int32_t>(Off(r) - R_2(list, 8)) / 0xd0;
        return static_cast<int32_t>((R_2(r, 0) << 8) | static_cast<uint32_t>(index));
    };
    // the sound of the bump: the advisor's own thing gets no entity
    auto startSound = [&]() {
        const uintptr_t owner = P_3(bump, 0x9c);
        uint16_t id = 0;
        TrackRideIdToThingId(&id, rideIdOf(owner));
        int32_t entity;
        if (id != gAdvisorThingId) {
            const uint16_t keep = id;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            entity = static_cast<int32_t>(R_2(reinterpret_cast<uintptr_t>(gThingArray[keep].mThing), 0x20));
        } else {
            entity = 0;
        }
        R_2(bump, 0x20) = static_cast<uint32_t>(SND_SendIndirectedEvent(entity, 0, gSoundBanks[7], 0, 0, 0, 0));
        const int32_t handle = static_cast<int32_t>(R_2(bump, 0x20));
        if (handle != 0) {
            const float fy = static_cast<float>(S_2(bump, 0x38));
            const float fx = static_cast<float>(S_2(bump, 0x34));
            const float qx = fx / 302.7f;
            const float qy = fy / 302.7f;
            const int32_t ix = ppc_fctiwz(static_cast<double>(qx));
            const int32_t iy = ppc_fctiwz(static_cast<double>(qy));
            R_2(bump, 0x20) = static_cast<uint32_t>(SOUND_MoveSoundEffect(handle, ix, 0, iy));
        }
    };
    auto instanceSub = [&]() {
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type;
        type.mIndex = static_cast<uint16_t>(R_2(ride, 0x10) & 0xffff);
        RideInfo* info = gRideDatabase.GetRideInfo(type);
        R_2(bump, 0xc) = InstanceRide(info->mSubRideTypes[0], 0x101, nullptr, 0, 0, nullptr, 0, 0, 0xffffffffu, 0);
    };
    auto rand = [&]() -> uint32_t { return gGameWorld->GetRand(); };
    auto nextNode = [&](uintptr_t node) -> uintptr_t {
        const uintptr_t n = P_3(node, 0x14);
        return n != 0 ? n : P_3(ride, 0xa8);
    };
    auto tableIndexOk = [&](int32_t i) { return !(i < 0 || i >= S_2(list, 0x24)); };

    uintptr_t node = 0;
    int32_t count = 0;

    const uint32_t sel = static_cast<uint32_t>(S_2(ride, 0)) + 14u;
    if (sel > 13u) goto kDefault;
    // the jump table at 0x1029cba0, for the kinds -14 to -1
    switch (sel) {
        case 0: case 3: case 8: case 11: case 13: goto kA;
        case 1: case 2: case 6: case 9: goto kD;
        case 4: case 5: goto kC;
        case 7: case 10: goto kC2;
        case 12: goto kE;
        default: goto kDefault;
    }

kA:
    if ((R_2(bump, 0) & 0x100000u) != 0) {
        int32_t tries = 100;
        for (;;) {
            const uint32_t modulus = R_2(P_3(ride, 0xc0), 0xc);
            int32_t dist = static_cast<int32_t>(Mod(rand(), modulus));
            const uint32_t rnd = rand();
            const int32_t sinv = gHostSineTable[rnd & 0x1ff];
            const int32_t cosv = gHostSineTable[((rnd & 0x1ff) + 0x80) & 0x1ff];
            const int32_t dx = static_cast<int32_t>(static_cast<uint32_t>(dist) * static_cast<uint32_t>(cosv));
            const int32_t dy = -static_cast<int32_t>(static_cast<uint32_t>(dist) * static_cast<uint32_t>(sinv));
            S_2(bump, 0x34) = S_2(ride, 0x74) + Div256_2(dx);
            S_2(bump, 0x38) = S_2(ride, 0x78) + Div256_2(dy);
            if (--tries == 0) break;
            uintptr_t other = P_3(list, 0x10);
            int32_t left = S_2(list, 0x14);
            bool hit = false;
            while (left != 0) {
                if ((R_2(other, 0) & 1u) != 0) {
                    left--;
                    if (other != bump) {
                        int32_t shift = 0;
                        int32_t ax = Labs_2(S_2(other, 0x34) - S_2(bump, 0x34));
                        int32_t ay = Labs_2(S_2(other, 0x38) - S_2(bump, 0x38));
                        while (ax > 0x8000 || ay > 0x7fff) {
                            ax >>= 1;
                            shift++;
                            ay >>= 1;
                        }
                        const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(ax) * static_cast<uint32_t>(ax) +
                                                                  static_cast<uint32_t>(ay) * static_cast<uint32_t>(ay));
                        const int32_t root = ppc_fctiwz(std::sqrt(static_cast<double>(sum)));
                        const int32_t reach = shift < 32 ? static_cast<int32_t>(static_cast<uint32_t>(root) << shift) : 0;
                        if (S_2(other, 0x64) + S_2(bump, 0x64) > reach) {
                            hit = true;
                            break;
                        }
                    }
                }
                other += 0xac;
            }
            if (!hit) break;
        }
        R_2(bump, 0) &= ~0x4000u;
        const uint32_t rnd = rand();
        R_2(bump, 0x54) = rnd & 0x1ff;
        R_2(bump, 0x50) = rnd & 0x1ff;
        R_2(bump, 0x24) = FindDataNodeIndexByID(gRideAnimSets[R_2(bump, 8)], 0x100, 2);
        R_2(bump, 0x28) = FindDataNodeIndexByID(gRideAnimSets[R_2(bump, 8)], 0x100, 1);
        if (S_2(ride, 0) == -1) {
            R_2(bump, 0) |= 0x3000000u;
            instanceSub();
        }
        S_2(bump, 0xa4) = -1;
    }
kB:
    if (R_2(bump, 0x20) == 0 && (R_2(bump, 0) & 0x404000u) == 0x404000u && S_2(P_3(bump, 0x9c), 0x50) == 2 &&
        (R_2(bump, 0) & 0x20u) == 0) {
        startSound();
    }
    {
        bool done = false;
        if ((R_2(bump, 0) & 0x40004u) == 0) {
            if ((rand() & 0xf) < 3) {
                int32_t index = 0;
                tagBUMP* target = FindTargetBump(bumpArg, rideArg, &index);
                if (target != nullptr) {
                    R_2(bump, 0) &= 0xfffbff81u;
                    R_2(bump, 0) |= 4u;
                    S_2(bump, 0x80) = index;
                    S_2(bump, 0x8c) = 0x5a;
                    done = true;
                }
            }
        }
        if (!done) {
            uint32_t steps;
            if ((R_2(bump, 0) & 8u) != 0 && (rand() & 0xf) < 4) {
                steps = 1;
                node = P_3(list, 0x20) + static_cast<uint32_t>(S_2(bump, 0x7c)) * 0x1c;
            } else {
                const uint32_t rnd = rand();
                steps = Mod(rnd, R_2(ride, 0xac));
                node = P_3(ride, 0xa8);
            }
            R_2(bump, 0) &= 0xfffbff81u;
            R_2(bump, 0) |= 0x10008u;
            while (steps != 0) {
                node = nextNode(node);
                steps--;
            }
            S_2(bump, 0x7c) = S_2(node, 4);
            S_2(bump, 0x8c) = 3;
        }
        R_2(bump, 0) |= 0x8000u;
    }
    goto kEnd;

kC:
    if ((R_2(bump, 0) & 0x100000u) != 0) R_2(bump, 0) |= 0x3c00u;
kC2:
    R_2(bump, 0) &= 0xfffbff81u;
    S_2(bump, 0x8c) = 0;
    if ((R_2(bump, 0) & 0x100000u) != 0) {
        startSound();
        R_2(bump, 0x24) = FindDataNodeIndexByID(gRideAnimSets[R_2(bump, 8)], 0x100, 1);
        if ((R_2(bump, 0) & 0x2000u) != 0) instanceSub();
    }
    if (S_2(ride, 0x50) == 1) {
        node = P_3(ride, 0xa8);
        count = S_2(ride, 0x5c);
        while (count != 0) {
            if ((R_2(node, 0) & 2u) != 0) {
                if (--count == 0) break;
                node = P_3(node, 0x14);
            }
        }
        S_2(bump, 0x7c) = S_2(node, 4);
        R_2(bump, 0) |= 0x10048u;
        goto kEnd;
    }
    if (S_2(ride, 0x50) == 2) {
        R_2(bump, 0) |= 0x10008u;
        if (!tableIndexOk(S_2(bump, 0x7c))) S_2(bump, 0x7c) = S_2(P_3(ride, 0xa4), 4);
        S_2(bump, 0x8c) = 0;
        const uint32_t rnd = rand();
        S_2(bump, 0x68) = (S_2(ride, 0x20) / 9) * (3 - static_cast<int32_t>(rnd & 7));
    }
    goto kEnd;

kD:
    R_2(bump, 0) &= 0xfffbff81u;
    S_2(bump, 0x8c) = 0;
    if ((R_2(bump, 0) & 0x100000u) != 0) {
        startSound();
        S_2(bump, 0x7c) = S_2(P_3(ride, 0xa4), 4);
    }
    R_2(bump, 0) |= 0x10008u;
    if (!tableIndexOk(S_2(bump, 0x7c))) S_2(bump, 0x7c) = S_2(P_3(ride, 0xa4), 4);
    S_2(bump, 0x8c) = 0;
    S_2(bump, 0x68) = 0;
    goto kEnd;

kE:
    if ((R_2(bump, 0) & 8u) != 0)
        node = P_3(list, 0x20) + static_cast<uint32_t>(S_2(bump, 0x7c)) * 0x1c;
    else
        node = P_3(ride, 0xa8);
    R_2(bump, 0) &= 0xfffbff81u;
    R_2(bump, 0) |= 0x10008u;
    node = nextNode(node);
    S_2(bump, 0x7c) = S_2(node, 4);
    S_2(bump, 0x8c) = 0;
    goto kEnd;

kDefault:
    R_2(bump, 0) &= 0xffffff81u;
    R_2(bump, 0) |= 0x10010u;
    S_2(bump, 0x7c) = static_cast<int32_t>(Mod(rand(), R_2(list, 0x24)));

kEnd:
    R_2(bump, 0) &= ~0x100000u;
}

// Unit v5087: the bump removal of the advisor world (main app): BMP_REMOVEBUMP 0x1002e614, BMP_DOCK 0x1002e740,
// RemovePeepsFromBump 0x1002e464, FindTargetBump 0x1002df10.
// The list (gColList), the ride records (0xd0 bytes) and the bumps (0xac bytes apart) are read by Mac offset, like in
// SetupHeadIDs.cpp; pointer fields inside them are Ptr32 offsets from the arena base.

uint16_t gAdvisorThingId;   // 0x1029c960 (name invented): the advisor's own row in gThingArray

namespace {

const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kPeepText[] = "Peep %d added to ride %d leaving list\n";
const char kOverText[] = "Ride Over - reset to loading\n";

inline uintptr_t Dec(uint32_t v) { return v != 0 ? reinterpret_cast<uintptr_t>(tpw::gArenaBase) + v : 0; }
inline uint32_t Enc(uintptr_t p) { return p != 0 ? tpw::ArenaOffset(reinterpret_cast<const void*>(p)) : 0; }
inline uintptr_t P_4(uintptr_t base, uint32_t off) { return Dec(R_2(base, off)); }

inline uintptr_t ListBase_2() { return reinterpret_cast<uintptr_t>(&gColList); }
inline bool ListIsLive_2() { return R_2(ListBase_2(), 0) == 0x4a454647u; }

// fctiwz as the hardware does it
int32_t F2I(float f) {
    const double d = f;
    if (d != d) return INT32_MIN;
    if (d >= 2147483648.0) return INT32_MAX;
    if (d <= -2147483649.0) return INT32_MIN;
    return static_cast<int32_t>(d);
}

// The ride's id as the list computes it: the ride's first word shifted up, or'ed with its index; 0 when the list is not live.
int32_t RideId_2(uintptr_t ride) {
    if (!ListIsLive_2()) return 0;
    const int32_t index = static_cast<int32_t>(static_cast<uint32_t>(ride - P_4(ListBase_2(), 8))) / 0xd0;
    return static_cast<int32_t>((R_2(ride, 0) << 8) | static_cast<uint32_t>(index));
}

// The first node of the ride's mesh set's header: the three floats the emitters are placed at.
void EmitterAt(uintptr_t bump, int32_t type) {
    tag_sRideAnimSet* set = gRideAnimSets[S_2(bump, 8)];
    const uintptr_t node = reinterpret_cast<uintptr_t>(set->mHeads->mHeader->mF78.get()) + 0x10;
    const float k = 1024.0f;
    const float x = *reinterpret_cast<const float*>(node + 0x30);
    const float y = *reinterpret_cast<const float*>(node + 0x34);
    const float z = *reinterpret_cast<const float*>(node + 0x38);
    const int32_t a = F2I(k * x);
    const int32_t b = F2I(k * y);
    const int32_t c = F2I(k * z);
    PAR_InitEmitter(type, a, b, c);
}

}  // namespace

// 0x1002df10, 288 bytes: picks a random other live bump of the same ride whose area meets bump's, chains them through +0x94.
tagBUMP* FindTargetBump(tagBUMP* bump, tagBUMPRIDE* ride, int32_t* index) {
    const uintptr_t g = ListBase_2();
    const uintptr_t self = reinterpret_cast<uintptr_t>(bump);
    if (!(S_2(reinterpret_cast<uintptr_t>(ride), 0x5c) > 1)) return nullptr;
    const uintptr_t first = P_4(g, 0x10);
    uintptr_t p = first;
    int32_t count = S_2(g, 0x14);
    uint32_t found = 0;
    uintptr_t last = 0;
    while (count != 0) {
        const uint32_t flags = R_2(p, 0);
        if ((flags & 1) != 0) {
            if (self != p && (flags & 0x20) == 0 && R_2(self, 0x9c) == R_2(p, 0x9c) &&
                DoAreasIntersect(reinterpret_cast<tabCOLOBJECT*>(P_4(self, 0x98)),
                                 reinterpret_cast<tabCOLOBJECT*>(P_4(p, 0x98))) != 0) {
                R_2(p, 0x94) = Enc(last);
                last = p;
                found++;
            }
            count--;
        }
        p += 0xac;
    }
    if (found == 0) return nullptr;
    const uint32_t rnd = gGameWorld->GetRand();
    int32_t n = static_cast<int32_t>(rnd - (rnd / found) * found);
    uintptr_t cur = last;
    while (--n >= 0) cur = P_4(cur, 0x94);
    if (index != nullptr) *index = static_cast<int32_t>(static_cast<uint32_t>(cur - first)) / 0xac;
    return reinterpret_cast<tagBUMP*>(cur);
}

// Unit v5139: the peep lists of a bump ride and BMP_LeaveBump (main app). The list at 0x102b56c0 is read by Mac offset:
// +0 magic 0x4a454647, +8 the ride records (0xd0 bytes each), +0x34 the head of the free list of peep nodes (0x14 bytes, next at +0x10).
// The pointer words of the list, the records and the nodes are Ptr32 (offsets from the arena base).

namespace {

uintptr_t P_5(uintptr_t base, uint32_t off) {
    const uint32_t v = R(base, off);
    return v != 0 ? reinterpret_cast<uintptr_t>(tpw::gArenaBase) + v : 0;
}
uint32_t Enc_2(uintptr_t p) { return p != 0 ? tpw::ArenaOffset(reinterpret_cast<void*>(p)) : 0; }

constexpr uint32_t kMagic_2 = 0x4a454647u;

// The ride record of a ride id (0 when the magic is wrong), and the id the record answers for itself (the original's check).
uintptr_t RideRecord(int32_t ride) {
    const uintptr_t g = ListBase();
    if (R(g, 0) != kMagic_2) return 0;
    return P_5(g, 8) + (static_cast<uint32_t>(ride) & 0xffu) * 0xd0u;
}

int32_t RideIdOf(uintptr_t rec) {
    const uintptr_t g = ListBase();
    if (R(g, 0) != kMagic_2) return 0;
    const int32_t diff = static_cast<int32_t>(rec - P_5(g, 8));
    return static_cast<int32_t>((R(rec, 0) << 8) | static_cast<uint32_t>(diff / 0xd0));
}

}  // namespace

// 0x1002e070, 240 bytes: moves a free node onto the ride's joining list (when the ride word +0x50 is set); answers 1 when added.
int32_t BMP_AddPeepToRideJoiningList(int32_t ride, uint32_t peep) {
    const uintptr_t g = ListBase();
    const uintptr_t rec = RideRecord(ride);
    if (ride == RideIdOf(rec) && static_cast<int32_t>(R(rec, 0x50)) != 0) {
        const uintptr_t node = P_5(g, 0x34);
        if (node != 0) {
            R(g, 0x34) = R(node, 0x10);
            R(node, 0x10) = R(rec, 0xc4);
            R(rec, 0xc4) = Enc_2(node);
            R(node, 0) = peep;
            R(node, 4) = static_cast<uint32_t>(ride);
            R(node, 8) = 0xffffffffu;
            TRACE("Peep %d added to next car to be launched on Ride %d\n");
            return 1;
        }
    }
    return 0;
}

// 0x1002e198, 168 bytes: pops the first peep node of the ride's leaving list onto the free list; answers its peep (0 when none).
int32_t BMP_PollForPeeps(int32_t ride) {
    const uintptr_t g = ListBase();
    const uintptr_t rec = RideRecord(ride);
    if (R(g, 0) != kMagic_2) return 0;
    if (ride != RideIdOf(rec)) return 0;
    const uintptr_t node = P_5(rec, 0xc8);
    if (node == 0) return 0;
    R(rec, 0xc8) = R(node, 0x10);
    R(node, 0x10) = R(g, 0x34);
    R(g, 0x34) = Enc_2(node);
    return static_cast<int32_t>(R(node, 0));
}

// 0x1002e268, 208 bytes: moves a free node onto the ride's leaving list holding the peep; warns when there is none.
// (A wrong magic with ride 0 reads through a null record, as the original does.)
void BMP_AddPeepToRideLeavingList(int32_t ride, uint32_t peep) {
    const uintptr_t g = ListBase();
    const uintptr_t rec = RideRecord(ride);
    if (ride == RideIdOf(rec)) {
        const uintptr_t node = P_5(g, 0x34);
        if (node != 0) {
            R(g, 0x34) = R(node, 0x10);
            R(node, 0x10) = R(rec, 0xc8);
            R(rec, 0xc8) = Enc_2(node);
            R(node, 0) = peep;
            R(node, 4) = static_cast<uint32_t>(ride);
            return;
        }
    }
    TRACE("###WARNING### Peep List full. You've just lost a Peep\n");
}

// 0x1002e370, 200 bytes: a bump leaves: when no peeps remain, the ride is flagged and its anim goes to state 0xc.
void BMP_LeaveBump(tagBUMP* bump) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(bump);
    if (RemovePeepsFromBump(bump, 1) != 0) return;
    R(P_5(b, 0x9c), 0x50) = 1;
    R(b, 0) &= 0xffffff81u;
    if (static_cast<int32_t>(R(b, 0x10)) == 0xc) return;
    R(b, 0x10) = 0xc;
    if (R(b, 8) != 0) FlushDeferredAnim(gRideAnimSets[R(b, 8)], 0);
    if (R(b, 8) != 0) SetCurRideAnim(gRideAnimSets[R(b, 8)], 0xc, 0, 2, 1.0f, 0);
}

// 0x1002e464, 380 bytes: gives the bump's peep nodes back to the ride's free list; answers the ride's count (+0x60).
int32_t RemovePeepsFromBump(tagBUMP* bump, int32_t emit) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(bump);
    uintptr_t node;
    while ((node = P_4(b, 0x30)) != 0) {
        R_2(b, 0x30) = R_2(node, 0x10);
        const uintptr_t ride = P_4(b, 0x9c);
        R_2(node, 0x10) = R_2(ride, 0xc8);
        R_2(ride, 0xc8) = Enc(node);
        S_2(P_4(b, 0x9c), 0x60) = S_2(P_4(b, 0x9c), 0x60) - 1;
        if (R_2(node, 8) != 0xffffffffu) {
            DisableRideHead(gRideAnimSets[S_2(b, 8)]->mHeads, R_2(node, 8));
            R_2(node, 8) = 0xffffffffu;
            if (emit != 0) EmitterAt(b, 0x13);
        }
        const int32_t id = RideId_2(P_4(b, 0x9c));
        TRACE(kPeepText, static_cast<int32_t>(R_2(node, 0)), id);
    }
    return S_2(P_4(b, 0x9c), 0x60);
}

// 0x1002e740, 836 bytes: removes a bump: its peeps, its sub ride instances, sound and emitter; answers the ride's +0x5c.
int32_t BMP_DOCK(tagBUMP* bump) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(bump);
    const uintptr_t g = ListBase_2();
    const uintptr_t ride = P_4(b, 0x9c);
    RemovePeepsFromBump(bump, 0);
    if ((R_2(b, 0) & 0x400000u) != 0) {
        R_2(ride, 0x58) = 0;
        R_2(b, 0) = R_2(b, 0) & 0xffbfffffu;
    }
    if (S_2(ride, 0x50) != 4) EmitterAt(b, 0xf);
    DestroyRideInstance(R_2(b, 8));
    if ((R_2(b, 0) & 0x02002000u) != 0) {
        DestroyRideInstance(R_2(b, 0xc));
        R_2(b, 0) = R_2(b, 0) & 0xfdffdfffu;
    }
    if (R_2(b, 0x20) != 0 && S_2(ride, 0x50) != 4) {
        const int32_t kind = S_2(ride, 0);
        if (kind == -10 || kind == -9 || kind == -7 || kind == -4) {
            const int32_t id = RideId_2(P_4(b, 0x9c));
            uint16_t thingId;
            TrackRideIdToThingId(&thingId, id);
            int32_t thingHandle;
            if (thingId == gAdvisorThingId) {
                thingHandle = 0;
            } else {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZero);
                thingHandle = S_2(reinterpret_cast<uintptr_t>(gThingArray[thingId].mThing), 0x20);
            }
            R_2(b, 0x20) = static_cast<uint32_t>(SND_SendIndirectedEvent(thingHandle, S_2(b, 0x20), gSoundBanks[7], 1, 0, 0, 0));
            if (R_2(b, 0x20) != 0) {
                const float fx = static_cast<float>(S_2(b, 0x34));
                const float fy = static_cast<float>(S_2(b, 0x38));
                const float q = 302.7000122070312f;
                const int32_t ax = F2I(fx / q);
                const int32_t az = F2I(fy / q);
                R_2(b, 0x20) = static_cast<uint32_t>(SOUND_MoveSoundEffect(S_2(b, 0x20), ax, 0, az));
            }
            R_2(b, 0x20) = 0;
        } else {
            SOUND_FadeSoundEffect(S_2(b, 0x20));
        }
    }
    if (S_2(b, 0x2c) != -1) {
        PAR_KillEmitter(S_2(b, 0x2c));
        S_2(b, 0x2c) = -1;
    }
    R_2(b, 0) = R_2(b, 0) & 0xfffffffeu;
    S_2(g, 0x14) = S_2(g, 0x14) - 1;
    S_2(ride, 0x5c) = S_2(ride, 0x5c) - 1;
    if (S_2(ride, 0x5c) == 0) {
        const int32_t kind = S_2(ride, 0);
        if (!(kind == -8 || kind == -5 || kind == -12 || kind == -13)) {
            TRACE(kOverText);
            S_2(ride, 0x50) = 1;
        }
    }
    return S_2(ride, 0x5c);
}

// 0x1002e614, 260 bytes: docks the bump of the ride with this id (with a: only one without peeps); answers 1 when one was found.
int32_t BMP_REMOVEBUMP(int32_t rideId, int32_t a) {
    const uintptr_t g = ListBase_2();
    uintptr_t ride = 0;
    if (ListIsLive_2()) ride = P_4(g, 8) + static_cast<uint32_t>(rideId & 0xff) * 0xd0;
    const int32_t id = RideId_2(ride);
    if (rideId == id) {
        uintptr_t p = P_4(g, 0x10);
        int32_t count = S_2(g, 0x14);
        while (count != 0) {
            if ((R_2(p, 0) & 1) != 0) {
                count--;
                if (P_4(p, 0x9c) == ride && (a == 0 || R_2(p, 0x30) == 0)) {
                    BMP_DOCK(reinterpret_cast<tagBUMP*>(p));
                    return 1;
                }
            }
            p += 0xac;
        }
    }
    if (a != 0) BMP_REMOVEBUMP(rideId, 0);
    return 0;
}

// 0x1002eaac
tagBUMP* BMP_StartBump(int32_t x, int32_t y, int32_t type) {
    uint8_t* world = World_4();
    uint8_t* bump = P(world, 0x10).get();
    if (bump != nullptr) {
        int32_t i = 0;
        for (; i < 0x100; ++i, bump += 0xac)
            if ((W(bump, 0) & 1) == 0) break;
        if (i >= 0x100) bump = nullptr;
    }
    if (bump == nullptr) return nullptr;
    W(world, 0x14) += 1;
    std::memset(bump, 0, 0xac);
    W(bump, 0) = 1;
    W(bump, 0x34) = static_cast<uint32_t>(x);
    W(bump, 0x38) = static_cast<uint32_t>(y);
    W(bump, 0x64) = static_cast<uint32_t>(type);
    W(bump, 0x80) = 0xffffffffu;
    W(bump, 0x7c) = 0xffffffffu;
    W(bump, 0x2c) = 0xffffffffu;
    W(bump, 0x28) = 0xffffffffu;
    W(bump, 0x24) = 0xffffffffu;
    W(bump, 0x20) = 0;
    W(bump, 0x30) = 0;
    W(bump, 0xa4) = 0;
    W(bump, 0xa8) = 0;
    float zero = 0.0f;
    std::memcpy(bump + 0xa0, &zero, 4);
    return reinterpret_cast<tagBUMP*>(bump);
}

// 0x1002ec84
tabBUOY* BMP_StartBuoy(int32_t x, int32_t y) {
    uint8_t* world = World_4();
    uint8_t* buoy = P(world, 0x20).get();
    bool found = false;
    if (buoy != nullptr) {
        for (uint32_t i = 0; static_cast<int32_t>(i) < 0x400; ++i, buoy += 0x1c) {
            if ((W(buoy, 0) & 1) == 0) {
                W(buoy, 4) = i;
                found = true;
                break;
            }
        }
    }
    if (!found) {
        TRACE("Out of Buoys\n");
        return nullptr;
    }
    W(world, 0x24) += 1;
    W(buoy, 0) = 1;
    W(buoy, 8) = static_cast<uint32_t>(x);
    W(buoy, 0xc) = static_cast<uint32_t>(y);
    W(buoy, 0x10) = 0;
    W(buoy, 0x14) = 0;
    W(buoy, 0x18) = 0;
    return reinterpret_cast<tabBUOY*>(buoy);
}

// 0x1002ebd0: the original leaves the buoy (or 0) in r3.
tabCOLOBJECT* BMP_AddBuoyToRide(tagBUMPRIDE* ride, int32_t x, int32_t y) {
    uint8_t* r = reinterpret_cast<uint8_t*>(ride);
    uint8_t* buoy = reinterpret_cast<uint8_t*>(BMP_StartBuoy(x, y));
    if (buoy != nullptr) {
        P(buoy, 0x10).set(r);
        W(r, 0xac) += 1;
        uint8_t* last = P(r, 0xa8).get();
        if (last == nullptr) {
            P(r, 0xa8).set(buoy);
        } else {
            while (P(last, 0x14).get() != nullptr) last = P(last, 0x14).get();
            P(last, 0x14).set(buoy);
        }
    }
    return reinterpret_cast<tabCOLOBJECT*>(buoy);
}

// Unit v51013: GetThingAtPosnIfEntrance(int, int), MoveCameraOverRideExit (camera over a ride's exit) and the collision object
// helpers of the advisor world (FreeTrackForRide, BMP_StartColObject, BMP_StartColObjectForRide).

// 0x1002edfc (216 bytes): the first free (live bit clear) of the 0x800 collision objects becomes a live object 0x1f at (x, y) with the radius
// twice; none free (or no table) is traced and answers NULL.
tabCOLOBJECT* BMP_StartColObject(int32_t x, int32_t y, int32_t radius) {
    tabCOLOBJECT* object = gColList.first.get();
    bool found = false;
    if (object != nullptr) {
        for (int32_t i = 0; i < 0x800; i++) {
            if ((object->flags & 1) == 0) {
                found = true;
                break;
            }
            object++;
        }
    }
    if (!found) {
        TRACE("Out of Collision Objects \n");
        return nullptr;
    }
    gColList.count += 1;
    object->flags = 0x1f;
    object->x = x;
    object->y = y;
    object->radiusY = radius;
    object->radiusX = radius;
    object->pad14[0] = 0;
    object->pad14[1] = 0;
    object->pad14[2] = 0;
    return object;
}

// 0x1002ed74 (72 bytes): starts an object and pushes it on the ride's list (ride +0xc0, the link at the object's +0x14). Like the original it
// does not look at a NULL answer.
void BMP_StartColObjectForRide(tagBUMPRIDE* ride, int32_t x, int32_t y, int32_t shape) {
    tabCOLOBJECT* object = BMP_StartColObject(x, y, shape);
    At<uint32_t>(object, 0x14) = At<uint32_t>(ride, 0xc0);
    At<Ptr32<tabCOLOBJECT>>(ride, 0xc0) = object;
}
// 0x1002ef00 (100 bytes): gives back every track piece of the ride (and each piece's links): the live bit goes down, the list counts at
// +0x1c (links) and +0x2c (pieces) of the collision world go down.
void FreeTrackForRide(tagBUMPRIDE* ride) {
    char* piece;
    while ((piece = At<Ptr32<char>>(ride, 0xbc).get()) != nullptr) {
        char* link;
        while ((link = At<Ptr32<char>>(piece, 0x1c).get()) != nullptr) {
            At<Ptr32<char>>(piece, 0x1c).mOffset = At<Ptr32<char>>(link, 0x14).mOffset;
            At<int32_t>(&gColList, 0x1c) -= 1;
            At<uint32_t>(link, 0) &= ~1u;
        }
        At<Ptr32<char>>(ride, 0xbc).mOffset = At<Ptr32<char>>(piece, 0x24).mOffset;
        At<uint32_t>(piece, 0) &= ~1u;
        At<int32_t>(&gColList, 0x2c) -= 1;
    }
}

// BMP_TrackSectionPlaceSection 0x1002ef98 (TPAdvisor.cpp): registers a track section on a ride record: a section record, the buoys
// (0x20 bytes, from the pool at list+0x18) that fence it, and a collision object (0x1c bytes, from the pool at list+0x20).
// The list (main app data 0x102b56c0), the ride records (0xd0 bytes), the sections (0x28 bytes), the buoys and the collision
// objects are read by Mac offset; their pointer words are arena offsets (Ptr32).

int32_t BMP_TrackSectionPlaceSection(int32_t id, int32_t x, int32_t y, int32_t type, int32_t f7, uint32_t f8, int32_t f9);

namespace {

uint8_t* A(uint32_t off) { return off != 0 ? tpw::gArenaBase + off : nullptr; }
uint32_t O(const uint8_t* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }
uint32_t& W_5(uint8_t* base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }

// The case each value of the type's low halfword (0..13) takes in the jump table at 0x1029cbd8 (0: nothing).
// 1 0x1002f138, 2 0x1002f2f0, 3 0x1002f4a0, 4 0x1002f658, 5 E (0x1002f810), 6 F (0x1002f8f8), 7 G; 8 and 9 are E and F entered
// after their "li r22, 0x100" (r22 stays 0, so the flag is 0x20 instead of 0x120).
const uint8_t kCaseOf[14] = {0, 1, 2, 3, 4, 1, 2, 3, 4, 8, 9, 7, 5, 6};

// One buoy from the pool at list+0x18, linked to the section; the pool full or missing: TRACE and the NULL the caller then reads.
uint8_t* NewBuoy(uint8_t* list, uint8_t* section, uint32_t a, uint32_t b, uint32_t c) {
    uint8_t* p = A(W_5(list, 0x18));
    if (p != nullptr) {
        int32_t i = 0;
        for (; i < 0x800; i++, p += 0x20)
            if ((W_5(p, 0) & 1) == 0) break;
        if (i >= 0x800) {
            TRACE("Out of Collision Objects \n");   // 0x1023cbba (the original's message for the buoy pool)
            p = nullptr;
        }
    } else {
        TRACE("Out of Collision Objects \n");
    }
    if (p != nullptr) {
        W_5(list, 0x1c) = W_5(list, 0x1c) + 1;
        W_5(p, 0) = 0x1f;
        W_5(p, 4) = a;
        W_5(p, 8) = b;
        W_5(p, 0x10) = c;
        W_5(p, 0xc) = c;
        W_5(p, 0x14) = 0;
        W_5(p, 0x18) = 0;
        W_5(p, 0x1c) = 0;
        W_5(p, 0x14) = W_5(section, 0x1c);
        W_5(section, 0x1c) = O(p);
        W_5(p, 0x18) = O(section);
    }
    return p;
}

}  // namespace

int32_t BMP_TrackSectionPlaceSection(int32_t id, int32_t x, int32_t y, int32_t type, int32_t f7, uint32_t f8, int32_t f9) {
    uint8_t* list = reinterpret_cast<uint8_t*>(&gColList);
    if (W_5(list, 0) != 0x4a454647u) return 0;
    uint8_t* ride = A(W_5(list, 8) + (static_cast<uint32_t>(id) & 0xffu) * 0xd0u);
    const int32_t diff = static_cast<int32_t>(O(ride) - W_5(list, 8));
    if (id != static_cast<int32_t>((W_5(ride, 0) << 8) | static_cast<uint32_t>(diff / 0xd0))) return 0;
    for (uint8_t* s = A(W_5(ride, 0xbc)); s != nullptr; s = A(W_5(s, 0x24))) {
        if (static_cast<int32_t>(W_5(s, 0x14)) == x && static_cast<int32_t>(W_5(s, 0x18)) == y) {
            TRACE("Duplicate track section - IGNORED!\n");
            return 0;
        }
    }
    uint8_t* sec = A(W_5(list, 0x28));
    if (sec != nullptr) {
        int32_t i = 0;
        for (; i < 0x400; i++, sec += 0x28)
            if ((W_5(sec, 0) & 1) == 0) break;
        if (i >= 0x400) sec = nullptr;
    }
    if (sec == nullptr) return 0;
    W_5(sec, 0) = 1;
    W_5(list, 0x2c) = W_5(list, 0x2c) + 1;
    W_5(sec, 4) = static_cast<uint32_t>(type);
    W_5(sec, 8) = static_cast<uint32_t>(f7);
    W_5(sec, 0xc) = f8;
    W_5(sec, 0x10) = static_cast<uint32_t>(f9);
    W_5(sec, 0x14) = static_cast<uint32_t>(x);
    W_5(sec, 0x18) = static_cast<uint32_t>(y);
    W_5(sec, 0x20) = O(ride);
    W_5(sec, 0x24) = 0;
    if (W_5(ride, 0xbc) == 0) {
        W_5(ride, 0xbc) = O(sec);
    } else {
        uint8_t* t = A(W_5(ride, 0xbc));
        while (W_5(t, 0x24) != 0) t = A(W_5(t, 0x24));
        W_5(t, 0x24) = O(sec);
    }

    const uint32_t ux = static_cast<uint32_t>(x);
    const uint32_t uy = static_cast<uint32_t>(y);
    const uint32_t a1 = W_5(ride, 0xc) + 0xc00u - W_5(ride, 8);      // 0xc00 + width
    auto a2 = [&]() { return 0xc00u - W_5(ride, 0xc) + W_5(ride, 8); };   // 0xc00 - width
    uint8_t* first = nullptr;
    uint32_t r25 = 0, r23 = 0;
    const uint32_t kind = static_cast<uint32_t>(type) & 0xffffu;
    uint8_t* p;
    const uint32_t c = kind < 14 ? kCaseOf[kind] : 0;
    switch (c) {
    case 1:
        p = NewBuoy(list, sec, ux + 0x1800, uy, a1);
        W_5(p, 0) &= 0xffffffebu;
        first = p;
        p = NewBuoy(list, sec, ux + 0x1800, uy, a2());
        W_5(p, 0) |= 0x40;
        W_5(p, 0) &= 0xffffffebu;
        r25 = 0xf84;
        r23 = 0xf84;
        break;
    case 2:
        p = NewBuoy(list, sec, ux, uy, a1);
        W_5(p, 0) &= 0xffffffedu;
        first = p;
        p = NewBuoy(list, sec, ux, uy, a2());
        W_5(p, 0) |= 0x40;
        W_5(p, 0) &= 0xffffffedu;
        r25 = 0x87c;
        r23 = 0xf84;
        break;
    case 3:
        p = NewBuoy(list, sec, ux, uy + 0x1800, a1);
        W_5(p, 0) &= 0xfffffff5u;
        first = p;
        p = NewBuoy(list, sec, ux, uy + 0x1800, a2());
        W_5(p, 0) |= 0x40;
        W_5(p, 0) &= 0xfffffff5u;
        r25 = 0x87c;
        r23 = 0x87c;
        break;
    case 4:
        p = NewBuoy(list, sec, ux + 0x1800, uy + 0x1800, a1);
        W_5(p, 0) &= 0xfffffff3u;
        first = p;
        p = NewBuoy(list, sec, ux + 0x1800, uy + 0x1800, a2());
        W_5(p, 0) |= 0x40;
        W_5(p, 0) &= 0xfffffff3u;
        r25 = 0xf84;
        r23 = 0x87c;
        break;
    case 5:
    case 8:
        p = NewBuoy(list, sec, ux + 0xc00, uy + 0xc00, 0xc20);
        W_5(p, 0) |= (c == 5 ? 0x120u : 0x20u);
        first = p;
        r25 = 0xc00;
        r23 = 0xc00;
        W_5(p, 0x10) = W_5(ride, 0xc) - W_5(ride, 8);
        break;
    case 6:
    case 9:
        p = NewBuoy(list, sec, ux + 0xc00, uy + 0xc00, W_5(ride, 0xc) - W_5(ride, 8));
        W_5(p, 0) |= (c == 6 ? 0x120u : 0x20u);
        first = p;
        r25 = 0xc00;
        r23 = 0xc00;
        W_5(p, 0x10) = 0xc20;
        break;
    case 7: {
        uint32_t d = 0xc00u - a2();
        d = d * d + 0x900000u;
        const uint32_t root = static_cast<uint32_t>(ppc_fctiwz(std::sqrt(static_cast<double>(static_cast<int32_t>(d)))));
        p = NewBuoy(list, sec, ux + 0xc00, uy + 0xc00, root);
        first = p;
        const int32_t k = static_cast<int32_t>(W_5(ride, 0));
        if (k == -5 || k == -8 || k == -12 || k == -13) W_5(p, 0) |= 0x80;
        p = NewBuoy(list, sec, ux + 0x1800, uy, a2());
        W_5(p, 0) |= 0x40;
        W_5(p, 0) &= 0xffffffebu;
        p = NewBuoy(list, sec, ux, uy, a2());
        W_5(p, 0) |= 0x40;
        W_5(p, 0) &= 0xffffffedu;
        p = NewBuoy(list, sec, ux, uy + 0x1800, a2());
        W_5(p, 0) |= 0x40;
        W_5(p, 0) &= 0xfffffff5u;
        p = NewBuoy(list, sec, ux + 0x1800, uy + 0x1800, a2());
        W_5(p, 0) |= 0x40;
        W_5(p, 0) &= 0xfffffff3u;
        r25 = 0xc00;
        r23 = 0xc00;
        break;
    }
    default:
        break;
    }

    if ((static_cast<uint32_t>(type) & 0x10000u) == 0) {
        uint8_t* col = A(W_5(list, 0x20));
        if (col != nullptr) {
            int32_t i = 0;
            for (; i < 0x400; i++, col += 0x1c) {
                if ((W_5(col, 0) & 1) == 0) {
                    W_5(col, 4) = static_cast<uint32_t>(i);
                    break;
                }
            }
            if (i >= 0x400) col = nullptr;
        }
        if (col == nullptr) TRACE("Out of Buoys\n");   // 0x1023cbac (the original's message for the collision object pool)
        if (col != nullptr) {
            W_5(list, 0x24) = W_5(list, 0x24) + 1;
            W_5(col, 0) = 1;
            W_5(col, 8) = ux + r25;
            W_5(col, 0xc) = (uy + 0x1800u) - r23;
            W_5(col, 0x10) = 0;
            W_5(col, 0x14) = 0;
            W_5(col, 0x18) = 0;
            W_5(col, 0x10) = O(ride);
            W_5(ride, 0xac) = W_5(ride, 0xac) + 1;
            uint8_t* head = A(W_5(ride, 0xa8));
            if (head == nullptr) {
                W_5(ride, 0xa8) = O(col);
            } else {
                while (W_5(head, 0x14) != 0) head = A(W_5(head, 0x14));
                W_5(head, 0x14) = O(col);
            }
            W_5(col, 0x18) = O(first);
            W_5(first, 0x1c) = O(col);
        }
    }
    return 1;
}

// 0x1002ffc4: LbFile_Read.
TbError BMPFile_Read(void* file, void* buffer, uint32_t size, uint32_t* transferred) {
    return LbFile_Read(file, buffer, size, transferred);
}

// Unit v5124: TrackRideIdToThingId (0x10030020, 240 bytes), main app.

// 0x1029cc10 (name invented): the thing id that ends a walk of the used list (halfword, 0 on the Mac's bss).
uint16_t gUsedListEnd;

// Walks the world's used list for the first kind 3 thing whose word at +0x24 is rideId; answers its id, or 0.
void TrackRideIdToThingId(uint16_t* out, int32_t rideId) {
    uint16_t id = gGameWorld->GetUsedThingHead().mIndex;
    while (id != gUsedListEnd) {
        uint16_t cur = id;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        if (thing->mKind == 3 && rideId == *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(thing) + 0x24)) {
            *out = thing->GetThingId().mIndex;
            return;
        }
        id = gGameWorld->GetNextUsedThing(thing).mIndex;
    }
    *out = 0;
}

// Unit v51116: the 8 byte scorers (a balance value read, or 0), two small destructors and the sound library's inline byte swaps.

namespace v51116_swap {
tagBUMPRIDE* USwap32_tagBUMPRIDE(tagBUMPRIDE* p) { return SwapPtr(p); }   // 0x10021bf8
tabCOLOBJECT* USwap32_tabCOLOBJECT(tabCOLOBJECT* p) { return SwapPtr(p); }   // 0x10021c6c
tagBUMP* USwap32_tagBUMP(tagBUMP* p) { return SwapPtr(p); }   // 0x10021ce0
tagPEEPOFF* USwap32_tagPEEPOFF(tagPEEPOFF* p) { return SwapPtr(p); }   // 0x10021d48
}  // namespace v51116_swap

// Coaster trains and cars (TPCoaster.cpp on the Mac, unit w192): InstanceCoasterTrains 0x10042084, DestroyCoasterTrains 0x10042344,
// GetCoaster1stPersonCar 0x100433f4, SaveTrainsAndCars 0x10043a18, LoadTrainsAndCars 0x10043e8c and TPCoaster.cpp's three swap instances
// 0x10043db8, 0x10043e00 and 0x10043e44, plus 0x10021bb4: the main app's one copy of USwapInplaceFloat (the inventory guesses TPAdvisor.cpp;
// TPCoaster's save and load and the later SaveCoasters / SaveRides call it). Every callee outside the unit is untranslated or translated
// elsewhere.
//
// The Mac keeps a train as 0x80 bytes (sTrainInstance) and a car as 0x60 (sCarInstance); the host structs hold native pointers. A train's type
// word on the Mac is the coaster type plus 0x2f0 (its embedded train type); the host's points at the coaster type (include/TPCoaster.h).
//
// The save file is little-endian. The Mac swaps every word (its swap templates swap unconditionally); a little-endian host writes the same bytes
// by not swapping, so the swaps here are real only on a big-endian host or with TPW_SWAP_FILE_WORDS (the tests' second library), as in
// src/rsse_save.cpp. The car records' inline byte swaps (no call on the Mac) use the same guard.

namespace TPCoasterSwap {

// 0x10021bb4 (12 bytes): the bits of the float.
void USwapInplaceFloat(float& value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    bits = Swap32(bits);
    std::memcpy(&value, &bits, sizeof bits);
}

}  // namespace TPCoasterSwap
