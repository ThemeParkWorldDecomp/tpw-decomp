// TbMapStreamer::CCatFixupTable::Load, sound 0x1002388c (336 bytes).
#include <new>
#include "TbMapStreamer.h"
#include "TbError.h"
#include "TbSoundBank.h"   // SamsUtilities::USwapInplace32(uint32_t&), the sound library's file-endian swap

// Reads a save game's category fix-up table: the byte size of the block, the entry count, then the block itself (count words,
// then one record per entry: a length byte and a name of that many bytes). Every entry word becomes the live category index
// of its name (0 for a record whose length byte is 0). Backslashes in a name become colons first.
TbError TbMapStreamer::CCatFixupTable::Load(bfl::FileSystem::CFile& file) {
    Unload();
    uint32_t size;
    file.Read(&size, 4);
    ::SamsUtilities::USwapInplace32(size);
    file.Read(&mCount, 4);   // the count sits at Mac +0
    ::SamsUtilities::USwapInplace32(mCount);
    mData = reinterpret_cast<uint32_t*>(::operator new[](size, std::nothrow));   // 0x1002ebfc
    file.Read(mData, size);
    uint8_t* p = reinterpret_cast<uint8_t*>(mData) + mCount * 4;
    for (uint32_t i = 0; i < mCount; ++i) {
        uint8_t length = *p++;
        if (length != 0) {
            char* q = reinterpret_cast<char*>(p);
            char c = *q++;
            while (c != 0) {
                if (c == '\\') q[-1] = ':';
                c = *q++;
            }
            mData[i] = TbMapStreamer::Instance()->FindCatagory(reinterpret_cast<const char*>(p));
            p += length;
        } else {
            mData[i] = 0;
        }
    }
    return TbError(0);
}
