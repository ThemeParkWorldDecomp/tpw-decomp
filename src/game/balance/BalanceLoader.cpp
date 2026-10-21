// BalanceLoader.cpp: name proven by an assert string of the main application that names BalanceLoader.cpp.

#include "bfl_FileSystem.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "ExitErr.h"
#include "LbFile.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// CBalanceTable.cpp (main app of SimTheme Park 2.04b3): the balance table reader,
// 0x1001ea28..0x10021074 ("BalanceLoader.cpp" in the original's own messages). Every tuning
// number the game keeps in a table (options, main, hoardings, online, resolution, sound and
// the ride, coaster, challenge and advisor tables) comes through Load.
//
// A table is a description table (one BalanceDescription per row, see CBalanceTable.h) plus
// value slots. Init gives every row a BalanceRowInfo (where its slot is and, for rows inside
// an array group, the distance between elements and the slot that counts them); Parse reads
// lines "Group.Field value" or "Group[3].Field value" (several fields may share a line) and
// stores each value in its slot.

namespace {

// The character tests of MSL's __ctype_map and __wctype_map in the "C" locale (the map
// bits 0x06 whitespace, 0x10 digit, 0xc0 letter): ASCII only, a byte of 0x80 or more is in no
// class (the wide map is only read below 0x100 and a negative char is far above it).
bool IsSpace(char c) { return c == ' ' || (c >= 9 && c <= 13); }
bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool IsAlnum(char c) { return IsDigit(c) || IsAlpha(c); }

// The messages (the string pool at 0x1023c580).
const char kMissingTableEnd[] = "Missing TABLE_END";                                        // +0x00
const char kTooManyFields[] = "Too many fields have been specified";                         // +0x12
const char kStringAllocFailed[] = "LbMem_Alloc failed to allocate string space during balance file parse";   // +0x36
const char kMissingMapDescription[] = "Missing map description";                             // +0x7c
const char kMissingMapMarker[] = "Missing map description initial marker";                   // +0x94
const char kMissingMapDelimiter[] = "Missing map delimter";                                  // +0xbb
const char kMapTooHigh[] = "Map too high";                                                   // +0xd0
const char kIllegalMapCharacter[] = "Illegal map character";                                 // +0xdd
const char kMapTooWide[] = "Map too wide";                                                   // +0xf3
const char kNoMapType[] = "No map_type specified in BalanceLoader.cpp/MapType";              // +0x100
const char kUnknownMapType[] = "Unknown map_type specified in BalanceLoader.cpp/MapType";    // +0x133
const char kArrayIndexTooHigh[] = "Balance file array index too high";                       // +0x16b
const char kNegativeValue[] = "Balance problem: invalid -ve value specified";                // +0x18d
const char kOutOfRange[] = "Balance Problem:  bounded value out of range";                   // +0x1ba
const char kParserFailure[] = "Balance parser internal failure 8-(";                         // +0x1e7
const char kInitialiseFailure[] = "InitialiseBalanceValues internal failure";                // +0x20b
const char kMapVariableError[] = "Map variable error:\n";                                    // +0x281
const char kMapVariableAlone[] = "Map var can be only field on line";                        // +0x296
const char kStringReadError[] = "String read error (missing value?)";                        // +0x2b8
const char kValueReadError[] = "Value read error (missing value?)";                          // +0x2db

// The symbols a map may use (0x1028e730, description flags 1, and 0x1028e814, flags 2).
const SymbolKey kSymbolKeysA[19] = {
    {'.', 0, 0}, {'*', 4, 0}, {'Q', 3, 0}, {'@', 1, 0}, {'+', 11, 0}, {'#', 16, 0}, {'8', 9, 1},
    {'6', 9, 4}, {'2', 9, 16}, {'4', 9, 64}, {'N', 10, 1}, {'E', 10, 4}, {'S', 10, 16}, {'W', 10, 64},
    {'D', 23, 0}, {'>', 23, 4}, {'<', 23, 64}, {'O', 9, 1}, {'X', 10, 1},
};
const SymbolKey kSymbolKeysB[16] = {
    {'.', 0, 0}, {'^', 0, 1}, {'_', 0, 16}, {'[', 0, 64}, {']', 0, 4}, {'J', 0, 20}, {'F', 0, 65}, {'7', 0, 5},
    {'L', 0, 80}, {'=', 0, 17}, {'H', 0, 68}, {'C', 0, 81}, {'U', 0, 84}, {'n', 0, 69}, {'3', 0, 21}, {'O', 0, 85},
};

// atoi of the Mac's C library: a number beyond the 32-bit limits gives the limit. The text
// begins with a digit.
int32_t Atoi(const char* text) {
    int64_t value = 0;
    for (; IsDigit(*text); ++text) {
        value = value * 10 + (*text - '0');
        if (value > INT32_MAX) value = INT32_MAX;
    }
    return static_cast<int32_t>(value);
}

// What Parse tells GAME_ASSERTMSG when a line is wrong. The original builds it in a 256-byte
// stack buffer with sprintf (the file name, the group, the element index, then a closing
// phrase); the host has room for 1,000 characters and cuts the rest.
class Report {
public:
    Report(const char* name, const char* group) : mLength(0) {
        mText[0] = 0;
        Add("Balance file ").Add(name).Add(" parse error\n");
        Add("Group name problem (").Add(group).Add(")\n");
    }
    // "[3]\n" after the group, for a line that named an element.
    Report& Element(bool inArray, int32_t index) {
        if (inArray) {
            char number[16];
            std::snprintf(number, sizeof number, "[%d]\n", index);
            Add(number);
        }
        return *this;
    }
    Report& Add(const char* text) {
        const size_t room = sizeof mText - 1 - mLength;
        size_t count = std::strlen(text);
        if (count > room) count = room;
        std::memcpy(mText + mLength, text, count);
        mLength += count;
        mText[mLength] = 0;
        return *this;
    }
    // Reports the message and gives the caller its 0.
    int32_t Fail() const {
        GAME_ASSERTMSG(0, mText);
        return 0;
    }

private:
    char mText[1024];
    size_t mLength;
};

}  // namespace

