// Sound-library arrays: Mac {data +0, size +4}. Pointer slots use native pointers on the host.
#pragma once
#include <cstdint>
#include <new>
#include <type_traits>
#include "TbDynamicStringTemplate.h"

// Four emitted CAutoArrayPtr instances at sound 0x100170d0, 0x1001faa4, 0x10021298, 0x100214a4.
template<class T> class CAutoArrayPtr {
public:
    explicit CAutoArrayPtr(T* data) : mData(data) {}
    explicit CAutoArrayPtr(uint32_t count) : mData(new T[count]) {} // sound 0x10009d30, unsigned char
    ~CAutoArrayPtr() { if (mData != nullptr) delete[] mData; }
    operator T*() const { return mData; }
    T* mData;
};

// The sound library's single-object owner (unit w79f), one instance in the library: CAutoPtr<CAudioPlaceHolderList> (constructor
// 0x10016324, destructor 0x10016270, operator-> 0x10016e18, operator T* 0x100172b4). Mac size 4: the pointer. The destructor calls
// the object's deleting destructor (vtable slot +8, flag 1) when the pointer is set, which is `delete`.
template<class T> class CAutoPtr {
public:
    explicit CAutoPtr(T* data) : mData(data) {}
    ~CAutoPtr() { delete mData; }
    CAutoPtr(const CAutoPtr&) = delete;
    CAutoPtr& operator=(const CAutoPtr&) = delete;
    T* operator->() const { return mData; }
    operator T*() const { return mData; }
    T* mData;
};

// Constructors 0x1001fd4c/1fe94/1fff4/2016c. Growth 0x100211a0/213a0/16fc8/1f954.
template<class T> class CResize {
public:
    CResize() : mData(nullptr), mSize(0) {}
    ~CResize() { Clear(); }
    T& operator[](uint32_t index) {
        if (index >= mSize) {
            CAutoArrayPtr<T> old(mData);
            mData = new T[index + 1];
            if (static_cast<T*>(old) != nullptr) {
                for (uint32_t i = 0; i < mSize; ++i) mData[i] = static_cast<T*>(old)[i];
            }
            mSize = index + 1;
        }
        return mData[index];
    }
    uint32_t Size() const { return mSize; }
    void Clear() {
        if (mData != nullptr) {
            delete[] mData;
            mData = nullptr;
            mSize = 0;
        }
    }
    bool Find(const T& value, uint32_t& index) {
        for (uint32_t i = 0; i < mSize; ++i) {
            if ((*this)[i] == value) {
                index = i;
                return true;
            }
        }
        return false;
    }
    T* mData;         // Mac +0
    uint32_t mSize;   // Mac +4
};

// 0x10023bb0 (banks), 0x10023c70 (categories): first null slot, or the current size.
template<class T> uint32_t GetNextFreeIndex(CResize<T>& array) {
    uint32_t i = 0;
    while (i < array.Size() && array[i] != nullptr) ++i;
    return i;
}
