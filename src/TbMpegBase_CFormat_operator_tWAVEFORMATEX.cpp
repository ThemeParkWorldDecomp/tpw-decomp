#include "TbMpegBase.h"
#include <cmath>
#include <cstring>
#include <new>
#include "ppc_math.h"

// 0x10002280: operator tWAVEFORMATEX*(): fills the record, answers the address of its wave format part (this + 4).
uint8_t* CFormat_operator_tWAVEFORMATEX(CFormat* self) {
    self->FillWaveFormatEx();
    return reinterpret_cast<uint8_t*>(self) + 4;
}

// 0x10001f38: CFile::Read; impl is the access object at this+4. No access object: -1.
int32_t CFile_Read(FileAccessRead* impl, void* dest, uint32_t size) {
    if (impl == nullptr) return -1;
    return impl->Read(dest, size);
}

// 0x100015b8: TbError::TbError(long): stores the value, answers this.
int32_t* TbError_ctor(int32_t* self, int32_t value) {
    *self = value;
    return self;
}

// 0x10000b88: TbError::operator long() const.
int32_t TbError_operator_long(const int32_t* self) { return *self; }

// The runtime's operator new[] (0x1002ebfc), delete[] (0x1002ebcc) and delete (0x1002ea94). Weak so that a test library can
// replace them with recording fakes (the arena replaces the global operators for the whole program).
__attribute__((weak)) void* VoidArrayNew(size_t n) { return ::operator new[](n); }
__attribute__((weak)) void VoidArrayDelete(void* p) { ::operator delete[](p); }
__attribute__((weak)) void VoidObjectDelete(void* p) { ::operator delete(p); }

// 0x10007340: ~CAutoArrayPtr<void*>(short freeFlag): frees the array, and the object when the flag is positive.
void*** CAutoArrayPtrVoid_dtor(void*** self, int16_t freeFlag) {
    if (self != nullptr) {
        if (*self != nullptr) VoidArrayDelete(*self);
        if (freeFlag > 0) VoidObjectDelete(self);
    }
    return self;
}

// 0x100073d4: operator void**() const.
void** CAutoArrayPtrVoid_operator_ptr(void*** self) { return *self; }

// 0x100074a8: CAutoArrayPtr<void*>(unsigned count): new void*[count] (4 bytes an element on the Mac).
void*** CAutoArrayPtrVoid_ctor(void*** self, uint32_t count) {
    *self = static_cast<void**>(VoidArrayNew(static_cast<size_t>(count << 2)));
    return self;
}
