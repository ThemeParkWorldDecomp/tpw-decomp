// CBalanceTable: main app, HoardingsBalance.cpp, 0x1001ea28..0x10021074 (the balance table
// reader, "BalanceLoader.cpp" in the original's messages).
// The Mac vptr is at +4, after mRows at +0. Native C++ supplies its own vptr/layout.
// The two virtual slots are GetBalanceDescription and GetBalanceValuesStart;
// destruction is NONVIRTUAL. The reader (Init, UnInit, Load, Parse and their helpers) is
// translated in src/CBalanceTable.cpp.
#pragma once
#include <cstddef>
#include <cstdint>

namespace bfl {
namespace FileSystem {
class CFile;
}
}  // namespace bfl

// Description row types. 0 blank, 1 group heading (a heading FOLLOWS its members), 2 start
// of an array group, 3 end of an array group (its name is the group's, its mArrayCount the
// capacity), 4 integer, 5 integer >= 0, 6 integer in [min, max), 7 float, 8 float >= 0,
// 9 float in [min, max), 10 string, 11 map (a RideShape, mFlags 1 or 2 picks the symbol
// table), 12 ends the table.
struct BalanceDescription {
    int32_t mType;             // 0x00, 12 terminates the description table
    char mName[32];            // 0x04
    int32_t mMinInteger;       // 0x24
    int32_t mMaxInteger;       // 0x28
    float mMinFloat;           // 0x2c
    float mMaxFloat;           // 0x30
    int32_t mArrayCount;       // 0x34
    uint32_t mFlags;           // 0x38
};
static_assert(sizeof(BalanceDescription) == 0x3c, "balance description stride");

// A balance value may be an integer, float, string or shape pointer. Original slots
// are four bytes; host slots must accommodate a native pointer.
union BalanceValue {
    int32_t mInteger;
    float mFloat;
    char* mString;
    void* mShape;
    uintptr_t mBits;
};

// What Init builds for each description row (12 bytes on the Mac, three words; the host's
// holds native pointers). A value row points at its first slot, at the number of slots between
// the elements of its array (0 outside arrays) and at the slot that counts the array's
// elements (null outside arrays); other rows are all zero.
struct BalanceRowInfo {
    BalanceValue* mValue;      // 0x00
    int32_t mStride;           // 0x04
    BalanceValue* mCount;      // 0x08
};

// A map value (type 11): up to 20 x 20 cells, stored bottom row first (the text's last line is
// row 0). 0xc88 bytes on the Mac and the host alike (no pointers inside).
struct RideShapeCell {
    int32_t mValue;            // 0x00
    uint8_t mFlag;             // 0x04, then three zero bytes
    uint8_t mPad[3];
};
struct RideShape {
    int32_t mWidth;            // 0x00, the longest row
    int32_t mHeight;           // 0x04, the number of rows
    RideShapeCell mCells[20][20];   // 0x08
};
static_assert(sizeof(RideShapeCell) == 8, "ride shape cell");
static_assert(sizeof(RideShape) == 0xc88, "ride shape");

// One map character: what a cell gets when the map text shows `mSymbol`. 12 bytes. The two
// tables of src/CBalanceTable.cpp are the ones the original keeps at 0x1028e730 (19 keys,
// description flags 1) and 0x1028e814 (16 keys, flags 2).
struct SymbolKey {
    char mSymbol;              // 0x00
    int32_t mValue;            // 0x04
    uint8_t mFlag;             // 0x08
};
static_assert(sizeof(SymbolKey) == 12, "symbol key");

class CBalanceTable {
public:
    virtual const BalanceDescription* GetBalanceDescription(int32_t row) = 0;
    virtual BalanceValue* GetBalanceValuesStart() = 0;

    // How a file is read into the table (the argument every caller passes as a constant).
    // kLoadFresh sets every value to its default first and reports a missing file as an
    // error (the first file of a table); kLoadOverlay keeps the values already there and
    // ignores a missing file (a second file that only overrides some values).
    enum LoadMode : int32_t { kLoadFresh = 0, kLoadOverlay = 1 };
    // InitialiseBalanceValues: kInitReset (0) releases the strings and maps first, kInitConstruct
    // (1) only clears the pointers (memory that was never initialised).
    enum InitType : int32_t { kInitReset = 0, kInitConstruct = 1 };

    // Builds the row information from the description table and clears the values (0x1001ea28).
    void Init();
    // Reads the table's .sam file (0x10020134); 1 on success.
    int32_t Load(const char* path, LoadMode mode);
    // Releases the strings, then the row information (0x1001ed40).
    void UnInit();
    void FreeStrings();                                  // 0x1001edb8
    // The same as Load, from an open file (0x100202c0).
    int32_t Load(const char* name, bfl::FileSystem::CFile& file, LoadMode mode);
    // Parses a file's text in place (carriage returns removed); 1 on success, 0 after a
    // message to GAME_ASSERTMSG (0x10020444). `name` only appears in the messages.
    int32_t Parse(const char* name, char* text, LoadMode mode);
    // The row of the member `field` of the group `group`, scanning from the end of the
    // table; `inArray` is set when the line named an element, `index` (0x1001f8f8).
    int32_t LocateBalanceTableRow(char* group, char* field, int32_t* row, int32_t inArray, int32_t index);
    // Store a value if it is in the row's range (0x1001fb5c and 0x1001fc68); 0 when it is not.
    int32_t AssignValue(int32_t* dest, int32_t row, int32_t value);
    int32_t AssignValue(float* dest, int32_t row, float value);
    void InitialiseBalanceValues(InitType type);         // 0x1001fd7c
    int32_t CountBalanceDescriptionRows();               // 0x10020fd0

    void* mRows;               // 0x00, a BalanceRowInfo per description row (owned by Init)
};

// The reader's helpers (BalanceLoader.cpp in the original; plain functions). Each takes the
// address of a cursor into the file's text and moves it past what it read; 1 on success.
int32_t read_array_row(char** cursor, int32_t* index);                       // 0x1001ee20
int32_t read_field_names(char** cursor, char (*names)[64], int32_t* count);  // 0x1001ef10
int32_t read_integer_value(char** cursor, int32_t* value);                   // 0x1001f060
int32_t read_float_value(char** cursor, float* value);                       // 0x1001f178
int32_t read_string(char** cursor, char** value);                            // 0x1001f2a4
int32_t recognise_delimit(char** cursor);                                    // 0x1001f414
// A map between two lines starting with "---"; `error` gets the reason on failure.
int32_t read_map_internal(char** cursor, RideShape** shape, char** error, const SymbolKey* keys,
                          int32_t keyCount);                                 // 0x1001f4c0
