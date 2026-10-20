// TPEmbed.cpp (main app of SimTheme Park 2.04b3): TickRideHeads 0x10200460, which turns the heads of a ride's
// embedded set towards the camera. A head is an embedded entry (flag 2) whose 0x40-byte block is a 4 x 4 matrix
// and whose word at +0xc is the handle of a script object; the function does not move the head, it gives the
// script object the head's position and three variables (13: which of 7 x 8 sampled directions looks closest to
// the camera, 9: the head's turn about its own axis as a fraction of a turn, 8: a constant, see below).
//
// The float work follows the listing instruction by instruction: the CodeWarrior compiler fused the products and
// sums that ppc_math.h's ppc_fmadds, ppc_fmsubs and ppc_fmsub stand for, and every angle becomes a sine table index
// as fctiwz does (ppc_fctiwz) and is masked with the table's mask.
#include "TPEmbed.h"

#include <cmath>
#include <cstring>

#include "TPCamera.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "analyser.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

float gRideHeadsVariable8;
int8_t gRideHeadsVariable8Set;

namespace {

constexpr float kMinusOne = -1.0f;            // 0x10265594
constexpr float kInitialVariable8 = 0.685f;   // 0x1026557c
constexpr float kTwoPi = 6.2831855f;          // 0x10265580
constexpr float kTwoThirdsPi = 2.0943952f;    // 0x10265584
constexpr float kSixthPi = 0.5235988f;        // 0x10265588
constexpr float kQuarterPi = 0.7853982f;      // 0x1026558c
constexpr float kMinusQuarterPi = -0.7853982f;   // 0x10265590
constexpr float kHalfPi = 1.5707964f;         // 0x102655a8

constexpr int32_t kRows = 7;      // directions are sampled in 7 rings of latitude (pi / 6 apart) ...
constexpr int32_t kColumns = 8;   // ... with 8 columns of longitude (pi / 4 apart) in each
constexpr uint32_t kHeadVariable = 13;
constexpr uint32_t kTurnVariable = 9;
constexpr uint32_t kConstantVariable = 8;

// The sine table entry of an angle already multiplied by the table's scale: the index is truncated as fctiwz does
// and masked. The cosine of an angle is the entry of angle + pi / 2.
float TableEntry(float scaledAngle) {
    const int32_t index = ppc_fctiwz(static_cast<double>(scaledAngle));
    return gTPGlobals.mSinTable[static_cast<uint32_t>(index) & gTPGlobals.mSinTableMask];
}

// The listing's frsp(fmsub(a, b, c * d)): the product of two floats is exact in double, the fused difference is
// rounded to double and then to single. One component of a cross product.
float CrossTerm(float a, float b, float c, float d) {
    const double product = static_cast<double>(c) * static_cast<double>(d);
    return static_cast<float>(ppc_fmsub(a, b, product));
}

// Finds the row and column of the sampled direction with the largest dot product with the unit direction
// (nx, ny, nz); the dot product has to beat 0, and a tie keeps the first. `best` is that sample's direction.
// The sample directions start at (0, 1, 0), turn about the y axis by pi / 4 for each column and about the x axis
// by pi / 6 for each row; the turns go through the sine table. When no sample beats 0 (the direction is all zeros or
// holds a NaN that the NaN test did not catch) the original leaves row, column and `best` as they were (registers
// and stack words of the previous head), so they are passed in by reference and the caller keeps them.
void FindClosestDirection(float nx, float ny, float nz, int32_t& row, int32_t& column, s3DPoint& best) {
    float vx = 0.0f;
    float vy = 1.0f;
    float vz = 0.0f;
    float bestDot = 0.0f;
    for (int32_t r = 0; r < kRows; ++r) {
        for (int32_t c = 0; c < kColumns; ++c) {
            float dot = vy * ny;
            dot = ppc_fmadds(vx, nx, dot);
            dot = ppc_fmadds(vz, nz, dot);
            if (bestDot < dot) {
                bestDot = dot;
                row = r;
                column = c;
                best.x = vx;
                best.y = vy;
                best.z = vz;
            }
            // Turn (vx, vz) about the y axis by pi / 4: sin(-pi / 4) is `minus`, sin(pi / 4) is `plus`.
            const float scale = gTPGlobals.mSinTableScale;
            const float minus = TableEntry(kMinusQuarterPi * scale);
            const float plus = TableEntry(kQuarterPi * scale);
            const float newX = ppc_fmadds(vx, plus, vz * minus);
            const float newZ = ppc_fmadds(-vx, minus, vz * plus);
            vx = newX;
            vz = newZ;
        }
        // Turn (vy, vz) about the x axis by pi / 6: sin(pi / 6) is `sine`, sin(2 pi / 3) is `cosine` (= cos(pi / 6)).
        const float scale = gTPGlobals.mSinTableScale;
        const float sine = TableEntry(kSixthPi * scale);
        const float cosine = TableEntry(kTwoThirdsPi * scale);
        const float newY = ppc_fmsubs(vy, cosine, vz * sine);
        const float newZ = ppc_fmadds(vy, sine, vz * cosine);
        vy = newY;
        vz = newZ;
    }
}

// The matrix that looks along the sample (row, column) with `best` as its third axis: row 1 is the "forward" vector
// worked out from the two angles (pi / 6 * row about x, -pi / 4 * column about y, from the start (0, 0, -1)), row 0
// forward x best and row 2 best.
void BuildLookMatrix(int32_t row, int32_t column, const s3DPoint& best, sMatrix* look) {
    const float scale = gTPGlobals.mSinTableScale;
    const float rowAngle = kSixthPi * static_cast<float>(row);
    const float rowSine = TableEntry(rowAngle * scale);
    const float rowCosine = TableEntry((kHalfPi + rowAngle) * scale);
    float forwardX = 0.0f;
    float forwardY = ppc_fmsubs(0.0f, rowCosine, kMinusOne * rowSine);
    float forwardZ = ppc_fmadds(0.0f, rowSine, kMinusOne * rowCosine);
    const float columnAngle = kMinusQuarterPi * static_cast<float>(column);
    const float columnSine = TableEntry(columnAngle * scale);
    const float columnCosine = TableEntry((kHalfPi + columnAngle) * scale);
    const float oldX = forwardX;
    const float oldZ = forwardZ;
    forwardX = ppc_fmadds(oldX, columnCosine, oldZ * columnSine);
    forwardZ = ppc_fmadds(-oldX, columnSine, oldZ * columnCosine);
    s3DPoint forward = {forwardX, forwardY, forwardZ};
    s3DPoint side = {CrossTerm(forwardY, best.z, forwardZ, best.y), CrossTerm(forwardZ, best.x, forwardX, best.z),
                     CrossTerm(forwardX, best.y, forwardY, best.x)};
    s3DPoint axis = best;
    SetMatrixIdentity(look);
    SetLMatrixOrientation(look, &side, &forward, &axis);
}

}  // namespace

