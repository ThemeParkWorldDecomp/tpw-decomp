#include "bfl_FileSystem.h"
#include "bfl_WadFile.h"
#include "bfl_Strings.h"
#include "bfl_ZLib.h"
#include "platform/macdoze.h"
#include "AudioPlaceHolder.h"
#include "ppc_math.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>


namespace bfl::maths {

// 0x100295f0 (260).
int32_t LbMaths_ArcTan(int32_t x, int32_t y) {
    const double k = gLbMathsAtanScale;
    const double a = std::atan(static_cast<double>(x) / static_cast<double>(y));
    if (x < 0) {
        if (y < 0) return ppc_fctiwz(ppc_fnmsub(a, k, 2048.0));
        return ppc_fctiwz(ppc_fmadd(-a, k, 1024.0));
    }
    if (y < 0) return ppc_fctiwz(-a * k);
    return ppc_fctiwz(ppc_fnmsub(a, k, 1024.0));
}

}  // namespace bfl::maths
