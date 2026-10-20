// The mesh file loader (engine library of SimTheme Park 2.04b3, 0x100483f4..0x1004b9c8 and
// AlignTo64Bit at 0x10039078; the original file's name is not in the binary, the inventory
// places the range after tb_printf_opengl.cpp).
//
// LoadM3D2Header reads a whole .md2 file into one block and makes it usable in place.
// The file is little-endian and holds offsets from its start where the structures of
// include/m3d2.h have pointers. The original does two things to every record: it swaps
// each field to the Mac's byte order, and it adds the block's address to each offset that
// is not 0. Here the swaps go through SamsUtilities' block swappers, which do nothing on a
// little-endian host, and an offset becomes a pointer with Ptr32::Relocate.
//
// Two bit fields are the exception to "swap each field": the Mac's compiler numbers the
// bits of a bit field from the other end, so the original REBUILDS those words instead of
// swapping them, and the engine's code then reads the rebuilt layout. They are rebuilt
// here on every host, from the file's little-endian value, into the value the Mac has:
//   a face's first word: index (15 bits) and flag (1 bit) become index << 1 | flag;
//   a compressed vertex: three 10-bit fields a, b, c (from bit 0 up) become
//   a << 22 | b << 12 | c << 2.
#include "m3d2.h"

#include <cstring>

#include "Wavelet.h"
#include "platform/samsutils.h"
#include "tb_loaders.h"

namespace {

// One field in the Mac's byte order (nothing to do on a little-endian host).
void Swap32(uint32_t& word) { SamsUtilities::_USwapBlock32(&word, 1); }
void Swap16(uint16_t& word) { SamsUtilities::_USwapBlock16(&word, 1); }
void SwapFloat(float& value) { SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&value), 1); }
template <class T>
void SwapPointer(Ptr32<T>& pointer) {
    SamsUtilities::_USwapBlock32(&pointer.mOffset, 1);
}
void SwapWords32(void* words, int32_t count) {
    SamsUtilities::_USwapBlock32(static_cast<uint32_t*>(words), count);
}
void SwapWords16(void* words, int32_t count) {
    SamsUtilities::_USwapBlock16(static_cast<uint16_t*>(words), count);
}

// The file's little-endian value of a field, whatever the host.
uint32_t FileWord16(const void* field) {
    const auto* b = static_cast<const uint8_t*>(field);
    return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8;
}
uint32_t FileWord32(const void* field) {
    const auto* b = static_cast<const uint8_t*>(field);
    return static_cast<uint32_t>(b[0]) | static_cast<uint32_t>(b[1]) << 8 | static_cast<uint32_t>(b[2]) << 16 |
           static_cast<uint32_t>(b[3]) << 24;
}

}  // namespace

// 0x10039078 and 0x100390cc: rounds a size or an address up to a multiple of eight; what
// was added goes onto *moved.
void AlignTo64Bit(uint32_t& value, uint32_t* moved) {
    const uint32_t aligned = (value + 7) & ~7u;
    if (moved != nullptr) *moved += aligned - value;
    value = aligned;
}
void AlignTo64Bit(void*& address, uint32_t* moved) {
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(address) + 7) & ~static_cast<uintptr_t>(7);
    if (moved != nullptr) *moved += static_cast<uint32_t>(aligned - reinterpret_cast<uintptr_t>(address));
    address = reinterpret_cast<void*>(aligned);
}

void s3DPoint::Byteswap() { SwapWords32(this, 3); }
void sAlignedBox::Byteswap() { SwapWords32(this, 6); }
void sQuatKey::Byteswap() {
    Swap16(mF00);
    Swap16(mF02);
    SwapWords32(mF04, 4);
}

// 0x10049574. The original hands the allocator the header's own block field, which the
// allocator clears inside the block it has just freed. Here the block is freed and the
// freed memory is left alone.
void FreeM3D2Header(tag_sM3D2Header* header) {
    void* block = header->mBlock;
    gFree(block);
}

