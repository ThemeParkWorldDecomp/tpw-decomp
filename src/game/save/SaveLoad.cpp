// SaveLoad.cpp: name proven by __sinit_SaveLoad_cpp in the main application.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "bfl_ZLib.h"
#include "CBalanceTable.h"
#include "CFixableTime.h"
#include "CFixedTime.h"
#include "Cheat.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "CPausableFixedTime.h"
#include "CPausableTime.h"
#include "CSaveableFixedTime.h"
#include "CSaveableTime.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "GameSys.h"
#include "HLSprite.h"
#include "InterfaceFontWrapper.h"
#include "LbFile.h"
#include "LbTime.h"
#include "LLCellSystem.h"
#include "MsgCheatMode.h"
#include "OptionsBalance.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "rsse.h"
#include "SaveLegalText.inc"
#include "SaveLoad.h"
#include "Serialise.h"
#include "SkyObjects.h"
#include "TbError.h"
#include "TbITable.h"
#include "TbResource.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPOptions.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "VanillaTime.h"
#include "World.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <new>
#include <vector>

// RideDB.cpp range of the main app (SimTheme Park 2.04b3): the ride types' names. The name database (CRideNameDB, which the
// inventory files under SaveLoad.cpp) builds a ride type's name from two strings of the object names string table; the ride
// database's three-argument GetTypeName gives the name the ride's own file named, or the built one.

// 0x102a5d3c, 281 rows and a row of zeros. The ids are ride types; the strings are numbers in OBJECT_NAMES.
const RideNameEntry gRideNameTable[282] = {
    {1150, 2, 3}, {1190, 4, 5}, {1180, 6, 7}, {1170, 8, 9},
    {1108, 10, 11}, {1106, 12, 13}, {1185, 14, 15}, {1100, 16, 17},
    {1101, 18, 19}, {1104, 20, 21}, {1103, 22, 23}, {1102, 24, 25},
    {1109, 26, 27}, {1110, 28, 29}, {1112, 30, 31}, {1160, 32, 33},
    {1140, 34, 35}, {1113, 36, 37}, {1501, 38, 1}, {1500, 39, 1},
    {1502, 40, 1}, {1207, 41, 1}, {1212, 42, 1}, {1206, 43, 1},
    {1203, 44, 1}, {1202, 45, 1}, {1209, 46, 1}, {1211, 47, 1},
    {1208, 48, 1}, {1303, 49, 1}, {1304, 50, 1}, {1305, 51, 1},
    {1309, 52, 1}, {1307, 53, 1}, {1401, 54, 1}, {1418, 55, 1},
    {1425, 56, 1}, {1400, 57, 1}, {1402, 58, 1}, {1419, 59, 1},
    {1406, 60, 1}, {1417, 61, 1}, {1411, 62, 1}, {1403, 63, 1},
    {1421, 64, 1}, {1405, 65, 1}, {1407, 66, 1}, {1414, 67, 1},
    {1413, 68, 1}, {1420, 69, 1}, {1428, 70, 1}, {1429, 71, 1},
    {1424, 72, 1}, {1423, 73, 1}, {1422, 74, 1}, {1427, 75, 1},
    {1426, 76, 1}, {1430, 77, 1}, {1431, 78, 79}, {1432, 80, 81},
    {1433, 82, 1}, {1434, 83, 1}, {1435, 84, 1}, {1600, 87, 1},
    {1605, 0, 1}, {1604, 89, 1}, {1601, 85, 86}, {1603, 90, 1},
    {1602, 88, 1}, {2150, 91, 92}, {2180, 93, 94}, {2190, 95, 96},
    {2170, 97, 98}, {2102, 99, 100}, {2111, 101, 102}, {2160, 103, 104},
    {2185, 105, 106}, {2110, 107, 108}, {2117, 109, 110}, {2140, 111, 112},
    {2100, 113, 114}, {2113, 115, 116}, {2103, 117, 118}, {2112, 119, 120},
    {2116, 123, 124}, {2118, 125, 126}, {2119, 127, 128}, {2120, 129, 130},
    {2501, 131, 1}, {2500, 132, 1}, {2502, 133, 1}, {2203, 134, 1},
    {2204, 135, 1}, {2202, 136, 1}, {2201, 137, 1}, {2200, 138, 1},
    {2205, 139, 1}, {2206, 140, 1}, {2207, 141, 1}, {2304, 142, 1},
    {2305, 143, 1}, {2303, 144, 1}, {2306, 145, 1}, {2300, 146, 1},
    {2412, 147, 1}, {2413, 148, 1}, {2414, 149, 1}, {2410, 150, 1},
    {2423, 151, 1}, {2404, 152, 1}, {2406, 153, 1}, {2411, 154, 1},
    {2415, 155, 1}, {2409, 156, 1}, {2408, 157, 1}, {2400, 158, 1},
    {2401, 159, 1}, {2402, 160, 1}, {2407, 161, 1}, {2403, 162, 1},
    {2416, 163, 1}, {2417, 164, 1}, {2418, 165, 1}, {2419, 166, 1},
    {2420, 167, 1}, {2421, 168, 1}, {2422, 169, 1}, {2424, 170, 1},
    {2425, 171, 1}, {2426, 172, 173}, {2427, 174, 175}, {2428, 176, 1},
    {2429, 177, 1}, {2430, 178, 1}, {2600, 181, 1}, {2605, 0, 1},
    {2604, 183, 1}, {2601, 179, 180}, {2603, 184, 1}, {2602, 182, 1},
    {4150, 185, 186}, {4185, 187, 188}, {4190, 189, 190}, {4170, 191, 192},
    {4102, 193, 194}, {4109, 195, 196}, {4101, 197, 198}, {4105, 199, 200},
    {4112, 201, 202}, {4110, 203, 204}, {4160, 205, 206}, {4180, 207, 208},
    {4140, 209, 210}, {4104, 211, 212}, {4103, 213, 214}, {4107, 215, 216},
    {4108, 217, 218}, {4111, 219, 220}, {4106, 221, 222}, {4100, 223, 224},
    {4500, 225, 1}, {4501, 226, 1}, {4502, 227, 1}, {4208, 228, 1},
    {4207, 229, 1}, {4202, 230, 1}, {4201, 231, 1}, {4204, 232, 1},
    {4205, 233, 1}, {4206, 234, 1}, {4203, 235, 1}, {4304, 236, 1},
    {4301, 237, 1}, {4302, 238, 1}, {4303, 239, 1}, {4300, 240, 1},
    {4405, 241, 1}, {4406, 242, 1}, {4413, 243, 1}, {4422, 244, 1},
    {4401, 245, 1}, {4417, 246, 1}, {4400, 247, 1}, {4415, 248, 1},
    {4416, 249, 1}, {4418, 250, 1}, {4402, 251, 1}, {4414, 252, 1},
    {4403, 253, 1}, {4407, 254, 1}, {4404, 255, 1}, {4408, 256, 1},
    {4410, 257, 1}, {4419, 258, 1}, {4411, 259, 1}, {4412, 260, 1},
    {4420, 261, 1}, {4421, 262, 1}, {4423, 263, 1}, {4424, 264, 1},
    {4425, 265, 266}, {4426, 267, 268}, {4427, 269, 1}, {4428, 270, 1},
    {4429, 271, 1}, {4600, 274, 1}, {4605, 0, 1}, {4604, 276, 1},
    {4601, 272, 273}, {4603, 277, 1}, {4602, 275, 1}, {3150, 278, 279},
    {3185, 280, 281}, {3180, 282, 283}, {3170, 284, 285}, {3140, 286, 287},
    {3101, 288, 289}, {3107, 290, 291}, {3100, 292, 293}, {3102, 294, 295},
    {3106, 296, 297}, {3160, 298, 299}, {3104, 300, 301}, {3105, 302, 303},
    {3109, 304, 305}, {3111, 306, 307}, {3108, 310, 311}, {3112, 312, 313},
    {3114, 314, 315}, {3113, 316, 317}, {3115, 318, 319}, {3190, 320, 321},
    {3116, 322, 323}, {3500, 324, 1}, {3501, 325, 1}, {3502, 326, 1},
    {3206, 327, 1}, {3200, 328, 1}, {3207, 330, 1}, {3205, 329, 1},
    {3201, 331, 1}, {3202, 332, 1}, {3203, 333, 1}, {3204, 334, 1},
    {3300, 335, 1}, {3301, 336, 1}, {3302, 337, 1}, {3303, 338, 1},
    {3304, 339, 1}, {3409, 340, 1}, {3408, 341, 1}, {3404, 342, 1},
    {3412, 343, 1}, {3402, 344, 1}, {3403, 345, 1}, {3415, 346, 1},
    {3416, 347, 1}, {3414, 348, 1}, {3405, 349, 1}, {3400, 350, 1},
    {3410, 351, 1}, {3411, 352, 1}, {3417, 353, 1}, {3418, 354, 1},
    {3413, 355, 1}, {3401, 356, 1}, {3419, 357, 1}, {3406, 358, 1},
    {3407, 359, 1}, {3420, 360, 1}, {3421, 361, 362}, {3422, 363, 364},
    {3423, 365, 1}, {3424, 366, 1}, {3425, 367, 1}, {3600, 370, 1},
    {3605, 0, 1}, {3604, 372, 1}, {3601, 368, 369}, {3603, 373, 1},
    {3602, 371, 1}, {0, 0, 0},
};

