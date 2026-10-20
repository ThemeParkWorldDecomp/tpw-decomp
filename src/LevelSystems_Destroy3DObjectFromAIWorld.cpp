// Unit v5901: the AI-world cell helpers of the level code (Destroy3DObjectFromAIWorld, Assign3DObjectToAIWorld,
// GetWorldGSquareHeight, DetermineInnerBaseInsets, GetRideIDAtCell).
#include <cmath>
#include <cstdio>
#include <cstring>
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "Main.h"
#include "MapWho.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include "platform/samsutils.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "TrackCell.h"
#include "m3d2.h"
#include "analyser.h"

// 0x1029cf10 (name invented): the halfword a thing id is compared with (0 at start).
uint16_t gLevelNoThingId = 0;

namespace {

const char kThingZeroMsg[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1023d2de

// The cell of (x, y) as the Mac computes it: world + 0x2d8 + (id16 - 1) * 0x44.
CMapCell* AICell(int32_t x, int32_t y) {
    uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
    int32_t index = static_cast<int32_t>(id) - 1;
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2d8 + index * 0x44);
}

void ClampXY(int32_t& x, int32_t& y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
    if (y >= gHeightfieldHeight) y = gHeightfieldHeight - 1;
}

// fctiwz: truncates toward zero, saturating (0x80000000 for NaN)
int32_t Trunc(float v) {
    if (v != v) return INT32_MIN;
    if (v >= 2147483648.0f) return INT32_MAX;
    if (v <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(v);
}

// The ride id of a cell with no track mesh: the thing of the cell's column override, when it is a ride (kind 3).
uint32_t RideOfOverride(int32_t x, int32_t y) {
    uint16_t id;
    LLCR_OverRide(&id, x, y);
    if (id != gLevelNoThingId) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMsg);
        const uint8_t* thing = static_cast<const uint8_t*>(gThingArray[id].mThing);
        if (thing != nullptr && thing[2] == 3) return *reinterpret_cast<const uint32_t*>(thing + 0x1c);
    }
    return 0;
}

}  // namespace

// 0x1005f3fc (268 bytes)
void Destroy3DObjectFromAIWorld(uint32_t a, uint32_t b) {
    int32_t x = static_cast<int32_t>(a);
    int32_t y = static_cast<int32_t>(b);
    ClampXY(x, y);
    if (CMap::IsXYValid(x, y)) {
        CMapCell* cell = AICell(x, y);
        if (cell != nullptr) {
            CTrackCell* track = cell->GetTrackCellEquiv();
            cell->Delete(0, 0);
            track->Delete(0, 0, true);
        }
    }
}

// 0x1005f53c (384 bytes); the instance is not read.
void Assign3DObjectToAIWorld(void* instance, uint32_t a, uint32_t b, uint32_t c, int32_t d) {
    (void)instance;
    int32_t x = static_cast<int32_t>(a);
    int32_t y = static_cast<int32_t>(b);
    ClampXY(x, y);
    if (CMap::IsXYValid(x, y)) {
        CMapCell* cell = AICell(x, y);
        if (cell != nullptr) {
            CTrackCell* track = cell->GetTrackCellEquiv();
            uint16_t saved = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(cell) + 0x10);
            cell->Delete(0, 0);
            cell->Modify(static_cast<CELL_MODES>(0x15), 0, 0);
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id;
            id.mIndex = saved;
            cell->ModifyParentID(id);
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(track) + 4) = c;
            if (d != 0)
                cell->SetFlag(static_cast<uint16_t>(cell->GetFlag() | 0x100));
            else
                cell->SetFlag(static_cast<uint16_t>(cell->GetFlag() & 0xfeff));
        }
    }
}

