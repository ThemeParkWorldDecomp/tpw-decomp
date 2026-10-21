// RideDB.cpp (main app): the database records and their balance values.
// Native pointers and BalanceValue slots give the host its own layout; comments give Mac offsets.
#pragma once
#include <cstdint>
#include "CBalanceTable.h"

namespace bfl { namespace FileSystem { class CFileStorage; } }

enum RideBalanceSlot : int32_t {
    kRideSlot_Id                              =    1,   // ride + 0x10: type 5
    kRideSlot_Name                            =    2,   // ride + 0x14: type 10
    kRideSlot_Shape                           =    3,   // ride + 0x18: type 11
    kRideSlot_Hoarding                        =    4,   // ride + 0x1c: type 11
    kRideSlot_EngineMapOffsetOverrideX        =    5,   // ride + 0x20: type 5
    kRideSlot_EngineMapOffsetOverrideY        =    6,   // ride + 0x24: type 5
    kRideSlot_EngineFootprintWidthOverride    =    7,   // ride + 0x28: type 5
    kRideSlot_EngineFootprintHeightOverride   =    8,   // ride + 0x2c: type 5
    kRideSlot_MapOffsetX                      =    9,   // ride + 0x30: type 5
    kRideSlot_MapOffsetY                      =   10,   // ride + 0x34: type 5
    kRideSlot_DontApplyOffset                 =   11,   // ride + 0x38: type 6
    kRideSlot_IsChoosable                     =   12,   // ride + 0x3c: type 6
    kRideSlot_HasQueue                        =   13,   // ride + 0x40: type 6
    kRideSlot_CanBreakDown                    =   14,   // ride + 0x44: type 6
    kRideSlot_RunsContinuously                =   15,   // ride + 0x48: type 6
    kRideSlot_WhichUIType                     =   16,   // ride + 0x4c: type 6
    kRideSlot_DontDeformBase                  =   17,   // ride + 0x50: type 6
    kRideSlot_OverwritePriority               =   18,   // ride + 0x54: type 5
    kRideSlot_AttractionValue                 =   19,   // ride + 0x58: type 5
    kRideSlot_NewAttractionDecayTime          =   20,   // ride + 0x5c: type 6
    kRideSlot_CreateParticleEffect            =   21,   // ride + 0x60: type 6
    kRideSlot_DestroyParticleEffect           =   22,   // ride + 0x64: type 6
    kRideSlot_RepairParticleEffect            =   23,   // ride + 0x68: type 6
    kRideSlot_UpgradeParticleEffect           =   24,   // ride + 0x6c: type 6
    kRideSlot_DurationUnit                    =   25,   // ride + 0x70: type 5
    kRideSlot_UsesCars                        =   26,   // ride + 0x74: type 5
    kRideSlot_PreviewAnimType                 =   27,   // ride + 0x78: type 10
    kRideSlot_PreviewAnimNum                  =   28,   // ride + 0x7c: type 5
    kRideSlot_FirstPersonSprites              =   29,   // ride + 0x80: type 6
    kRideSlot_DoHeadProcessing                =   30,   // ride + 0x84: type 6
    kRideSlot_TurnOffAnimatingTextures        =   31,   // ride + 0x88: type 6
    kRideSlot_TurnOffScrollingTextures        =   32,   // ride + 0x8c: type 6
    kRideSlot_RideTypeStringIndex             =   33,   // ride + 0x90: type 5
    kRideSlot_DownloadedRideNumber            =   34,   // ride + 0x94: type 6
    kRideSlot_IsPreviewRide                   =   35,   // ride + 0x98: type 6
    kRideSlot_WhichTrackType                  =   36,   // ride + 0x9c: type 6
    kRideSlot_BumperType                      =   37,   // ride + 0xa0: type 4
    kRideSlot_NorthXAdjust                    =   38,   // ride + 0xa4: type 4
    kRideSlot_NorthYAdjust                    =   39,   // ride + 0xa8: type 4
    kRideSlot_EastXAdjust                     =   40,   // ride + 0xac: type 4
    kRideSlot_EastYAdjust                     =   41,   // ride + 0xb0: type 4
    kRideSlot_SouthXAdjust                    =   42,   // ride + 0xb4: type 4
    kRideSlot_SouthYAdjust                    =   43,   // ride + 0xb8: type 4
    kRideSlot_WestXAdjust                     =   44,   // ride + 0xbc: type 4
    kRideSlot_WestYAdjust                     =   45,   // ride + 0xc0: type 4
    kRideSlot_GoldenTicketCost                =   46,   // ride + 0xc4: type 5
    kRideSlot_Length                          =   47,   // ride + 0xc8: type 5
    kRideSlot_Width                           =   48,   // ride + 0xcc: type 5
    kRideSlot_Height                          =   49,   // ride + 0xd0: type 5
    kRideSlot_EntryCellStandPosX              =   50,   // ride + 0xd4: type 7
    kRideSlot_EntryCellStandPosY              =   51,   // ride + 0xd8: type 7
    kRideSlot_ExitCellAppearPosX              =   52,   // ride + 0xdc: type 7
    kRideSlot_ExitCellAppearPosY              =   53,   // ride + 0xe0: type 7
    kRideSlot_InitPricePerUse                 =   54,   // ride + 0xe4: type 5
    kRideSlot_FatigueEffect                   =   55,   // ride + 0xe8: type 5
    kRideSlot_InitChanceOfLoosing             =   56,   // ride + 0xec: type 6
    kRideSlot_ProvidesSecurity                =   57,   // ride + 0xf0: type 6
    kRideSlot_ProvidesRelief                  =   58,   // ride + 0xf4: type 6
    kRideSlot_HoldsLitter                     =   59,   // ride + 0xf8: type 6
    kRideSlot_ChillsYouOut                    =   60,   // ride + 0xfc: type 6
    kRideSlot_RequiresTeleport                =   61,   // ride + 0x100: type 6
    kRideSlot_RideHandlesSprite               =   62,   // ride + 0x104: type 6
    kRideSlot_RequiresWatering                =   63,   // ride + 0x108: type 6
    kRideSlot_NonWinnableSideshow             =   64,   // ride + 0x10c: type 6
    kRideSlot_IsFireworks                     =   65,   // ride + 0x110: type 6
    kRideSlot_ISIndoors                       =   66,   // ride + 0x114: type 6
    kRideSlot_CannotRide                      =   67,   // ride + 0x118: type 6
    kRideSlot_ConstrainCamera                 =   68,   // ride + 0x11c: type 6
    kRideSlot_PukeReductionLevel              =   69,   // ride + 0x120: type 5
    kRideSlot_MinCapacity                     =   70,   // ride + 0x124: type 5
    kRideSlot_MaxCapacity                     =   71,   // ride + 0x128: type 5
    kRideSlot_MinDuration                     =   72,   // ride + 0x12c: type 5
    kRideSlot_MaxDuration                     =   73,   // ride + 0x130: type 5
    kRideSlot_MinSpeed                        =   74,   // ride + 0x134: type 5
    kRideSlot_MaxSpeed                        =   75,   // ride + 0x138: type 5
    kRideSlot_ExcitementLevel                 =   76,   // ride + 0x13c: type 6
    kRideSlot_InitCostOfGoods                 =   77,   // ride + 0x140: type 5
    kRideSlot_ThirstEffect                    =   78,   // ride + 0x144: type 4
    kRideSlot_HungerEffect                    =   79,   // ride + 0x148: type 4
    kRideSlot_VomitEffect                     =   80,   // ride + 0x14c: type 4
    kRideSlot_HappinessEffect                 =   81,   // ride + 0x150: type 4
    kRideSlot_LitterEffect                    =   82,   // ride + 0x154: type 4
    kRideSlot_SpecialIngredient               =   83,   // ride + 0x158: type 6
    kRideSlot_AppearanceEffect                =   84,   // ride + 0x15c: type 6
    kRideSlot_InitPrizeValue                  =   85,   // ride + 0x160: type 5
    kRideSlot_ShopType                        =   86,   // ride + 0x164: type 6
    kRideSlot_ExciteFactor                    =   87,   // ride + 0x168: type 6
    kRideSlot_RipOffOK                        =   88,   // ride + 0x16c: type 5
    kRideSlot_NumSimultAnims                  =   89,   // ride + 0x170: type 5
    kRideSlot_Cost                            =   90,   // ride + 0x174: type 5
    kRideSlot_Group                           =   91,   // ride + 0x178: type 6
    kRideSlot_Category                        =   92,   // ride + 0x17c: type 6
    kRideSlot_Upgrades_CostOfResearch         =   93,   // ride + 0x180: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_EstimatedPriceVariance =   94,   // ride + 0x184: array Upgrades[3], element 0, stride 16 slots, type 6
    kRideSlot_Upgrades_ScrapValueYear1        =   95,   // ride + 0x188: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_ScrapValueYear2        =   96,   // ride + 0x18c: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_ScrapValueYear3        =   97,   // ride + 0x190: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_ScrapValueYear4        =   98,   // ride + 0x194: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_InitCapacity           =   99,   // ride + 0x198: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_RedLineCapacity        =  100,   // ride + 0x19c: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_InitDuration           =  101,   // ride + 0x1a0: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_RedLineDuration        =  102,   // ride + 0x1a4: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_InitSpeed              =  103,   // ride + 0x1a8: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_RedLineSpeed           =  104,   // ride + 0x1ac: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_WearRate               =  105,   // ride + 0x1b0: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_QueueWaitTimeConstant  =  106,   // ride + 0x1b4: array Upgrades[3], element 0, stride 16 slots, type 7
    kRideSlot_Upgrades_CostOfUpgrade          =  107,   // ride + 0x1b8: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_DurationOfUpgrade      =  108,   // ride + 0x1bc: array Upgrades[3], element 0, stride 16 slots, type 5
    kRideSlot_Upgrades_Count                  =  141,   // ride + 0x240: the element count of Upgrades (capacity 3)
    kRideSlot_SupplementalMeshes_FileName     =  142,   // ride + 0x244: array SupplementalMeshes[8], element 0, stride 1 slots, type 10
    kRideSlot_SupplementalMeshes_Count        =  150,   // ride + 0x264: the element count of SupplementalMeshes (capacity 8)
    kRideSlot_Attraction_NewBonus             =  151,   // ride + 0x268: array Attraction[3], element 0, stride 1 slots, type 5
    kRideSlot_Attraction_Count                =  154,   // ride + 0x274: the element count of Attraction (capacity 3)
    kRideSlot_SignTextures_FileName           =  155,   // ride + 0x278: array SignTextures[2], element 0, stride 1 slots, type 10
    kRideSlot_SignTextures_Count              =  157,   // ride + 0x280: the element count of SignTextures (capacity 2)
    kRideSlot_UpgradesId                      =  158,   // ride + 0x284: type 5
    kRideSlot_UpgradeType                     =  159,   // ride + 0x288: type 5
    kRideSlot_SlotCount = 160,   // the values array holds this many slots (slot 0 is never used); the object ends at +0x28c
};

