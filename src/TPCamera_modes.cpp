// TPCamera.cpp (main app of SimTheme Park 2.04b3), unit w141: the camera modes and the first person walk and ride.
//   __sinit_TPCamera_cpp 0x10036518 (36 bytes)  copies the start position over the camera position (a file-scope dynamic initialiser)
//   SetCameraMode 0x1003384c (792), ToggleFirstPersonWalkAround 0x10033b90 (508), Setup1stPersonRideMeshes 0x10032c64 (428),
//   SetupRideRideMatrixAndSuch 0x10032e40 (384), ToggleRideRideIn1stPerson 0x10032ff4 (408), SaveCamera 0x10035f4c (680),
//   LoadCamera 0x10036218 (732).
//
// gFirstPersonFlags: 2 walking, 4 riding a ride, 0x10 (with the others, see TPCamera.h), 0x200 procedural texturing was on, 0x400 a ride
// entered from walking (leaving it returns to walking), 0x800 the look angles were reset, 0x1000 cleared at the end of SetCameraMode.
// SaveCamera and LoadCamera swap the globals in place, as the original does (the file's words are little-endian).
#include <cmath>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "ESP_sprites.h"
#include "LbFile.h"
#include "MapCell.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "UIInterface.h"
#include "World.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "ppc_math.h"

tag_sRideAnimSet* gCameraHeadsRideSet;
uint32_t* gFirstPersonRideFlagWord;
void* gFirstPersonRideNode;
float gCameraSavedPosition[3];
float gCameraSavedYaw;
int8_t gRideRideOnceGuard;
int8_t gRideRideOnceValue;

namespace {
constexpr double kHalfPiD = 1.5707963705062866;    // the float pi / 2 as a double (0x10264688)
constexpr float kHalfPi = 1.57079637f;             // 0x3fc90fdb
constexpr float kQuarterPi = 0.785398185f;         // 0x3f490fdb

// The Mac swaps the word in place (4 bytes reversed): the file is little-endian, so on the little-endian host the word already is
// the file's (the same convention as serialise::SwapWord in include/Serialise.h): the bytes are taken low to high.
uint32_t Swap(uint32_t v) {
    uint8_t bytes[4];
    for (int i = 0; i < 4; ++i) bytes[i] = static_cast<uint8_t>(v >> (8 * i));
    std::memcpy(&v, bytes, 4);
    return v;
}

void SwapWords(void* words, int32_t count) {
    uint32_t* w = static_cast<uint32_t*>(words);
    for (int32_t i = 0; i < count; ++i) w[i] = Swap(w[i]);
}

// Sets or clears the 0x8000 bit of the node of the embedded entry.
void SetNodeBit(tag_sM3D2Header* header, uint32_t entry, bool on) {
    tag_sMiniNode* const node = EmbeddedNode(header, entry);
    if (on) node->mFlags |= 0x8000;
    else node->mFlags &= ~0x8000u;
}
}  // namespace

// 0x10036518: the dynamic initialiser of TPCamera.cpp.
static void InitCameraPosition() {
    std::memcpy(gCameraPosition, gCameraStartPosition, sizeof gCameraPosition);
}
static const int kCameraPositionInit = (InitCameraPosition(), 0);