namespace {
// The object names table (the string table gText keeps at 0x59c: its resource 1010, mObjectNames).
const TbITable* ObjectNames() { return static_cast<const TbITable*>(gText.mResourcePointers[20]); }
}  // namespace

// Both parts, then the name: "<A> <B>" (no blank when B is empty), or "<A><B>" when A ended with a hyphen. A part the table
// does not have is empty. The copy to the caller is at most 0x80 characters whatever size it asked for (a negative size is
// not cut, as the count of wcsncpy is unsigned).
bool CRideNameDB::GetName(int32_t id, char16_t* buffer, int32_t size) {
    if (!GetNameA(id, mPartA, 0x40, false)) swprintf(mPartA, 0x40, u"");
    if (!GetNameB(id, mPartB, 0x40)) swprintf(mPartB, 0x40, u"");
    bool joinWithBlank = true;
    const int32_t length = static_cast<int32_t>(wcslen(mPartA));
    if (length > 0 && mPartA[length - 1] == u'-') {
        mPartA[length - 1] = 0;
        joinWithBlank = false;
    }
    if (joinWithBlank) swprintf(mWhole, 0x40, u"%ls%ls%ls", mPartA, mPartB[0] != 0 ? u" " : u"", mPartB);
    else swprintf(mWhole, 0x40, u"%ls%ls", mPartA, mPartB);
    if (size > 0x80) size = 0x80;
    wcsncpy(buffer, mWhole, static_cast<uint32_t>(size));
    return true;
}
// The first part of a ride type's name. The table is searched from the top for 0x14a rows or up to its row of zeros; a
// found row's first string is formatted into the buffer, and with `trim` a hyphen at the end is cut off (the name goes on
// in the second part).
bool CRideNameDB::GetNameA(int32_t id, char16_t* buffer, int32_t size, bool trim) {
    for (int32_t row = 0; row < 0x14a; ++row) {
        if (gRideNameTable[row].mId == 0) return false;
        if (id == gRideNameTable[row].mId) {
            ObjectNames()->FormatString(buffer, static_cast<uint32_t>(size), gRideNameTable[row].mTextA);
            if (trim) {
                const int32_t length = static_cast<int32_t>(wcslen(buffer));
                if (length > 0 && buffer[length - 1] == u'-') buffer[length - 1] = 0;
            }
            return true;
        }
    }
    return false;
}

bool CRideNameDB::GetNameB(int32_t id, char16_t* buffer, int32_t size) {
    for (int32_t row = 0; row < 0x14a; ++row) {
        if (gRideNameTable[row].mId == 0) return false;
        if (id == gRideNameTable[row].mId) {
            ObjectNames()->FormatString(buffer, static_cast<uint32_t>(size), gRideNameTable[row].mTextB);
            return true;
        }
    }
    return false;
}

