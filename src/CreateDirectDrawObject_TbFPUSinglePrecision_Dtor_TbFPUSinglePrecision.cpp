#include "WinScrn.h"
#include "TbFPUPrecision.h"
#include <cstddef>
#include <cstring>
#include <new>

// 0x1006ab38: stores its vtable, then the base vtable, runs TbFPUFlag::Reset (the base
// destructor body), and deletes the object when the hidden flag is positive (the
// compiler's deleting destructor).
TbFPUSinglePrecision::~TbFPUSinglePrecision() {}
