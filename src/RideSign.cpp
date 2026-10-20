// Ride signs (main app): 0x101ee290, 0x101eea14 and 0x101eed5c.
#include "TPEngine.h"
#include "SignRenderer.h"
#include "Debug.h"
#include "FS_System.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "Text.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "tb_resource_texture.h"
#include "ui_ErrorDisplay.h"
#include "platform/msl.h"

#include <cstdio>
#include <cstring>

int32_t GenerateRideSign(tag_sRideAnimSet* set, char* folder, char* name, char* fontPath,
                         const char16_t* sign1, const char16_t* sign2) {
    SignRenderer renderer;
    if (sign1 == nullptr) return 0;
    tag_sM3D2Header* header = set->mHeads->mHeader;
    char16_t text1[256];
    char16_t text2[256];
    if (sign2 != nullptr) {
        wcscpy(text1, sign1);
        wcscpy(text2, sign2);
    } else {
        const uint32_t length = wcslen(sign1);
        bool split = false;
        for (uint32_t i = 0; i < length; ++i) {
            const int32_t step = static_cast<int32_t>((i + 1) >> 1);
            const int32_t at = static_cast<int32_t>(length >> 1) + ((i & 1) ? -step : step);
            if (sign1[at] != u' ') continue;
            wcsncpy(text1, sign1, at);
            text1[at] = 0;
            wcsncpy(text2, sign1 + at + 1, length - at);
            split = true;
            break;
        }
        if (!split) {
            wcscpy(text1, sign1);
            text2[0] = 0;
        }
    }
    uint32_t indices[2] = {0xffffffff, 0xffffffff};
    sTextureInfo* infos[2] = {};
    sTexture* textures[2] = {};
    RideSignTextureRecord* records[2] = {};
    RideSignTextureRecord* block = nullptr;
    int32_t found = 0;
    bool needs = false;
    for (uint32_t i = 0; i < header->mTextureCount; ++i) {
        sTextureInfo& info = header->mTextureInfos[i];
        sTexture& texture = header->mTextures[i];
        int32_t which;
        if (info.mFlags & 8) which = 0;
        else if (info.mFlags & 0x10) which = 1;
        else {
            if ((info.mFlags & 2) || info.mFrames != 1) continue;
            if (NS_MacDoze::_stricmp(info.mName, "sign1.tga") == 0) which = 0;
            else if (NS_MacDoze::_stricmp(info.mName, "sign2.tga") == 0) which = 1;
            else continue;
            if (indices[which] != 0xffffffff) continue;
            if (block == nullptr) block = static_cast<RideSignTextureRecord*>(MReleaseAllocF(0x2c, 1));
            records[which] = block + which;
            records[which]->mWidth[0] = 0;
            records[which]->mWidth[1] = 0x80;
            records[which]->mHeight[0] = 0;
            records[which]->mHeight[1] = 0x80;
            info.mName = reinterpret_cast<char*>(records[which]);
            info.mFlags |= which == 0 ? 0xa : 0x12;
            texture.mF00 |= 0x480;
            needs = true;
        }
        ++found;
        texture.mF00 |= 0x400;
        records[which] = reinterpret_cast<RideSignTextureRecord*>(info.mName.get());
        if (!(records[which]->mFlags & 2)) needs = true;
        indices[which] = i;
        infos[which] = &info;
        textures[which] = &texture;
    }
    auto fail = [&]() {
        for (int32_t i = 0; i < 2; ++i) {
            if (infos[i] != nullptr) infos[i]->mFlags &= ~2u;
            if (textures[i] != nullptr) textures[i]->mF00 &= ~0x80u;
        }
        if (block != nullptr) MReleaseFreeF(block);
        return 0;
    };
    if (found != 2) {
        if (found == 1) ERRORBox("Ride %s does not have correctly set up signs\n", header->mName);
        return fail();
    }
    int32_t result = 1;
    if (needs) {
        tag_sMesh* mesh = set->mHeads;
        CMesh* engine = mesh->mEngineMesh != nullptr ? mesh->mEngineMesh : mesh->mTypeEngineMesh;
        result = engine->GenerateRideSign(indices, 2);
        records[0]->mData.mOffset = indices[0];
        records[1]->mData.mOffset = indices[1];
        records[0]->mFlags |= 2;
        records[1]->mFlags |= 2;
    }
    if (result == 0) return fail();
    auto findNode = [&](sTexture* texture) -> tag_sNode* {
        tag_sM3D2Header* h = set->mHeads->mHeader;
        for (uint32_t i = 0; i < h->mAllNodeCount; ++i) {
            tag_sNode& node = h->mNodes[i];
            for (uint32_t j = 0; j < node.mFaceSetCount; ++j)
                if (node.mFaceSets[j].mTexture.get() == texture) return &node;
        }
        return nullptr;
    };
    tag_sNode* node1 = findNode(textures[0]);
    tag_sNode* node2 = findNode(textures[1]);
    StripRideWrapping(set, node1, 0.02f, nullptr, nullptr);
    if (node2 != node1) StripRideWrapping(set, node2, 0.02f, nullptr, nullptr);
    char folderCopy[1040];
    char settings[1024];
    std::strcpy(folderCopy, folder);
    const size_t length = std::strlen(folderCopy);
    if (length != 0 && folderCopy[length - 1] == ':') folderCopy[length - 1] = 0;
    if (name == nullptr) {
        char* colon = std::strrchr(folderCopy, ':');
        name = colon != nullptr ? colon + 1 : folderCopy;
    }
    std::sprintf(settings, "%s:%s_%s.sgn", folderCopy, gText.GetLanguageName(), name);
    if (CheckFileExists(settings) == 0) std::sprintf(settings, "%s:%s.sgn", folderCopy, name);
    renderer.sign_load_settings(settings);
    renderer.sign_install_fonts(fontPath);
    const int32_t format = gei_TextureSettings.mFormat;
    sign_renderer_options options = {text1, text2, records[0]->mData.get(), records[1]->mData.get(),
                                      0x80, 0x80, (format <= 2 || format == 4 || format >= 6) ? 2 : 1};
    auto* dc = NS_MacDoze::GetDC(gMainWindow);
    renderer.sign_render(dc, options);
    NS_MacDoze::ReleaseDC(gMainWindow, dc);
    header->mFlags |= 0x8000;
    if (!(set->mFlags & 1)) set->mFlags |= 0x8000000;
    return 1;
}

void DestroyRideSign(tag_sRideAnimSet* set) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    if (!(header->mFlags & 0x8000)) return;
    for (uint32_t i = 0; i < header->mTextureCount; ++i) {
        sTextureInfo& info = header->mTextureInfos[i];
        if (!(info.mFlags & 8)) continue;
        MReleaseFreeF(info.mName.get());
        info.mName = nullptr;
        return;
    }
}

int32_t RegenerateRideSign(uint32_t instance, char* folder, char* name,
                           const char16_t* sign1, const char16_t* sign2) {
    if (instance == 0) return 0;
    const char* path = gTPFileInfo.GetLocalPath().mData;
    if (path == nullptr) return 0;
    char fontPath[1032];
    const size_t length = std::strlen(path);
    std::sprintf(fontPath, length != 0 && path[length - 1] == ':' ? "%s%s" : "%s:%s", path, "Data:Fonts");
    tag_sRideAnimSet* set = gRideAnimSets[instance];
    const int32_t result = gPlayerSettings.mCurrent.mRenderer == 0 ? 1 :
        GenerateRideSign(set, folder, name, fontPath, sign1, sign2);
    if (set != nullptr && !(set->mFlags & 1)) set->mFlags |= 0x8000000;
    return result;
}
