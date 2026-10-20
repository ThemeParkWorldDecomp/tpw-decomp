// The original two-pass ride-animation loader. Native records grow with their pointers;
// mesh and animation headers themselves retain their on-disk 32-bit layout.
#include "TPEngine.h"
#include "TPConfig.h"
#include "TPGlobals.h"
#include "Quickload.h"
#include "Debug.h"
#include "rsse.h"
#include "platform/macdoze.h"
#include <cstdio>
#include <cstring>

int32_t LoadRideAnimSet(char* name, uint32_t flags, tag_sRideAnimSet** output, char* loadedName, uint32_t timeCount) {
    tag_sRideAnimSet local{};
    char filename[1040], stem[1024];
    char* path = GetMeshPath();
    if (path == nullptr || (flags & 0x200) != 0) std::sprintf(filename, "%s", name);
    else std::sprintf(filename, "%s%s", path, name);
    char* dot = std::strrchr(filename, '.');
    if (dot == nullptr || dot == filename || dot[1] == ':') std::strcat(filename, ".md2");
    std::strcpy(stem, name);
    dot = std::strrchr(stem, '.');
    // The original's comparison is against the other buffer, so a leading dot here is
    // also removed. Keep that behaviour rather than correcting its filename handling.
    if (dot != nullptr && dot[1] != ':') *dot = '\0';
    local.mHeads = LoadMesh(filename, nullptr, 1, gTPGlobals.mEngineFlags & 8, 0);
    if (local.mHeads == nullptr) return 0;
    UpdateScratchGeomBufferSize(local.mHeads);
    tag_sM3D2Header* header = local.mHeads->mHeader;
    if ((flags & 0x20000) == 0 && (header->mFlags & 4) != 0) {
        ERRORBox("Ride %s has relative animation incorrectly set\nStripping flag, animation will not work!\n", header->mName);
        header->mFlags = (header->mFlags & ~4u) | 0x1000000;
    }
    if (loadedName != nullptr) std::strcpy(loadedName, filename);
    if ((flags & 8) != 0) {
        for (uint32_t i = 0; i < header->mNodeCount; ++i) {
            tag_sNode& node = header->mNodes.get()[i];
            if (NS_MacDoze::_stricmp(node.mName.get(), "Sky") == 0) node.mFlags |= 0x4000000;
        }
    }
    if ((flags & 0x100) != 0) local.mFlags |= 8;
    if ((flags & 0x10) != 0) header->mFlags |= 0x800;
    if ((flags & 0x80) != 0) header->mFlags |= 0x100;
    if ((flags & 0x800) != 0) header->mFlags |= 0x20000;
    if ((flags & 0x1000) != 0) local.mFlags |= 0x100000;
    if ((flags & 0x2000) != 0) local.mFlags |= 0x2000000;
    if ((flags & 0x4000) != 0) header->mFlags |= 0x200000;
    if ((flags & 0x8000) != 0) local.mFlags |= 0x10000000;
    if ((flags & 0x800000) != 0) local.mFlags |= 0x20000000;

    constexpr char kinds[13] = "CDILSMEUWBRO";
    sRideAnimEntry* nextEntry = nullptr;
    for (int32_t pass = 0; pass < 2; ++pass) {
        if (pass != 0) {
            local.mAnimations = static_cast<tag_sAnimHeader**>(MReleaseAllocF(local.mAnimationCount * sizeof(tag_sAnimHeader*), 1));
            local.mAnimationHeaders = static_cast<tag_sM3D2Header**>(MReleaseAllocF(local.mAnimationCount * sizeof(tag_sM3D2Header*), 1));
            nextEntry = static_cast<sRideAnimEntry*>(MReleaseAllocF(local.mAnimationCount * sizeof(sRideAnimEntry), 1));
            local.mEntryAllocation = nextEntry;
            local.mAnimationCount = 0;
        }
        for (int32_t kind = 0; kind < 12; ++kind) {
            sRideAnimGroup& group = local.mGroups[kind];
            const uint32_t firstPassCount = group.mCount;
            group.mCount = 0;
            bool numbered = pass == 0 || (group.mFlags & 2) != 0;
            while (true) {
                if (numbered) std::sprintf(filename, "%s%s%c%u.md2", path, stem, kinds[kind], group.mCount + 1);
                else std::sprintf(filename, "%s%s%c.md2", path, stem, kinds[kind]);
                tag_sM3D2Header* animationHeader = nullptr;
                bool found;
                if (pass != 0) {
                    found = false;
                    if ((group.mFlags & 3) != 0) {
                        animationHeader = LoadM3D2Header(filename, 0);
                        if (animationHeader == nullptr || animationHeader->mAnim == nullptr) {
                            if (animationHeader != nullptr) DestroyMeshHeader(animationHeader);
                        } else found = true;
                    }
                } else found = DoesFileExist(filename, gQuickloadEnabled == 0 ? 0 : 2) != 0;
                if (pass != 0 && group.mCount == 0) group.mFlags &= ~3u;
                if (!found) {
                    if (numbered && group.mCount == 0) { numbered = false; continue; }
                    break;
                }
                if (pass != 0) {
                    if (group.mEntries == nullptr) group.mEntries = nextEntry;
                    ++nextEntry;
                    local.mAnimations[local.mAnimationCount] = animationHeader->mAnim;
                    local.mAnimationHeaders[local.mAnimationCount] = animationHeader;
                    group.mEntries[group.mCount] = {local.mAnimationCount, animationHeader->mAnim};
                }
                group.mFlags |= numbered ? 2 : 1;
                ++group.mCount;
                ++local.mAnimationCount;
                if (group.mCount == firstPassCount || !numbered) break;
            }
        }
    }
    *output = static_cast<tag_sRideAnimSet*>(MReleaseAllocF(sizeof(tag_sRideAnimSet) + timeCount * 0x38, 1));
    local.mTimeControls = reinterpret_cast<uint8_t*>(*output) + sizeof(tag_sRideAnimSet);
    local.mTimeCount = static_cast<uint16_t>(timeCount);
    for (uint32_t i = 0; i < local.mAnimationCount; ++i) RAInitType(local.mHeads, local.mAnimations[i]);
    if (local.mHeads != nullptr && local.mHeads->mHeader != nullptr) {
        if ((flags & 0x20) != 0) {
            header->mFlags |= 0x20;
            header->mF78->mFlags |= 0x80800;
            local.mFlags |= 0x1000000;
            OverrideActiveChannels(&local);
        }
        if ((flags & 0x40) != 0) {
            header->mFlags |= 0x80;
            for (uint32_t i = 0; i < header->mNodeCount; ++i) header->mNodes.get()[i].mFlags |= 0x20000;
        }
    }
    if ((flags & 0x400) != 0) StripRideWrapping(&local, nullptr, 0.02f, nullptr, nullptr);
    CorrectNodeFlags1(&local);
    if ((flags & 8) == 0) CreateMeshTypeEngineMesh(local.mHeads);
    if ((flags & 10) != 0) {
        InitRideAnimSet(&local, 1, flags & 0x40000, 0);
        if ((flags & 8) == 0) CreateEngineMesh(local.mHeads);
        if ((flags & 6) == 6) local.mFlags |= 4;
        if ((flags & 8) != 0 && header->mHField != nullptr) {
            header->mFlags |= 0x400;
            header->mFA8[3] = 0;
            header->mFA8[2] = static_cast<uint8_t>(header->mHField->mF18[0] - 1);
            header->mFA8[1] = 0;
            header->mFA8[0] = static_cast<uint8_t>(header->mHField->mF18[1] - 1);
        }
    }
    TPInitialiseEmbeddedData(local.mHeads);
    if (*output != nullptr) **output = local;
    if ((flags & 10) != 0) (*output)->mSelf = *output;
    return 1;
}