void Setup1stPersonRideMeshes(int32_t ride) {
    const uint32_t last = gRideCountB + gRideCountA;
    for (uint32_t s = 1; s <= last; ++s) {
        tag_sRideAnimSet* const set = gRideAnimSets[s];
        if (set == nullptr) continue;
        tag_sM3D2Header* const header = set->mHeads->mHeader;
        const sEmbeddedData* record = header->mEmbedded.get();
        for (uint32_t i = 0; i < header->mEmbeddedCount; ++i, ++record) {
            if ((gFirstPersonFlags & 0x16) == 0) SetNodeBit(header, i, false);
            else if ((record->mFlags & 0x4000000) != 0) SetNodeBit(header, i, true);
            else if ((record->mFlags & 0x8000000) != 0) SetNodeBit(header, i, false);
        }
    }
    if (ride != 0 && (gFirstPersonFlags & 4) != 0 && gRideAnimSets[ride] != nullptr) {
        tag_sM3D2Header* const header = gRideAnimSets[ride]->mHeads->mHeader;
        const sEmbeddedData* record = header->mEmbedded.get();
        for (uint32_t i = 0; i < header->mEmbeddedCount; ++i, ++record) {
            if ((record->mFlags & 0x10000000) != 0) SetNodeBit(header, i, true);
            else if ((record->mFlags & 0x20000000) != 0) SetNodeBit(header, i, false);
        }
    }
}

int32_t SetCameraMode(int32_t mode, int32_t toggle, int32_t ride, int32_t force, int32_t* out, int32_t resetTurn) {
    bool enter = false;
    bool leave = false;
    int32_t result = 0;
    gFirstPersonFlags &= ~0x800u;
    const uint32_t previous = gFirstPersonFlags & 0x16;
    if (out != nullptr) *out = 0;
    if (mode == 2) {
        if ((gFirstPersonFlags & 0x84) == 0 || force != 0) {
            gFirstPersonFlags &= 0xffffff43u;
            if (toggle != 0) {
                if ((gFirstPersonFlags & 2) != 0) gFirstPersonFlags &= ~2u;
                else gFirstPersonFlags |= 2;
            } else {
                gFirstPersonFlags |= 2;
            }
            if ((gFirstPersonFlags & 2) != 0) {
                enter = previous == 0;
                result = 1;
            } else if (previous != 0) {
                leave = true;
            }
        }
    } else if (mode == 4) {
        if ((gFirstPersonFlags & 0x92) == 0 || force != 0) {
            gFirstPersonFlags &= 0xffffff45u;
            if (toggle != 0) {
                if ((gFirstPersonFlags & 4) != 0) gFirstPersonFlags &= ~4u;
                else gFirstPersonFlags |= 4;
            } else {
                gFirstPersonFlags |= 4;
            }
            if ((gFirstPersonFlags & 4) != 0) {
                if (previous == 0) {
                    enter = true;
                    if (resetTurn != 0) {
                        gFirstPersonFlags |= 0x800;
                        gFirstPersonTurn = 0.0f;
                        gFirstPersonPitch = 0.0f;
                    }
                } else {
                    gFirstPersonFlags |= 0x400;
                }
                result = 1;
            } else if (previous != 0) {
                if ((gFirstPersonFlags & 0x400) != 0) {
                    gFirstPersonFlags &= ~0x400u;
                    gFirstPersonFlags |= 2;
                    if (out != nullptr) *out = 1;
                } else {
                    leave = true;
                }
            }
        }
    } else if (mode == 0x40) {
        gFirstPersonFlags &= 0xffffff41u;
        leave = previous != 0;
        result = 1;
    }
    if (enter) {
        DisableRideHighlight(1);
        gUIInterface.ShowFirstPersonWindow(1);
        if (IsProceduralTexturingActive() != 0) {
            gFirstPersonFlags |= 0x200;
            DisableProceduralTexturing();
        }
        tag_sRideAnimSet* const set = gCameraHeadsRideSet;
        if (set != nullptr) {
            if ((set->mFlags & 0x4000) != 0) EnableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
            else EnableMeshDraw(set->mHeads);
        }
    } else if (leave) {
        DisableRideHighlight(0);
        gUIInterface.ShowFirstPersonWindow(0);
        if ((gFirstPersonFlags & 0x200) != 0) {
            gFirstPersonFlags &= ~0x200u;
            EnableProceduralTexturing();
        }
    }
    Setup1stPersonRideMeshes(ride);
    gFirstPersonFlags &= ~0x1000u;
    return result;
}

