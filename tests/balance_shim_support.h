// Native storage access for balance tests, whose pointer layout differs from the Mac.
#pragma once
#include <cstring>
#include <new>
#include "CBalanceTable.h"

template<class T> void BalanceSeed(T* object, uint32_t rows, const uint32_t* values, int32_t count) {
    object->mRows = reinterpret_cast<void*>(static_cast<uintptr_t>(rows));
    for (int32_t i = 0; i < count; ++i) object->mValues[i].mBits = values[i];
}
template<class T> void BalanceRead(T* object, uint64_t* rows, uint64_t* values, int32_t count) {
    *rows = reinterpret_cast<uintptr_t>(object->mRows);
    for (int32_t i = 0; i < count; ++i) values[i] = object->mValues[i].mBits;
}
template<class T> void* BalanceCreate(uint32_t rows, const uint32_t* values, int32_t count) {
    void* storage = ::operator new(sizeof(T));
    std::memset(storage, 0, sizeof(T));
    auto* object = new (storage) T;
    BalanceSeed(object, rows, values, count);
    return object;
}
template<class T> void* BalanceDestroy(T* object, int16_t flag, uint64_t* rows, uint64_t* values, int32_t count) {
    auto* rowAt = reinterpret_cast<unsigned char*>(&object->mRows);
    auto* valuesAt = reinterpret_cast<unsigned char*>(object->mValues);
    object->T::~T();
    uintptr_t rowBits;
    std::memcpy(&rowBits, rowAt, sizeof rowBits);
    *rows = rowBits;
    for (int32_t i = 0; i < count; ++i) {
        uintptr_t bits;
        std::memcpy(&bits, valuesAt + i * sizeof(BalanceValue), sizeof bits);
        values[i] = bits;
    }
    if (flag > 0) ::operator delete(object);
    return object;
}
