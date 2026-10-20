#include "tpw_probe.h"
// Ride types and ride instances (main app of SimTheme Park 2.04b3; the inventory's TPRide.cpp
// range): LoadRideType 0x10068fd0 (1,216 bytes), InstanceRide 0x10069b24 (1,496),
// DestroyRideInstance 0x1006a3ec (464) and DestroyAllRideTypes 0x1006a9f0 (84).
//
// A ride type is a ride animation set loaded from a mesh file (LoadRideAnimSet), numbered 1 up
// in gRideTypes. An instance is a copy of a type's set (InstanceRideAnimSet), numbered 1 up in
// gRideAnimSets. The lobby makes every mesh of the island one of each. Everything the four
// functions call besides the library's string functions belongs to other files.
#include <cstdio>
#include <cstring>

#include "Debug.h"
#include "Game.h"
#include "Quickload.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "rsse.h"
#include "tb_mesh.h"

tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];
uint32_t gRideFirstFree;
uint32_t gRideCountA;
uint32_t gRideCountB;
tag_sRideAnimSet* gRideTypes[kMaxRideTypes];
uint32_t gRideTypeCount;

namespace {

// The flags LoadRideType hands to LoadRideAnimSet for the flags it is given.
struct sFlagMap {
    uint32_t mRequest;
    uint32_t mLoad;
};
constexpr sFlagMap kFlagMap[] = {
    {0x10, 0x2},        {0x20, 0x10},      {0x40, 0x20},       {0x2, 0x40},     {0x100, 0x1},
    {0x200, 0x100},     {0x4, 0x80},       {0x4000, 0x400},    {0x10000, 0x800}, {0x40000, 0x1000},
    {0x100000, 0x2000}, {0x200000, 0x4000}, {0x400000, 0x8000}, {0x1000000, 0x20000},
};

// True when the text has no extension of its own: no '.' at all, or a '.' that a ':' follows
// (a folder called ".", as in "disc:.:name").
bool NeedsExtension(const char* dot) { return dot == nullptr || dot[1] == ':'; }

}  // namespace

// Loads the ride mesh `name` of `folder` and records it as the next ride type. Returns the type's
// number, or 0 when the folder is not set up or the mesh does not load. The size (width and depth
// in cells) comes from `a` (two ints) when `d` or `e` is 0.
// The original stores the loader's flag 1 into the set LoadRideAnimSet made even when that call
// failed, through a pointer it never set (stack garbage); here a failed load that left no set is
// not stored to.
uint32_t LoadRideType(char* folder, char* name, void* a, void* b, uint32_t flags, uint32_t id, uint32_t c,
                      const char16_t* sign1, const char16_t* sign2, int32_t d, int32_t e) {
    tpw_probe::Hit(tpw_probe::kLoadRideType);
    if (d == 0) d = a != nullptr ? static_cast<int32_t*>(a)[0] : 0;
    if (e == 0) e = a != nullptr ? static_cast<int32_t*>(a)[1] : 0;
    uint32_t loadFlags = 0;
    for (const sFlagMap& map : kFlagMap) {
        if ((flags & map.mRequest) != 0) loadFlags |= map.mLoad;
    }
    if (c == 0) c = 1;
    char file[1024];
    std::strcpy(file, name);
    if (NeedsExtension(std::strrchr(file, '.'))) std::strcat(file, ".md2");
    char shared[1024];
    BuildSharedTexturePath(shared);
    int32_t loaded = SetupDirectoryForMeshLoad(file, folder, shared);
    tag_sRideAnimSet* set = nullptr;
    char loadedName[1026];
    if (loaded == 1) {
        loaded = LoadRideAnimSet(name, loadFlags, &set, loadedName, c);
        if (set != nullptr) set->mFlags |= 1;
    }
    if (loaded == 1 && (flags & 0x20000) != 0) {
        // The "p" variant of the mesh, kept in the set (+0xd0).
        char variant[1024];
        std::snprintf(variant, sizeof variant, "p%s", name);
        LoadRideAnimSet(variant, 0, &set->mFD0, nullptr, 1);
    }
    if (loaded != 1) { tpw_probe::Hit(tpw_probe::kInstanceRideFailed, 1000); return 0; }
    set->mFF8 = id;
    set->mFF0 = d;
    set->mFF4 = e;
    if ((flags & 1) != 0) CalculateSkirtVertices(set, gHeightfieldHeader, a, d, e);
    if ((flags & 0x2000) != 0) {
        tag_sMesh* mesh = set->mHeads;
        if (mesh->mRootNode != nullptr) SetBaseBlendFlags(mesh, mesh->mRootNode, 1);
        else SetBaseBlendFlags(mesh, mesh->mHeader->mF78.get(), 1);
        if (set->mFD0 != nullptr) {
            mesh = set->mFD0->mHeads;
            SetBaseBlendFlags(mesh, mesh->mHeader->mF78.get(), 1);
        }
    }
    CorrectFaceFlags1(set->mHeads, flags & 0x400000);
    if ((flags & 8) != 0 && b != nullptr) GenerateHoardingMeshes(gGame.GetCurrentThemeName().mData, set, b);
    if ((flags & 0x80) != 0) SetupRideHeads(set->mHeads);
    if ((flags & 0x400) != 0) {
        // The height map file: the mesh's file name with ".hmp" in place of its extension.
        char* dot = std::strrchr(loadedName, '.');
        if (NeedsExtension(dot)) std::strcat(loadedName, ".hmp");
        else std::strcpy(dot, ".hmp");
        GetRideHeightMap(set, d, e, loadedName, a, flags & 0x800);
        if (set->mFD0 != nullptr) set->mFD0->mHeightMap = set->mHeightMap;
    }
    char fontPath[1024];
    if (GetFontPath(fontPath) == 1) {
        if (sign1 == nullptr) {
            const char16_t* typeName = gRideDatabase.GetTypeName(static_cast<uint16_t>(id));
            GenerateRideSign(set, folder, name, fontPath, typeName, nullptr);
        } else {
            GenerateRideSign(set, folder, name, fontPath, sign1, sign2);
        }
    } else {
        ERRORBox("Ridename texture generator: Could not find local path\n");
    }
    CorrectNodeFlags2(set);
    ++gRideTypeCount;
    gRideTypes[gRideTypeCount] = set;
    return gRideTypeCount;
}