int32_t SetupRideRideMatrixAndSuch(int32_t ride, int32_t node) {
    if (ride == 0) return 0;
    if ((gFirstPersonFlags & 4) != 0) {
        if (gFirstPersonRideFlagWord != nullptr) *gFirstPersonRideFlagWord &= ~4u;
        gFirstPersonRideInstance = 0;
        gFirstPersonRideNode = nullptr;
        gFirstPersonRideFlagWord = nullptr;
        gFirstPersonRideMatrix = nullptr;
        return 1;
    }
    tag_sRideAnimSet* const set = gRideAnimSets[ride];
    if (set == nullptr) return 0;
    uint32_t index = FindDataNodeIndexByID(set, 0x1000, static_cast<uint32_t>(node));
    if (node != 1 && index == 0xffffffffu) index = FindDataNodeIndexByID(set, 0x1000, 1);
    if (index == 0xffffffffu) return 0;
    tag_sMesh* const mesh = set->mHeads;
    gFirstPersonRideInstance = static_cast<uint32_t>(ride);
    gFirstPersonRideNode = EmbeddedNode(mesh->mHeader, index);
    sEmbeddedRecord* const record = &static_cast<sEmbeddedSet*>(mesh->mF28)->mRecords[index];
    gFirstPersonRideFlagWord = &record->mFlags;
    gFirstPersonRideMatrix = reinterpret_cast<sMatrix*>(record->mBlock);
    gFirstPersonRideSet = set;
    record->mFlags |= 4;
    return 1;
}

void ToggleRideRideIn1stPerson(int32_t ride, int32_t a, int32_t b, int32_t* out, int32_t node, int32_t c) {
    if (gRideRideOnceGuard == 0) {
        gRideRideOnceValue = 0;
        gRideRideOnceGuard = 1;
    }
    if (out != nullptr) *out = 0;
    if (gFirstPersonRideFlagWord != nullptr) *gFirstPersonRideFlagWord &= ~4u;
    gFirstPersonRideNode = nullptr;
    gFirstPersonRideMatrix = nullptr;
    gFirstPersonRideFlagWord = nullptr;
    if ((gFirstPersonFlags & 4) != 0 && gFirstPersonRideSet != nullptr) gFirstPersonRideSet->mFlags &= ~0x2000u;
    gFirstPersonRideSet = nullptr;
    if (IsCoaster(static_cast<uint32_t>(ride)) != 0) ride = static_cast<int32_t>(GetCoaster1stPersonCar(static_cast<uint32_t>(ride)));
    if (SetupRideRideMatrixAndSuch(ride, node) != 0) {
        if (SetCameraMode(4, 1, ride, b, out, c) != 0) {
            tag_sMesh* const mesh = gRideAnimSets[ride]->mHeads;
            if (mesh->mF28 != nullptr) TPProcessEmbeddedData(mesh);
            gRideAnimSets[ride]->mFlags |= 0x2000;
        }
    }
    if (a == 1 || (gFirstPersonFlags & 2) != 0) ESP_SwitchToFirstPersonSprites();
    else ESP_SwitchToThirdPersonSprites();
}

