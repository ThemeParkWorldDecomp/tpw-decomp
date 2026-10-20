// Host side of the node gamut tests (src/NodeGamuts.cpp with src/rsse_matrix.cpp). The unit
// calls nothing outside itself, so there are no fakes and no call log: a case builds a tree
// of nodes in the arena, calls a function and dumps everything the function could touch.
// Each entry point takes and returns plain 32-bit words (float bit patterns included), laid
// out as tests/test_NodeGamuts.py lays them out.
#include "TPEngine.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include <cstring>

namespace {
constexpr int kMaxNodes = 12;
constexpr int kNodeDescWords = 27;   // flags, parent, child, sibling (index + 1), matrix, box, gamut bytes
constexpr int kNodeWords = sizeof(tag_sNode) / 4;
constexpr int kHeaderWords = sizeof(tag_sM3D2Header) / 4;
constexpr int kFieldWords = sizeof(sHField) / 4;

tag_sNode* nodes;
tag_sM3D2Header* header;
sHField* field;

// A link field as the index of its node plus one, 0 for null, 0xfffffffe for anything else.
uint32_t LinkIndex(uint32_t offset) {
    if (offset == 0) return 0;
    const auto* target = static_cast<const uint8_t*>(tpw::gArenaBase) + offset;
    const auto* base = reinterpret_cast<const uint8_t*>(nodes);
    if (target < base || target >= base + sizeof(tag_sNode) * kMaxNodes) return 0xfffffffeu;
    return static_cast<uint32_t>((target - base) / sizeof(tag_sNode)) + 1;
}

void Allocate() {
    if (nodes != nullptr) return;
    nodes = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode) * kMaxNodes));
    header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    field = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
}
}  // namespace

extern "C" {
// The point through the matrix: out = the matrix afterwards (16 words), then the point.
void tpw_ng_vector(const uint32_t* matrix, const uint32_t* point, uint32_t* out) {
    sMatrix m;
    s3DPoint p;
    std::memcpy(&m, matrix, sizeof m);
    std::memcpy(&p, point, sizeof p);
    VectorThroughLMatrixNoW(&m, &p);
    std::memcpy(out, &m, sizeof m);
    std::memcpy(out + 16, &p, sizeof p);
}

// destination = source * destination; with `alias` the destination is also the source.
// out = the destination (16 words), then the source (16 words).
void tpw_ng_combine(const uint32_t* source, const uint32_t* destination, int32_t alias, uint32_t* out) {
    sMatrix s, d;
    std::memcpy(&s, source, sizeof s);
    std::memcpy(&d, destination, sizeof d);
    CombineMatricesNoWDest2nd(alias ? &d : &s, &d);
    std::memcpy(out, &d, sizeof d);
    std::memcpy(out + 16, alias ? &d : &s, sizeof s);
}

// desc: op (0 SetupHFieldNodeGamuts, 1 DoCalcNodeHFieldGamut), node count, the four global
// limits, the height field's six words (origin x z, cell x z, counts x z), the matrix
// (16 words, op 1), then the nodes. out: the four globals, the height field record, the
// header, then every node, with the three links as node indices plus one.
void tpw_ng_gamut(const uint32_t* desc, uint32_t* out) {
    Allocate();
    const int32_t op = static_cast<int32_t>(desc[0]);
    const int32_t count = static_cast<int32_t>(desc[1]);
    gNodeGamutMinX = static_cast<int32_t>(desc[2]);
    gNodeGamutMaxX = static_cast<int32_t>(desc[3]);
    gNodeGamutMinZ = static_cast<int32_t>(desc[4]);
    gNodeGamutMaxZ = static_cast<int32_t>(desc[5]);
    std::memset(field, 0xa5, sizeof *field);
    std::memcpy(reinterpret_cast<uint8_t*>(field) + 8, desc + 6, 24);
    std::memset(header, 0xa5, sizeof *header);
    header->mHField = field;
    header->mF78 = &nodes[0];
    sMatrix matrix;
    std::memcpy(&matrix, desc + 12, sizeof matrix);
    std::memset(nodes, 0xa5, sizeof(tag_sNode) * kMaxNodes);
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t* d = desc + 28 + i * kNodeDescWords;
        tag_sNode& node = nodes[i];
        node.mFlags = d[0];
        node.mF04 = d[1] != 0 ? &nodes[d[1] - 1] : nullptr;
        node.mF0C = d[2] != 0 ? &nodes[d[2] - 1] : nullptr;
        node.mF08 = d[3] != 0 ? &nodes[d[3] - 1] : nullptr;
        std::memcpy(&node.mMatrix, d + 4, sizeof node.mMatrix);
        std::memcpy(&node.mBox, d + 20, sizeof node.mBox);
        std::memcpy(node.mF90, d + 26, 4);
    }
    if (op == 0) SetupHFieldNodeGamuts(header);
    else DoCalcNodeHFieldGamut(&nodes[0], field, matrix);
    int n = 0;
    out[n++] = static_cast<uint32_t>(gNodeGamutMinX);
    out[n++] = static_cast<uint32_t>(gNodeGamutMaxX);
    out[n++] = static_cast<uint32_t>(gNodeGamutMinZ);
    out[n++] = static_cast<uint32_t>(gNodeGamutMaxZ);
    std::memcpy(out + n, field, sizeof *field);
    n += kFieldWords;
    const auto* h = reinterpret_cast<const uint32_t*>(header);
    for (int i = 0; i < kHeaderWords; ++i) {
        if (i == 0x6c / 4) out[n++] = h[i] == tpw::ArenaOffset(field) ? 1 : 0xfffffffeu;   // the height field
        else if (i == 0x78 / 4) out[n++] = LinkIndex(h[i]);                              // the root node
        else out[n++] = h[i];
    }
    for (int32_t i = 0; i < kMaxNodes; ++i) {
        const auto* w = reinterpret_cast<const uint32_t*>(&nodes[i]);
        for (int k = 0; k < kNodeWords; ++k) out[n++] = k >= 1 && k <= 3 ? LinkIndex(w[k]) : w[k];
    }
}
}
