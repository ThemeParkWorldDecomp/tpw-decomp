// TPCamera.cpp (main app of SimTheme Park 2.04b3): HandleCamera (0x1003491c, 5,644 bytes), unit w135: the park camera's frame.
//
// Every float is a single and every operation is done in single precision in the listing's order; a fused multiply-add of the listing is
// one of ppc_fmadds / ppc_fmsubs / ppc_fnmsubs below (27 in the listing: 23 fmadds, 1 fmsubs, 3 fnmsubs; the source has the same 27 calls).
// The constants are written as their bit patterns from the listing's constant pool. The
// C library's sin, cos, sqrt and fmod are the host's. The callees GetState, DetermineHFieldHeight, GetNextGridsquare, RideRideIfRequired,
// MatrixDifferenceTiny and ApplyHeadsOnRidesBumpage are not translated (declared in TPCamera.h and keyboard.h).
#include <cmath>
#include <cstring>

#include "Clock.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "keyboard.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "ui_main.h"

// Globals (BSS 0x102b59f8..0x102b5bf0 and initialised data 0x1029cc60..0x1029ccb8; names invented).
int8_t gCameraWalkOnce;
float gCameraF59FC;
int8_t gCameraWideZoomMaxOnce;
float gCameraWideZoomMax;
int8_t gCameraWideZoomOnce;
float gCameraWideZoomMin;
float gCameraGroundHeight;
sMatrix* gFirstPersonRideMatrix;
float gCameraTilt[2];
float gCameraPitch;
float gCameraPointerAnchor[2];
float gFirstPersonTurn;
float gFirstPersonPitch;
float gCameraYaw;
float gCameraYawSnap = 5.0000002374872565e-4f;       // 0x3a03126f
float gCameraStopSpeed = 9.99999974737875e-5f;       // 0x38d1b717
float gCameraYawRateIso = 200.0f;
float gCameraYawRate = 140.0f;
float gCameraPitchMin = 0.0f;
float gCameraPitchMax = __builtin_bit_cast(float, 0x3fbc430eu);
float gCameraTiltMinX = 0.0f;
float gCameraTiltMaxX = 0.0f;
float gCameraTiltMinZ = 0.0f;
float gCameraTiltMaxZ = 0.0f;

namespace {

constexpr float Bits(uint32_t b) { return __builtin_bit_cast(float, b); }

// The constant pool (0x10264608..0x102646dc), by bit pattern.
constexpr float kHalfPi = Bits(0x3fc90fdbu);       // 0x102646d0
constexpr float kQuarterPi = Bits(0x3f490fdbu);    // 0x10264680, and its negative 0xbf490fdb
constexpr float kK0012 = Bits(0x3a9d4952u);        // 0.0012
constexpr float kK0002 = Bits(0x3b03126fu);        // 0.002
constexpr float kK005 = Bits(0x3d4ccccdu);         // 0.05
constexpr float kK001 = Bits(0x3c23d70au);         // 0.01
constexpr float kK002 = Bits(0x3ca3d70au);         // 0.02
constexpr float kK0001 = Bits(0x3a83126fu);        // 0.001
constexpr float kOneAndAHalfPitch = Bits(0x3f91361eu);   // 1.134464
constexpr double kTwoPiD = __builtin_bit_cast(double, 0x401921fb60000000ull);   // 0x10264600
constexpr double kHalfPiD = __builtin_bit_cast(double, 0x3ff921fb60000000ull);  // 0x10264688

inline s3DPoint* AsPoint(float* p) { return reinterpret_cast<s3DPoint*>(p); }

inline bool Key(int16_t id) { return gKeyTableCamera->GetState(id) != 0; }

// The exact conversion of the listing: a 32-bit unsigned value through the 2^52 trick, rounded to single by the fsubs.
inline float FromU32(uint32_t v) { return static_cast<float>(v); }

}  // namespace

