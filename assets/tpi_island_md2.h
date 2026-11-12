// tpi_island_md2.h: an in-memory form of the engine's M3D2 mesh (include/m3d2.h, ".MD2" files) for the Theme Park Inc
// island builder (tpi_island.h): a reader for the player's Theme Park Inc pieces, a writer that lays a file out the way
// the originals are (the layout ps1_convert.cpp writes), and the edits the builder makes: a uniform scale baked into the
// positions, a piece re-parented under another mesh's root, and generated geometry (the island slab and its fence).
// Only the mesh tables (and a terrain's height field, kept as it is) are modelled; animation blocks are patched in place
// by tpi_island.cpp and never pass through here. The Theme Park Inc converter's terrain repair (tpi_terrain.h) uses the
// same reader and writer to add the obstacles' meshes to a world's terrain.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets::tpi_island {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// A row-major 4x4 matrix as the files hold it: the translation is row 3, and a point transforms as p * M (row vector).
using Matrix = std::array<float, 16>;
Matrix IdentityMatrix();
Matrix RotationY(float radians);
Matrix Translation(float x, float y, float z);
Matrix Multiply(const Matrix& a, const Matrix& b);   // a then b: p * a * b
Vec3 Transform(const Vec3& p, const Matrix& m);

struct Md2Texture {
    uint16_t material = 0x31;         // sTexture mF00
    uint16_t f02 = 0;                 // sTexture mF02
    uint32_t infoFlags = 0;           // sTextureInfo mFlags (bit 0: the frames animate)
    uint8_t f08[2] = {0, 0};          // sTextureInfo mF08
    std::vector<std::string> frames;  // one file name per frame, "name.tga"
};

struct Md2Face {
    uint16_t normal = 0;      // the file's word: an index into the node's points (the loader shifts it)
    uint16_t corner[3] = {0, 0, 0};
};

struct Md2FaceSet {
    int texture = 0;          // index into the model's textures, -1 for a face set without one (a zero pointer in the file)
    uint16_t firstFace = 0, faceCount = 0, firstCorner = 0, lastCorner = 0;
    uint8_t flags[4] = {0, 0, 0, 0};
};

struct Md2Node {
    std::string name;
    uint32_t flags = 1;       // 1 a geometry node, 0x200 a mini node (an attachment point without geometry)
    uint16_t f52 = 0;
    int parent = -1;          // index into Md2Model::nodes, -1 for the root
    Matrix matrix = IdentityMatrix();   // relative to the parent
    bool mini = false;
    // Geometry (full nodes). Corners are (vertex, uv, normal) tuples the faces index; points are the corner normals
    // followed by the face normals, as the originals and ps1_convert.cpp lay them out.
    std::vector<Vec3> vertices;
    std::vector<Vec3> points;
    std::vector<float> u, v;              // per corner
    std::vector<uint16_t> cornerVertex;   // per corner: its vertex (tag_sNode mF94)
    std::vector<Md2FaceSet> faceSets;
    std::vector<Md2Face> faces;
    std::array<float, 6> box = {0, 0, 0, 0, 0, 0};   // min xyz, max xyz, in the node's own frame
    uint8_t f90[4] = {0, 0, 0, 0};
};

struct Md2Model {
    std::string name;             // the header's name (a file name, up to 19 characters)
    uint32_t flags = 1;           // 1 geometry, 2 height field, 0x20 animation
    std::vector<Md2Texture> textures;
    std::vector<Md2Node> nodes;   // node 0 is the root; the writer puts full nodes first, then minis
    std::array<float, 6> box = {0, 0, 0, 0, 0, 0};
    // A terrain's height field (header 0x6c: the sHField record, its heights, then its quads, as in the file) and the
    // header words the builder has no use for (0x0c, 0x10, 0x14 and 0x46). Only ReadMd2 with keepTerrain fills them,
    // so that a terrain mesh (tpi_terrain.cpp) survives a rewrite; the island's meshes leave them empty, and the writer
    // then writes no height field, zeros and the full node count, as before.
    std::vector<uint8_t> heightField;
    std::array<uint32_t, 3> f0c = {0, 0, 0};
    int f46 = -1;
};

// Reads a mesh file's tables. False with a message for anything but a geometry mesh the engine would load.
// keepTerrain also keeps the height field and the unused header words (Md2Model::heightField).
bool ReadMd2(const std::vector<uint8_t>& bytes, Md2Model& model, std::string& err, bool keepTerrain = false);
// Writes the model as a version 0xdd mesh without an animation block or embedded records.
std::vector<uint8_t> WriteMd2(const Md2Model& model);

// The nodes' world matrices (each node's matrix times its parents', root first) in the model's own frame.
std::vector<Matrix> WorldMatrices(const Md2Model& model);
// The box of all vertices in the model's frame (the root's own matrix is left out: the engine replaces it when it
// places the mesh). False when the model has no vertex.
bool GeometryBox(const Md2Model& model, std::array<float, 6>& box);
// Multiplies every position, translation and box by `scale`, leaving the normals alone.
void BakeScale(Md2Model& model, float scale);
// Folds the root node's matrix into the model: its rotation (and any scale) goes into the root's own vertices and normals
// and into the matrices of its children, its translation is dropped (the game places the root itself), and the root's
// matrix becomes the identity. The gates and some pieces model their parts turned and let the root turn them back.
void BakeRoot(Md2Model& model);
// Appends `piece` under `into`'s root: its textures are merged (equal names and flags share a slot), its root's matrix
// becomes the placement (the game overwrites a placed ride root's matrix the same way), mini nodes with no geometry below them are dropped. The piece's root must be a full
// node. False with a message otherwise.
bool AttachPiece(Md2Model& into, const Md2Model& piece, const Matrix& placement, std::string& err);
// Recomputes every node's box from its vertices and the model's box from the whole geometry.
void RecomputeBoxes(Md2Model& model);

// A generated triangle list that becomes one node (faces grouped by texture into face sets, flat normals).
class NodeBuilder {
public:
    // Adds a triangle: corners in order, texture `texture` (an index into the model's textures).
    void Triangle(const Vec3& a, const Vec3& b, const Vec3& c, float ua, float va, float ub, float vb, float uc, float vc,
                  uint16_t texture);
    // Adds a quad a b c d (two triangles) with corner uvs.
    void Quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float ua, float va, float ub, float vb, float uc,
              float vc, float ud, float vd, uint16_t texture);
    // Adds an axis-aligned box with every face textured `texture`, uvs in world units over `uvScale`.
    void Box(const Vec3& min, const Vec3& max, float uvScale, uint16_t texture);
    Md2Node Build(const std::string& name) const;
    bool Empty() const { return tris_.empty(); }

private:
    struct Tri {
        Vec3 p[3];
        float u[3], v[3];
        uint16_t texture;
    };
    std::vector<Tri> tris_;
};

}  // namespace tpw::assets::tpi_island
