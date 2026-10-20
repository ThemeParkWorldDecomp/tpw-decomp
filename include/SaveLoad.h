// SaveLoad.cpp (main app): CSaveLoad, the park file reader and writer. PARTIAL: Load is
// translated (src/SaveLoad.cpp, unit w108) and LoadHeader with the header's Serialise
// (src/SaveLoadHeader.cpp, unit w114); LoadModules and the Pre and PostLoadOperations are
// declared for Load but not translated.
#pragma once
#include <cstddef>
#include <cstdint>

#include "TbDynamicStringTemplate.h"

namespace bfl::FileSystem {
class CFile;
}

// A save file's header, filled by Load (0x3d4 bytes; no pointers, so the host layout is the Mac's). The names are invented from the
// strings Serialise passes to LOADVAR (unit w114); the file holds only the first 50 units of the park description.
struct CSaveLoadHeader {
    // 0x1015b8b0 (1,412 bytes, src/SaveLoadHeader.cpp): reads (save == 0) or writes (any other value) the fields in the order of the
    // members below, 282 values in all; 0 on success, 1 at the first value that fails. A version other than 0 or 1 (signed compares)
    // does nothing and answers 0. `unused` is ignored.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);

    char16_t mCityName[16];                // 0x000
    char16_t mCityDescription[128];        // 0x020
    float mCityX;                          // 0x120
    float mCityY;                          // 0x124
    float mCityZ;                          // 0x128
    char16_t mParkName[16];                // 0x12c
    char16_t mAuthorPlayerName[16];        // 0x14c
    char16_t mAuthorEmail[50];             // 0x16c
    char16_t mParkDescription[255];        // 0x1d0  (the array is 255 units because 0x1d0 + 2 * 255 = 0x3ce exactly; only 50 are serialised)
    uint16_t mParkDatePublishedDay;        // 0x3ce
    uint16_t mParkDatePublishedMonth;      // 0x3d0
    uint16_t mParkDatePublishedYear;       // 0x3d2
};
static_assert(sizeof(CSaveLoadHeader) == 0x3d4, "CSaveLoadHeader layout");
static_assert(offsetof(CSaveLoadHeader, mCityDescription) == 0x020, "CSaveLoadHeader layout");
static_assert(offsetof(CSaveLoadHeader, mCityX) == 0x120, "CSaveLoadHeader layout");
static_assert(offsetof(CSaveLoadHeader, mParkName) == 0x12c, "CSaveLoadHeader layout");
static_assert(offsetof(CSaveLoadHeader, mAuthorPlayerName) == 0x14c, "CSaveLoadHeader layout");
static_assert(offsetof(CSaveLoadHeader, mAuthorEmail) == 0x16c, "CSaveLoadHeader layout");
static_assert(offsetof(CSaveLoadHeader, mParkDescription) == 0x1d0, "CSaveLoadHeader layout");
static_assert(offsetof(CSaveLoadHeader, mParkDatePublishedDay) == 0x3ce, "CSaveLoadHeader layout");
static_assert(offsetof(CSaveLoadHeader, mParkDatePublishedYear) == 0x3d2, "CSaveLoadHeader layout");

// The save file key LoadHeader compares the file's key word with (0x102a6cb4, initialised 0x01221985; defined in
// src/SaveLoadHeader.cpp). The saver writes the word as it is in memory, so the Mac's files hold the bytes 01 22 19 85.
extern uint32_t gSaveFileKey;
// The legal text of the save file, one per language in the order of the language byte the saver chooses (0 English, 1 Spanish,
// 2 Italian, 3 Swedish, 4 German, 5 French, 6 Japanese), built by the static initialisation of SaveLoad.cpp (0x1015b484,
// src/SaveLoadHeader.cpp). 0x1037fe1c on the Mac, 0x14 bytes each there. LoadHeader compares the file's text with one of them.
extern TbDynamicStringTemplate<char16_t> gSaveLegalText[7];

// The debug switch of the file open test in CSaveLoad::Load (the word at 0x102a6cb8, 0 in the binary, nothing writes it; its
// name is not known). Load goes on when "the file opened" equals "the word is 0", so a nonzero word turns the test round: a file
// that opens is refused and one that does not open is read.
extern uint32_t gSaveLoadOpenTestWord;