// The game clock (SimTheme Park 2.04b3): the time classes CClock (src/Clock.cpp) is built
// from, 28 functions in the inventory's Game.cpp, RideDB.cpp, SaveLoad.cpp and
// FS_MasterSave.cpp ranges, and the user interface's timeGetTime clock.
//
//   CFixedTime -> CPausableFixedTime -> CSaveableFixedTime      (ReadFixed)
//   CVariableTime -> CPausableTime -> CFixableTime -> CSaveableTime      (Read)
//
// Both chains count milliseconds of LbTime_GetClock, which is a 32-bit number that wraps.
// A reading adds the unsigned difference to the previous reading to a double and hands out
// that double as an unsigned long (CodeWarrior's saturating conversion), so a clock that
// goes backwards adds almost 2^32 milliseconds. Pausing keeps the reading it paused at and
// the time spent paused; the fixed-step mode holds the reading until NextFrame moves it.

// 0x101573f8
int32_t CSaveableFixedTime::Save(void* file) {
    uint32_t transferred;
    LbFile_Write(file, &mSavedTime, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    return 1;
}

// 0x101574ac
int32_t CSaveableFixedTime::Load(void* file) {
    uint32_t transferred;
    LbFile_Read(file, &mSavedTime, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    return 1;
}

// 0x10157550
int32_t CSaveableFixedTime::ReadSaveableFixedTime() { return mAdjustment + mPausableTime.ReadPausableFixedTime(); }

// 0x101575c8
void CSaveableFixedTime::AdjustTime() {
    mAdjustment = static_cast<int32_t>(mSavedTime) - mPausableTime.ReadPausableFixedTime();
}
// 0x1015763c
int32_t CSaveableTime::Save(void* file) {
    uint32_t transferred;
    LbFile_Write(file, &mSavedTime, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    return 1;
}

// 0x101576ec: a failed read leaves whatever the file call put in the field.
int32_t CSaveableTime::Load(void* file) {
    uint32_t transferred;
    LbFile_Read(file, &mSavedTime, 4, &transferred);
    if ((transferred >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    return 1;
}

// ---- CSaveableTime and CSaveableFixedTime

// A saved clock is one word, mSavedTime, in a little-endian file. The Mac swaps the word
// in place around the call (stwbrx) and back again afterwards; this host is little-endian,
// so the field already is in file order and goes to the file as it is. Both ways stop
// with a text when the count of bytes moved is not four to seven (count >> 2 == 1), and
// nothing else is looked at: not the library's own result.

// 0x1015778c
int32_t CSaveableTime::ReadSaveableTime() { return mAdjustment + ReadFixableTime(); }

// 0x101577fc: after a load, makes the clock read the saved time now.
void CSaveableTime::AdjustTime() { mAdjustment = static_cast<int32_t>(mSavedTime) - ReadFixableTime(); }

namespace {
constexpr uint32_t kTrace = 0x499602d2;   // the trace level of the progress messages

// The temporary allocator, made on first use (operator new(8) and its constructor, stored in the import gMemoryTemporary).
SamsUtilities::UAllocatorTemporary& SaveTemporaryAllocator() {
    if (SamsUtilities::gMemoryTemporary == nullptr) SamsUtilities::gMemoryTemporary = new SamsUtilities::UAllocatorTemporary;
    return *SamsUtilities::gMemoryTemporary;
}
}  // namespace

// Writes the park: the version word (500, little endian), the header (SaveHeader), then the modules (SaveModules) into a memory
// file, which is packed with zlib and appended to the file. 1 on success; else 0 with mError set (1 open, 2 no memory, 3 write,
// 5 SaveModules failed). The original's file is closed and deleted on most failures.
int32_t CSaveLoad::Save(const char* path, CSaveLoadHeader* header, LoadType type) {
    GAME_ASSERTMSG(type != kLoadHeaderOnly, "Cannot have header-only save\n");
    void* handle;
    if (!LbFile_Open(handle, path, 0x40000010).Succeeded()) {
        CTRACE(0, "Failed to open %s", path);
        mError = kErrorOpenOrRead;
        return 0;
    }

    uint32_t version = 500;   // little endian in the file: the host's own order
    uint32_t written;
    if (!LbFile_Write(handle, &version, 4, &written).Succeeded()) {
        GAME_ASSERTMSG(0, "Write to %s failed", path);
        LbFile_Close(handle);
        LbFile_Delete(path);
        mError = kErrorMemoryWrite;
        return 0;
    }
    if (SaveHeader(handle, header) == 0) {
        GAME_ASSERTMSG(0, "Save Header FAILED!");
        LbFile_Close(handle);
        LbFile_Delete(path);
        mError = kErrorMemoryWrite;
        return 0;
    }
    CTRACE(kTrace, "Saved header: %s", path);
    LbFile_Close(handle);

    CTRACE(kTrace, "Opening memory file for saving,\n");
    // In the arena: BMP_Save's chunks keep the file's address in a 32-bit field (the Mac's file object was on the stack).
    tpw::ArenaHeld<bfl::FileSystem::CMemFile> held;
    bfl::FileSystem::CMemFile& file = *held;
    if (!file.Open(0x200).Succeeded()) {
        CTRACE(kTrace, "ERROR: Couldn't not open memory file for saving!");
        mError = kErrorNoMemory;
        return 0;
    }
    PreSaveOperations();
    if (SaveModules(&file, type) == 0) {
        GAME_ASSERTMSG(0, "Save FAILED!");
        file.Close();
        LbFile_Delete(path);
        mError = 5;
        return 0;
    }
    CTRACE(kTrace, "Saved: %s", path);

    const int32_t size = file.GetSize();
    uint8_t* raw = SaveTemporaryAllocator().Allocate(static_cast<uint32_t>(size));
    if (raw == nullptr) {
        file.Close();
        LbFile_Delete(path);
        mError = kErrorNoMemory;
        return 0;
    }
    file.Seek(0, 0);
    file.Read(raw, static_cast<uint32_t>(size));   // (nothing when the file has no access object)
    file.Close();

    uint8_t* packed = SaveTemporaryAllocator().Allocate(static_cast<uint32_t>(size));
    if (packed == nullptr) {
        LbFile_Delete(path);
        SaveTemporaryAllocator().Free(raw);
        mError = kErrorNoMemory;
        return 0;
    }
    const uint32_t packedSize = LbZLib_MemCompress(raw, packed, static_cast<uint32_t>(size), nullptr);
    GAME_ASSERTMSG(static_cast<uint32_t>(size) >= packedSize, "Need to allocate a bigger buffer during save game!");
    GAME_ASSERTMSG(packedSize != 0, "Save game is too small to be saved - increase size of Layout save padding action!");
    SaveTemporaryAllocator().Free(raw);

    bfl::FileSystem::CDiscFile disc;
    if (!disc.Open(path, 0xC0000030).Succeeded()) {
        CTRACE(0, "Failed to open %s", path);
        SaveTemporaryAllocator().Free(packed);
        LbFile_Delete(path);
        mError = kErrorOpenOrRead;
        return 0;
    }
    disc.Seek(0, 2);
    const uint32_t count = static_cast<uint32_t>(disc.Write(packed, packedSize));   // -1 without an access object
    if (packedSize != count) {
        SaveTemporaryAllocator().Free(packed);
        disc.Close();
        mError = kErrorMemoryWrite;
        return 0;
    }
    SaveTemporaryAllocator().Free(packed);
    disc.Close();
    return 1;
}

// SaveLoad.cpp (main app 0x10157fec..): the park file reader. PARTIAL: only CSaveLoad::Load
// (0x10157fec) so far; LoadHeader, LoadModules and the Pre and PostLoadOperations are declared in
// SaveLoad.h and still stand-ins.

uint32_t gSaveLoadOpenTestWord;   // 0x102a6cb8, 0 in the binary

namespace {

// The temporary allocator, made on first use (operator new(8) and its constructor, stored in the import gMemoryTemporary).
// The original tests the pointer at every use.
SamsUtilities::UAllocatorTemporary& TemporaryAllocator() {
    if (SamsUtilities::gMemoryTemporary == nullptr) SamsUtilities::gMemoryTemporary = new SamsUtilities::UAllocatorTemporary;
    return *SamsUtilities::gMemoryTemporary;
}

// The file's little endian version word as the host has it (the original, on a big endian Mac, swaps the bytes it read).
uint32_t FileWord(uint32_t word) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(word);
#else
    return word;
#endif
}

// The newest version this game loads for a park of the player's folder.
constexpr int32_t kNewestVersion = 500;
// The trace class of the save file messages.
constexpr uint32_t kTraceSaveLoad = 0x499602d2;

}  // namespace


// Reads a park file: the version word, the header, then the rest of the file, which is a zlib block of the modules. The block is
// unpacked into a memory file and LoadModules reads that. Type 0 stops after the header; type 1 (a downloaded park) skips the
// Pre and PostLoadOperations. On every failure the reason goes to mError and the answer is 0; the file objects are destroyed on
// every path (the disc file's destructor closes it).
//
// Quirks kept: the open test is the debug word's (see SaveLoad.h); a block that is not a zlib block is released with delete[]
// instead of the allocator's Free; the CTRACE of a short read is made before the packed buffer is freed.
int32_t CSaveLoad::Load(const char* path, CSaveLoadHeader* header, LoadType type) {
    GAME_ASSERTMSG(type != kLoadHeaderOnly || header != nullptr,
                   "Cannot have a header-only load but supply no header pointer\n");
    bfl::FileSystem::CDiscFile file;
    const bool opened = file.Open(path, 0x80000000).Succeeded();
    if (opened != (gSaveLoadOpenTestWord == 0)) {
        CTRACE(0, "Couldn't open file %s", path);
        mError = kErrorOpenOrRead;
        return 0;
    }

    uint32_t word = 0;
    if (file.Read(&word, 4) != 4) {
        DEBUG_MESSAGE("File read failed", 0);
        file.Close();
        mError = kErrorOpenOrRead;
        return 0;
    }
    const int32_t version = static_cast<int32_t>(FileWord(word));
    if (type == kLoadPark && version > kNewestVersion) {
        mError = kErrorFutureVersion;
        DEBUG_MESSAGE("Trying to load future version savegame into earlier game - get a patch\n", 0);
        file.Close();
        return 0;
    }

    if (LoadHeader(file, type, header) == 0) {
        CTRACE(kTraceSaveLoad, "Could not load saved game header!");
        file.Close();
        mError = kErrorHeader;
        return 0;
    }
    if (type == kLoadHeaderOnly) return 1;

    // what is left of the file
    const int32_t fileSize = file.GetSize();
    const int32_t position = file.Seek(0, 1);
    const int32_t remaining = static_cast<int32_t>(static_cast<uint32_t>(fileSize) - static_cast<uint32_t>(position));
    if (remaining == 0) {
        mError = kErrorEmptyOrBad;
        return 0;
    }
    uint8_t* packed = TemporaryAllocator().Allocate(static_cast<uint32_t>(remaining));
    if (file.Read(packed, static_cast<uint32_t>(remaining)) != remaining) {
        CTRACE(kTraceSaveLoad, "Could not read %d bytes out of compressed save file", remaining);
        TemporaryAllocator().Free(packed);
        mError = kErrorPackedRead;
        return 0;
    }
    file.Close();
    const uint32_t unpackedSize = LbZLib_MemDecompressedSize(packed);
    if (unpackedSize == 0) {
        delete[] packed;
        mError = kErrorEmptyOrBad;
        return 0;
    }
    uint8_t* unpacked = TemporaryAllocator().Allocate(unpackedSize);
    if (unpacked == nullptr) {
        TemporaryAllocator().Free(packed);
        mError = kErrorNoMemory;
        return 0;
    }
    LbZLib_MemDecompress(packed, unpacked, nullptr);
    TemporaryAllocator().Free(packed);

    // In the arena: BMP_Load's chunks keep the file's address in a 32-bit field (the Mac's file object was on the stack).
    tpw::ArenaHeld<bfl::FileSystem::CMemFile> held;
    bfl::FileSystem::CMemFile& memory = *held;
    if (!memory.Open(unpackedSize).Succeeded()) {
        TemporaryAllocator().Free(unpacked);
        mError = kErrorNoMemory;
        return 0;
    }
    if (memory.Write(unpacked, unpackedSize) < 0) {
        TemporaryAllocator().Free(unpacked);
        memory.Close();
        mError = kErrorMemoryWrite;
        return 0;
    }
    memory.Seek(0, 0);
    TemporaryAllocator().Free(unpacked);
    if (type != kLoadOnline) PreLoadOperations();
    const int32_t loaded = LoadModules(memory, type);
    file.Close();
    memory.Close();
    if (type != kLoadOnline) PostLoadOperations();
    if (loaded == 0) {
        GAME_ASSERTMSG(0, "Loading failed.  System state now (possibly) inconsistent.");
        mError = kErrorEmptyOrBad;
        return 0;
    }
    CTRACE(kTraceSaveLoad, "Loaded savegame: %s", path);
    return 1;
}

// SaveLoad.cpp (main app), unit v51006: CSaveLoad::SaveHeader (0x10159a08), PostLoadOperations (0x101588c4) and PreLoadOperations
// (0x101587e4).

// Clears the world before a park is read: the height field, the map, the cell system, the engine objects, the things, the ride in
// first person, the bitmaps, the coasters, the particle system, the advisor (0) and the user interface.
void CSaveLoad::PreLoadOperations() {
    RestoreLevelHeightfield();
    gGameWorld->GetMap()->DeleteMapInEngine();
    LLCC_ResetMap(0x80, 0x80);
    gGameWorld->DeleteEngineObjects();
    gGameWorld->DeleteAllThings();
    gGameWorld->InitThingHeap();
    StopRidingRideIn1stPerson();
    BMP_FreeAll();
    DestroyCoasterInstances();
    RSSE_Close();
    ADV_Kill(0);
    gUIInterface.PreLevelLoad();
}



// Rebuilds what the engine holds after a park is read. The particle system is low spec when the current detail is 0.
void CSaveLoad::PostLoadOperations() {
    InitialiseWeather();
    InitialiseBlobShadow();
    FLY_RestoreMeshes();
    gGameWorld->RestoreEngineObjects();
    gGameWorld->GetMap()->UpdateMapInEngine();
    gGameWorld->GetControlManager()->CheckAllControlsInitialised();
    RecalculateParkDarkening();
    gRestartParkClocks = 1;
    gClock.PostLoad();
    TickRideAnims();
    RegenerateRideSigns();
    gUIInterface.PostLevelLoad();
    ADV_Kill(1);
    if (gPlayerSettings.mCurrent.mDetail == 0) {
        PAR_SetLowSpec(1);
    } else {
        PAR_SetLowSpec(0);
    }
    ESP_FixSpriteIDsFromDetail();
    SOUND_SetPauseOn3DSoundsOnly(0);
}

// CSaveLoad::LoadModules (main app 0x101589f0, 4,052 bytes): reads every module of a park from the memory file.

// The static CCheat of the game window and its guard byte (0x102da934 and 0x102da940; defined in src/ui_main_gamewin.cpp).
alignas(4) extern uint8_t gGameWinCheat[12];
extern int8_t gGameWinCheatGuard;

namespace {

// Reads the four byte tag that precedes a module. The answer of LbFile_Read is not looked at: the original tests only that the
// transferred count divided by 4 is 1 (so 4 to 7 pass). The tag is the file's bytes read as a little endian word. A short read
// reports "read failed", a different tag reports `text`; both end the load.
bool ReadModuleTag(void* handle, uint32_t tag, const char* text) {
    uint32_t word = 0;
    uint32_t count = 0;
    LbFile_Read(handle, &word, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return false;
    }
    if (word != tag) {
        GAME_ASSERTMSG(0, text);
        return false;
    }
    return true;
}

}  // namespace

// Quirks kept: the logfile calls go to the global gSaveLoad whatever `this` is; a failure never closes the logfile; a type other than
// 1 reads everything (0, 2, ...); the module tags are checked before the module they announce; the answers of ADV_Load,
// SOUND_LoadStatus and UIInterface::Load are not read.
int32_t CSaveLoad::LoadModules(bfl::FileSystem::CFile& file, LoadType type) {
    void* handle = &file;
    int32_t start = LbFile_GetPosition(handle);
    gSaveLoad.OpenLogfile(0);

    if (gActionRecorder.Serialise(handle, 0, 1, 0) != 0) {
        DEBUG_MESSAGE("Restore action recording FAILED", 0);
        return 0;
    }
    CTRACE(2, "GActionRec: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    start = LbFile_GetPosition(handle);
    if (type == kLoadOnline) {
        gSaveLoad.CloseLogfile();
        return 1;
    }
    if (gGameWorld->Load(handle) == 0) {
        DEBUG_MESSAGE("Restore world FAILED", 0);
        return 0;
    }
    gSaveLoad.CloseLogfile();
    CTRACE(2, "World: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // scripts
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x57524c44, "The SAD_AI module did not load the same number of bytes that it saved")) return 0;
    if (SCR_LoadAllScripts(handle) != 0) {
        DEBUG_MESSAGE("Restore scripts FAILED", 0);
        return 0;
    }
    CTRACE(2, "Scripts: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // particles
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x53505343, "The SAD_SPRITE_SCRIPTS module did not load the same number of bytes that it saved")) return 0;
    if (PAR_LoadStatus(handle) != 0) {
        DEBUG_MESSAGE("PAR_SaveStatus FAILED", 0);
        return 0;
    }
    PAR_KillAndHideAll2DObjects();
    CTRACE(2, "Particles: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // message centre (made on first use: the Mac's operator new(0x1d0) and __construct_array of 29 headers)
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x50415254, "The SAD_PARTICLES module did not load the same number of bytes that it saved")) return 0;
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;
    if (gMessageCentre->Load(handle) != 0) {
        DEBUG_MESSAGE("Message centre load failed horribly", 0);
        return 0;
    }
    CTRACE(2, "MsgCntr: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // clock
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x4d455353, "The SAD_MESSAGE module did not load the same number of bytes that it saved")) return 0;
    if (gClock.Load(handle) == 0) {
        DEBUG_MESSAGE("Clock failed to save", 0);
        return 0;
    }
    CTRACE(2, "Clock: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // vanilla time
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x434c4f4b, "The SAD_CLOCK module did not load the same number of bytes that it saved")) return 0;
    if (!gVanillaTime.Load(handle)) {
        DEBUG_MESSAGE("Vanilla time failed to load", 0);
        return 0;
    }
    CTRACE(2, "VanillaTime: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // game system
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x56414e54, "The SAD_VANILLA_TIME module did not load the same number of bytes that it saved")) return 0;
    if (GAMESYSTEM_Load(handle) == 0) {
        DEBUG_MESSAGE("Game System failed to load", 0);
        return 0;
    }
    CTRACE(2, "GameSystem: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // rides
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x47535953, "The SAD_GAMESYS module did not load the same number of bytes that it saved")) return 0;
    if (LoadRides(handle) == 0) {
        DEBUG_MESSAGE("Ride System failed to load", 0);
        return 0;
    }
    CTRACE(2, "Ridesystem: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // track rides
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x52535953, "The SAD_RIDESYS module did not load the same number of bytes that it saved")) return 0;
    if (BMP_Load(handle) == 0) {
        DEBUG_MESSAGE("Track rides failed to load", 0);
        return 0;
    }
    CTRACE(2, "TrackRides: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // flying rides
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x5452414b, "The SAD_TRACK module did not load the same number of bytes that it saved")) return 0;
    if (FLY_Load(handle) != 0) {
        DEBUG_MESSAGE("Flying rides failed to load", 0);
        return 0;
    }
    CTRACE(2, "FlyingRides: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // RSSE
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x464c5952, "The SAD_FLYERS module did not load the same number of bytes that it saved")) return 0;
    if (RSSE_Load(handle) != 0) {
        DEBUG_MESSAGE("RSSE scripts failed to load", 0);
        return 0;
    }
    CTRACE(2, "RSSE: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // camera
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x52535345, "The SAD_RSSE module did not load the same number of bytes that it saved")) return 0;
    if (LoadCamera(handle) == 0) {
        DEBUG_MESSAGE("Camera failed to load", 0);
        return 0;
    }
    CTRACE(2, "Camera: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // coasters
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x4b414d45, "The SAD_CAMERA module did not load the same number of bytes that it saved")) return 0;
    if (LoadCoasters(handle) == 0) {
        DEBUG_MESSAGE("Coasters failed to load", 0);
        return 0;
    }
    CTRACE(2, "Coasters: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // advisor
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x434f4153, "The SAD_COASTERS module did not load the same number of bytes that it saved")) return 0;
    ADV_Load(handle);
    CTRACE(2, "Advisor: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // sound
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x41445653, "The SAD_ADV module did not load the same number of bytes that it saved")) return 0;
    SOUND_LoadStatus(handle);
    CTRACE(2, "Sound: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // cheat
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x534f554e, "The SAD_SOUND module did not load the same number of bytes that it saved")) return 0;
    if (gGameWinCheatGuard == 0) {
        new (gGameWinCheat) CCheat();
        gGameWinCheatGuard = 1;
    }
    if (reinterpret_cast<CCheat*>(gGameWinCheat)->Load(handle) == 0) {
        DEBUG_MESSAGE("Cheat system failed to load", 0);
        return 0;
    }
    CTRACE(2, "Cheat: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // advisor scoring
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x43485453, "The SAD_CHEAT module did not load the same number of bytes that it saved")) return 0;
    if (TheScoring().Load(handle) == 0) {
        DEBUG_MESSAGE("Advisor scoring system failed to load", 0);
        return 0;
    }
    CTRACE(2, "AdvisorScoring: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    // user interface
    start = LbFile_GetPosition(handle);
    if (!ReadModuleTag(handle, 0x41445343, "The SAD_ADV_SCORING module did not load the same number of bytes that it saved")) return 0;
    gUIInterface.Load(handle);
    CTRACE(2, "UI: loaded %d bytes\n", LbFile_GetPosition(handle) - start);

    LbFile_GetPosition(handle);   // the answer is dropped
    return 1;
}