int32_t SaveCamera(void* file) {
    uint32_t count = 0;
    SwapWords(&gCameraF68, 1);
    LbFile_Write(file, &gCameraF68, 4, &count);
    SwapWords(&gCameraF68, 1);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(&gCameraF5BF4, 1);
    LbFile_Write(file, &gCameraF5BF4, 4, &count);
    SwapWords(&gCameraF5BF4, 1);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(&gFirstPersonFlags, 1);
    LbFile_Write(file, &gFirstPersonFlags, 4, &count);
    SwapWords(&gFirstPersonFlags, 1);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(gCameraPosition, 3);
    LbFile_Write(file, gCameraPosition, 12, &count);
    SwapWords(gCameraPosition, 3);
    if (count / 12 != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(gCameraSavedPosition, 3);
    LbFile_Write(file, gCameraSavedPosition, 12, &count);
    SwapWords(gCameraSavedPosition, 3);
    if (count / 12 != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    SwapWords(&gCameraSavedYaw, 1);
    LbFile_Write(file, &gCameraSavedYaw, 4, &count);
    SwapWords(&gCameraSavedYaw, 1);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("write failed");
        return 0;
    }
    return 1;
}

int32_t LoadCamera(void* file) {
    uint32_t count = 0;
    LbFile_Read(file, &gCameraF68, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    SwapWords(&gCameraF68, 1);
    LbFile_Read(file, &gCameraF5BF4, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    SwapWords(&gCameraF5BF4, 1);
    LbFile_Read(file, &gFirstPersonFlags, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    SwapWords(&gFirstPersonFlags, 1);
    LbFile_Read(file, gCameraPosition, 12, &count);
    SwapWords(gCameraPosition, 3);
    if (count / 12 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    LbFile_Read(file, gCameraSavedPosition, 12, &count);
    SwapWords(gCameraSavedPosition, 3);
    if (count / 12 != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    LbFile_Read(file, &gCameraSavedYaw, 4, &count);
    if ((count >> 2) != 1) {
        SamsUtilities::UDebug("read failed");
        return 0;
    }
    SwapWords(&gCameraSavedYaw, 1);
    gFirstPersonFlags = 0;
    gCameraYaw = gCameraF5BF4;
    if (gPlayerSettings.mCurrent.mIsometricOn) {
        const float f = static_cast<float>(std::fmod(static_cast<double>(gCameraF5BF4), kHalfPiD));
        if (!(f < 0.0f)) {
            if (!(f > kQuarterPi)) gCameraF5BF4 = gCameraF5BF4 - f;
            else gCameraF5BF4 = gCameraF5BF4 + (kHalfPi - f);
        } else {
            if (!(f < -kQuarterPi)) gCameraF5BF4 = gCameraF5BF4 - f;
            else gCameraF5BF4 = gCameraF5BF4 - (kHalfPi + f);
        }
    }
    return 1;
}

void ToggleFirstPersonWalkAround() {
    bool allowed = (gFirstPersonFlags & 0x16) != 0;
    if (!allowed) {
        int32_t x = ppc_fctiwz(static_cast<double>(gPointerWorldPosn.x / 10.0f));
        int32_t z = ppc_fctiwz(static_cast<double>(gPointerWorldPosn.z / 10.0f));
        if (x < 0) x = 0;
        if (z < 0) z = 0;
        if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
        if (z >= gHeightfieldHeight) z = gHeightfieldHeight - 1;
        const uint16_t id = static_cast<uint16_t>(x + (z << 7) + 1);
        unsigned char* const base = reinterpret_cast<unsigned char*>(gGameWorldRef) + 0x2d8;
        const CMapCell* const cell = reinterpret_cast<const CMapCell*>(base + (static_cast<int32_t>(id) - 1) * 0x44);
        allowed = cell->IsWalkable() != 0 || cell->IsPath() != 0 || cell->IsQueue() != 0 || cell->IsGrass() != 0;
    }
    if (!allowed) return;
    SetCameraMode(2, 1, 0, 0, nullptr, 0);
    if ((gFirstPersonFlags & 0x16) != 0) {
        std::memcpy(gCameraSavedPosition, gCameraPosition, sizeof gCameraSavedPosition);
        gCameraSavedYaw = gCameraF5BF4;
        gCameraPosition[0] = gPointerWorldPosn.x;
        gCameraPosition[1] = 0.0f;
        gCameraPosition[2] = gPointerWorldPosn.z;
        ESP_SwitchToFirstPersonSprites();
    } else {
        std::memcpy(gCameraPosition, gCameraSavedPosition, sizeof gCameraPosition);
        gCameraF5BF4 = gCameraSavedYaw;
        ESP_SwitchToThirdPersonSprites();
    }
}