// Gives every row its BalanceRowInfo. Value slots are numbered from 1 (slot 0 is never used) in
// row order. An array group (type 2 .. type 3) takes one run of slots per element: its rows
// get the slots of the first element and a stride of the rows in the group, so element k of
// a row is `mValue[k * mStride]`; the slot after the last element counts the elements
// (0x1001ea28).
void CBalanceTable::Init() {
    const int32_t rowCount = CountBalanceDescriptionRows();
    mRows = nullptr;
    mRows = LbMem_Alloc(static_cast<uint32_t>(rowCount) * sizeof(BalanceRowInfo));
    if (mRows == nullptr) return;
    BalanceRowInfo* rows = static_cast<BalanceRowInfo*>(mRows);
    int32_t slot = 1;
    int32_t stride = 0;
    int32_t arrayCount = 0;
    BalanceValue* countSlot = nullptr;
    for (int32_t row = 0; row < rowCount; ++row) {
        const int32_t type = GetBalanceDescription(row)->mType;
        if (type == 3) {
            rows[row].mValue = nullptr;
            rows[row].mStride = 0;
            rows[row].mCount = nullptr;
            // The first element's slots are used up; the others and the count follow.
            slot = (arrayCount - 1) * stride + slot + 1;
            stride = 0;
            arrayCount = 0;
            countSlot = nullptr;
        } else if (type < 3) {
            if (type == 1) {
                rows[row].mValue = nullptr;
                rows[row].mStride = 0;
                rows[row].mCount = nullptr;
            } else if (type < 1) {
                if (type > -1) {
                    rows[row].mValue = nullptr;
                    rows[row].mStride = 0;
                    rows[row].mCount = nullptr;
                }
            } else {
                // Start of an array group: find its end (the first type 3 before a row of type
                // 0 to 2). The row that stops the search gives the element count.
                int32_t end = row + 1;
                bool found = false;
                while (end < rowCount) {
                    const int32_t endType = GetBalanceDescription(end)->mType;
                    if (endType == 3) {
                        found = true;
                        break;
                    }
                    if (static_cast<uint32_t>(endType) <= 2) break;
                    ++end;
                }
                if (!found) GAME_ASSERTMSG(0, kMissingTableEnd);
                stride = end - row - 1;
                arrayCount = GetBalanceDescription(end)->mArrayCount;
                BalanceValue* values = GetBalanceValuesStart();
                rows[row].mValue = nullptr;
                rows[row].mStride = 0;
                countSlot = values + (slot + stride * arrayCount);
                rows[row].mCount = nullptr;
            }
        } else if (type < 12) {
            // Types 4 to 11: one slot (the first element's).
            rows[row].mValue = GetBalanceValuesStart() + slot;
            ++slot;
            rows[row].mStride = stride;
            rows[row].mCount = countSlot;
        }
    }
    InitialiseBalanceValues(kInitConstruct);
}

