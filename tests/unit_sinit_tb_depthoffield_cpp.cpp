// Test library for the tb_depthoffield.cpp static initialiser (unit v5705), main app:
//   src/engine/tb_depthoffield.cpp defines the function and the ten-float table.
// sin and cos are libm on both sides (no fakes).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

extern float gDepthOfFieldTable[10];
void __sinit_tb_depthoffield_cpp();

extern "C" {

uint64_t tpw_dof_addr(const char* name) {
    if (std::strcmp(name, "table") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gDepthOfFieldTable));
    return 0;
}

uint64_t tpw_dof_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_dof_layout(int64_t* out) {
    out[0] = static_cast<int64_t>(sizeof(gDepthOfFieldTable));
    return 1;
}

void tpw_dof_run() { __sinit_tb_depthoffield_cpp(); }

}  // extern "C"