// Writes what LoadHeader reads, after the version word: the language byte, the legal text (0x500 bytes, little endian UTF-16, of the
// language the text system names), the 0x100-byte ride record (gParkRequiredRides in its first 0x20 bytes), the key word as it is in
// memory (the file holds the bytes 01 22 19 85), the flag word (little endian 1 when a header is given) and the header (Serialise
// with save 1). 1 on success; 0 when a write fails (negative result or a count other than the length) or Serialise answers 1.
// Quirks kept: one count variable serves every write (a write that leaves it alone is judged by the last value); an unknown language
// name selects text 0; any Serialise answer other than 1 counts as success.
int32_t CSaveLoad::SaveHeader(void* file, CSaveLoadHeader* header) {
    uint8_t block[0x501];   // [0] the language byte, then the text (the original's text is at the odd address 0x16d)
    uint8_t* const text = block + 1;
    std::memset(text, 0, 0x500);

    TbDynamicStringTemplate<char> name(gText.GetLanguageName());
    if (name == "English") {
        block[0] = 0;
    } else if (name == "Spanish") {
        block[0] = 1;
    } else if (name == "Italian") {
        block[0] = 2;
    } else if (name == "Swedish") {
        block[0] = 3;
    } else if (name == "German") {
        block[0] = 4;
    } else if (name == "French") {
        block[0] = 5;
    } else if (name == "Japanese") {
        block[0] = 6;
    } else {
        block[0] = 0;
    }

    TbDynamicStringTemplate<char16_t>& legal = gSaveLegalText[block[0]];
    const char16_t* const source = legal.mData;
    std::memcpy(text, source, static_cast<size_t>(legal.Length()) << 1);
    SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(text), 0x280);

    uint32_t written = 0;
    if (!LbFile_Write(file, &block[0], 1, &written).Succeeded() || written != 1) return 0;

    if (!LbFile_Write(file, text, 0x500, &written).Succeeded() || written != 0x500) return 0;

    uint8_t rides[0x100];
    std::memset(rides, 0, 0x100);
    std::memcpy(rides, gParkRequiredRides, 0x20);
    if (!LbFile_Write(file, rides, 0x100, &written).Succeeded() || written != 0x100) return 0;

    // (the key goes out as the Mac holds it, most significant byte first)
    uint32_t key = __builtin_bswap32(gSaveFileKey);
    if (!LbFile_Write(file, &key, 4, &written).Succeeded() || written != 4) return 0;

    uint32_t flag = header != nullptr ? 1u : 0u;   // (little endian in the file: the host's own order)
    if (!LbFile_Write(file, &flag, 4, &written).Succeeded() || written != 4) return 0;

    if (header == nullptr) return 1;
    if (header->Serialise(file, 1, 1, 0) == 1) return 0;
    return 1;
}