// Releases the strings and maps, then the row information (0x1001ed40).
void CBalanceTable::UnInit() {
    FreeStrings();
    if (mRows != nullptr) {
        LbMem_Free(mRows);
        mRows = nullptr;
    }
}

void CBalanceTable::FreeStrings() {
    if (mRows != nullptr) InitialiseBalanceValues(kInitReset);
}

// ---------------------------------------------------------------------------------------
// The line readers. Each takes the address of the cursor and moves it past what it read.

// "[12]." at the cursor: the index (atoi, so it saturates) and the cursor after the dot. 0 if
// the text is not a digit string closed by "]." (0x1001ee20).
int32_t read_array_row(char** cursor, int32_t* index) {
    char* p = *cursor + 1;
    if (!IsDigit(*p)) return 0;
    *index = Atoi(p);
    while (IsDigit(*p)) ++p;
    if (*p != ']') return 0;
    if (p[1] != '.') return 0;
    *cursor = p + 2;
    return 1;
}

// Up to 16 field names, each of up to 64 letters and digits, separated by '.' or a space (a
// name at the end of the text, or one followed by any other character, fails). The names go in
// `names` (zeroed first; a name of 64 characters has no terminator), their number in `count`
// and the cursor after the separator that ended the last one (0x1001ef10).
int32_t read_field_names(char** cursor, char (*names)[64], int32_t* count) {
    char* p = *cursor;
    int32_t n = 0;
    std::memset(names, 0, 16 * 64);
    for (;;) {
        if (!IsAlpha(*p)) return 0;
        int32_t i = 0;
        while (i < 64 && IsAlnum(*p)) {
            names[n][i] = *p;
            ++p;
            ++i;
        }
        const char after = *p;
        ++n;
        if (after != '.' && !IsSpace(after)) return 0;
        const char separator = *p;
        ++p;
        if (separator == '.' && n < 16) continue;
        break;
    }
    if (n == 16 && *p == '.') {
        GAME_ASSERTMSG(0, kTooManyFields);
        return 0;
    }
    *cursor = p;
    *count = n;
    return 1;
}

// An integer: optional '-' and digits, ended by white space or the end of the text; the
// white space before it is skipped. The digits are read by MSL's __strtoul, which stops at 2^32
// - 1 on overflow; the sign is applied to that unsigned value (0x1001f060).
int32_t read_integer_value(char** cursor, int32_t* value) {
    const char* start = *cursor;
    while (IsSpace(*start)) ++start;
    const char* end = start;
    const bool negative = *end == '-';
    if (negative) ++end;
    while (IsDigit(*end)) ++end;
    if (!IsSpace(*end) && *end != 0) return 0;
    *cursor = const_cast<char*>(end);
    uint32_t magnitude = 0;
    bool overflow = false;
    for (const char* digit = start + (negative ? 1 : 0); IsDigit(*digit); ++digit) {
        const uint64_t next = static_cast<uint64_t>(magnitude) * 10 + static_cast<uint64_t>(*digit - '0');
        if (next > 0xffffffffull) {
            overflow = true;
        } else {
            magnitude = static_cast<uint32_t>(next);
        }
    }
    if (overflow) magnitude = 0xffffffffu;
    *value = static_cast<int32_t>(negative ? 0u - magnitude : magnitude);
    return 1;
}