void TickRideHeads(tag_sMesh*& meshRef) {
    // Where the original keeps the winning sample between heads (registers r30, r31 and stack words): see
    // FindClosestDirection. The caller's values at the first head are not known; 0 here.
    int32_t row = 0;
    int32_t column = 0;
    s3DPoint best = {0.0f, 0.0f, 0.0f};
    if ((static_cast<sEmbeddedSet*>(meshRef->mF28)->mFlags & 4) == 0) return;
    const uint32_t count = meshRef->mHeader->mEmbeddedCount;
    for (uint32_t i = 0; i < count; ++i) {
        tag_sMesh* mesh = meshRef;
        sEmbeddedRecord* record = &static_cast<sEmbeddedSet*>(mesh->mF28)->mRecords[i];
        if ((record->mFlags & 2) == 0) continue;
        tag_sMiniNode* node = EmbeddedNode(mesh->mHeader, i);
        float* block = reinterpret_cast<float*>(record->mBlock);
        // A mirrored node flips the head's x axis and z axis rows (the block is changed every tick).
        if ((node->mFlags & 0x400) != 0) {
            block[0] = block[0] * kMinusOne;
            block[1] = block[1] * kMinusOne;
            block[2] = block[2] * kMinusOne;
            block[8] = block[8] * kMinusOne;
            block[9] = block[9] * kMinusOne;
            block[10] = block[10] * kMinusOne;
        }
        const int32_t handle = static_cast<int32_t>(record->mF0C);
        if ((node->mFlags & 0x8000) != 0) {
            SCR_SetVariable(handle, kHeadVariable, -1);
            continue;
        }
        SCR_SetScriptPosition(handle, block[12], block[13], block[14], 0);
        // L = the camera's matrix combined with the inverse of the head's: the camera as seen from the head.
        sMatrix toCamera;
        std::memcpy(&toCamera, block, sizeof toCamera);
        InvertMatrix(&toCamera, &toCamera);
        CombineMatricesNoWDest2nd(gCurrentCameraMatrix, &toCamera);
        // The unit direction to the camera. The length is worked out in double precision: x * x fused with the
        // single-rounded y * y, then the single-rounded z * z added.
        const float x = toCamera.m[3][0];
        const float y = toCamera.m[3][1];
        const float z = toCamera.m[3][2];
        const float ySquared = y * y;
        const float zSquared = z * z;
        double sum = ppc_fmadd(static_cast<double>(x), static_cast<double>(x), static_cast<double>(ySquared));
        sum = static_cast<double>(zSquared) + sum;
        const float length = static_cast<float>(std::sqrt(sum));
        const float nx = x / length;
        const float ny = y / length;
        const float nz = z / length;
        if (std::isnan(nx)) {
            // __fpclassifyf answered FP_NAN: the direction is 0 / 0.
            row = 0;
            column = 0;
        } else {
            FindClosestDirection(nx, ny, nz, row, column, best);
        }
        if (row == 0 || row == 6) column = 0;
        sMatrix look;
        BuildLookMatrix(row, column, best, &look);
        sMatrix lookInverse;
        InvertMatrix(&lookInverse, &look);
        CombineMatricesNoWDest2nd(&toCamera, &lookInverse);
        sMatrix turn = lookInverse;
        const float angle = GetLMatrixRotationAroundZAxisYOnly(&turn);
        SCR_SetVariable(handle, kHeadVariable, column + row * 8);
        SCR_SetFloatVariable(handle, kTurnVariable, angle / kTwoPi);
        if (gRideHeadsVariable8Set == 0) {
            gRideHeadsVariable8 = kInitialVariable8;
            gRideHeadsVariable8Set = 1;
        }
        SCR_SetFloatVariable(handle, kConstantVariable, gRideHeadsVariable8);
    }
}