class CRideBalance : public CBalanceTable {
public:
    const BalanceDescription* GetBalanceDescription(int32_t row) override; // 0x10156f80
    BalanceValue* GetBalanceValuesStart() override; // 0x10156fcc
    BalanceValue mValues[kRideSlot_SlotCount]; // Mac +8, slot 0 unused
};
extern const BalanceDescription gRideBalanceDescriptions[132]; // 0x10299854

struct RideInfo {
    void DeallocStrings(); // 0x101550bc
    ~RideInfo(); // 0x1015457c, deleting form used by the database
    void LoadStrings(bfl::FileSystem::CFile& file); // 0x101559f8
    void LoadSounds(bfl::FileSystem::CFileStorage& storage); // 0x10156b28
    void LoadParticles(bfl::FileSystem::CFileStorage& storage); // 0x10156bc4
    void SetIsCustomRide(); // 0x10156f40
    uint32_t GetCustomParticleEffect(int32_t index) const; // 0x10156db0
    int32_t GetCustomSoundBank() const; // 0x10156df8
    BalanceValue& Value(RideBalanceSlot slot) { return mBalance.mValues[slot]; }
    const BalanceValue& Value(RideBalanceSlot slot) const { return mBalance.mValues[slot]; }

    uint16_t mId; // 0x000
    CRideBalance mBalance; // 0x004, value slot s at 0x00c + 4*s
    char mFolder[0x400]; // 0x28c
    char16_t mItemName[0x40]; // 0x68c
    char16_t mTypeName[0x72]; // 0x70c, loader copies at most 0x40 characters
    uint32_t mRideTypeNumber; // 0x78c
    int32_t mEntryX; // 0x790
    int32_t mEntryY; // 0x794
    uint8_t mEntryFlag; // 0x798, only the byte is written
    int32_t mExitX; // 0x79c
    int32_t mExitY; // 0x7a0
    uint8_t mExitFlag; // 0x7a4
    int32_t mUIType; // 0x7a8
    int32_t mMapOffsetX; // 0x7ac
    int32_t mMapOffsetY; // 0x7b0
    int32_t mSubRideCount; // 0x7b4
    uint32_t mSubRideTypes[8]; // 0x7b8
    union {
        struct {
            uint32_t mF7D8; // 0x7d8, unknown; particle filename digit 0 writes here
            uint32_t mEmitters[4]; // 0x7dc
            int32_t mSoundCategory; // 0x7ec
        };
        uint32_t mEmitterWords[6];
    };
    union {
        struct {
            const char16_t* mName; // 0x7f0
            const char16_t* mSignA; // 0x7f4
            const char16_t* mSignB; // 0x7f8
            const char16_t* mString3; // 0x7fc
        };
        const char16_t* mStrings[4];
    };
    uint32_t mFlags; // 0x800, bit 0 is custom ride
    int32_t mLoadFailed; // 0x804
};