// A float: optional '-', digits and at most one '.', ended by white space or the end of the
// text, under 100 characters. A failure on length has moved the cursor already. atof gives the
// number (0x1001f178).
int32_t read_float_value(char** cursor, float* value) {
    const char* start = *cursor;
    while (IsSpace(*start)) ++start;
    const char* end = start;
    int32_t dots = 0;
    if (*end == '-') ++end;
    while (IsDigit(*end) || (*end == '.' && dots < 2)) {
        if (*end == '.') ++dots;
        ++end;
    }
    if (!IsSpace(*end) && *end != 0) return 0;
    // Two dots always fail: the loop only stops on a third, which is no white space.
    if (dots > 1 && *end != '.') return 0;
    *cursor = const_cast<char*>(end);
    if (end - start >= 100) return 0;
    *value = static_cast<float>(std::atof(start));
    return 1;
}

// A string in double quotes on one line, copied into new memory (0x1001f2a4). `value` is
// cleared first and set to the copy even when the allocation fails (then null).
int32_t read_string(char** cursor, char** value) {
    *value = nullptr;
    char* p = *cursor;
    while (IsSpace(*p)) ++p;
    if (*p != '"') return 0;
    int32_t length = 0;
    char* q = p + 1;
    while (*q != '"' && *q != '\n' && *q != 0) {
        ++q;
        ++length;
    }
    if (*q != '"') return 0;
    char* copy = static_cast<char*>(LbMem_Alloc(static_cast<uint32_t>(length) + 1));
    *value = copy;
    if (copy == nullptr) {
        GAME_ASSERTMSG(0, kStringAllocFailed);
        return 0;
    }
    q = p + 1;
    while (*q != '"' && *q != '\n' && *q != 0) *copy++ = *q++;
    *copy = 0;
    *cursor = q + 1;
    return 1;
}

// A line of three or more '-' characters: the cursor goes past that line (0x1001f414).
int32_t recognise_delimit(char** cursor) {
    char* p = *cursor;
    if (p[0] != '-') return 0;
    if (p[1] != '-') return 0;
    if (p[2] != '-') return 0;
    p += 3;
    while (*p != 0 && *p != '\n') ++p;
    if (*p == '\n') ++p;
    *cursor = p;
    return 1;
}

// A map: the rest of the current line is skipped, then a "---" line, then up to 20 rows of up
// to 20 symbols (spaces ignored) and a closing "---" line. Each symbol is looked up in `keys`.
// The rows are stored in reverse order (the last text line is row 0). The result is a new
// RideShape. On failure `error` names the reason, except when the allocation fails (reported
// to gExitError, `error` left null) (0x1001f4c0).
int32_t read_map_internal(char** cursor, RideShape** shape, char** error, const SymbolKey* keys,
                          int32_t keyCount) {
    RideShape map;
    std::memset(&map, 0, sizeof map);
    char* p = *cursor;
    *error = nullptr;
    while (IsSpace(*p)) ++p;
    while (*p != 0 && *p != '\n') ++p;
    if (*p == 0) {
        *error = const_cast<char*>(kMissingMapDescription);
        return 0;
    }
    ++p;
    if (!recognise_delimit(&p)) {
        *error = const_cast<char*>(kMissingMapMarker);
        return 0;
    }
    int32_t width = 0;
    int32_t height = 0;
    while (!recognise_delimit(&p)) {
        if (*p == 0) {
            *error = const_cast<char*>(kMissingMapDelimiter);
            return 0;
        }
        int32_t columns = 0;
        while (*p != 0 && *p != '\n') {
            const char symbol = *p;
            if (symbol != ' ') {
                if (height == 20) {
                    *error = const_cast<char*>(kMapTooHigh);
                    return 0;
                }
                int32_t k = 0;
                while (k < keyCount && symbol != keys[k].mSymbol) ++k;
                if (k == keyCount) {
                    *error = const_cast<char*>(kIllegalMapCharacter);
                    return 0;
                }
                if (columns == 20) {
                    *error = const_cast<char*>(kMapTooWide);
                    return 0;
                }
                map.mCells[height][columns].mValue = keys[k].mValue;
                map.mCells[height][columns].mFlag = keys[k].mFlag;
                ++columns;
            }
            ++p;
        }
        if (columns > width) width = columns;
        ++height;
        while (*p != 0 && *p != '\n') ++p;
        if (*p == '\n') ++p;
    }
    map.mWidth = width;
    map.mHeight = height;
    // Turn the rows over: the first text line becomes the last row. The original swaps rows
    // beyond the twentieth (blank lines can make the height larger) through memory past the
    // grid; here those rows read as empty and are dropped.
    for (int32_t r = 0; r < height / 2; ++r) {
        const int32_t other = height - 1 - r;
        RideShapeCell top[20];
        RideShapeCell bottom[20];
        std::memset(top, 0, sizeof top);
        std::memset(bottom, 0, sizeof bottom);
        if (r < 20) std::memcpy(top, map.mCells[r], sizeof top);
        if (other < 20) std::memcpy(bottom, map.mCells[other], sizeof bottom);
        if (r < 20) std::memcpy(map.mCells[r], bottom, sizeof bottom);
        if (other < 20) std::memcpy(map.mCells[other], top, sizeof top);
    }
    RideShape* result = static_cast<RideShape*>(LbMem_Alloc(sizeof(RideShape)));
    if (result == nullptr) {
        gExitError.SetError(CExitError::ErrorType(8), 0, nullptr);
        return 0;
    }
    std::memcpy(result, &map, sizeof map);
    *shape = result;
    *cursor = p;
    return 1;
}

