// Test library for src/CBalanceTable.cpp (libtpw_unit_BalanceTable.dylib; see
// tests/test_BalanceTable.py and calllog.h): the balance table reader with the real table
// classes (src/*Balance.cpp) and one synthetic table the test describes row by row.
// Everything the reader calls outside itself is a fake that records: the report function,
// the library's heap (LbMem_Alloc/LbMem_Free, blocks filled with 0xa5), the file calls, the
// exit error and an open file. The virtual calls of a synthetic table are recorded too.
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "CBalanceTable.h"
#include "Debug.h"
#include "ExitErr.h"
#include "HoardingsBalance.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "OnlineBalance.h"
#include "OptionsBalance.h"
#include "ResolutionBalance.h"
#include "SoundBalance.h"
#include "bfl_FileSystem.h"
#include "calllog.h"

using namespace calllog;

namespace {

// The bytes the fake file calls serve (set by the test).
std::vector<char> gFileText;

// A table the test describes: any rows, any number of value slots.
class CSyntheticBalance : public CBalanceTable {
public:
    CSyntheticBalance(const BalanceDescription* rows, int32_t rowCount, int32_t slotCount, const BalanceDescription* beyond)
        : mDescriptions(rows, rows + rowCount), mValues(static_cast<size_t>(slotCount)), mHasBeyond(beyond != nullptr) {
        std::memset(mValues.data(), 0xa5, mValues.size() * sizeof(BalanceValue));
        if (beyond != nullptr) mBeyond = *beyond;
    }
    // A row outside the table answers with the last one (the terminator), as the PowerPC
    // side's fake does, so a runaway scan ends instead of reading past the rows; a table
    // can have a row of its own for what lies past its end.
    const BalanceDescription* GetBalanceDescription(int32_t row) override {
        ev("GetBalanceDescription", {row});
        const int32_t last = static_cast<int32_t>(mDescriptions.size()) - 1;
        if (row > last && mHasBeyond) return &mBeyond;
        return &mDescriptions[row < 0 || row > last ? last : row];
    }
    BalanceValue* GetBalanceValuesStart() override {
        ev("GetBalanceValuesStart");
        return mValues.data();
    }

    std::vector<BalanceDescription> mDescriptions;
    std::vector<BalanceValue> mValues;
    BalanceDescription mBeyond{};
    bool mHasBeyond;
};

// The open file the Load overload with a file reads: 8 bytes on the Mac, a vtable pointer
// and the implementation.
class FakeFile : public bfl::FileSystem::CFile {
public:
    int32_t Read(void* dest, uint32_t size) override {
        ev("CFile::Read", {lab(this), size});
        const size_t count = gFileText.size() < size ? gFileText.size() : size;
        if (count != 0) std::memcpy(dest, gFileText.data(), count);
        return static_cast<int32_t>(ret("CFile::Read", size));
    }
    int32_t GetSize() override {
        ev("CFile::GetSize", {lab(this)});
        return static_cast<int32_t>(ret("CFile::GetSize", 0));
    }
};

FakeFile gFile;

// The table's value slots, asked without the virtual call being recorded.
BalanceValue* values_of(CBalanceTable* table) {
    const int32_t was = tpw_cl_on;
    tpw_cl_on = 0;
    BalanceValue* values = table->GetBalanceValuesStart();
    tpw_cl_on = was;
    return values;
}

// The table classes' sizes (their value slots follow the vtable pointer and mRows).
size_t SizeOfTable(int32_t kind) {
    switch (kind) {
    case 0: return sizeof(CHoardingsBalance);
    case 1: return sizeof(CMainBalance);
    case 2: return sizeof(COnlineBalance);
    case 3: return sizeof(COptionsBalance);
    case 4: return sizeof(CSoundBalance);
    default: return sizeof(CResolutionBalance);
    }
}

}  // namespace

// ---- fakes of the reader's callees

// The original reports nothing; the fake records the text.
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {condition}, {format});
}

// An allocation the test can refuse (the queue "LbMem_Alloc": 0 refuses, the default accepts).
void* LbMem_Alloc(uint32_t size) {
    if (ret("LbMem_Alloc", 1) == 0) {
        ev("LbMem_Alloc", {size, 0});
        return nullptr;
    }
    void* block = std::malloc(size != 0 ? size : 1);
    std::memset(block, 0xa5, size);
    ev("LbMem_Alloc", {size, newlab(block)});
    return block;
}
void LbMem_Free(void* block) {
    ev("LbMem_Free", {lab(block)});
    forget(block);
    std::free(block);
}