// Makes an instance of ride type `type` (its "p" variant with flag 0x400 when it has one) and
// puts it in gRideAnimSets: at number `f`, or the first free one when `f` is 0xffffffff, which also
// keeps the three counters (see TPEngine.h). Returns the number, or 0 for no such type or an
// instance that could not be made. The flags: 1 and 2 and 0x80 become set flags 4, 0x4000 and
// 0x8000; 0x26 places the ride from `params` (2 also sets the world's bounding area, 4 the
// hoardings' heights, 8 the skirt deformation, with a height from GetRideRequiredYOffset when
// params has an object); 0x100 turns the mesh's drawing on; 0x200 makes a coaster of it.
// `a` is the number of time controls, `b` is kept in the set (+0xd4), `c` and `g` go to InstanceCoaster.
// The original also reads params+0xc when `d` or `e` is 0 and then drops it. With flag 8 but none
// of 0x26 the height it passes on is whatever the caller left in f31 (0 here), and with 0x200 or
// 8 but without 0x26 the cell it passes on is stack garbage (0 here).
uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c,
                      int32_t d, int32_t e, uint32_t f, int32_t g) {
    tpw_probe::Hit(tpw_probe::kInstanceRide);
    if (type == 0) { tpw_probe::Hit(tpw_probe::kInstanceRideFailed); return 0; }
    tag_sRideAnimSet* typeSet = gRideTypes[type];
    if (typeSet == nullptr) { tpw_probe::Hit(tpw_probe::kInstanceRideFailed); return 0; }
    uint32_t instanceFlags = (flags & 0x800) != 0 ? 0x231f : 0x211f;
    tag_sRideAnimSet* instance = nullptr;
    int32_t made;
    if ((flags & 0x400) != 0 && typeSet->mFD0 != nullptr) {
        made = InstanceRideAnimSet(typeSet->mFD0, nullptr, instanceFlags, &instance, a);
    } else {
        made = InstanceRideAnimSet(typeSet, nullptr, instanceFlags, &instance, a);
    }
    if (made != 1) { tpw_probe::Hit(tpw_probe::kInstanceRideFailed); return 0; }
    instance->mFlags &= ~1u;
    instance->mFE4 = flags;
    instance->mFD4 = b;
    // The number is index + 1.
    uint32_t index;
    if (f == 0xffffffff) {
        index = gRideFirstFree;
        gRideAnimSets[index + 1] = instance;
        if (index == gRideCountB) {
            ++gRideFirstFree;
            ++gRideCountB;
        } else {
            // A hole below the end was filled: look for the next one.
            uint32_t next = index + 1;
            while (next <= gRideCountB + gRideCountA) {
                if (gRideAnimSets[next + 1] == nullptr) {
                    gRideFirstFree = next;
                    break;
                }
                ++next;
            }
            --gRideCountA;
            ++gRideCountB;
        }
    } else {
        index = f - 1;
        gRideAnimSets[f] = instance;
    }
    if ((flags & 1) != 0) instance->mFlags |= 4;
    if ((flags & 2) != 0) instance->mFlags |= 0x4000;
    if ((flags & 0x80) != 0) instance->mFlags |= 0x8000;
    InitRideAnimSet(instance, 1, 0, 0);
    if (IsValidAnimation(instance, 0, 0) != 0) {
        SetCurRideAnim(instance, 0, 0, 0, 1.0f, 0);
        SetCurRideAnim(instance, 0xd, 0, 0, 1.0f, 0);
    }
    int32_t x = 0;
    int32_t z = 0;
    int32_t cellX = 0;
    int32_t cellZ = 0;
    float y = 0.0f;
    if ((flags & 0x26) != 0) {
        x = params->mX;
        z = params->mZ;
        const int32_t rotation = params->mRotation;
        void* object = params->mF0C;
        instance->mFE8 = params->mX;
        instance->mFEC = params->mZ;
        if (gHeightfieldHeader != nullptr && gHeightfieldHeader->mHField != nullptr) {
            CorrectRidePosnForRotation(&x, &z, rotation, &cellX, &cellZ, instance->mFF0, instance->mFF4);
        }
        if (object != nullptr && (flags & 8) != 0) {
            y = GetRideRequiredYOffset(cellX, cellZ, object, instance->mFF0, instance->mFF4, flags, rotation);
        } else {
            sHField* field = gHeightfieldHeader->mHField;
            const uint32_t at = static_cast<uint32_t>(x) + static_cast<uint32_t>(z) * (field->mF18[0] + 1);
            y = field->mHeights.get()[at];
        }
        SetRidePositionAndRotation(instance, cellX, cellZ, x, z, rotation, y, flags, instance->mFF0, instance->mFF4);
    }
    if ((flags & 8) != 0) {
        DoMeshSkirtDeform(instance->mHeads, gHeightfieldHeader, y, 0);
        if ((gGameOptions & 0x4000) != 0 && (instance->mHeads->mF00 & 0x40) != 0) GetBlendedBaseTextures(instance);
    }
    if ((flags & 2) != 0) SetWorldBoundingArea(instance->mHeads->mHeader, instance->mHeads->mHeader);
    if ((flags & 4) != 0) SetHoardingYOffsets(instance);
    tag_sMesh* mesh = instance->mHeads;
    if (mesh->mRootNode != nullptr) {
        int32_t calculated = 0;
        CalculateRootNodeFaceNormals(mesh->mRootNode, mesh->mF10, calculated);
        if (calculated != 0) mesh->mF30 = mesh->mRootNode->mPoints.get() + mesh->mRootNode->mVertexCount;
    }
    if ((flags & 0x100) != 0) {
        if ((instance->mFlags & 0x4000) == 0) EnableMeshDraw(instance->mHeads);
        else EnableMeshDraw(instance->mHeads, instance->mFC0, instance->mFC4);
    }
    if (instance->mHeads->mF28 != nullptr && (instance->mFlags & 0x20000000) == 0) TPProcessEmbeddedData(instance->mHeads);
    if ((flags & 0x200) != 0 && IsCoasterType(type) != 0) InstanceCoaster(type, index + 1, cellX, cellZ, g, c);
    // The header remembers its instance, and the engine mesh calls AnimateRideCallback every frame.
    instance->mHeads->mHeader->mRideSet = instance;
    if (instance->mHeads->mEngineMesh != nullptr) instance->mHeads->mEngineMesh->mCallback = AnimateRideCallback;
    if ((instance->mFlags & 0x1000000) != 0) OverrideActiveChannels(instance);
    if ((instance->mFlags & 0x2000000) != 0) StartFlashingObject(instance);
    tag_sM3D2Header* header = instance->mHeads->mHeader;
    tag_sNode* node = header->mNodes.get();
    for (uint32_t left = header->mNodeCount; left != 0; --left, ++node) node->mFlags |= 0x10000;
    header->mFlags |= 0x40000;
    RecalculateInvalidFaceNormals(instance->mHeads->mHeader);
    return index + 1;
}