// The ride masks (32 bytes, bit n - 1 of the record's DownloadedRideNumber balance value is ride n; unit w114, defined in src/game/World.cpp): the rides the
// ride database has (CWorld::DoIHaveTheseRides rebuilds it on every call) and the rides the park being loaded needs (CSaveLoad::LoadHeader
// copies the first 0x20 bytes of the file's ride record here). Names invented; they sit just before gThingArray (0x1034cbfc).
extern uint8_t gRidesOwned[0x20];          // 0x1034cb98
extern uint8_t gParkRequiredRides[0x20];   // 0x1034cbb8; CWorld::FlagItemAsSelected sets its bits and SetupNewLevel clears it (it is also the first 0x20 bytes of the save header's ride record)

// The text the loader leaves in a ride's name when its file has none (a wide string in the initialised data; the lookups
// compare the POINTER).
extern const char16_t gRideMissingName[];   // 0x102a5ce2, "MISSING STRING"

// One row of the table that maps a ride type's number to the two strings of its name in the object names string table
// (OBJECT_NAMES): the name and the rest of the name ("Roller" and "Coaster"). 12 bytes on the Mac.
struct RideNameEntry {
    int32_t mId;       // 0x00  the ride type's number; 0 ends the table
    uint32_t mTextA;   // 0x04  the string number of the first part
    uint32_t mTextB;   // 0x08  and of the second (1 is an empty string)
};
extern const RideNameEntry gRideNameTable[282];   // 0x102a5d3c: 281 rows and the terminator

