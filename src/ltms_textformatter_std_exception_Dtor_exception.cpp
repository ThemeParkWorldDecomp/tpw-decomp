// std::exception::~exception (ltms 0x10008304): the MSL exception destructor of the "ltms" library. A NULL object is left alone; else
// the vptr is stored (the vtable at TOC r2 + 0x104c) and, when the hidden delete flag (a short in r4) is positive, the object is freed
// by operator delete (the runtime's, glue 0x1000f694; here the class's static, a fake in the test). The original's exception cleanup
// (__unexpected) is not reachable. Answers this.
#include <cstddef>
#include <cstdint>
#include <new>

#include "TbSoundSampleMPEG2.h"

struct LtmsStdException {
    static void operator delete(void* object);   // the runtime's operator delete (0x1000f694)
};

// The Mac's vtable object at 0x1003bc3c (r2 0x1003abf0 + 0x104c); only its address is stored.
const void* gLtmsStdExceptionVtbl[2] = {nullptr, nullptr};

void* LtmsStdExceptionDtor(void* self, int16_t deleteFlag);   // 0x10008304 (108)

void* LtmsStdExceptionDtor(void* self, int16_t deleteFlag) {
    if (self != nullptr) {
        *static_cast<const void**>(self) = gLtmsStdExceptionVtbl;
        if (deleteFlag > 0) LtmsStdException::operator delete(self);
    }
    return self;
}
