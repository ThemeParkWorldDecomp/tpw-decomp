// tb_qsort.h: CQSort, the engine's quick sort over records and over lists of pointers to
// records (engine library; the inventory files 0x1001fd38..0x1002042c under
// tb_resource_display.cpp).
//
// The sorter is a three word object (12 bytes on the Mac; one pointer makes the host copy
// larger): the size of a record, the offset of the 32-bit key inside it and a scratch pointer.
// The entry points with six arguments fill the three words from their arguments and sort;
// the ones with three sort with whatever the words hold. There is one object in use, in the
// engine's BSS (gQSort).
//
// The sort is the classic Hoare scheme, not stable: it takes the middle element's key as the
// pivot, the scans never leave [low, high] (the first scan is bounded by high and the second
// by low, so the pivot need not exist), and both halves are sorted recursively. Records
// with equal keys end up in an order that depends on the exact scheme, so it is kept as it
// is.
//
// Two forms. ulAsc and ulDes sort the records themselves, mSize bytes each starting at
// `data`, with an unsigned key at mKeyOffset inside the record, and swap them through the
// scratch buffer mTemp. pulAsc and pfDes sort an array of POINTERS to records (a pointer is
// four bytes on the Mac, so the Mac passes mSize but never reads it): the key is read through
// each pointer at mKeyOffset, unsigned for pulAsc, a float for pfDes (descending: the
// scans compare with the pivot as `key > pivot` and `key < pivot`, so NaN keys stop both
// scans); the swap goes through mTemp itself, which holds the pointer being moved when
// the call ends.
//
// Host note: an array of pointers holds native pointers, and the keys in the records are read
// at the offset the caller gives (the host record layouts differ from the Mac's).
#pragma once
#include <cstdint>

class CQSort {
public:
    // Records, key ascending / descending (unsigned 32-bit).
    void ulAsc(void* data, int32_t low, int32_t high);       // 0x1001ffe8
    void ulDes(void* data, int32_t low, int32_t high);       // 0x10020160
    // Array of pointers to records, unsigned key ascending.
    void pulAsc(void* data, int32_t low, int32_t high);      // 0x1001fd38
    // Array of pointers to records, float key descending (not in any unit list: SortEntries
    // needs it).
    void pfDes(void* data, int32_t low, int32_t high);       // 0x1001fe90

    // Set the three words, then sort data[low..high] (both ends included).
    void ulAsc(void* data, void* temp, int32_t size, int32_t keyOffset, int32_t low, int32_t high);   // 0x10020398
    void ulDes(void* data, void* temp, int32_t size, int32_t keyOffset, int32_t low, int32_t high);   // 0x100203f8
    void pulAsc(void* data, void* temp, int32_t size, int32_t keyOffset, int32_t low, int32_t high);  // 0x100202d8
    void pfDes(void* data, void* temp, int32_t size, int32_t keyOffset, int32_t low, int32_t high);   // 0x10020338

    int32_t mSize;        // 0x00  bytes in a record (read by ulAsc and ulDes only)
    int32_t mKeyOffset;   // 0x04  where the key is in a record
    void* mTemp;          // 0x08  scratch: a record-sized buffer, or for the pointer sorts the pointer in transit
};

// The sorter the texture cache uses. Engine BSS 0x100b161c (not exported; name invented).
extern CQSort gQSort;
