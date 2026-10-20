#include "tb_decode_wavelet.h"
#include <cmath>
#include <cstring>
#include "Wavelet.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "tb_loaders.h"
#include "tb_resource_texture.h"

// tb_depthoffield.cpp static data at 0x10052b08: ten floats (100, then three (sin, cos, 0) triples).
float gDepthOfFieldTable[10];

// 0x10006864 (192 B): the file's static initialiser.
void __sinit_tb_depthoffield_cpp() {
    float* t = gDepthOfFieldTable;
    t[0] = 100.0f;
    t[1] = static_cast<float>(std::sin(0.0));
    t[2] = static_cast<float>(std::cos(0.0));
    float zero = 0.0f;
    t[3] = zero;
    t[4] = static_cast<float>(std::sin(0.0));
    t[5] = static_cast<float>(std::cos(0.0));
    t[6] = zero;
    t[7] = static_cast<float>(std::sin(3.1415927410125732));
    t[8] = static_cast<float>(std::cos(3.1415927410125732));
    t[9] = zero;
}