// ---- the header's tables ---------------------------------------------------------------
void FixMainPointers(tag_sM3D2Header* header, const void* base) {
    header->mPaths.Relocate(base);
    header->mTextures.Relocate(base);
    header->mTextureInfos.Relocate(base);
    header->mVertexSets.Relocate(base);
    header->mPoints.Relocate(base);
    header->mUVSets.Relocate(base);
    header->mFaceSets.Relocate(base);
    header->mFaces.Relocate(base);
    header->mHField.Relocate(base);
    header->mNodes.Relocate(base);
    header->mMiniNodes.Relocate(base);
    header->mF78.Relocate(base);
    header->mAnim.Relocate(base);
    header->mEmbedded.Relocate(base);
    header->mShapes.Relocate(base);
}

void FixEmbeddedRecords(sEmbeddedData* record, uint16_t count, const void* base) {
    if (record == nullptr) return;
    for (int32_t i = 0; i < count; ++i, ++record) {
        Swap32(record->mFlags);
        Swap32(record->mF04);
        Swap32(record->mF08);
        SwapPointer(record->mMorphParent);
        SwapPointer(record->mLight);
        if (record->mMorphParent) {
            record->mMorphParent.Relocate(base);
            sTrackMorphingParent* parent = record->mMorphParent;
            Swap16(parent->mF00);
            Swap16(parent->mBack);
            SwapWords32(parent->mF04, 3);
            // Four more words lie mBack bytes before the structure.
            if ((record->mFlags & 0x40000) != 0) SwapWords32(reinterpret_cast<uint8_t*>(parent) - parent->mBack, 4);
        }
        if (record->mLight) {
            record->mLight.Relocate(base);
            SwapWords32(record->mLight->mF00, 6);
        }
    }
}

void FixTextureInfoRecords(sTextureInfo* info, uint16_t count, int32_t oneFrame, const void* base) {
    for (uint32_t n = count; n != 0; --n, ++info) {
        Swap32(info->mFlags);
        SwapPointer(info->mFolder);
        Swap16(info->mFrames);
        SwapPointer(info->mName);
        if (info->mFolder) info->mFolder.mOffset = 0;
        info->mName.Relocate(base);
        if (oneFrame != 0) {
            info->mFlags &= ~1u;
            info->mFrames = 1;
        }
    }
}

void FixHeightFieldRecords(sHField* field, const void* base) {
    if (field == nullptr) return;
    Swap32(field->mF00);
    Swap16(field->mHeightCount);
    Swap16(field->mQuadCount);
    SwapWords32(field->mF08, 4);
    SwapWords32(field->mF18, 2);
    SwapWords32(field->mF20, 2);
    SwapPointer(field->mHeights);
    SwapPointer(field->mQuads);
    if (field->mHeights) {
        field->mHeights.Relocate(base);
        SwapWords32(field->mHeights.get(), field->mHeightCount);
    }
    if (field->mQuads) {
        field->mQuads.Relocate(base);
        SwapWords16(field->mQuads.get(), field->mQuadCount * 2);
    }
}

void FixUVRecords(sUVSet* sets, uint16_t count, const void*) {
    if (sets != nullptr) SwapWords32(sets, static_cast<int32_t>(count) << 3);
}

void FixShapeRecords(tag_sShape* shape, uint16_t count, const void* base) {
    if (shape == nullptr) return;
    for (int32_t i = 0; i < count; ++i, ++shape) {
        Swap32(shape->mF00);
        Swap16(shape->mPointCount);
        Swap16(shape->mWordPairs);
        SwapPointer(shape->mPoints);
        SwapPointer(shape->mWords);
        if (shape->mPoints) {
            shape->mPoints.Relocate(base);
            SwapWords32(shape->mPoints.get(), shape->mPointCount * 3);
        }
        if (shape->mWords) {
            shape->mWords.Relocate(base);
            SwapWords16(shape->mWords.get(), static_cast<int32_t>(shape->mWordPairs) << 1);
        }
    }
}

void FixTextureRecords(sTexture* texture, uint16_t count, const void*) {
    if (texture == nullptr) return;
    for (uint32_t n = count; n != 0; --n, ++texture) {
        Swap16(texture->mF00);
        Swap16(texture->mF02);
        SwapPointer(texture->mTexture);
    }
}

void FixFaceSetRecords(sFaceSet* set, uint16_t count, const void* base) {
    if (set == nullptr) return;
    for (uint32_t n = count; n != 0; --n, ++set) {
        SwapPointer(set->mTexture);
        Swap16(set->mFirstFace);
        Swap16(set->mFaceCount);
        Swap16(set->mF08);
        Swap16(set->mF0A);
        set->mTexture.Relocate(base);
    }
}