int32_t LbFile_GetLength(const char* name) {
    ev("LbFile_GetLength", {}, {name});
    return static_cast<int32_t>(ret("LbFile_GetLength", -1));
}
// Serves the test's bytes (as many as fit) and answers with the queued TbError.
TbError LbFile_Load(const char* name, void* buffer, uint32_t capacity, int32_t* length) {
    ev("LbFile_Load", {capacity}, {name});
    const size_t count = gFileText.size() < capacity ? gFileText.size() : capacity;
    if (count != 0) std::memcpy(buffer, gFileText.data(), count);
    *length = static_cast<int32_t>(count);
    return TbError(static_cast<int32_t>(ret("LbFile_Load", 0)));
}

CExitError gExitError;
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) {
    ev("CExitError::SetError", {lab(this), static_cast<int64_t>(type), value}, {text});
}

// The bfl file class's members the vtable needs (only Read and GetSize are ever called).
bfl::FileSystem::CFile::CFile() : mImplementation(nullptr) {}
bfl::FileSystem::CFile::~CFile() {}
void bfl::FileSystem::CFile::Close() {}
int32_t bfl::FileSystem::CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t bfl::FileSystem::CFile::GetSize() { return -1; }
int32_t bfl::FileSystem::CFile::GetPos() { return -1; }
void* bfl::FileSystem::CFile::MapFile() { return nullptr; }
TbError bfl::FileSystem::CFile::Flush() { return TbError(-1); }
int32_t bfl::FileSystem::CFile::GetMacFileReference() { return -1; }

// ---- the test entry points