void HandleCamera() {
    const uint32_t isometric = static_cast<uint8_t>(gPlayerSettings.mCurrent.mIsometricOn);
    if (HandleExternalCamera()) return;

    const uint32_t now = static_cast<uint32_t>(gClock.ReadFixed());
    float dt;
    if (gCameraF5AAC != 0) {
        if (isometric != 0) {
            // The first frame in the isometric view: the key yaw is made a multiple of a quarter turn.
            const float y = static_cast<float>(std::fmod(static_cast<double>(gCameraF5BF4), kHalfPiD));
            if (!(y < 0.0f)) {
                if (y > kQuarterPi) gCameraF5BF4 = gCameraF5BF4 + (kHalfPi - y);
                else gCameraF5BF4 = gCameraF5BF4 - y;
            } else if (!(y < -kQuarterPi)) {
                gCameraF5BF4 = gCameraF5BF4 - y;
            } else {
                gCameraF5BF4 = gCameraF5BF4 - (kHalfPi + y);
            }
        }
        dt = 0.0f;
        gCameraF5AAC = 0;
    } else {
        dt = FromU32(now + 1u) - FromU32(gCameraClockStamp);
    }
    if (dt > 200.0f) dt = 100.0f;
    if (dt <= 0.0f) dt = 1.0f;

    // A button that went down since the last frame remembers where the pointer was.
    if (gCameraModeB != gCameraModeA && (gCameraModeB & 4) == 0 && (gCameraModeA & 4) != 0) {
        std::memcpy(gCameraPointerAnchor, gPointerScreenXY, sizeof gCameraPointerAnchor);
    }
    gCameraModeA &= ~0x78u;
    if (Key(6) || Key(0x12)) gCameraModeA |= 8;
    if (Key(7) || Key(0x13)) gCameraModeA |= 0x10;
    if (Key(0) || Key(0x15)) gCameraModeA |= 0x20;
    if (Key(1) || Key(0x16)) gCameraModeA |= 0x40;

    float f30 = 0.0f;
    float f31 = 0.0f;   // a first-person view's key pitch
    if ((gFirstPersonFlags & 0x16) != 0) {
        f31 = 0.0f;
        if ((gFirstPersonFlags & 0x3c) == 0) {
            const uint32_t mode = gCameraModeA;
            if (mode & 8) gCameraF5BF4 = ppc_fnmsubs(kK0012, dt, gCameraF5BF4);
            if (mode & 0x10) gCameraF5BF4 = ppc_fmadds(kK0012, dt, gCameraF5BF4);
            if (mode & 0x40) f31 = f31 + Bits(0xbdcccccdu);
            if (mode & 0x20) f31 = f31 + Bits(0x3dcccccdu);
        }
    } else {
        if (isometric != 0) {
            const uint32_t mode = gCameraModeA;
            if ((mode & 8) && !(gCameraModeB & 8)) gCameraF5BF4 = gCameraF5BF4 + kHalfPi;
            if ((mode & 0x10) && !(gCameraModeB & 0x10)) gCameraF5BF4 = gCameraF5BF4 - kHalfPi;
        } else {
            const uint32_t mode = gCameraModeA;
            if (mode & 8) gCameraF5BF4 = ppc_fmadds(kK0002, dt, gCameraF5BF4);
            if (mode & 0x10) gCameraF5BF4 = ppc_fnmsubs(kK0002, dt, gCameraF5BF4);
        }
        const uint32_t mode = gCameraModeA;
        if (mode & 0x40) gCameraF68 = ppc_fmadds(kK005, dt, gCameraF68);
        if (mode & 0x20) gCameraF68 = ppc_fnmsubs(kK005, dt, gCameraF68);
    }

    if (gCameraModeA & 0x100) {
        // Reset: the home zoom, no key yaw, the home position.
        gCameraF68 = Bits(0x42dc0000u);
        gCameraF5BF4 = 0.0f;
        std::memcpy(gCameraPosition, gCameraStartPosition, sizeof gCameraPosition);
        gCameraModeA &= ~0x100u;
        gFirstPersonFlags &= ~0x1000u;
    }
    gCameraModeB = gCameraModeA;

    // The pointer's offset from the middle of the game window, in half windows (up is positive).
    float f28 = (gPointerScreenXY[0] - gGameWindowCentreX) / gGameWindowHalfWidth;
    float f27 = (-(gPointerScreenXY[1] - gGameWindowCentreY)) / gGameWindowHalfHeight;
    int32_t edge;       // the pointer is at a window edge with edge scrolling on
    int32_t firstOk;    // a first-person view that is not held back
    int32_t keyScroll = 0;
    if ((gFirstPersonFlags & 0x16) != 0) {
        edge = 0;
        firstOk = (gFirstPersonFlags & 0x800) == 0 ? 1 : 0;
    } else {
        if ((gTPWFlags & 8) != 0 && (gFirstPersonFlags & 0x100) == 0 &&
            (f28 < Bits(0xbf6e147bu) || f28 > Bits(0x3f6e147bu) || f27 < Bits(0xbf6e147bu) || f27 > Bits(0x3f6e147bu))) {
            edge = 1;
        } else {
            edge = 0;
        }
        firstOk = 0;
        if (Key(0xd) || Key(0xe) || Key(0xf) || Key(0x10)) keyScroll = 1;
    }

    // Which sides of the map the position is outside of: 1 x < 0, 4 z < 0, 2 x past the end, 8 z past the end.
    uint32_t bounds = 0;
    {
        const sHField* field = gHeightfieldHeader->mHField;
        const float width = field->mF08[2] * static_cast<float>(field->mF18[0]);
        const float depth = field->mF08[3] * static_cast<float>(field->mF18[1]);
        const float px = gCameraPosition[0];
        const float pz = gCameraPosition[2];
        if (px < 0.0f) bounds |= 1;
        if (pz < 0.0f) bounds |= 4;
        if (px > width) bounds |= 2;
        if (pz > depth) bounds |= 8;
    }

    float* const vel = gCameraF5BB4;
    if ((gCameraModeA & 4) == 0 && edge == 0 && firstOk == 0 && keyScroll == 0) {
        // Path A: nothing pushes the camera. The velocity dies away and the yaw eases towards the key yaw.
        const float scale = ppc_fmadds(kK001, dt, 1.0f);
        vel[0] = vel[0] / scale;
        vel[1] = vel[1] / scale;
        vel[2] = vel[2] / scale;
        const float target = gCameraF5BF4;
        float f3;
        if (isometric != 0) {
            const float least = gCameraYawRateIso < dt ? gCameraYawRateIso : dt;
            f3 = least / Bits(0x43480000u);
        } else {
            const float least = gCameraYawRate < dt ? gCameraYawRate : dt;
            f3 = least / Bits(0x430c0000u);
        }
        gCameraYaw = ppc_fmadds(gCameraYaw, 1.0f - f3, target * f3);
        if (static_cast<float>(std::fabs(static_cast<double>(gCameraYaw) - static_cast<double>(target))) < gCameraYawSnap) gCameraYaw = target;
    } else {
        // Path B: something pushes the camera: f30 sideways, f31 forward (or up the screen).
        if ((gFirstPersonFlags & 0x16) != 0) {
            f30 = 0.0f;
            if ((gFirstPersonFlags & 0x3c) != 0) {
                if (f28 < Bits(0xbe4ccccdu)) gFirstPersonTurn = Bits(0x40600000u) * (f28 - Bits(0xbe4ccccdu));
                else if (f28 > Bits(0x3e4ccccdu)) gFirstPersonTurn = Bits(0x40600000u) * (f28 - Bits(0x3e4ccccdu));
                else gFirstPersonTurn = f30;
            } else {
                if (f28 < Bits(0xbecccccdu)) gFirstPersonTurn = Bits(0x40000000u) * (f28 - Bits(0xbecccccdu));
                else if (f28 > Bits(0x3ecccccdu)) gFirstPersonTurn = Bits(0x40000000u) * (f28 - Bits(0x3ecccccdu));
                else gFirstPersonTurn = f30;
            }
            if (f27 < Bits(0xbecccccdu)) gFirstPersonPitch = (-f27) - Bits(0x3ecccccdu);
            else if (f27 > Bits(0x3e4ccccdu)) gFirstPersonPitch = Bits(0x3f99999au) * (Bits(0x3e4ccccdu) - f27);
            else gFirstPersonPitch = 0.0f;
            if ((gCameraModeA & 4) != 0 && (gFirstPersonFlags & 0x3c) == 0) f31 = f31 + Bits(0x3dcccccdu);
        } else {
            const bool rmbOff = gPlayerSettings.mCurrent.mRMBScrollOn == 0;
            if ((gCameraModeA & 4) != 0 && !rmbOff) {
                gFirstPersonFlags &= ~0x1000u;
                f30 = f28;
                f31 = f27;
            } else {
                f30 = 0.0f;
                f31 = f30;
                if (!rmbOff) {
                    f28 = f30;
                    f27 = f30;
                }
                if (Key(0xd)) f27 = 1.0f;
                if (Key(0xe)) f27 = -1.0f;
                if (Key(0xf)) f28 = -1.0f;
                if (Key(0x10)) f28 = 1.0f;
                if (f28 < Bits(0xbf6e147bu)) {
                    f30 = f28 - Bits(0xbf6e147bu);
                    gFirstPersonFlags &= ~0x1000u;
                    f30 = Bits(0x41200000u) * f30;
                } else if (f28 > Bits(0x3f6e147bu)) {
                    f30 = f28 - Bits(0x3f6e147bu);
                    gFirstPersonFlags &= ~0x1000u;
                    f30 = Bits(0x41200000u) * f30;
                }
                if (f27 < Bits(0xbf6e147bu)) {
                    f31 = f27 - Bits(0xbf6e147bu);
                    gFirstPersonFlags &= ~0x1000u;
                    f31 = Bits(0x41200000u) * f31;
                } else if (f27 > Bits(0x3f6e147bu)) {
                    f31 = f27 - Bits(0x3f6e147bu);
                    gFirstPersonFlags &= ~0x1000u;
                    f31 = Bits(0x41200000u) * f31;
                }
            }
        }

        if ((gFirstPersonFlags & 0x16) != 0) {
            if ((gFirstPersonFlags & 0x3c) == 0) {
                // Walking: the key yaw turns with the pointer, and the push is turned by it.
                const float turnStep = kK0012 * gFirstPersonTurn;
                gCameraF5BF4 = ppc_fmadds(turnStep, dt, gCameraF5BF4);
                const float s = static_cast<float>(std::sin(static_cast<double>(gCameraF5BF4)));
                const float c = static_cast<float>(std::cos(static_cast<double>(gCameraF5BF4)));
                const float a = f31 * s;
                const float n = ppc_fneg(f30);
                const float b = f31 * c;
                f30 = ppc_fmadds(f30, c, a);
                f31 = ppc_fmadds(n, s, b);
            }
            gCameraYaw = gCameraF5BF4;
        } else {
            const float target = gCameraF5BF4;
            float f3;
            if (isometric != 0) {
                const float least = gCameraYawRateIso < dt ? gCameraYawRateIso : dt;
                f3 = least / Bits(0x43480000u);
            } else {
                const float least = gCameraYawRate < dt ? gCameraYawRate : dt;
                f3 = least / Bits(0x430c0000u);
            }
            gCameraYaw = ppc_fmadds(gCameraYaw, 1.0f - f3, target * f3);
            if (static_cast<float>(std::fabs(static_cast<double>(gCameraYaw) - static_cast<double>(target))) < gCameraYawSnap) gCameraYaw = target;
            const float s = static_cast<float>(std::sin(static_cast<double>(gCameraYaw)));
            const float c = static_cast<float>(std::cos(static_cast<double>(gCameraYaw)));
            const float a = f31 * s;
            const float n = ppc_fneg(f30);
            const float b = f31 * c;
            f30 = ppc_fmadds(f30, c, a);
            f31 = ppc_fmadds(n, s, b);
        }

        // The push changes the velocity, which is halved each frame; against a map edge a velocity that points out decays instead.
        const float root = static_cast<float>(std::sqrt(static_cast<double>(gCameraF68)));
        const float f3 = kK002 * root;
        const float oldX = vel[0];
        const float oldZ = vel[2];
        vel[0] = ppc_fmadds(f30, f3, vel[0]) * 0.5f;
        vel[1] = 0.0f;
        vel[2] = ppc_fmadds(f31, f3, vel[2]) * 0.5f;
        if ((bounds & 1) != 0 && vel[0] < 0.0f) vel[0] = oldX / ppc_fmadds(kK001, dt, 1.0f);
        if ((bounds & 4) != 0 && vel[2] < 0.0f) vel[2] = oldZ / ppc_fmadds(kK001, dt, 1.0f);
        if ((bounds & 2) != 0 && vel[0] > 0.0f) vel[0] = oldX / ppc_fmadds(kK001, dt, 1.0f);
        if ((bounds & 8) != 0 && vel[2] > 0.0f) vel[2] = oldZ / ppc_fmadds(kK001, dt, 1.0f);
    }

    // A velocity this small in all three axes is nothing.
    if (static_cast<float>(std::fabs(static_cast<double>(vel[0]))) < gCameraStopSpeed &&
        static_cast<float>(std::fabs(static_cast<double>(vel[1]))) < gCameraStopSpeed &&
        static_cast<float>(std::fabs(static_cast<double>(vel[2]))) < gCameraStopSpeed) {
        vel[0] = 0.0f;
        vel[1] = 0.0f;
        vel[2] = 0.0f;
    }

    // The move. Walking on the ground goes grid square by grid square (the ground and the rides can stop it); the rest just adds.
    if ((gFirstPersonFlags & 0x16) != 0 && (gFirstPersonFlags & 0x3c) == 0) {
        s3DPoint step;
        step.x = vel[0] * dt;
        step.y = vel[1] * dt;
        step.z = vel[2] * dt;
        float* const parts[3] = {&step.x, &step.y, &step.z};
        for (float* part : parts) {
            if (*part >= Bits(0xb8d1b717u) && *part <= Bits(0x38d1b717u)) *part = 0.0f;
        }
        if (step.x != 0.0f || step.z != 0.0f) {
            s3DPoint out;
            out.x = out.y = out.z = 0.0f;
            int32_t more;
            do {
                more = GetNextGridsquare(AsPoint(gCameraPosition), &step, &out);
                if (RideRideIfRequired(&out) != 0) more = 0;
                std::memcpy(gCameraPosition, &out, sizeof out);
            } while (more != 0);
        }
    } else {
        gCameraPosition[0] = ppc_fmadds(vel[0], dt, gCameraPosition[0]);
        gCameraPosition[2] = ppc_fmadds(vel[2], dt, gCameraPosition[2]);
    }

    if ((gCameraModeA & 0x80) != 0) {
        if (gCameraWideZoomOnce == 0) {
            gCameraWideZoomMin = Bits(0x42200000u);
            gCameraWideZoomOnce = 1;
        }
        if (gCameraWideZoomMaxOnce == 0) {
            gCameraWideZoomMax = Bits(0x43960000u);
            gCameraWideZoomMaxOnce = 1;
        }
        if ((gCameraModeA & 0x200) != 0) {
            if (gCameraF68 < gCameraWideZoomMin) gCameraF68 = gCameraWideZoomMin;
            else if (gCameraF68 > gCameraWideZoomMax) gCameraF68 = gCameraWideZoomMax;
        } else {
            if (gCameraF68 < Bits(0x428c0000u)) gCameraF68 = Bits(0x428c0000u);
            else if (gCameraF68 > Bits(0x43020000u)) gCameraF68 = Bits(0x43020000u);
        }
    }
    std::memcpy(gCameraTarget, gCameraPosition, sizeof gCameraTarget);

    // The view point: where the camera is relative to the target (x +0, y +4, z +8 of gCameraViewPoint).
    if ((gFirstPersonFlags & 0x16) != 0) {
        if (gCameraWalkOnce == 0) {
            gCameraF59FC = 0.0f;
            gCameraWalkOnce = 1;
        }
        gCameraViewPoint.x = 0.0f;
        gCameraViewPoint.z = 0.0f;
        float f26 = 0.0f;   // not set in the original when there is no height field (a stale register); 0 here
        float f25 = 0.0f;
        const tag_sM3D2Header* header = gHeightfieldHeader;
        if (header != nullptr && header->mHField != nullptr && header->mHField->mHeights != nullptr &&
            header->mHField->mQuads != nullptr && (gFirstPersonFlags & 0x3c) == 0) {
            f26 = gCameraTarget[0] + gCameraViewPoint.x;
            f25 = gCameraTarget[2] + gCameraViewPoint.z;
        }
        if ((gFirstPersonFlags & 0x3c) == 0) {
            gCameraViewPoint.y = Bits(0x40a00000u) + DetermineHFieldHeight(f26, f25, 1);
        }
    } else {
        gCameraViewPoint.x = 0.0f;
        gCameraViewPoint.y = 0.0f;
        gCameraViewPoint.z = -gCameraF68;
        const float f4 = gCameraF68 - Bits(0x428c0000u);
        const float t = f4 / Bits(0x42700000u);
        gCameraPitch = ppc_fmadds(kQuarterPi, 1.0f - t, kOneAndAHalfPitch * t);
        gCameraPitch = gCameraPitch >= gCameraPitchMin ? gCameraPitch : gCameraPitchMin;
        gCameraPitch = gCameraPitchMax >= gCameraPitch ? gCameraPitch : gCameraPitchMax;
        const float s = static_cast<float>(std::sin(static_cast<double>(gCameraPitch)));
        const float c = static_cast<float>(std::cos(static_cast<double>(gCameraPitch)));
        const float y = gCameraViewPoint.y;
        const float z = gCameraViewPoint.z;
        gCameraViewPoint.y = ppc_fmsubs(y, c, z * s);
        gCameraViewPoint.z = ppc_fmadds(y, s, z * c);
        // The ground under the target, averaged with the last frame's, lifts the view point.
        gCameraGroundHeight = gCameraGroundHeight + DetermineHFieldHeight(gCameraTarget[0], gCameraTarget[2], 1);
        gCameraGroundHeight = gCameraGroundHeight * 0.5f;
        gCameraViewPoint.y = gCameraViewPoint.y + gCameraGroundHeight;
        // The tilt from the velocity: the square root of its size, scaled.
        if (vel[0] < 0.0f) {
            const float root = static_cast<float>(std::sqrt(100.0 * static_cast<double>(-vel[0])));
            gCameraTilt[0] = ppc_fneg(root / Bits(0x43480000u) - kK0001);
            gCameraTilt[0] = gCameraTilt[0] < gCameraTiltMinX ? gCameraTilt[0] : gCameraTiltMinX;
        } else {
            const float root = static_cast<float>(std::sqrt(100.0 * static_cast<double>(vel[0])));
            gCameraTilt[0] = root / Bits(0x43480000u) - kK0001;
            gCameraTilt[0] = gCameraTiltMaxX < gCameraTilt[0] ? gCameraTilt[0] : gCameraTiltMaxX;
        }
        if (vel[2] < 0.0f) {
            const float root = static_cast<float>(std::sqrt(100.0 * static_cast<double>(-vel[2])));
            gCameraTilt[1] = ppc_fneg(root / Bits(0xc3480000u) - kK0001);
            gCameraTilt[1] = gCameraTilt[1] < gCameraTiltMinZ ? gCameraTilt[1] : gCameraTiltMinZ;
        } else {
            const float root = static_cast<float>(std::sqrt(100.0 * static_cast<double>(vel[2])));
            gCameraTilt[1] = root / Bits(0xc3480000u) - kK0001;
            gCameraTilt[1] = gCameraTiltMaxZ < gCameraTilt[1] ? gCameraTilt[1] : gCameraTiltMaxZ;
        }
    }

    // The view point turned by the yaw and moved to the target.
    {
        const float s = static_cast<float>(std::sin(static_cast<double>(gCameraYaw)));
        const float c = static_cast<float>(std::cos(static_cast<double>(gCameraYaw)));
        const float x = gCameraViewPoint.x;
        const float n = ppc_fneg(x);
        gCameraViewPoint.x = ppc_fmadds(x, c, gCameraViewPoint.z * s);
        gCameraViewPoint.z = ppc_fmadds(n, s, gCameraViewPoint.z * c);
        gCameraViewPoint.x = gCameraViewPoint.x + gCameraTarget[0];
        gCameraViewPoint.y = gCameraViewPoint.y + gCameraTarget[1];
        gCameraViewPoint.z = gCameraViewPoint.z + gCameraTarget[2];
    }

    // The camera matrix.
    if ((gFirstPersonFlags & 0x16) != 0) {
        if ((gFirstPersonFlags & 0x3c) != 0) {
            // Riding: the ride's matrix, bumped by the ride, then turned by the pointer.
            std::memcpy(&gCameraMatrix, gFirstPersonRideMatrix, sizeof(sMatrix));
            ApplyHeadsOnRidesBumpage(gFirstPersonRideMatrix, &gCameraMatrixA, dt, &gCameraMatrix);
            std::memcpy(&gCameraMatrixA, gFirstPersonRideMatrix, sizeof(sMatrix));
            sMatrix local;
            SetMatrixIdentity(&local);
            float angles[2] = {gFirstPersonPitch, gFirstPersonTurn};
            RotateLMatrixXYZNoTrans(&local, kRotateXY, angles);
            CombineMatricesNoWDest2nd(&local, &gCameraMatrix);
        } else {
            SetLMatrixOrientationHi(&gCameraMatrix, gFirstPersonPitch, gCameraYaw);
        }
    } else {
        SetLMatrixOrientationHi(&gCameraMatrix, gCameraPitch, gCameraYaw);
        RotateLMatrixXYZNoTransHi(&gCameraMatrix, kRotateZX, gCameraTilt);
    }
    if ((gFirstPersonFlags & 0x3c) != 0) {
        gCameraMatrix.m[3][0] = gFirstPersonRideMatrix->m[3][0];
        gCameraMatrix.m[3][1] = gFirstPersonRideMatrix->m[3][1];
        gCameraMatrix.m[3][2] = gFirstPersonRideMatrix->m[3][2];
    } else {
        gCameraMatrix.m[3][0] = gCameraViewPoint.x;
        gCameraMatrix.m[3][1] = gCameraViewPoint.y;
        gCameraMatrix.m[3][2] = gCameraViewPoint.z;
    }
    if (MatrixDifferenceTiny(&gCameraMatrix, &gCameraMatrixB) == 0) {
        gei_ViewMatrix->Load(gCameraMatrix);
    } else {
        gCameraF5BF4 = static_cast<float>(std::fmod(static_cast<double>(gCameraF5BF4), kTwoPiD));
        gCameraYaw = static_cast<float>(std::fmod(static_cast<double>(gCameraYaw), kTwoPiD));
    }
    gCurrentCameraMatrix = &gCameraMatrix;
    std::memcpy(&gCameraMatrixB, &gCameraMatrix, sizeof(sMatrix));
    gCameraClockStamp = now;
    InvertMatrix(&gInverseCameraMatrix, gCurrentCameraMatrix);
}