void FixFaceRecords(sFace* face, uint16_t count, const void*) {
    if (face == nullptr) return;
    for (uint32_t n = count; n != 0; --n, ++face) {
        const uint32_t file = FileWord16(&face->mIndexAndFlag);
        face->mIndexAndFlag = static_cast<uint16_t>((file & 0x7fff) << 1 | file >> 15);
        SwapWords16(face->mF02, 3);
    }
}

void FixNodePointers(tag_sNode* node, const void* base) {
    node->mF04.Relocate(base);
    node->mF08.Relocate(base);
    node->mF0C.Relocate(base);
    node->mVertices.Relocate(base);
    node->mPoints.Relocate(base);
    node->mUVs.Relocate(base);
    node->mFaceSets.Relocate(base);
    node->mFaces.Relocate(base);
    node->mF94.Relocate(base);
    node->mName.Relocate(base);
}

// The node's points have no count of their own: there are as many as its faces' largest
// index needs, and at least mF5E. (Its faces and face sets have been fixed by now.)
void FixNodeRecords(tag_sNode* node, uint16_t count, const void* base) {
    if (node == nullptr) return;
    for (int32_t i = 0; i < count; ++i, ++node) {
        Swap32(node->mFlags);
        SwapPointer(node->mF04);
        SwapPointer(node->mF08);
        SwapPointer(node->mF0C);
        node->mMatrix.Byteswap();
        Swap16(node->mF50);
        Swap16(node->mF52);
        SwapPointer(node->mName);
        Swap16(node->mVertexCount);
        Swap16(node->mFaceSetCount);
        Swap16(node->mF5C);
        Swap16(node->mF5E);
        SwapPointer(node->mVertices);
        SwapPointer(node->mPoints);
        SwapPointer(node->mUVs);
        SwapPointer(node->mFaceSets);
        SwapPointer(node->mFaces);
        SwapPointer(node->mF74);
        node->mBox.Byteswap();
        SwapPointer(node->mF94);
        SwapPointer(node->mF98);
        FixNodePointers(node, base);
        if (node->mVertices) {
            SwapWords32(node->mVertices.get(), static_cast<int32_t>((((node->mVertexCount + 3) >> 2) * 0x30u) >> 2));
        }
        if (node->mF94) SwapWords16(node->mF94.get(), node->mF5E);
        if (node->mPoints) {
            int32_t last = static_cast<int32_t>(node->mF5E) - 1;
            const sFaceSet* set = node->mFaceSets;
            if (set != nullptr) {
                for (int32_t s = 0; s < node->mFaceSetCount; ++s, ++set) {
                    const sFace* face = node->mFaces.get() + set->mFirstFace;
                    for (uint32_t n = set->mFaceCount; n != 0; --n, ++face) {
                        const int32_t index = face->mIndexAndFlag >> 1;
                        if (last < index) last = index;
                    }
                }
            }
            SwapWords32(node->mPoints.get(), (last + 1) * 3);
        }
    }
}

void FixMiniNodeRecords(tag_sMiniNode* node, uint16_t count, const void* base) {
    for (uint32_t n = count; n != 0; --n, ++node) {
        Swap32(node->mFlags);
        SwapPointer(node->mF04);
        SwapPointer(node->mF08);
        SwapPointer(node->mF0C);
        node->mMatrix.Byteswap();
        Swap16(node->mF50);
        Swap16(node->mF52);
        SwapPointer(node->mName);
        node->mF04.Relocate(base);
        node->mF08.Relocate(base);
        node->mF0C.Relocate(base);
        node->mName.Relocate(base);
    }
}

// ---- animation -------------------------------------------------------------------------
void FixAnimPointers(tag_sAnimHeader* anim, const void* base) {
    anim->mScaleKeys.Relocate(base);
    anim->mQuatKeys.Relocate(base);
    anim->mNodeAnims.Relocate(base);
    anim->mNodesToDisable.Relocate(base);
    anim->mF3C.Relocate(base);
    anim->mAnimatedTextures.Relocate(base);
    anim->mShapes.Relocate(base);
    anim->mUVAnims.Relocate(base);
}

