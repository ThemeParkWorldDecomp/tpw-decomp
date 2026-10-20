// tb_qsort.cpp: CQSort (engine library of SimTheme Park 2.04b3; the inventory files
// 0x1001fd38..0x1002042c under tb_resource_display.cpp). See include/tb_qsort.h for the
// scheme. Four sorts (record or pointer list, ascending or descending), each with a short
// entry point that fills the object's three words first.
#include "tb_qsort.h"

#include <cstring>

CQSort gQSort;

namespace {

// The 32-bit key at `at`, read as the processor of the original does (the records of the
// Mac are big-endian and so are its keys; a host record holds a key in its own order).
uint32_t ReadKey(const uint8_t* at) {
    uint32_t key;
    std::memcpy(&key, at, sizeof key);
    return key;
}

float ReadFloatKey(const uint8_t* at) {
    float key;
    std::memcpy(&key, at, sizeof key);
    return key;
}

// The middle index, as the original rounds it: (low + high) >> 1 on the signed sum.
int32_t Middle(int32_t low, int32_t high) { return static_cast<int32_t>(static_cast<uint32_t>(low) + static_cast<uint32_t>(high)) >> 1; }

}  // namespace

// ---- Records ---------------------------------------------------------------------------

void CQSort::ulAsc(void* data, int32_t low, int32_t high) {
    uint8_t* base = static_cast<uint8_t*>(data);
    const uint32_t pivot = ReadKey(base + mSize * Middle(low, high) + mKeyOffset);
    uint8_t* left = base + mSize * low;
    uint8_t* right = base + mSize * high;
    int32_t i = low;
    int32_t j = high;
    do {
        while (i <= high && ReadKey(left + mKeyOffset) < pivot) {
            ++i;
            left += mSize;
        }
        while (j > low && ReadKey(right + mKeyOffset) > pivot) {
            --j;
            right -= mSize;
        }
        if (i > j) break;
        // Swap the two records through the scratch buffer. (When both are the same record the
        // middle copy moves it onto itself.)
        std::memcpy(mTemp, left, static_cast<size_t>(mSize));
        std::memmove(left, right, static_cast<size_t>(mSize));
        std::memcpy(right, mTemp, static_cast<size_t>(mSize));
        ++i;
        --j;
        left += mSize;
        right -= mSize;
    } while (i <= j);
    if (low < j) ulAsc(data, low, j);
    if (high > i) ulAsc(data, i, high);
}

void CQSort::ulDes(void* data, int32_t low, int32_t high) {
    uint8_t* base = static_cast<uint8_t*>(data);
    const uint32_t pivot = ReadKey(base + mSize * Middle(low, high) + mKeyOffset);
    uint8_t* left = base + mSize * low;
    uint8_t* right = base + mSize * high;
    int32_t i = low;
    int32_t j = high;
    do {
        while (i <= high && ReadKey(left + mKeyOffset) > pivot) {
            ++i;
            left += mSize;
        }
        while (j > low && ReadKey(right + mKeyOffset) < pivot) {
            --j;
            right -= mSize;
        }
        if (i > j) break;
        std::memcpy(mTemp, left, static_cast<size_t>(mSize));
        std::memmove(left, right, static_cast<size_t>(mSize));
        std::memcpy(right, mTemp, static_cast<size_t>(mSize));
        ++i;
        --j;
        left += mSize;
        right -= mSize;
    } while (i <= j);
    if (low < j) ulDes(data, low, j);
    if (high > i) ulDes(data, i, high);
}

// ---- Arrays of pointers to records -----------------------------------------------------

void CQSort::pulAsc(void* data, int32_t low, int32_t high) {
    void** list = static_cast<void**>(data);
    const uint32_t pivot = ReadKey(static_cast<const uint8_t*>(list[Middle(low, high)]) + mKeyOffset);
    void** left = list + low;
    void** right = list + high;
    int32_t i = low;
    int32_t j = high;
    do {
        while (i <= high && ReadKey(static_cast<const uint8_t*>(*left) + mKeyOffset) < pivot) {
            ++i;
            ++left;
        }
        while (j > low && ReadKey(static_cast<const uint8_t*>(*right) + mKeyOffset) > pivot) {
            --j;
            --right;
        }
        if (i > j) break;
        ++i;
        --j;
        // The pointer in transit is kept in mTemp (the Mac overwrites the pointer to the
        // scratch buffer the entry point stored there).
        mTemp = *left;
        *left = *right;
        ++left;
        *right = mTemp;
        --right;
    } while (i <= j);
    if (low < j) pulAsc(data, low, j);
    if (high > i) pulAsc(data, i, high);
}

void CQSort::pfDes(void* data, int32_t low, int32_t high) {
    void** list = static_cast<void**>(data);
    const float pivot = ReadFloatKey(static_cast<const uint8_t*>(list[Middle(low, high)]) + mKeyOffset);
    void** left = list + low;
    void** right = list + high;
    int32_t i = low;
    int32_t j = high;
    do {
        while (i <= high && ReadFloatKey(static_cast<const uint8_t*>(*left) + mKeyOffset) > pivot) {
            ++i;
            ++left;
        }
        while (j > low && ReadFloatKey(static_cast<const uint8_t*>(*right) + mKeyOffset) < pivot) {
            --j;
            --right;
        }
        if (i > j) break;
        ++i;
        --j;
        mTemp = *left;
        *left = *right;
        ++left;
        *right = mTemp;
        --right;
    } while (i <= j);
    if (low < j) pfDes(data, low, j);
    if (high > i) pfDes(data, i, high);
}

// ---- The entry points that fill the object first ---------------------------------------

void CQSort::ulAsc(void* data, void* temp, int32_t size, int32_t keyOffset, int32_t low, int32_t high) {
    mSize = size;
    mKeyOffset = keyOffset;
    mTemp = temp;
    ulAsc(data, low, high);
}

void CQSort::ulDes(void* data, void* temp, int32_t size, int32_t keyOffset, int32_t low, int32_t high) {
    mSize = size;
    mKeyOffset = keyOffset;
    mTemp = temp;
    ulDes(data, low, high);
}

void CQSort::pulAsc(void* data, void* temp, int32_t size, int32_t keyOffset, int32_t low, int32_t high) {
    mSize = size;
    mKeyOffset = keyOffset;
    mTemp = temp;
    pulAsc(data, low, high);
}

void CQSort::pfDes(void* data, void* temp, int32_t size, int32_t keyOffset, int32_t low, int32_t high) {
    mSize = size;
    mKeyOffset = keyOffset;
    mTemp = temp;
    pfDes(data, low, high);
}