class CSaveLoad {
public:
    // What Load reads: 0 only the header (the header pointer must be given), 1 the park downloaded in an online game (no
    // Pre or PostLoadOperations), 2 a park of the player's folder (a file of a later version than 500 is refused).
    enum LoadType : int32_t { kLoadHeaderOnly = 0, kLoadOnline = 1, kLoadPark = 2 };
    enum LogfileState : int32_t {};   // 0 saving, 1 loading
    // The reason Load answered 0, kept in mError (nothing resets it on a success).
    enum LoadError : int32_t {
        kErrorOpenOrRead = 1,       // the file did not open, or its first word could not be read
        kErrorNoMemory = 2,         // a buffer could not be allocated, or the memory file could not be opened
        kErrorMemoryWrite = 3,      // the memory file refused the unpacked bytes
        kErrorPackedRead = 4,       // the packed bytes could not all be read
        kErrorEmptyOrBad = 6,       // nothing to read, an unreadable block, or LoadModules answered 0 (LoadHeader sets 6 too: the
                                    // file's key word is not the game's)
        kErrorHeader = 7,           // LoadHeader answered 0
        kErrorMissingRides = 8,     // LoadHeader: the park needs a ride the ride database does not have (DoIHaveTheseRides answered 0)
        kErrorFutureVersion = 9     // a type 2 file of a version above 500
    };

    // 0x10157fec (1,960 bytes): 1 when the file was read, else 0 with mError set. The file is a version word (little endian), the
    // header (LoadHeader) and a zlib block of the modules, which LoadModules reads from a memory file made of the unpacked bytes.
    int32_t Load(const char* path, CSaveLoadHeader* header, LoadType type);
    // 0x10157868 (1,844 bytes, untranslated; unit v5034): writes the park; 1 on success, else 0.
    int32_t Save(const char* path, CSaveLoadHeader* header, LoadType type);
    // 0x10159e50 (748 bytes, src/SaveLoadHeader.cpp, unit w114): reads what follows the version word, answers 1 or 0: the language byte,
    // the legal text (compared with gSaveLegalText), the 0x100-byte ride record (the park's needed rides, 0x20 bytes of it kept in
    // gParkRequiredRides), the key word, the flag word and, when the flag is 1, the header (Serialise). A failed read, a text that
    // differs and a failing Serialise answer 0 and leave mError alone; a ride the database lacks sets 8 and a wrong key 6.
    int32_t LoadHeader(bfl::FileSystem::CFile& file, LoadType type, CSaveLoadHeader* header);
    // 0x101589f0 (4,052 bytes, untranslated): reads the modules from the memory file; 0 when it cannot.
    int32_t LoadModules(bfl::FileSystem::CFile& file, LoadType type);
    void PreLoadOperations();    // 0x101587e4, 172 bytes, untranslated
    void PostLoadOperations();   // 0x101588c4, 248 bytes, untranslated
    // 0x1015b3dc: notes a serialised value of `size` bytes in the debug log.
    void WriteSerialiseLog(int32_t size, const char* name, LogfileState state);
    void OpenLogfile(int32_t state);   // 0x1015b370 (8 bytes, stand-in; LoadModules calls it with 0 on the global gSaveLoad)
    void CloseLogfile();               // 0x1015b3a4 (8 bytes, stand-in)
    // 0x1015a190 (4,508 bytes, src/SaveLoad_CSaveLoad_SaveModules.cpp, unit v5628): writes the modules to `file`, each followed by its
    // four-character tag; 1 on success, else 0. Type 1 writes the action recorder only. `this` is not used (the log is gSaveLoad's).
    int32_t SaveModules(void* file, LoadType type);
    // Unit v51006: 0x10159a08 (1,032 bytes, src/SaveLoad_CSaveLoad_SaveHeader_2.cpp): writes what LoadHeader reads (language byte, legal
    // text, ride record, key, flag word, header when given); 1 on success, else 0. `this` is not used.
    int32_t SaveHeader(void* file, CSaveLoadHeader* header);
    // Unit v51020: 0x1015b428 (40 bytes, stand-in): called by Save before the modules are written.
    void PreSaveOperations();