extern "C" {

// Makes a table: kinds 0 to 5 are CHoardingsBalance, CMainBalance, COnlineBalance,
// COptionsBalance, CSoundBalance and CResolutionBalance, 6 a synthetic table over `rowCount`
// description rows (60 bytes each, little endian) and `slotCount` value slots (and `beyond`,
// when not null, the row every row number past the last one answers with). Whatever the
// constructors did is undone: the object is left as raw memory (0xa5) after its vtable
// pointer, as the original's static memory is before Init.
void* tpw_bt_make(int32_t kind, const BalanceDescription* rows, int32_t rowCount, int32_t slotCount,
                  const BalanceDescription* beyond) {
    if (kind == 6) {
        CBalanceTable* table = new CSyntheticBalance(rows, rowCount, slotCount, beyond);
        std::memset(&table->mRows, 0xa5, sizeof table->mRows);
        return table;
    }
    const size_t size = SizeOfTable(kind);
    unsigned char* storage = static_cast<unsigned char*>(::operator new(size));
    std::memset(storage, 0, size);
    CBalanceTable* table = nullptr;
    switch (kind) {
    case 0: table = new (storage) CHoardingsBalance; break;
    case 1: table = new (storage) CMainBalance; break;
    case 2: table = new (storage) COnlineBalance; break;
    case 3: table = new (storage) COptionsBalance; break;
    case 4: table = new (storage) CSoundBalance; break;
    default: table = new (storage) CResolutionBalance; break;
    }
    table->UnInit();
    std::memset(storage + sizeof(void*), 0xa5, size - sizeof(void*));
    return table;
}

// Gives the made table back (a synthetic table is deleted, the others are plain storage).
void tpw_bt_destroy(void* object) {
    CBalanceTable* table = static_cast<CBalanceTable*>(object);
    if (CSyntheticBalance* synthetic = dynamic_cast<CSyntheticBalance*>(table)) {
        delete synthetic;
    } else {
        ::operator delete(object);
    }
}

// The row information and the value slots, as the test reads them. rows: three numbers per
// description row: the slot a row's value is in (-1 for none), its stride and the slot of its
// count (-1 for none); -2 for a row that Init never wrote, -3 for all of them when there is no
// row information at all. slots: every slot's eight bytes.
void tpw_bt_dump(void* object, int32_t rowCount, int32_t slotCount, int64_t* rows, uint64_t* slots) {
    CBalanceTable* table = static_cast<CBalanceTable*>(object);
    BalanceValue* values = values_of(table);
    for (int32_t i = 0; i < slotCount; ++i) slots[i] = values[i].mBits;
    BalanceRowInfo* info = static_cast<BalanceRowInfo*>(table->mRows);
    for (int32_t i = 0; i < rowCount; ++i) {
        int64_t* out = rows + 3 * i;
        if (info == nullptr || reinterpret_cast<uintptr_t>(info) == 0xa5a5a5a5a5a5a5a5ull) {
            out[0] = out[1] = out[2] = -3;
            continue;
        }
        unsigned char bytes[sizeof(BalanceRowInfo)];
        std::memcpy(bytes, &info[i], sizeof bytes);
        bool unset = true;
        for (unsigned char byte : bytes) unset = unset && byte == 0xa5;
        if (unset) {
            out[0] = out[1] = out[2] = -2;
            continue;
        }
        out[0] = info[i].mValue != nullptr ? info[i].mValue - values : -1;
        out[1] = info[i].mStride;
        out[2] = info[i].mCount != nullptr ? info[i].mCount - values : -1;
    }
}

// The size Init's allocation of the row information has on the host, per description row.
int32_t tpw_bt_row_info_size() { return static_cast<int32_t>(sizeof(BalanceRowInfo)); }

// Puts something in a slot: kind 0 a 32-bit integer (`value`), 1 a string of `text` (a block
// of the fake heap, so that freeing it is recorded), 2 a map block (0xc88 bytes of `value`).
void tpw_bt_seed(void* object, int32_t slot, int32_t kind, const char* text, int32_t value) {
    CBalanceTable* table = static_cast<CBalanceTable*>(object);
    BalanceValue* values = values_of(table);
    if (kind == 0) {
        values[slot].mInteger = value;
    } else if (kind == 1) {
        const size_t length = std::strlen(text) + 1;
        char* block = static_cast<char*>(std::malloc(length));
        std::memcpy(block, text, length);
        newlab(block);
        values[slot].mString = block;
    } else {
        void* block = std::malloc(sizeof(RideShape));
        std::memset(block, value, sizeof(RideShape));
        newlab(block);
        values[slot].mShape = block;
    }
}

void* tpw_bt_exit_error() { return &gExitError; }
void tpw_bt_set_file_text(const char* text, int32_t length) {
    gFileText.assign(text, text + length);
}
void* tpw_bt_file() { return &gFile; }

// The table's members.
void tpw_bt_init(void* object) { static_cast<CBalanceTable*>(object)->Init(); }
void tpw_bt_uninit(void* object) { static_cast<CBalanceTable*>(object)->UnInit(); }
void tpw_bt_free_strings(void* object) { static_cast<CBalanceTable*>(object)->FreeStrings(); }
void tpw_bt_initialise(void* object, int32_t type) {
    static_cast<CBalanceTable*>(object)->InitialiseBalanceValues(static_cast<CBalanceTable::InitType>(type));
}
int32_t tpw_bt_count(void* object) { return static_cast<CBalanceTable*>(object)->CountBalanceDescriptionRows(); }
int32_t tpw_bt_locate(void* object, const char* group, const char* field, int32_t* row, int32_t inArray, int32_t index) {
    char groupCopy[80];
    char fieldCopy[80];
    std::strcpy(groupCopy, group);
    std::strcpy(fieldCopy, field);
    return static_cast<CBalanceTable*>(object)->LocateBalanceTableRow(groupCopy, fieldCopy, row, inArray, index);
}
int32_t tpw_bt_assign_int(void* object, int32_t slot, int32_t row, int32_t value) {
    CBalanceTable* table = static_cast<CBalanceTable*>(object);
    int32_t* dest = &values_of(table)[slot].mInteger;
    return table->AssignValue(dest, row, value);
}
int32_t tpw_bt_assign_float(void* object, int32_t slot, int32_t row, uint32_t bits) {
    CBalanceTable* table = static_cast<CBalanceTable*>(object);
    float value;
    std::memcpy(&value, &bits, sizeof value);
    float* dest = &values_of(table)[slot].mFloat;
    return table->AssignValue(dest, row, value);
}
int32_t tpw_bt_parse(void* object, const char* name, char* text, int32_t mode) {
    return static_cast<CBalanceTable*>(object)->Parse(name, text, static_cast<CBalanceTable::LoadMode>(mode));
}
int32_t tpw_bt_load_path(void* object, const char* path, int32_t mode) {
    return static_cast<CBalanceTable*>(object)->Load(path, static_cast<CBalanceTable::LoadMode>(mode));
}
int32_t tpw_bt_load_file(void* object, const char* name, int32_t mode) {
    return static_cast<CBalanceTable*>(object)->Load(name, gFile, static_cast<CBalanceTable::LoadMode>(mode));
}

// The line readers, one text and a cursor offset in; the cursor's offset comes back in
// `cursorOut` whatever the result.
int32_t tpw_bt_read_array_row(char* text, int32_t start, int32_t* index, int32_t* cursorOut) {
    char* cursor = text + start;
    const int32_t ok = read_array_row(&cursor, index);
    *cursorOut = static_cast<int32_t>(cursor - text);
    return ok;
}
int32_t tpw_bt_read_field_names(char* text, int32_t start, char (*names)[64], int32_t* count, int32_t* cursorOut) {
    char* cursor = text + start;
    const int32_t ok = read_field_names(&cursor, names, count);
    *cursorOut = static_cast<int32_t>(cursor - text);
    return ok;
}
int32_t tpw_bt_read_integer_value(char* text, int32_t start, int32_t* value, int32_t* cursorOut) {
    char* cursor = text + start;
    const int32_t ok = read_integer_value(&cursor, value);
    *cursorOut = static_cast<int32_t>(cursor - text);
    return ok;
}
int32_t tpw_bt_read_float_value(char* text, int32_t start, float* value, int32_t* cursorOut) {
    char* cursor = text + start;
    const int32_t ok = read_float_value(&cursor, value);
    *cursorOut = static_cast<int32_t>(cursor - text);
    return ok;
}
// The string comes back as a pointer to the fake heap's block (null when none).
int32_t tpw_bt_read_string(char* text, int32_t start, char** value, int32_t* cursorOut) {
    char* cursor = text + start;
    const int32_t ok = read_string(&cursor, value);
    *cursorOut = static_cast<int32_t>(cursor - text);
    return ok;
}
int32_t tpw_bt_recognise_delimit(char* text, int32_t start, int32_t* cursorOut) {
    char* cursor = text + start;
    const int32_t ok = recognise_delimit(&cursor);
    *cursorOut = static_cast<int32_t>(cursor - text);
    return ok;
}
// keySet 1 or 2 picks the symbol table the way a description row's flags do (the test's own
// copy of the two tables, which the original keeps at 0x1028e730 and 0x1028e814).
int32_t tpw_bt_read_map(char* text, int32_t start, int32_t keySet, RideShape** shape, char* error, int32_t* cursorOut) {
    static const SymbolKey keysA[19] = {
        {'.', 0, 0}, {'*', 4, 0}, {'Q', 3, 0}, {'@', 1, 0}, {'+', 11, 0}, {'#', 16, 0}, {'8', 9, 1},
        {'6', 9, 4}, {'2', 9, 16}, {'4', 9, 64}, {'N', 10, 1}, {'E', 10, 4}, {'S', 10, 16}, {'W', 10, 64},
        {'D', 23, 0}, {'>', 23, 4}, {'<', 23, 64}, {'O', 9, 1}, {'X', 10, 1},
    };
    static const SymbolKey keysB[16] = {
        {'.', 0, 0}, {'^', 0, 1}, {'_', 0, 16}, {'[', 0, 64}, {']', 0, 4}, {'J', 0, 20}, {'F', 0, 65}, {'7', 0, 5},
        {'L', 0, 80}, {'=', 0, 17}, {'H', 0, 68}, {'C', 0, 81}, {'U', 0, 84}, {'n', 0, 69}, {'3', 0, 21}, {'O', 0, 85},
    };
    char* cursor = text + start;
    char* message = nullptr;
    *shape = nullptr;
    const int32_t ok = keySet == 1 ? read_map_internal(&cursor, shape, &message, keysA, 19)
                                   : read_map_internal(&cursor, shape, &message, keysB, 16);
    std::strcpy(error, message != nullptr ? message : "");
    *cursorOut = static_cast<int32_t>(cursor - text);
    return ok;
}

}  // extern "C"
