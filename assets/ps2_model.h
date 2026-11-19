// ps2_model.h: the PS2 character model (.mps) and animation set (.aps) readers of the PS2 Guest Models converter
// (ps2_convert.h), and the skinning that turns a clip into vertex positions. Field layouts as read from the PS2 code
// (SLUS_200.99, static reading only), the same as the research decoders: an .mps holds the mesh nodes (0xa0 bytes each,
// their triangle strips and skin) then the skeleton's bones (0x60 bytes each); an .aps holds groups of clips, each either
// skeletal (per bone rotation and position keys, 30 keys a second) or vertex animated (per mesh node, every vertex's
// position in every frame: the walk).
//
// Positions are in "model space": x, y on the ground, z up, in the PS2's units (a kid is about 16000 tall).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets {

struct Ps2Influence {
    int bone = 0;     // index into the skeleton (node nmesh + bone)
    float weight = 0;
    float local[3] = {0, 0, 0};   // the vertex in the bone's space
};

struct Ps2SkinVertex {
    std::vector<Ps2Influence> influences;
    float bind[3] = {0, 0, 0};   // model space, from the strips
};

struct Ps2Corner {
    uint32_t vertex = 0;
    float u = 0, v = 0;   // texture coordinates as stored (1.0 = one texture width; v 0 is the image's top row)
};

struct Ps2Triangle {
    Ps2Corner corners[3];
    std::string texture;   // the material's first texture name, lower case, no extension ("boybody1")
};

struct Ps2Mps {
    uint32_t triangles = 0;   // the header's count (the recognition check)
    uint32_t meshes = 0;
    std::vector<std::string> boneNames;
    std::vector<int> boneParent;            // -1 for a root
    std::vector<uint32_t> meshFirstVertex;  // per mesh node: its first vertex in `vertices`
    std::vector<uint32_t> meshVertexCount;
    std::vector<Ps2SkinVertex> vertices;
    std::vector<Ps2Triangle> tris;
};

// False with a reason on a damaged file; every offset is checked.
bool ParsePs2Mps(const std::vector<uint8_t>& bytes, Ps2Mps& out, std::string& reason);

struct Ps2RotKey {
    uint16_t time = 0;
    float q[4] = {0, 0, 0, 1};   // x, y, z, w
};
struct Ps2PosKey {
    uint16_t time = 0;
    float p[3] = {0, 0, 0};
};
struct Ps2BoneTrack {
    int bone = 0;
    std::vector<Ps2RotKey> rot;
    std::vector<Ps2PosKey> pos;
};
struct Ps2NodeTrack {
    uint32_t node = 0;             // mesh node
    uint32_t vertices = 0;
    std::vector<int16_t> values;   // frame-major: frame f, vertex i at (f * vertices + i) * 3; x, y, z as the strips
};
struct Ps2Clip {
    int group = 0, clip = 0;
    uint32_t flags = 0, length = 0;   // length in frames (1/30 s)
    bool skeletal = false;
    bool shared = false;   // skeletal with no keys of its own: the keys come from another character's same clip
    std::vector<Ps2BoneTrack> bones;
    std::vector<Ps2NodeTrack> nodes;
};
struct Ps2Aps {
    std::vector<Ps2Clip> clips;
};

bool ParsePs2Aps(const std::vector<uint8_t>& bytes, Ps2Aps& out, std::string& reason);

// The model's vertices (model space, x y z per vertex) in `clip` at time `t` (frames, 0..length). A skeletal clip
// skins on the CPU (each influence: local rotated by the bone's key rotation, plus its key position; weighted). A bone
// the clip has no track for follows its nearest tracked ancestor, at the offset from that ancestor it has in the first
// of `others` (other skeletal clips of the same skeleton) that tracks both; with none it moves rigidly with the
// ancestor. A vertex animated clip reads its frames (a mesh node it has no track for keeps its bind pose).
// `rigid` (optional) counts the bones the skin uses that fell back to moving rigidly with an ancestor.
std::vector<float> Ps2Pose(const Ps2Mps& model, const Ps2Clip& clip, const std::vector<const Ps2Clip*>& others, float t,
                           int* rigid = nullptr);

// The bind pose (the strips' own positions).
std::vector<float> Ps2Bind(const Ps2Mps& model);

}  // namespace tpw::assets