// SaveLoad.cpp (main app 0x10159e50 and 0x1015b484) and the header's Serialise (0x1015b8b0, the inventory files it under Text.cpp): the
// park file's header (unit w114). LoadHeader reads what follows the version word, the header's Serialise reads or writes its fields, and
// the static initialisation of SaveLoad.cpp builds the table of the seven legal texts LoadHeader checks.

TbDynamicStringTemplate<char16_t> gSaveLegalText[7] = {
    TbDynamicStringTemplate<char16_t>(kSaveLegalText0), TbDynamicStringTemplate<char16_t>(kSaveLegalText1),
    TbDynamicStringTemplate<char16_t>(kSaveLegalText2), TbDynamicStringTemplate<char16_t>(kSaveLegalText3),
    TbDynamicStringTemplate<char16_t>(kSaveLegalText4), TbDynamicStringTemplate<char16_t>(kSaveLegalText5),
    TbDynamicStringTemplate<char16_t>(kSaveLegalText6),
};   // 0x1037fe1c

uint32_t gSaveFileKey = 0x01221985;   // 0x102a6cb4

namespace {

// The file's key word as the Mac compares it: the original loads the four bytes it read with a big endian load and compares that with the
// key, no swap in between. The saver writes the key from memory in the same way, so the Mac's files hold the bytes 01 22 19 85, and a
// file saved by the PC game (85 19 22 01) is refused.
uint32_t KeyWordAsLoadedOnTheMac(uint32_t raw) {
    uint8_t bytes[4];
    std::memcpy(bytes, &raw, 4);
    return static_cast<uint32_t>(bytes[0]) << 24 | static_cast<uint32_t>(bytes[1]) << 16 | static_cast<uint32_t>(bytes[2]) << 8 |
           static_cast<uint32_t>(bytes[3]);
}

}  // namespace