// Takes instance `instance` out of the game: what points at it forgets it, its animation set is
// destroyed and its number is given back (the three counters, see TPEngine.h).
void DestroyRideInstance(uint32_t instance) {
    if (instance == 0) return;
    tag_sRideAnimSet* set = gRideAnimSets[instance];
    if (set == nullptr) {
        CTRACE(5, "Cannot find a ride to destroy at slot %d - returning\n", instance);
        return;
    }
    if (gHighlightedRide == set) UnhighlightRide();
    if (set == gPotentialHighlightedRide) gPotentialHighlightedRide = nullptr;
    if (IsRideFirstPersonRide(instance) != 0) StopRidingRideIn1stPerson();
    if ((set->mFlags & 0x4000000) != 0) StopFlashingObject(set);
    DestroyCoasterInstanceFromEntranceID(instance);
    DestroyRideAnimSetInstance(set, nullptr, 0);
    gRideAnimSets[instance] = nullptr;
    const uint32_t index = instance - 1;
    if (gRideCountA == 0) {
        if (index == gRideCountB - 1) {
            --gRideFirstFree;
            --gRideCountB;
        } else {
            gRideCountA = 1;
            gRideFirstFree = index;
            --gRideCountB;
        }
    } else if (index == gRideCountB + gRideCountA - 1) {
        // The last number goes: the free numbers that now end the range go with it.
        --gRideCountB;
        while (gRideCountA != 0 && gRideAnimSets[gRideCountB + gRideCountA] == nullptr) --gRideCountA;
    } else {
        ++gRideCountA;
        --gRideCountB;
        if (index < gRideFirstFree) gRideFirstFree = index;
    }
}

// Destroys every ride type, last first, and forgets them.
void DestroyAllRideTypes() {
    for (uint32_t type = gRideTypeCount; type != 0; --type) DestroyRideType(type);
    gRideTypeCount = 0;
}