    uint32_t mF00 = 0;     // 0x00  not known (the static initialisation of SaveLoad.cpp clears it)
    int32_t mError = 0;    // 0x04  a LoadError
    uint8_t mF08[0x14 - 0x08] = {};   // 0x08  LoadModules keeps a word here
    uint32_t mF14 = 0x10000;   // 0x14  0x10000 after the static initialisation of SaveLoad.cpp (0x1037ff10); not known (it may be a
                               //       separate global that happens to follow)
    uint32_t mF18 = 0;     // 0x18  0 after the static initialisation (0x1037ff14)
    // (the object goes on: the extent is not known)
};
extern CSaveLoad gSaveLoad;   // 0x1037fefc, original name unknown

// Unit v5206: the module loaders CSaveLoad::LoadModules calls (each reads its module from the memory file handle).
int32_t SCR_LoadAllScripts(void* file);   // 0x100d9ccc: 0 when loaded
int32_t PAR_LoadStatus(void* file);       // 0x100bb7d8: 0 when loaded
void PAR_KillAndHideAll2DObjects();       // 0x100be494
int32_t RSSE_Load(void* file);            // 0x100d56bc: 0 when loaded
int32_t LoadCamera(void* file);           // 0x10036218: 1 when loaded
void SOUND_LoadStatus(void* file);        // 0x100de48c
int32_t GAMESYSTEM_Load(void* file);      // 0x10236b30 (stand-in): 0 when it failed
int32_t LoadRides(void* file);            // 0x1006b878 (stand-in): 0 when it failed
// Unit v5890 (src/RestoreAnimTime_LoadRides_2.cpp): what LoadRides needs.
struct tag_sRideAnimSet;
struct sAnimTimeControl;
void ReactivateHoardings(tag_sRideAnimSet* set);                              // 0x10061840 (344 bytes, untranslated)
void OutputAnimTimeStats(sAnimTimeControl* control, tag_sRideAnimSet* set);   // 0x1006aa70 (568 bytes, untranslated)
extern int32_t gLoadRidesTbErrorZero;     // 0x1029d054, the zero word the compiler keeps for LoadRides' TbError comparison (names invented)
extern int32_t gLoadRidesShape[2];        // 0x102c6d04, the two words LoadRides builds a ride's shape from when the database has no record
int32_t BMP_Load(void* file);             // 0x10023850 (src/BMP_Load.cpp): 0 when it failed
int32_t FLY_Load(void* file);             // 0x10079aa8 (stand-in): 0 when loaded
int32_t LoadCoasters(void* file);         // 0x10044d68 (stand-in): 0 when it failed
void ADV_Load(void* file);                // 0x10009cb8 (stand-in): the answer is not read
class CAdvisorScoring;
CAdvisorScoring& TheScoring();            // the shared scoring singleton (Mac object 0x102b3ac8, guard byte 0x102b4100)

// Unit v5628: the module savers CSaveLoad::SaveModules calls (each writes its module to the file handle).
int32_t SaveRides(void* file);            // 0x1006acf8 (stand-in): 0 when it failed
int32_t BMP_Save(void* file);             // 0x10022464 (stand-in): 0 when it failed
int32_t FLY_Save(void* file);             // 0x10079248 (stand-in): 0 when saved
int32_t SaveCoasters(void* file);         // 0x10044280 (stand-in): 0 when it failed
void ADV_Save(void* file);                // 0x10009788 (stand-in): the answer is not read
// The constants SaveModules compares the result of each tag's write with (0x102a6cbc to 0x102a6cfc, one word per tag in the order
// of the writes: WRLD SPSC PART MESS CLOK VANT GSYS RSYS TRAK FLYR RSSE KAME COAS ADVS SOUN CHTS ADSC; 0 in the binary and nothing
// writes them; defined in src/SaveLoad_CSaveLoad_SaveModules.cpp). Each comparison is (result >= 0) == (word == 0), see TbError.h.
extern uint32_t gSaveModulesTagTestWords[17];
// Unit v51028 (src/RideTypes_OutputAnimTimeStats_SaveRides_2.cpp): the zero word the compiler keeps for SaveRides' TbError comparison.
extern uint32_t gSaveRidesTbErrorZero;    // 0x1029d050 (name invented; nothing writes it)

// Unit v51006: what PreLoadOperations calls first.
void RestoreLevelHeightfield();           // 0x10063cc0 (52 bytes, stand-in)