void FixAnimTextureRecords(sAnimatedTexture* texture, uint16_t count, const void* base) {
    if (texture == nullptr) return;
    for (int32_t i = 0; i < count; ++i, ++texture) {
        Swap16(texture->mF00);
        Swap16(texture->mPairCount);
        SwapPointer(texture->mPairs);
        if (texture->mPairs) {
            texture->mPairs.Relocate(base);
            SwapWords16(texture->mPairs.get(), static_cast<int32_t>(texture->mPairCount) << 1);
        }
    }
}

void FixAnimNodePointers(tag_sNodeAnim* anim, const void* base) {
    anim->mShapes.Relocate(base);
    anim->mScaleKeys.Relocate(base);
    anim->mQuatKeys.Relocate(base);
    anim->mPath.Relocate(base);
    anim->mUVAnim.Relocate(base);
    anim->mEnables.Relocate(base);
    anim->mF34.Relocate(base);
}

void FixAnimScaleKeyRecords(sScaleKey* key, uint16_t count) {
    if (key == nullptr) return;
    for (uint32_t n = count; n != 0; --n, ++key) {
        Swap16(key->mF00);
        Swap16(key->mF02);
        SwapWords32(key->mF04, 3);
    }
}

void FixAnimQuatKeys(sQuatKey* key, uint16_t count, const void*) {
    if (key == nullptr) return;
    for (uint32_t n = count; n != 0; --n, ++key) key->Byteswap();
}

void FixAnimNodesToDisable(uint16_t* nodes, uint16_t count, const void*) {
    if (nodes != nullptr) SwapWords16(nodes, count);
}

void FixAnimPathRecords(sPathController* path, const void* base) {
    if (path == nullptr) return;
    Swap32(path->mF00);
    SwapWords32(&path->mWordCount, 1);
    Swap32(path->mF08);
    SwapPointer(path->mWords);
    if (path->mWords) {
        path->mWords.Relocate(base);
        SwapWords32(path->mWords.get(), path->mWordCount);
    }
}

void FixAnimEnableRecords(int16_t* enables, uint16_t count) {
    if (enables != nullptr) SwapWords16(enables, count);
}

void FixAnimUVRecords(sUVAnim* anim, uint16_t count, const void* base) {
    if (anim == nullptr) return;
    for (uint32_t n = count; n != 0; --n, ++anim) {
        Swap32(anim->mTrackCount);
        SwapPointer(anim->mTrack);
        SwapWords32(&anim->mCount, 1);
        SwapPointer(anim->mF0C);
        SwapPointer(anim->mF10);
        if (anim->mTrack) {
            anim->mTrack.Relocate(base);
            SwapWords16(anim->mTrack.get(), static_cast<int32_t>(anim->mTrackCount * 2));
        }
        if (anim->mF0C) {
            anim->mF0C.Relocate(base);
            SwapWords16(anim->mF0C.get(), anim->mCount);
        }
        if (anim->mF10) {
            anim->mF10.Relocate(base);
            SwapWords32(anim->mF10.get(), anim->mCount << 1);
        }
    }
}