// The row of `field` in the group `group` (0x1001f8f8). The group's heading (type 1, or type 3 for
// an array group) is found first, scanning from the end of the table; its members are the
// rows above it, down to the first blank row or array start. A line with an element index
// needs an array group and the index must be below its capacity.
int32_t CBalanceTable::LocateBalanceTableRow(char* group, char* field, int32_t* row, int32_t inArray,
                                             int32_t index) {
    int32_t i = CountBalanceDescriptionRows();
    while (i >= 0) {
        if (GetBalanceDescription(i)->mType == 1 || GetBalanceDescription(i)->mType == 3) {
            if (std::strcmp(GetBalanceDescription(i)->mName, group) == 0) break;
        }
        --i;
    }
    if (i < 0) return 0;
    if (inArray != 0) {
        if (GetBalanceDescription(i)->mType == 1) return 0;
    }
    if (inArray == 0) {
        if (GetBalanceDescription(i)->mType == 3) return 0;
    }
    if (inArray != 0) {
        if (index >= GetBalanceDescription(i)->mArrayCount) {
            GAME_ASSERTMSG(0, kArrayIndexTooHigh);
            return 0;
        }
    }
    --i;
    while (i >= 0) {
        if (GetBalanceDescription(i)->mType == 0) break;
        if (GetBalanceDescription(i)->mType == 2) break;
        if (std::strcmp(GetBalanceDescription(i)->mName, field) == 0) {
            *row = i;
            return 1;
        }
        --i;
    }
    return 0;
}

// Stores an integer in the slot of row `row` if its type allows the value: type 4 anything, 5
// not negative, 6 from the minimum up to (not including) the maximum. Any other type is
// reported as an internal failure and stored all the same (0x1001fb5c).
int32_t CBalanceTable::AssignValue(int32_t* dest, int32_t row, int32_t value) {
    const BalanceDescription* description = GetBalanceDescription(row);
    const int32_t type = description->mType;
    if (type == 5) {
        if (value < 0) {
            GAME_ASSERTMSG(0, kNegativeValue);
            return 0;
        }
    } else if (type >= 5) {
        if (type >= 7) {
            GAME_ASSERTMSG(0, kParserFailure);
        } else if (value < description->mMinInteger || value >= description->mMaxInteger) {
            GAME_ASSERTMSG(0, kOutOfRange);
            return 0;
        }
    } else if (type < 4) {
        GAME_ASSERTMSG(0, kParserFailure);
    }
    *dest = value;
    return 1;
}

