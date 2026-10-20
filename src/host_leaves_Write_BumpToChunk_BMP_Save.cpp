// Unit v5630: the advisor save, BMP_Save (main app 0x10022464, 5068 B, TPAdvisor.cpp on the Mac).
// It writes the bump world as chunks: 1 the save, 2 the build time and date, then per live ride record 3 (its words), 4 per
// node of its 0xbc list, 5 per live bump of the ride (the bump, its collision object, its table entry) with 9 per node of the
// bump's 0x30 list, 7 per node of the ride's 0xc8 list, 8 per node of its 0xc4 list and 6 (the ride id alone).
// Every value is put in file order before the write (the file is little-endian); a chunk's header is written raw when it opens and
// again, swapped, by Write_CloseChunk.
#include <cstdint>
#include <cstring>
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "LbFile.h"
#include "platform/macdoze.h"

// 0x1023cbfa and 0x1023cc03: the build's __TIME__ and __DATE__ (the original measures a second copy of each, at 0x1023c9c3
// and 0x1023c9cc, with strlen; the contents are the same).
extern const char gBMPSaveTime[] = "17:56:39";
extern const char gBMPSaveDate[] = "Jan  8 2001";

namespace {

const uint32_t kWorldMagic = 0x4a454647;   // 'JEFG'

// The file is little-endian: the Mac swaps each big-endian value before the write; on the host the value is already in file
// order, so the swap does nothing (a byte swap here wrote big-endian words that BMP_Load read back swapped; v5L15).
uint32_t Swap(uint32_t value) { return value; }

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
    uint32_t buffer = Swap(value);
    WriteBytes(chunk, &buffer, 4);
}

void WritePair(tabBMPCHUNK* chunk, uint32_t first, uint32_t second) {
    uint32_t buffer[2] = {Swap(first), Swap(second)};
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