// The node animation's morph data: with flag 0x4000 a short record with one list of
// points, otherwise a morph set with its key data and compressed vertices. Unlike every
// other pointer of the file this one is relocated here, not by FixAnimNodePointers.
void FixAnimMorphRecords(tag_sNodeAnim* anim, const void* base) {
    if (!anim->mMorph) return;
    anim->mMorph.Relocate(base);
    if ((anim->mFlags & 0x4000) != 0) {
        auto* points = reinterpret_cast<sMorphPoints*>(anim->mMorph.get());
        SwapPointer(points->mF00);
        Swap32(points->mPointCount);
        SwapPointer(points->mPoints);
        points->mF00.Relocate(base);
        if (points->mPoints) {
            points->mPoints.Relocate(base);
            SwapWords32(points->mPoints.get(), static_cast<int32_t>(points->mPointCount * 3));
        }
        return;
    }
    sMorphSet* set = anim->mMorph;
    Swap16(set->mF00);
    Swap16(set->mKeyDataCount);
    Swap16(set->mF04);
    Swap16(set->mDupedCount);
    Swap16(set->mF08);
    SwapPointer(set->mKeyData);
    SwapPointer(set->mDuped);
    set->mF14.Byteswap();
    set->mF20.Byteswap();
    if (set->mKeyData) {
        set->mKeyData.Relocate(base);
        sCMKeyData* key = set->mKeyData;
        for (int32_t i = 0; i < set->mKeyDataCount; ++i, ++key) {
            Swap16(key->mVertexCount);
            Swap16(key->mKeyCount);
            SwapPointer(key->mF04);
            SwapPointer(key->mF08);
            SwapPointer(key->mVertices);
            Swap32(key->mF10);
            if (key->mF04) {
                key->mF04.Relocate(base);
                SwapWords16(key->mF04.get(), key->mKeyCount);
            }
            if (key->mF08) {
                key->mF08.Relocate(base);
                SwapWords16(key->mF08.get(), key->mVertexCount);
            }
            if (key->mVertices) {
                key->mVertices.Relocate(base);
                sCVertex* vertex = key->mVertices;
                for (uint32_t n = static_cast<uint32_t>(key->mKeyCount) * key->mVertexCount; n != 0; --n, ++vertex) {
                    const uint32_t file = FileWord32(&vertex->mPacked);
                    vertex->mPacked = (file & 0x3ff) << 22 | ((file >> 10) & 0x3ff) << 12 | ((file >> 20) & 0x3ff) << 2;
                }
            }
        }
    }
    if (set->mDuped) {
        set->mDuped.Relocate(base);
        SwapWords16(set->mDuped.get(), set->mDupedCount * 2);
    }
}

void FixAnimNodeRecords(tag_sNodeAnim* anim, uint16_t count, const void* base) {
    if (anim == nullptr) return;
    for (int32_t i = 0; i < count; ++i, ++anim) {
        Swap32(anim->mF00);
        Swap32(anim->mFlags);
        Swap32(anim->mF08);
        Swap32(anim->mF0C);
        Swap16(anim->mF10);
        Swap16(anim->mF12);
        Swap16(anim->mF14);
        Swap16(anim->mEnableCount);
        SwapPointer(anim->mShapes);
        SwapPointer(anim->mQuatKeys);
        SwapPointer(anim->mScaleKeys);
        SwapPointer(anim->mPath);
        SwapPointer(anim->mMorph);
        SwapPointer(anim->mUVAnim);
        SwapPointer(anim->mEnables);
        SwapPointer(anim->mF34);
        SwapPointer(anim->mNode);
        FixAnimNodePointers(anim, base);
        FixAnimPathRecords(anim->mPath, base);
        FixAnimMorphRecords(anim, base);
        FixAnimEnableRecords(anim->mEnables, anim->mEnableCount);
        FixAnimUVRecords(anim->mUVAnim, 1, base);
    }
}