// The same for a float: type 7 anything, 8 not negative, 9 from the minimum up to (not
// including) the maximum. A NaN passes every test (0x1001fc68).
int32_t CBalanceTable::AssignValue(float* dest, int32_t row, float value) {
    const BalanceDescription* description = GetBalanceDescription(row);
    const int32_t type = description->mType;
    if (type == 8) {
        if (value < 0.0f) {
            GAME_ASSERTMSG(0, kNegativeValue);
            return 0;
        }
    } else if (type >= 8) {
        if (type >= 10) {
            GAME_ASSERTMSG(0, kParserFailure);
        } else if (value < description->mMinFloat || value >= description->mMaxFloat) {
            GAME_ASSERTMSG(0, kOutOfRange);
            return 0;
        }
    } else if (type < 7) {
        GAME_ASSERTMSG(0, kParserFailure);
    }
    *dest = value;
    return 1;
}

// Sets every value to its default: 0 for integers and floats, the minimum for the bounded
// ones, a null pointer for strings and maps (freed first unless `type` is kInitConstruct), and
// each array's element count to 0. Walks the array groups with their capacity, so every
// element is set (0x1001fd7c).
void CBalanceTable::InitialiseBalanceValues(InitType type) {
    BalanceRowInfo* const rows = static_cast<BalanceRowInfo*>(mRows);
    const int32_t rowCount = CountBalanceDescriptionRows();
    int32_t row = 0;
    while (row < rowCount) {
        int32_t arrayCount;
        if (GetBalanceDescription(row)->mType == 2) {
            // The group's end. The original tests the row `row`, not the one it is looking at,
            // against the table's end: a group without an end runs off the table.
            int32_t end = row;
            while (row < rowCount) {
                if (GetBalanceDescription(end)->mType == 3) break;
                ++end;
            }
            arrayCount = GetBalanceDescription(end)->mArrayCount;
        } else {
            arrayCount = 1;
        }
        bool finished = false;
        ++row;
        while (row < rowCount && !finished) {
            BalanceRowInfo& info = rows[row];
            const uint32_t rowType = static_cast<uint32_t>(GetBalanceDescription(row)->mType);
            if (rowType <= 11) {
                switch (rowType) {
                case 0:
                case 2:
                    GAME_ASSERTMSG(0, kInitialiseFailure);
                    break;
                case 1:
                case 3:
                    finished = true;
                    break;
                case 4:
                case 5:
                    for (int32_t k = 0; k < arrayCount; ++k) info.mValue[k * info.mStride].mInteger = 0;
                    if (info.mCount != nullptr) info.mCount->mInteger = 0;
                    break;
                case 6:
                    for (int32_t k = 0; k < arrayCount; ++k) {
                        info.mValue[k * info.mStride].mInteger = GetBalanceDescription(row)->mMinInteger;
                    }
                    if (info.mCount != nullptr) info.mCount->mInteger = 0;
                    break;
                case 7:
                case 8:
                    for (int32_t k = 0; k < arrayCount; ++k) info.mValue[k * info.mStride].mFloat = 0.0f;
                    if (info.mCount != nullptr) info.mCount->mInteger = 0;
                    break;
                case 9:
                    for (int32_t k = 0; k < arrayCount; ++k) {
                        info.mValue[k * info.mStride].mFloat = GetBalanceDescription(row)->mMinFloat;
                    }
                    if (info.mCount != nullptr) info.mCount->mInteger = 0;
                    break;
                default:   // 10 and 11
                    for (int32_t k = 0; k < arrayCount; ++k) {
                        BalanceValue* slot = &info.mValue[k * info.mStride];
                        if (type == kInitConstruct) {
                            slot->mString = nullptr;
                        } else if (slot->mString != nullptr) {
                            LbMem_Free(slot->mString);
                            slot->mString = nullptr;
                        }
                    }
                    if (info.mCount != nullptr) info.mCount->mInteger = 0;
                    break;
                }
            }
            ++row;
        }
    }
}

