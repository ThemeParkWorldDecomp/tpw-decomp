// SaveLoad.cpp (main app 0x10159e50 and 0x1015b484) and the header's Serialise (0x1015b8b0, the inventory files it under Text.cpp): the
// park file's header (unit w114). LoadHeader reads what follows the version word, the header's Serialise reads or writes its fields, and
// the static initialisation of SaveLoad.cpp builds the table of the seven legal texts LoadHeader checks.
#include "SaveLoad.h"

#include <cstring>

#include "Debug.h"
#include "LbFile.h"
#include "RideDB.h"
#include "Serialise.h"
#include "World.h"
#include "platform/samsutils.h"

#include "SaveLegalText.inc"

// ---- the static initialisation of SaveLoad.cpp (0x1015b484, 324 bytes): the object's words are set (mF00, mError, mF14 = 0x10000 and
// mF18 = 0, in the member initialisers of CSaveLoad), then the seven texts are constructed one after the other and their destructors
// registered for the end of the program (the host's static destructors).
CSaveLoad gSaveLoad;   // 0x1037fefc, original name unknown

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

// One run of equal values of the header: `count` values of a type, one name for all of them (the original names them "xxx[i]"). The
// first call that fails ends the run. The loaders and the savers are the same function with a different callee.
template<class Value> int32_t SerialiseRun(bool load, void* file, Value* values, int32_t count, const char* name) {
    for (int32_t i = 0; i < count; ++i) {
        const int32_t failed = load ? LOADVAR<Value>(file, values + i, name, 1) : SAVEVAR<Value>(file, values + i, name, 1);
        if (failed != 0) return 1;
    }
    return 0;
}

}  // namespace

// Reads (save == 0) or writes the header's 282 values in the order of the members, each with noLog = 1, and answers 1 at the first one
// that fails. A version outside 0 and 1 (signed compares, both directions) does nothing and answers 0; `unused` is ignored.
int32_t CSaveLoadHeader::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    (void)unused;
    if (version >= 2 || version < 0) return 0;
    const bool load = save == 0;
    if (SerialiseRun(load, file, mCityName, 16, "strCityName[i]") != 0) return 1;
    if (SerialiseRun(load, file, mCityDescription, 128, "strCityDescription[i]") != 0) return 1;
    if (SerialiseRun(load, file, &mCityX, 1, "fCityX") != 0) return 1;
    if (SerialiseRun(load, file, &mCityY, 1, "fCityY") != 0) return 1;
    if (SerialiseRun(load, file, &mCityZ, 1, "fCityZ") != 0) return 1;
    if (SerialiseRun(load, file, mParkName, 16, "strParkName[i]") != 0) return 1;
    if (SerialiseRun(load, file, mAuthorPlayerName, 16, "strAuthorPlayerName[i]") != 0) return 1;
    if (SerialiseRun(load, file, mAuthorEmail, 50, "strAuthorEmail[i]") != 0) return 1;
    if (SerialiseRun(load, file, mParkDescription, 50, "strParkDescription[i]") != 0) return 1;
    if (SerialiseRun(load, file, &mParkDatePublishedDay, 1, "wParkDatePublished_d") != 0) return 1;
    if (SerialiseRun(load, file, &mParkDatePublishedMonth, 1, "wParkDatePublished_m") != 0) return 1;
    if (SerialiseRun(load, file, &mParkDatePublishedYear, 1, "wParkDatePublished_y") != 0) return 1;
    return 0;
}

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