// ---- the loader ------------------------------------------------------------------------
tag_sM3D2Header* LoadM3D2Header(char* name, uint32_t flags) {
    if (name == nullptr) return nullptr;
    void* file = gFileOpen(name, "rb");
    if (file == nullptr) {
        SamsUtilities::UDebug("Error: Could not open M3D2 file '%s' for reading", name);
        return nullptr;
    }
    const int32_t size = gFileTell(file);   // the game installs its "size of the file" here
    void* block = gMalloc(static_cast<uint32_t>(size + 4));
    void* start = block;
    AlignTo64Bit(start, nullptr);
    gFileRead(start, static_cast<uint32_t>(size), 1, file);
    gFileClose(file);

    auto* header = static_cast<tag_sM3D2Header*>(start);
    Swap32(header->mMagic);
    Swap32(header->mVersion);
    Swap32(header->mAnimVersion);
    Swap32(header->mF0C);
    Swap32(header->mF10);
    Swap32(header->mF14);
    Swap32(header->mF2C);
    Swap32(header->mFlags);
    SwapWords16(&header->mF34, 12);
    SwapPointer(header->mPaths);
    SwapPointer(header->mTextures);
    SwapPointer(header->mTextureInfos);
    SwapPointer(header->mVertexSets);
    SwapPointer(header->mPoints);
    SwapPointer(header->mUVSets);
    SwapPointer(header->mFaceSets);
    SwapPointer(header->mFaces);
    SwapPointer(header->mHField);
    SwapPointer(header->mNodes);
    SwapPointer(header->mMiniNodes);
    SwapPointer(header->mF78);
    SwapPointer(header->mEmbedded);
    header->mBox.Byteswap();
    SwapPointer(header->mAnim);
    SwapPointer(header->mSelf);
    SwapFloat(header->mFA0);
    SwapPointer(header->mFA4);
    SwapPointer(header->mShapes);
    header->mBlock = block;

    const auto refuse = [&](const char* format) -> tag_sM3D2Header* {
        SamsUtilities::UDebug(format, name);
        if (start != nullptr) gFree(block);
        return nullptr;
    };
    if (header->mMagic != kM3D2Magic) return refuse("%s (BadFileType)");
    if (header->mVersion > kM3D2Version) return refuse("%s (OldCode)");
    if (header->mVersion < kM3D2Version && (flags & kM3D2OldMeshesToo) == 0) return refuse("%s (DeadMesh)");
    if ((flags & kM3D2NoAnimation) == 0) {
        if (!header->mAnim) return refuse("%s (Anim is Master)");
    } else if (header->mAnim) {
        return refuse("%s (Master is Anim)");
    }

    bool reported = false;
    header->mSelf = header;
    FixMainPointers(header, start);
    FixEmbeddedRecords(header->mEmbedded, header->mEmbeddedCount, start);
    if ((header->mFlags & 1) != 0) {
        FixTextureInfoRecords(header->mTextureInfos, header->mTextureCount, static_cast<int32_t>(flags & kM3D2OneFrame), start);
        FixHeightFieldRecords(header->mHField, start);
        FixUVRecords(header->mUVSets, header->mUVSetCount, start);
        FixShapeRecords(header->mShapes, header->mShapeCount, start);
        FixTextureRecords(header->mTextures, header->mTextureCount, start);
        FixFaceSetRecords(header->mFaceSets, header->mFaceSetCount, start);
        FixFaceRecords(header->mFaces, header->mFaceCount, start);
        FixNodeRecords(header->mNodes, header->mNodeCount, start);
        FixMiniNodeRecords(header->mMiniNodes, static_cast<uint16_t>(header->mAllNodeCount - header->mNodeCount), start);
    }
    tag_sAnimHeader* anim = header->mAnim;
    if (anim == nullptr) {
        if (header->mVersion < kM3D2Version) {
            SamsUtilities::UDebug("%s (OldMesh)", name);
            reported = true;
        }
    } else {
        Swap32(anim->mF00);
        Swap32(anim->mF04);
        Swap32(anim->mF08);
        SwapWords16(&anim->mShapeCount, 9);
        SwapPointer(anim->mShapes);
        SwapPointer(anim->mScaleKeys);
        SwapPointer(anim->mQuatKeys);
        SwapPointer(anim->mNodeAnims);
        SwapPointer(anim->mAnimatedTextures);
        SwapPointer(anim->mUVAnims);
        SwapPointer(anim->mNodesToDisable);
        SwapPointer(anim->mF3C);
        SwapPointer(anim->mF40);
        SwapPointer(anim->mF44);
        if (header->mAnimVersion > kM3D2AnimVersion) {
            header->mAnim = nullptr;
            header->mFlags &= ~0x20u;
            SamsUtilities::UDebug("%s (OldCode)", name);
            reported = true;
        } else if (header->mAnimVersion < kM3D2AnimVersion) {
            header->mAnim = nullptr;
            header->mFlags = 0;
            SamsUtilities::UDebug("%s (DeadAnim)", name);
            reported = true;
        } else {
            FixAnimPointers(anim, start);
            FixShapeRecords(anim->mShapes, anim->mShapeCount, start);
            FixAnimTextureRecords(anim->mAnimatedTextures, anim->mAnimatedTextureCount, start);
            FixAnimNodeRecords(anim->mNodeAnims, anim->mNodeAnimCount, start);
            FixAnimScaleKeyRecords(anim->mScaleKeys, anim->mScaleKeyCount);
            FixAnimQuatKeys(anim->mQuatKeys, anim->mQuatKeyCount, start);
            FixAnimNodesToDisable(anim->mNodesToDisable, anim->mDisableCount, start);
            // The original also reports "(OldAnim)" for a version below the one it takes,
            // which the test above has already turned away: it cannot happen.
        }
    }
    if (!reported) SamsUtilities::UDebug("%s (Ok)", name);
    return header;
}