// Reads the file `path` and parses it (0x10020134). A file that does not exist is an error
// for kLoadFresh only; an empty file is fine. A read that fails leaks the buffer.
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    const int32_t length = LbFile_GetLength(path);
    if (length == -1) {
        if (mode == kLoadFresh) gExitError.SetError(CExitError::ErrorType(3), 0, path);
        return 0;
    }
    if (length == 0) return 1;
    char* buffer = static_cast<char*>(LbMem_Alloc(static_cast<uint32_t>(length) + 1));
    if (buffer != nullptr) {
        int32_t loaded;
        const TbError error = LbFile_Load(path, buffer, static_cast<uint32_t>(length), &loaded);
        if (error.Succeeded()) {
            buffer[length] = 0;
            const int32_t result = Parse(path, buffer, mode);
            LbMem_Free(buffer);
            return result;
        }
    }
    gExitError.SetError(CExitError::ErrorType(3), 0, path);
    return 0;
}

// The same from an open file (0x100202c0). An empty file is an error here; a short read leaks
// the buffer; Read's result is compared unsigned, so a failure (-1) passes.
int32_t CBalanceTable::Load(const char* name, bfl::FileSystem::CFile& file, LoadMode mode) {
    const int32_t size = file.GetSize();
    if (size <= 0) {
        gExitError.SetError(CExitError::ErrorType(3), 0, name);
        return 0;
    }
    char* buffer = static_cast<char*>(LbMem_Alloc(static_cast<uint32_t>(size) + 1));
    if (buffer == nullptr) {
        gExitError.SetError(CExitError::ErrorType(8), 0, name);
        return 0;
    }
    if (static_cast<uint32_t>(file.Read(buffer, static_cast<uint32_t>(size))) < static_cast<uint32_t>(size)) {
        gExitError.SetError(CExitError::ErrorType(3), 0, name);
        return 0;
    }
    buffer[size] = 0;
    const int32_t result = Parse(name, buffer, mode);
    LbMem_Free(buffer);
    return result;
}
// Parses a file's text. The carriage returns are taken out first (the text is changed in
// place); then each line is "Group.Field... value..." or "Group[index].Field... value..."
// with white space between the parts; a blank line or one starting with '#' is skipped. White
// space before a line includes line ends, so a line after a blank one is read twice. One
// value follows for each field name, except for a map, which stands alone: the lines after
// the field are the map, and the line after its closing "---" is skipped. After the first
// error nothing more is read (0x10020444).
int32_t CBalanceTable::Parse(const char* name, char* text, LoadMode mode) {
    if (mode == kLoadFresh) InitialiseBalanceValues(kInitReset);
    char* out = text;
    for (const char* in = text; *in != 0; ++in) {
        if (*in != '\r') *out++ = *in;
    }
    *out = 0;
    BalanceRowInfo* const rows = static_cast<BalanceRowInfo*>(mRows);
    char* cursor = text;
    while (*cursor != 0) {
        char* p = cursor;
        while (IsSpace(*p)) ++p;
        if (*p != 0 && *p != '#' && *p != '\n') {
            // The group's name: letters and digits, then '.' or '['.
            char group[65];
            std::memset(group, 0, sizeof group);
            int32_t length = 0;
            while (IsAlnum(*p) && length < 64) {
                group[length] = *p;
                ++length;
                ++p;
            }
            if (length == 0 || (*p != '.' && *p != '[')) return Report(name, group).Fail();
            int32_t index = 0;
            bool inArray = false;
            if (*p == '[') {
                if (!read_array_row(&p, &index)) return Report(name, group).Fail();
                inArray = true;
            } else {
                ++p;
            }
            char names[16][64];
            int32_t fieldCount;
            if (!read_field_names(&p, names, &fieldCount)) {
                return Report(name, group).Element(inArray, index).Fail();
            }
            for (int32_t k = 0; k < fieldCount; ++k) {
                char* field = names[k];
                int32_t row;
                if (!LocateBalanceTableRow(group, field, &row, inArray ? 1 : 0, index)) {
                    return Report(name, group).Element(inArray, index).Add("Field: ").Add(field).Add("\n").Fail();
                }
                BalanceRowInfo& info = rows[row];
                // (The original fetches the description again for every comparison.)
                if (GetBalanceDescription(row)->mType == 11) {
                    if (fieldCount != 1) {
                        return Report(name, group).Add(kMapVariableError).Add(kMapVariableAlone).Fail();
                    }
                    const uint32_t flags = GetBalanceDescription(row)->mFlags;
                    const SymbolKey* keys = nullptr;
                    int32_t keyCount = 0;
                    RideShape* shape = nullptr;
                    char* mapError = nullptr;
                    bool read = false;
                    if (flags == 1) {
                        keys = kSymbolKeysA;
                        keyCount = 19;
                    } else if (flags == 2) {
                        keys = kSymbolKeysB;
                        keyCount = 16;
                    } else if (flags == 0) {
                        GAME_ASSERTMSG(0, kNoMapType);
                    } else {
                        GAME_ASSERTMSG(0, kUnknownMapType);
                    }
                    if (keys != nullptr) read = read_map_internal(&cursor, &shape, &mapError, keys, keyCount) != 0;
                    if (!read) {
                        // (With no usable map type the original adds an unset pointer here.)
                        Report report(name, group);
                        report.Add(kMapVariableError);
                        if (mapError != nullptr) report.Add(mapError);
                        return report.Fail();
                    }
                    BalanceValue* slot = info.mValue;
                    if (inArray) {
                        if (info.mCount->mInteger < index + 1) info.mCount->mInteger = index + 1;
                        slot = info.mValue + info.mStride * index;
                    }
                    GetBalanceDescription(row);
                    if (slot->mShape != nullptr) LbMem_Free(slot->mShape);
                    slot->mShape = shape;
                } else if (GetBalanceDescription(row)->mType == 10) {
                    char* string;
                    if (!read_string(&p, &string)) {
                        return Report(name, group).Element(inArray, index).Add(kStringReadError).Fail();
                    }
                    BalanceValue* slot = info.mValue;
                    if (inArray) {
                        if (info.mCount->mInteger < index + 1) info.mCount->mInteger = index + 1;
                        slot = info.mValue + info.mStride * index;
                    }
                    GetBalanceDescription(row);
                    if (slot->mString != nullptr) LbMem_Free(slot->mString);
                    slot->mString = string;
                } else if (GetBalanceDescription(row)->mType == 7 || GetBalanceDescription(row)->mType == 8 ||
                           GetBalanceDescription(row)->mType == 9) {
                    float value;
                    if (!read_float_value(&p, &value)) {
                        return Report(name, group).Element(inArray, index).Add(kValueReadError).Fail();
                    }
                    BalanceValue* slot = info.mValue;
                    if (inArray) {
                        if (info.mCount->mInteger < index + 1) info.mCount->mInteger = index + 1;
                        slot = info.mValue + info.mStride * index;
                    }
                    if (!AssignValue(&slot->mFloat, row, value)) {
                        return Report(name, group).Element(inArray, index).Fail();
                    }
                } else {
                    int32_t value;
                    if (!read_integer_value(&p, &value)) {
                        return Report(name, group).Element(inArray, index).Add(kValueReadError).Fail();
                    }
                    BalanceValue* slot = info.mValue;
                    if (inArray) {
                        if (info.mCount->mInteger < index + 1) info.mCount->mInteger = index + 1;
                        slot = info.mValue + info.mStride * index;
                    }
                    if (!AssignValue(&slot->mInteger, row, value)) {
                        return Report(name, group).Element(inArray, index).Fail();
                    }
                }
            }
        }
        // To the end of the line the reading of this one began on.
        while (*cursor != 0 && *cursor != '\n') ++cursor;
        if (*cursor == '\n') ++cursor;
    }
    return 1;
}

// ---------------------------------------------------------------------------------------
// The table.

// Counts the rows before the one of type 12 (0x10020fd0).
int32_t CBalanceTable::CountBalanceDescriptionRows() {
    int32_t row = -1;
    do {
        ++row;
    } while (GetBalanceDescription(row)->mType != 12);
    return row;
}
