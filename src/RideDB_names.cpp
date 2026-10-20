// RideDB.cpp range of the main app (SimTheme Park 2.04b3): the ride types' names. The name database (CRideNameDB, which the
// inventory files under SaveLoad.cpp) builds a ride type's name from two strings of the object names string table; the ride
// database's three-argument GetTypeName gives the name the ride's own file named, or the built one.
#include "RideDB.h"

#include "Text.h"
#include "TPEngine.h"
#include "analyser.h"
#include "TbITable.h"
#include "platform/msl.h"

const char16_t gRideMissingName[] = u"MISSING STRING";   // 0x102a5ce2

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

// The ride type's name. The database remembers the last type it found (its id and record) and answers it without a search:
// the cache starts as id 0 with no record, so the first question for type 0 finds nothing. A record whose file named the
// ride (a name that is neither missing nor the "MISSING STRING" text the loader leaves) gives that name; anything else, the
// name builder's.
void CRideDatabase::GetTypeName(uint16_t id, char16_t* buffer, uint32_t size) {
    RideInfo* ride;
    if (id == mCachedId) {
        ride = mCachedRide;
    } else {
        ride = nullptr;
        for (int32_t i = 0; i < mRideCount; ++i) {
            if (mRides[i]->mId == id) {
                mCachedId = id;
                mCachedRide = mRides[i];
                ride = mRides[i];
                break;
            }
        }
    }
    if (ride == nullptr || ride->mName == nullptr || ride->mName == gRideMissingName) {
        mNames.GetName(id, buffer, static_cast<int32_t>(size));
    } else {
        wcsncpy(buffer, ride->mName, size);
    }
}

// The ride type's own name: the text inside its record (RideInfo::mTypeName), or NULL when the database has no such type.
// The lookup is the one above (the cache first, a hit by search is remembered).
const char16_t* CRideDatabase::GetTypeName(uint16_t id) {
    RideInfo* ride;
    if (id == mCachedId) {
        ride = mCachedRide;
    } else {
        ride = nullptr;
        for (int32_t i = 0; i < mRideCount; ++i) {
            if (mRides[i]->mId == id) {
                mCachedId = id;
                mCachedRide = mRides[i];
                ride = mRides[i];
                break;
            }
        }
    }
    if (ride == nullptr) return nullptr;
    return ride->mTypeName;
}

// The first part of the ride's sign text. The name builder's table is asked first (without the hyphen trim); a type it does
// not have gets the sign text of its record. A type the database does not have reads the record at address 0 on the Mac
// (mSignA is at 0x7f4), so the translation dereferences NULL as well.
void CRideDatabase::GetSignNameA(uint16_t id, char16_t* buffer, uint32_t size) {
    if (!mNames.GetNameA(id, buffer, static_cast<int32_t>(size), false)) {
        RideInfo* ride;
        if (id == mCachedId) {
            ride = mCachedRide;
        } else {
            ride = nullptr;
            for (int32_t i = 0; i < mRideCount; ++i) {
                if (mRides[i]->mId == id) {
                    mCachedId = id;
                    mCachedRide = mRides[i];
                    ride = mRides[i];
                    break;
                }
            }
        }
        wcsncpy(buffer, ride->mSignA, size);
    }
}

// The second part, the same way (mSignB is at 0x7f8).
void CRideDatabase::GetSignNameB(uint16_t id, char16_t* buffer, uint32_t size) {
    if (!mNames.GetNameB(id, buffer, static_cast<int32_t>(size))) {
        RideInfo* ride;
        if (id == mCachedId) {
            ride = mCachedRide;
        } else {
            ride = nullptr;
            for (int32_t i = 0; i < mRideCount; ++i) {
                if (mRides[i]->mId == id) {
                    mCachedId = id;
                    mCachedRide = mRides[i];
                    ride = mRides[i];
                    break;
                }
            }
        }
        wcsncpy(buffer, ride->mSignB, size);
    }
}

// Unit w210: the ride record's name (mItemName) of a ride type, a wide string; CObject::GetFileName asks for it. NULL when no record has the id.
const char16_t* CRideDatabase::GetFileName(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    RideInfo* ride;
    if (id.mIndex == mCachedId) {
        ride = mCachedRide;
    } else {
        ride = nullptr;
        for (int32_t i = 0; i < mRideCount; ++i) {
            if (mRides[i]->mId == id.mIndex) {
                mCachedId = id.mIndex;
                mCachedRide = mRides[i];
                ride = mRides[i];
                break;
            }
        }
    }
    return ride != nullptr ? ride->mItemName : nullptr;
}
