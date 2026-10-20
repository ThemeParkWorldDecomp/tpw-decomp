#include <cmath>
#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ppc_math.h"

// The clamp bounds of the angle fraction (new globals, initialised data of the Mac).
float gPylonClampMax = 1.0f;   // 0x1029cd48
float gPylonClampMin = 0.0f;   // 0x1029cd4c

namespace {

// The brace's pylon record (mData is the Mac's word +0x100, a pylon: sPylonRecord on the host).
inline sPylonRecord* PylonOf(const tag_sTrackBrace* brace) {
    return reinterpret_cast<sPylonRecord*>(const_cast<sTrackBraceData*>(brace->mData));
}

// The coaster type's control at +0xa8 (A) or +0x1c8 (B).
inline sPylonControl* ControlOf(const tag_sCoasterInstance* instance, bool second) {
    tag_sCoasterType* type = reinterpret_cast<tag_sCoasterType*>(const_cast<uint32_t*>(instance->mType));
    return second ? &type->mPylonsB : &type->mPylonsA;
}

}  // namespace

// 0x100411e4 (2108 bytes): whether a pylon may stand on the cell (x, z). With `a` (a brace) the cells b, c are the brace's own and d, e the next
// brace's; without it the editor's braces decide. The registers the original leaves uninitialised (the next brace's cell when the brace has no
// next one) are zero here.
int32_t IsValidPylonPosition(uint32_t x, uint32_t z, void* a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    tag_sCoasterInstance* edit = gCoasterEditInstance;
    int32_t valid = 1;
    tag_sTrackBrace* cand = nullptr;                          // r25
    tag_sTrackBrace* cur = static_cast<tag_sTrackBrace*>(a);  // r26
    uint32_t px = b, pz = c, qx = d, qz = e;                  // r5, r0, r11, r10

    if (a == nullptr) {
        px = pz = qx = qz = 0;
        if (edit->mBlock3C & 2) {
            if (gCoasterInputFlags & 0x100) {
                tag_sTrackBrace* mv = gPylonMoveBrace;
                tag_sTrackBrace* nx = mv->mNext;
                cand = mv;
                if (nx->mFlags & 0x10000) cur = nullptr;
                else cur = nx;
            } else {
                cur = edit->mPreviewBrace;
            }
        } else {
            if (gCoasterInputFlags & 0x100) {
                cand = gPylonMoveBrace;
                cur = cand->mNext;
            } else {
                cur = edit->mBraces;
            }
        }
        if (cur != nullptr) {
            px = PylonOf(cur)->mCellX;
            pz = PylonOf(cur)->mCellZ;
            tag_sTrackBrace* nx = cur->mNext;
            if (nx != nullptr) {
                qx = PylonOf(nx)->mCellX;
                qz = PylonOf(nx)->mCellZ;
            }
        }
    }

    sHField* field = gHeightfieldHeader->mHField;
    uint32_t width = field->mF18[0];
    if (x >= width || z >= field->mF18[1]) return 0;
    if (a == nullptr) {
        if (gHeightfieldCopy->mQuads[x + width * z].mF00 & 1) return 0;
    }

    const uint32_t in = gCoasterInputFlags;
    const bool in100 = (in & 0x100) != 0;
    tag_sTrackBrace* second = nullptr;   // r3
    tag_sTrackBrace* prev = nullptr;     // r20
    if (in100 || a != nullptr) {
        if (cur != nullptr) {
            prev = cur->mPrev;
            if (prev == nullptr) prev = edit->mLastBrace->mPrev;
            second = cur->mNext;
            if (second == nullptr) second = edit->mBraces;
        }
    } else {
        second = cur != nullptr ? cur->mNext : nullptr;
    }

    if (second != nullptr && in100) {
        uint32_t f = second->mFlags;
        if ((f & 0x10000) || (cur->mFlags & 0x10000) || (prev->mFlags & 0x10000)) {
            if ((f & 0x20000) || (cur->mFlags & 0x20000) || (prev->mFlags & 0x20000)) second = nullptr;
        }
    }

    sPylonControl* ctl;
    if (in100 || a != nullptr) {
        if (cur != nullptr) ctl = ControlOf(edit, (cur->mFlags & 0x40) != 0 || (in & 0x40) != 0);
        else ctl = nullptr;
    } else {
        ctl = ControlOf(edit, (edit->mPreviewBrace->mFlags & 0x40) != 0 || (in & 0x40) != 0);
    }

    if (cur != nullptr) {
        if (px == x && pz == z) {
            valid = 0;
        } else {
            uint32_t dx = px - x;
            uint32_t dz = pz - z;
            uint32_t d2 = dx * dx + dz * dz;
            bool bad = false;
            if (d2 < static_cast<uint32_t>(ctl->mF9C) || d2 > static_cast<uint32_t>(ctl->mFA0)) {
                if (cur->mNumber == 2 || (PylonOf(cur)->mFlags & 2) == 0) bad = true;
            }
            if (bad) {
                valid = 0;
            } else if (second != nullptr) {
                float angle = std::fabs(XZAngleBetweenPylons(x, z, px, pz, qx, qz));
                float root = static_cast<float>(std::sqrt(static_cast<double>(static_cast<float>(d2))));
                float t = (root - ctl->mFA4) / (ctl->mFA8 - ctl->mFA4);
                // bge / ble after fcmpo are "not less" / "not greater": taken when unordered, so a NaN passes through.
                t = (gPylonClampMax < t) ? gPylonClampMax : t;
                t = (t < gPylonClampMin) ? gPylonClampMin : t;
                const float inv = 1.0f - t;
                float lo, hi;
                const float* ang = ctl->mAngles;
                if (PylonOf(cur)->mFlags & 2) {
                    lo = ppc_fmadds(ang[0], inv, ang[2] * t);
                    hi = ppc_fmadds(ang[1], inv, ang[3] * t);
                } else {
                    lo = ppc_fmadds(ang[4], inv, ang[6] * t);
                    hi = ppc_fmadds(ang[5], inv, ang[7] * t);
                }
                if (angle < lo || angle > hi) valid = 0;
            }
        }
    }

    if (in100 || a != nullptr) {
        if (cur != nullptr) ctl = reinterpret_cast<sPylonControl*>(PylonOf(prev)->mControl);
        else ctl = PylonOf(gPylonMoveBrace)->mControl;
    } else {
        ctl = ControlOf(edit, (in & 0x40) != 0);
    }

    if (valid != 0) {
        const uint32_t cell = x + z * width;
        sPylonRecord* found = nullptr;
        for (tag_sCoasterInstance* inst = gCoasterInstances; inst != nullptr && found == nullptr; inst = inst->mNext) {
            sPylonRecord* node = reinterpret_cast<sPylonRecord*>(inst->mPylons);
            while (node != nullptr && ((node->mFlags & 5) != 0 || node->mIndex != cell)) node = node->mNext;
            found = node;
        }
        if (in100) {
            if (found != nullptr && a == nullptr) {
                tag_sTrackBrace* mv = gPylonMoveBrace;
                if (reinterpret_cast<tag_sCoasterInstance*>(found->mInstance) != edit) {
                    valid = 0;
                } else if (found->mControl != ctl) {
                    valid = 0;
                } else if (mv != nullptr && PylonOf(mv) == found && found->mCount > static_cast<uint32_t>(found->mControl->mFCC)) {
                    valid = 0;
                } else if ((mv == nullptr || PylonOf(mv) != found) && found->mCount >= static_cast<uint32_t>(found->mControl->mFCC)) {
                    valid = 0;
                } else if (found->mBraces[0]->mFlags & 0x600) {
                    valid = 0;
                }
            }
        } else if (found != nullptr && a == nullptr) {
            if (reinterpret_cast<tag_sCoasterInstance*>(found->mInstance) != edit) {
                valid = 0;
            } else if (edit->mBlock3C & 2) {
                tag_sTrackBrace* br = edit->mBrace64;
                if (PylonOf(br)->mIndex == cell) {
                    if (gCoasterWord1790 == 1) valid = 0;
                    cand = br;
                } else if (found->mControl != ctl) {
                    valid = 0;
                } else if (found->mCount >= static_cast<uint32_t>(found->mControl->mFCC)) {
                    valid = 0;
                } else if (found->mBraces[0]->mFlags & 0x600) {
                    valid = 0;
                }
            } else {
                if ((edit->mBlock3C & 1) == 0) {
                    tag_sTrackBrace* br = edit->mLastBrace;
                    if (cell == PylonOf(br)->mIndex) cand = br;
                }
                if (found->mControl != ctl) {
                    valid = 0;
                } else if (found->mCount >= static_cast<uint32_t>(found->mControl->mFCC)) {
                    valid = 0;
                } else if (found->mBraces[0]->mFlags & 0x200) {
                    valid = 0;
                }
            }
        }
    }

    if (cand != nullptr && cand->mPrev != nullptr && (cand->mFlags & 0x10000) == 0) {
        uint32_t nx, nz;
        if (cand->mNext != nullptr) {
            nx = PylonOf(cand->mNext)->mCellX;
            nz = PylonOf(cand->mNext)->mCellZ;
        } else {
            sPylonRecord* p = PylonOf(edit->mBraces);
            nx = p->mCellX;
            nz = p->mCellZ;
        }
        sPylonRecord* pp = PylonOf(cand->mPrev);
        const uint32_t cx = pp->mCellX;
        const uint32_t cz = pp->mCellZ;
        int32_t ok = 0;
        if (valid != 0) {
            if (IsValidPylonPosition(cx, cz, cand, x, z, nx, nz) != 0) ok = 1;
        }
        valid = ok & 0xff;

        tag_sTrackBrace* far;
        if ((gCoasterInputFlags & 0x100) != 0 && (edit->mBlock3C & 1) != 0) {
            far = cand->mPrev->mPrev;
            if (far == nullptr) far = edit->mLastBrace->mPrev;
        } else {
            far = cand->mPrev->mPrev;
        }
        if (far != nullptr) {
            tag_sTrackBrace* near = cand->mPrev;
            if ((near->mFlags & 0x10000) == 0) {
                sPylonRecord* fp = PylonOf(far);
                ok = 0;
                if (valid != 0) {
                    if (IsValidPylonPosition(fp->mCellX, fp->mCellZ, near, cx, cz, x, z) != 0) ok = 1;
                }
                valid = ok & 0xff;
            }
        }
    }
    return valid;
}