// 0x1005f160 (620 bytes)
float GetWorldGSquareHeight(int32_t x, int32_t y, uint32_t* extra) {
    uint32_t ride;
    if (extra != nullptr && *extra != 0) {
        ride = *extra;
    } else {
        ride = LLCC_GetTrackMeshAt(x, y);
        if (ride == 0) ride = RideOfOverride(x, y);
    }
    float fx = 10.0f * static_cast<float>(x);
    float fz = 10.0f * static_cast<float>(y);
    if (ride != 0) {
        if (extra != nullptr) *extra = ride;
        tag_sRideAnimSet* set = gRideAnimSets[ride];
        if (set != nullptr) {
            const uint8_t* block = static_cast<const uint8_t*>(set->mHeightMap);
            if (block != nullptr) {
                tag_sMesh* mesh = set->mHeads;
                int32_t rotation = mesh->mF10;
                const uint8_t* node = reinterpret_cast<const uint8_t*>(mesh->mHeader->mF78.get());
                float nx, nz, ny;
                std::memcpy(&nx, node + 0x40, 4);
                std::memcpy(&ny, node + 0x44, 4);
                std::memcpy(&nz, node + 0x48, 4);
                float u = fx - nx;
                float v = fz - nz;
                float a = u, b = v;
                if (rotation == 0xb4) {
                    a = -u - 10.0f;
                    b = -v - 10.0f;
                } else if (rotation < 0xb4) {
                    if (rotation == 0x5a) {
                        a = -v - 10.0f;
                        b = u;
                    }
                } else if (rotation == 0x10e) {
                    a = v;
                    b = -u - 10.0f;
                }
                int32_t row = Trunc(b / 10.0f);
                int32_t col = Trunc(a / 10.0f);
                uint16_t width = *reinterpret_cast<const uint16_t*>(block + 8);
                // The heights (+0x10): a 32-bit word of the map image, an arena offset on the host like sRideHeightMap's other two.
                Ptr32<const uint8_t> data;
                data.mOffset = *reinterpret_cast<const uint32_t*>(block + 0x10);
                int32_t index = col + static_cast<int32_t>(static_cast<uint32_t>(row) * width);
                float byte = static_cast<float>(static_cast<double>(data[index]));
                float scaled = 100.0f * byte;
                return scaled / 255.0f + ny;
            }
        }
    }
    if (extra == nullptr) return DetermineHFieldHeightQuick(fx, fz);
    *extra = 0;
    return 0.0f;
}

// 0x1005f738 (316 bytes): for each of the four corners, the half vector to the second nearest point (in x and z).
// With fewer than two points the zero answer goes to insets[0] every time (the original never advances its output).
void DetermineInnerBaseInsets(s2DPoint* corners, s3DPoint* points, int32_t count, s2DPoint* insets) {
    for (int32_t i = 0; i < 4; ++i) {
        float best = 3.4028234663852886e+38f;
        float second = best;
        int32_t bestIndex = -1;
        int32_t secondIndex = -1;
        for (int32_t j = 0; j < count; ++j) {
            float dx = points[j].x - corners[i].x;
            float dz = points[j].z - corners[i].y;
            float dzz = dz * dz;
            double sum = static_cast<double>(dx) * static_cast<double>(dx) + static_cast<double>(dzz);
            float d = static_cast<float>(std::sqrt(sum));
            if (d < best) {
                second = best;
                secondIndex = bestIndex;
                best = d;
                bestIndex = j;
            } else if (d < second) {
                second = d;
                secondIndex = j;
            }
        }
        if (secondIndex == -1) {
            insets[0].x = 0.0f;
            insets[0].y = 0.0f;
        } else {
            insets[i].x = (points[secondIndex].x - corners[i].x) * 0.5f;
            insets[i].y = (points[secondIndex].z - corners[i].y) * 0.5f;
        }
    }
}

// 0x1005f080 (184 bytes)
int32_t GetRideIDAtCell(int32_t x, int32_t y) {
    uint32_t ride = LLCC_GetTrackMeshAt(x, y);
    if (ride == 0) ride = RideOfOverride(x, y);
    return static_cast<int32_t>(ride);
}
