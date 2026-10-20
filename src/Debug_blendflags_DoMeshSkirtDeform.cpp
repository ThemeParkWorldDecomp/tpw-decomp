#include "TPEngine.h"
#include "m3d2.h"
#include "Debug.h"
#include "rsse.h"

// Deforms the skirt of a mesh against a height field: once for the mesh's own list (the block of the mesh's parent mesh), then once for
// every node of the header that has a texture record (its list is the word at texture + 4). Does nothing without a height-field header
// or without a height field in it. Afterwards flags the first node 0x10000 and the header 0x40000.
void DoMeshSkirtDeform(tag_sMesh* mesh, tag_sM3D2Header* header, float y, uint32_t a) {
    tag_sM3D2Header* meshHeader = mesh->mHeader;
    tag_sM3D2Header* self = meshHeader->mSelf.get();
    if (header == nullptr) return;
    if (header->mHField.get() == nullptr) return;

    sMatrix matrix;
    SetMatrixIdentity(&matrix);
    uint16_t* list = static_cast<uint16_t*>(mesh->mF08->mBlock14);
    if ((meshHeader->mFlags & 0x100) != 0) a |= 1;
    DoOrSetupSkirtDeform(self->mF78.get(), meshHeader->mF78.get(), &list, matrix, header->mHField.get(), 1, nullptr, y, nullptr, nullptr,
                         a, nullptr, 0);

    tag_sNode* node = meshHeader->mNodes.get();
    for (uint32_t n = meshHeader->mNodeCount; n != 0; --n, ++node) {
        if (node->mF98.get() == nullptr) continue;
        list = reinterpret_cast<uint16_t*>(node->mF98->mTexture.get());
        DoOrSetupSkirtDeform(self->mF78.get(), meshHeader->mF78.get(), &list, matrix, header->mHField.get(), 1, nullptr, y, nullptr,
                             nullptr, a, node, 1);
    }
    meshHeader->mF78->mFlags |= 0x10000;
    meshHeader->mFlags |= 0x40000;
}