// Reads what follows the version word of a park file and answers 1, or 0 when it cannot. The file is: one byte (the language of the legal
// text), 0x500 bytes (the legal text, 640 little endian UTF-16 units), 0x100 bytes (the ride record), four bytes (the key word), four
// bytes (a little endian int: 1 when the header follows, 0 when not) and the header. Every read fails when its result is negative or its
// count is not the length asked for; a failed read answers 0 and leaves mError alone. `type` 2 (a park of the player's folder) lets the
// ride database leave out its preview rides.
//
// Quirks kept: the language byte is not range checked (the table has 7 texts); the text is compared as a NUL terminated string and the
// original does not terminate it (the host buffer has one more unit, 0, so a text with no 0 is compared in its 640 units); the text
// mismatch, a failing Serialise and a failed read leave mError as it was, while a ride record the database cannot satisfy sets 8 and a
// wrong key 6; gParkRequiredRides is overwritten before the key is read, so it holds the record of a file whose key is wrong; a flag
// word other than 0 and 1 reads the header too (with a NULL header the original calls Serialise on NULL and crashes); a flag of 0 with a
// header asks for one the file does not have: the assert reports and the answer is 1 with the header untouched.
int32_t CSaveLoad::LoadHeader(bfl::FileSystem::CFile& file, LoadType type, CSaveLoadHeader* header) {
    void* const handle = &file;
    uint32_t count = 0;

    uint8_t language = 0;
    if (!LbFile_Read(handle, &language, 1, &count).Succeeded() || count != 1) return 0;

    char16_t text[0x281];
    text[0x280] = 0;
    if (!LbFile_Read(handle, text, 0x500, &count).Succeeded() || count != 0x500) return 0;
    SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(text), 0x280);
    // (the original's index is the byte zero extended, times 0x14, from the table; here the same element)
    if (*(gSaveLegalText + language) != text) {
        GAME_ASSERTMSG(0, "The save game legal text has been jiggered with!\n");
        return 0;
    }

    char rides[0x100];
    if (!LbFile_Read(handle, rides, 0x100, &count).Succeeded() || count != 0x100) return 0;
    // (the world is only the receiver of the call: the function never reads it)
    if (gGameWorld->DoIHaveTheseRides(rides, type == kLoadPark ? 1 : 0) == 0) {
        mError = kErrorMissingRides;
        return 0;
    }
    std::memcpy(gParkRequiredRides, rides, 0x20);

    uint32_t key = 0;
    if (!LbFile_Read(handle, &key, 4, &count).Succeeded() || count != 4) return 0;
    if (KeyWordAsLoadedOnTheMac(key) != gSaveFileKey) {
        mError = kErrorEmptyOrBad;
        return 0;
    }

    uint32_t flagWord = 0;
    if (!LbFile_Read(handle, &flagWord, 4, &count).Succeeded() || count != 4) return 0;
    flagWord = serialise::UnswapWord(flagWord);

    if (flagWord == 0 && header != nullptr) {
        GAME_ASSERTMSG(0, "You didn't save a header with this file but you've requested to load one - the load will work but you'll get a "
                          "duff header back\n");
    } else if (flagWord == 1 && header == nullptr) {
        GAME_ASSERTMSG(0, "You saved a header with this file but haven't requested to load it again - the load would fail if I hadn't "
                          "picked this up\n");
        CSaveLoadHeader skipped;
        if (skipped.Serialise(handle, 0, 1, 0) != 0) return 0;
    } else if (flagWord != 0 || header != nullptr) {
        if (header->Serialise(handle, 0, 1, 0) != 0) return 0;
    }
    return 1;
}