// The name builder (Mac 0x180 bytes of scratch text in the database, at 0x52c). The three buffers hold the first part, the
// second part and the whole name.
class CRideNameDB {
public:
    // 0x101570c4: the ride type's name in `buffer` (at most `size`, and never more than 0x80 characters): the two parts, a
    // blank apart, or joined without one when the first part ends with a hyphen (which is dropped). A part that is not in the
    // table is empty. 1.
    bool GetName(int32_t id, char16_t* buffer, int32_t size);
    // 0x10157250: the first part of the name of ride type `id` from the string table; false when the id is not in the table.
    // `trim` asks for a trailing hyphen to be cut off.
    bool GetNameA(int32_t id, char16_t* buffer, int32_t size, bool trim);
    bool GetNameB(int32_t id, char16_t* buffer, int32_t size);   // 0x1015734c: the second part

    char16_t mPartA[0x40];       // 0x000
    char16_t mPartB[0x40];       // 0x080
    char16_t mWhole[0x40];       // 0x100
};

// RideDB.cpp's token and line helpers (external linkage for the separate loader source).
void copy_line(const char16_t** destination, const char16_t* text); // 0x10155170
int32_t comp(const char16_t* left, const char16_t* right); // 0x10155244
int32_t find_string_id(const char16_t* text); // 0x10155604
const char16_t* find_line(const char16_t* text); // 0x101556a0
extern const char16_t* const gRideStringTags[4]; // 0x102a5d00
