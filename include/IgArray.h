// IgArray.h: the engine's growable array (engine library of SimTheme Park 2.04b3), the shape
// of MFC's CArray<TYPE, ARG_TYPE>: the elements, their count, the capacity and a growth step.
// The 2.04b3 engine has one instantiation, CIgArray<float, float> (Wavelet.cpp's decoder
// builds four on its stack); its members are out of line at the addresses below and defined
// in src/engine/Wavelet.cpp, src/engine/EngineUtil.cpp for that instantiation. The elements are trivial: the element
// construction and destruction loops of the original are empty counting loops.
#pragma once
#include <cstdint>

template <class TYPE, class ARG_TYPE>
class CIgArray {
public:
    CIgArray();                                  // 0x1003ee44 (float, float)
    ~CIgArray();                                 // 0x1003ee88
    // Resizes to `size` elements; the new ones are zeroed, growth beyond the capacity copies the
    // elements to a new block of at least `capacity + step`. A `growBy` other than -1 becomes the
    // step.
    void SetSize(int32_t size, int32_t growBy);  // 0x1003ef48
    TYPE* GetData();                             // 0x1003d1f8

    TYPE* mData;                 // 0x00  new[], NULL while empty
    int32_t mSize;               // 0x04
    int32_t mCapacity;           // 0x08
    int32_t mGrowBy;             // 0x0c  0: an eighth of the size, at least 4 and at most 1024
    // The Mac object is 0x1c bytes (the decoder's stack slots are that far apart); no member
    // touches these three words, the constructor included.
    uint32_t mF10[3];            // 0x10
};