// CSaveLoad::SaveModules (main app, SaveLoad.cpp; unit v5628): writes the park's modules to the save file, each followed by its tag.

uint32_t gSaveModulesTagTestWords[17] = {};   // 0x102a6cbc .. 0x102a6cfc

namespace {

const char kDelimiterFailed[] = "Couldn't write delimeter";   // (sic)

// Writes a module's tag, the four characters as a big-endian word in the source, byte reversed into the file (the PC's order), and
// answers whether the write's result passes the comparison with the tag's constant word: (result >= 0) == (word == 0).
bool WriteTag(void* file, uint32_t tag, int32_t index) {
    uint32_t word = serialise::SwapWord(tag);
    const bool succeeded = LbFile_Write(file, &word, 4, nullptr).Succeeded();
    return succeeded == (gSaveModulesTagTestWords[index] == 0);
}

}  // namespace

// The modules in order: the action recorder (alone for type 1, the online park), the world, the scripts, the particles, the message
// centre, the clock, the vanilla time, the game system, the rides, the track rides, the flying rides, the RSSE scripts, the camera,
// the coasters, the advisor, the sound, the cheat state, the advisor scoring and the interface. A module that fails, or a tag whose
// write does not pass, answers 0 at once. The debug log (gSaveLoad's, opened with state 0) covers the recorder and the world.
//
// Quirks kept: `this` is not used; a failing action recorder returns without closing the log; the message centre is made when there
// is none (the original tests operator new's answer, which is never NULL); the scripts, the advisor, the sound and the interface
// answer nothing that is read; the advisor's size is traced with the coasters' text, and the interface's with "UI: loaded"; the
// position read after the last trace is not used.
int32_t CSaveLoad::SaveModules(void* file, LoadType type) {
    int32_t start = LbFile_GetPosition(file);
    gSaveLoad.OpenLogfile(0);
    if (gActionRecorder.Serialise(file, 1, 1, 0) != 0) return 0;
    CTRACE(2, "GActionRec: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);
    if (type == kLoadOnline) {
        gSaveLoad.CloseLogfile();
        return 1;
    }

    if (gGameWorld->Save(file) == 0) {
        DEBUG_MESSAGE("Save world FAILED", 0);
        gSaveLoad.CloseLogfile();
        return 0;
    }
    gSaveLoad.CloseLogfile();
    if (!WriteTag(file, 0x57524c44u /* WRLD */, 0)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "World: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    SCR_SaveAllScripts(file);
    if (!WriteTag(file, 0x53505343u /* SPSC */, 1)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Scripts: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (PAR_SaveStatus(file, 0) != 0) {
        DEBUG_MESSAGE("PAR_SaveStatus failed", 0);
        return 0;
    }
    if (!WriteTag(file, 0x50415254u /* PART */, 2)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Particles: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;
    if (gMessageCentre->Save(file) != 0) {
        DEBUG_MESSAGE("Message centre save failed horribly", 0);
        return 0;
    }
    if (!WriteTag(file, 0x4d455353u /* MESS */, 3)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "MessageCentre: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (gClock.Save(file) == 0) {
        DEBUG_MESSAGE("Clock failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x434c4f4bu /* CLOK */, 4)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Clock: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (!gVanillaTime.Save(file)) {
        DEBUG_MESSAGE("Vanilla time failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x56414e54u /* VANT */, 5)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "VanillaTime: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (GAMESYSTEM_Save(file) == 0) {
        DEBUG_MESSAGE("Game System failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x47535953u /* GSYS */, 6)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "GameSystem: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (SaveRides(file) == 0) {
        DEBUG_MESSAGE("Ride System failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x52535953u /* RSYS */, 7)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "RideSystem: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (BMP_Save(file) == 0) {
        DEBUG_MESSAGE("Track rides failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x5452414bu /* TRAK */, 8)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "TrackRides: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (FLY_Save(file) != 0) {
        DEBUG_MESSAGE("Flying rides failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x464c5952u /* FLYR */, 9)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "FlyingRides: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (RSSE_Save(file) != 0) {
        DEBUG_MESSAGE("RSSE scripts failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x52535345u /* RSSE */, 10)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "RSSE: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (SaveCamera(file) == 0) {
        DEBUG_MESSAGE("Camera failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x4b414d45u /* KAME */, 11)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Camera: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    if (SaveCoasters(file) == 0) {
        DEBUG_MESSAGE("Coasters failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x434f4153u /* COAS */, 12)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Coasters: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    ADV_Save(file);
    if (!WriteTag(file, 0x41445653u /* ADVS */, 13)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Coasters: saved %d bytes\n", LbFile_GetPosition(file) - start);   // (sic: the coasters' text)
    start = LbFile_GetPosition(file);

    SOUND_SaveStatus(file);
    if (!WriteTag(file, 0x534f554eu /* SOUN */, 14)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Sound: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    // the function's static CCheat, made on first use (no destructor is registered)
    if (gGameWinCheatGuard == 0) {
        new (gGameWinCheat) CCheat();
        gGameWinCheatGuard = 1;
    }
    if (reinterpret_cast<CCheat*>(gGameWinCheat)->Save(file) == 0) {
        DEBUG_MESSAGE("Cheat system failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x43485453u /* CHTS */, 15)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "Cheat: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    // (the original inlines the shared static: guard byte 0x102b4100, object 0x102b3ac8, its destructor registered)
    if (TheScoring().Save(file) == 0) {
        DEBUG_MESSAGE("Advisor scoring system failed to save", 0);
        return 0;
    }
    if (!WriteTag(file, 0x41445343u /* ADSC */, 16)) {
        DEBUG_MESSAGE(kDelimiterFailed, 0);
        return 0;
    }
    CTRACE(2, "AdvisorScoring: saved %d bytes\n", LbFile_GetPosition(file) - start);
    start = LbFile_GetPosition(file);

    gUIInterface.Save(file);
    CTRACE(2, "UI: loaded %d bytes\n", LbFile_GetPosition(file) - start);
    LbFile_GetPosition(file);
    return 1;
}

// Unit v51118: small stand-ins of the main app (CTimeSig's three members are inline in include/TimeSig.h and are tested through it;
// CSkyObjects, CSaveLoad, InterfaceFontWrapper, the ride database and balance destructors, two messages, and the std containers'
// destructors, constructors and allocator functions of the master save).

void CSaveLoad::OpenLogfile(int32_t) {}
void CSaveLoad::CloseLogfile() {}
// 0x1015b3dc, 0x1015b370, 0x1015b3a4: the debug log is gone; the Mac answers 0, 0 and 1 in r3 (the members are declared void).
void CSaveLoad::WriteSerialiseLog(int32_t, const char*, LogfileState) {}
// 0x1015b428
void CSaveLoad::PreSaveOperations() { gClock.PreSave(); }
