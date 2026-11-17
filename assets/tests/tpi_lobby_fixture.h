// Synthetic stand-ins for the island screen's files in lobby.wad (an island with its wave band and two texture
// animations, the terrain with its sea and its animation), shaped as the originals are: built here, no game data.
// Shared by host_tpi_ripple.cpp, host_tpi_lobby_sea.cpp and host_tpi_island.cpp.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "tpi_island_md2.h"
#include "tpi_lobby_sea.h"
#include "tpi_ripple.h"
#include "tpi_wad.h"

namespace tpi_lobby_fixture {

using namespace tpw::assets;
using namespace tpw::assets::tpi_island;

// The sea's planar mapping in its node's frame (any affine one will do).
inline float SeaU(float x, float z) { return 0.25f + x / 150.0f + z / 600.0f; }
inline float SeaV(float x, float z) { return -0.5f + z / 150.0f - x / 900.0f; }

// A disc of `sides` corners on each of two rings round a centre corner, one corner per vertex (as the originals'),
// the centre at y `centreY`, the edge level at `edgeY`, mapped by SeaU and SeaV, every face in texture slot `texture`,
// wound clockwise seen from above when `clockwise` (the corner normals and face normals up).
inline Md2Node MakeDisc(const std::string& name, int sides, float radius, float centreY, float edgeY, uint16_t texture,
                        bool clockwise) {
    Md2Node n;
    n.name = name;
    n.flags = 1;
    auto add = [&](float x, float y, float z) {
        n.vertices.push_back(Vec3{x, y, z});
        n.cornerVertex.push_back(static_cast<uint16_t>(n.vertices.size() - 1));
        n.u.push_back(SeaU(x, z));
        n.v.push_back(SeaV(x, z));
        return static_cast<uint16_t>(n.u.size() - 1);
    };
    const uint16_t centre = add(0, centreY, 0);
    std::vector<uint16_t> ring[2];
    for (int k = 0; k < 2; ++k)
        for (int j = 0; j < sides; ++j) {
            const float a = 6.2831853f * (static_cast<float>(j) + 0.5f * static_cast<float>(k)) / static_cast<float>(sides);
            const float r = radius * static_cast<float>(k + 1) / 2;
            ring[k].push_back(add(r * std::cos(a), k == 0 ? (centreY + edgeY) / 2 : edgeY, r * std::sin(a)));
        }
    auto face = [&](uint16_t a, uint16_t b, uint16_t c) {
        // Angles rise anticlockwise (atan2(z, x)): a, b, c in that order turn one way seen from above.
        Md2Face f;
        f.corner[0] = a;
        f.corner[1] = clockwise ? c : b;
        f.corner[2] = clockwise ? b : c;
        n.faces.push_back(f);
    };
    for (int j = 0; j < sides; ++j) {
        const int k = (j + 1) % sides;
        face(centre, ring[0][static_cast<size_t>(j)], ring[0][static_cast<size_t>(k)]);
        face(ring[0][static_cast<size_t>(j)], ring[1][static_cast<size_t>(j)], ring[0][static_cast<size_t>(k)]);
        face(ring[0][static_cast<size_t>(k)], ring[1][static_cast<size_t>(j)], ring[1][static_cast<size_t>(k)]);
    }
    for (size_t c = 0; c < n.u.size(); ++c) n.points.push_back(Vec3{0, 1, 0});
    for (Md2Face& f : n.faces) {
        f.normal = static_cast<uint16_t>(n.points.size());
        n.points.push_back(Vec3{0, 1, 0});
    }
    Md2FaceSet set;
    set.texture = texture;
    set.firstFace = 0;
    set.faceCount = static_cast<uint16_t>(n.faces.size());
    set.firstCorner = 0;
    set.lastCorner = static_cast<uint16_t>(n.u.size() - 1);
    n.faceSets.push_back(set);
    return n;
}

// The terrain's sea, as Base.MD2 lays it out: a root mini node; "Sea_Under", an opaque bowl moved to (550.5, -17.2,
// 553.1) and turned about y, its rim at local y 16.89; "Sea", a see-through flat disc, child of Sea_Under, put back
// level and unturned at y 0. With one embedded record of 20 bytes without pointers.
constexpr float kSeaX = 550.507f, kSeaZ = 553.101f, kSeaTurn = -2.0944f;
inline Md2Model MakeSeaModel(int sides = 24, float radius = 528.0f) {
    Md2Model m;
    m.name = "Base.MD2";
    m.flags = 1;
    Md2Texture under, sea;
    under.material = 0x31;
    under.frames.push_back("lak2.TGA");
    sea.material = 0x33;
    sea.frames.push_back("lak3.TGA");
    m.textures = {under, sea};
    Md2Node root;
    root.name = "root";
    root.flags = 0x300;
    root.mini = true;
    root.parent = -1;
    m.nodes.push_back(root);
    Md2Node bowl = MakeDisc("Sea_Under", sides, radius, 0.0f, 16.89f, 0, true);
    bowl.parent = 0;
    bowl.matrix = Multiply(RotationY(kSeaTurn), Translation(kSeaX, -17.172f, kSeaZ));
    m.nodes.push_back(bowl);
    Md2Node top = MakeDisc("Sea", sides + 1, radius, 0.0f, 0.0f, 1, false);
    top.parent = 1;
    top.matrix = Multiply(RotationY(-kSeaTurn), Translation(0, 17.172f, 0));
    m.nodes.push_back(top);
    RecomputeBoxes(m);
    return m;
}

inline std::vector<uint8_t> SeaRecord() {
    std::vector<uint8_t> r(20, 0);
    r[0] = 0x01;
    r[1] = 0x40;
    r[4] = 7;
    return r;
}

inline std::vector<uint8_t> MakeSeaBase(const Md2Model& m) {
    std::vector<uint8_t> bytes = WriteMd2(m);
    AppendEmbeddedRecords(bytes, SeaRecord(), 1);
    return bytes;
}

// The terrain's animation: every corner of both discs slides by (-1, -1) over 200 frames.
inline std::vector<uint8_t> MakeSeaAnim(const Md2Model& m, const std::vector<uint8_t>& base, float du = -1, float dv = -1) {
    UvAnim anim;
    anim.f00 = 0x10;
    anim.frames = 200;
    for (size_t i = 1; i < m.nodes.size(); ++i) {
        UvNodeAnim node;
        node.SetNode(static_cast<uint16_t>(FileNodeIndex(m, i)));
        const Md2Node& n = m.nodes[i];
        for (size_t c = 0; c < n.u.size(); ++c)
            node.tracks.push_back({UvKey{0, n.u[c], n.v[c]}, UvKey{200, n.u[c] + du, n.v[c] + dv}});
        anim.nodes.push_back(node);
    }
    return WriteUvAnim(base, "Basem1.MD2", anim);
}

// An original-style island: a ground quad and a wave band round a square of side 20 (inner edge on the square, 4
// out, quads 5 long), both in the root node; the band's texture "Ripple.tga", material 0x33.
inline Md2Model MakeIsle() {
    Md2Model m;
    m.name = "Jun_isle.MD2";
    m.flags = 1;
    Md2Texture ground, ripple;
    ground.frames.push_back("grass.tga");
    ripple.material = 0x33;
    ripple.frames.push_back("Ripple.tga");
    m.textures = {ground, ripple};
    const std::vector<Vec3> square = {{-10, 2.5f, -10}, {10, 2.5f, -10}, {10, 2.5f, 10}, {-10, 2.5f, 10}};
    NodeBuilder b;
    b.Quad(Vec3{-10, 9, -10}, Vec3{-10, 9, 10}, Vec3{10, 9, 10}, Vec3{10, 9, -10}, 0, 0, 0, 1, 1, 1, 1, 0, 0);
    for (const RippleQuad& q : RippleRing(square, 4.0f, 5.0f))
        b.Quad(q.p[0], q.p[3], q.p[2], q.p[1], q.u[0], q.v[0], q.u[3], q.v[3], q.u[2], q.v[2], q.u[1], q.v[1], 1);
    Md2Node root = b.Build("Jun_isle");
    root.parent = -1;
    m.nodes.push_back(root);
    RecomputeBoxes(m);
    return m;
}

// One of the island's animations: the band's corners slide by (drift, drift) over `frames`, the ground's stay.
inline std::vector<uint8_t> MakeIsleAnim(const Md2Model& m, const std::vector<uint8_t>& mesh, const std::string& name,
                                         uint16_t frames, float drift, uint8_t recordMark) {
    UvAnim anim;
    anim.f00 = 0x14;
    anim.frames = frames;
    UvNodeAnim node;
    node.record[8] = recordMark;
    node.SetNode(0);
    const Md2Node& root = m.nodes[0];
    for (size_t c = 0; c < root.u.size(); ++c) {
        bool band = false;
        for (const Md2FaceSet& s : root.faceSets)
            band = band || (s.texture == 1 && c >= s.firstCorner && c <= s.lastCorner);
        const float d = band ? drift : 0.0f;
        node.tracks.push_back({UvKey{0, root.u[c], root.v[c]}, UvKey{frames, root.u[c] + d, root.v[c] + d}});
    }
    anim.nodes.push_back(node);
    return WriteUvAnim(mesh, name, anim);
}

// A lobby.wad's members with all of the above, under the originals' paths ('\' separated, mixed case, as a wad has
// them); `withSea` false leaves the terrain out.
inline std::vector<TpiWadMember> MakeLobby(bool withSea = true) {
    std::vector<TpiWadMember> w;
    const Md2Model isle = MakeIsle();
    const std::vector<uint8_t> isleBytes = WriteMd2(isle);
    w.push_back({"terrain\\Jun_isle.MD2", isleBytes});
    w.push_back({"terrain\\Jun_isleM1.MD2", MakeIsleAnim(isle, isleBytes, "Jun_isleM1.MD2", 100, -1.0f, 0x11)});
    w.push_back({"terrain\\Jun_isleM2.MD2", MakeIsleAnim(isle, isleBytes, "Jun_isleM2.MD2", 200, -2.0f, 0x22)});
    w.push_back({"terrain\\textures\\Ripple.wct", std::vector<uint8_t>{'W', 'C', 'T', 1}});
    w.push_back({"terrain\\stexture\\Ripple.wct", std::vector<uint8_t>{'W', 'C', 'T', 2}});
    if (withSea) {
        const Md2Model sea = MakeSeaModel();
        const std::vector<uint8_t> base = MakeSeaBase(sea);
        w.push_back({"terrain\\Base.MD2", base});
        w.push_back({"terrain\\Basem1.MD2", MakeSeaAnim(sea, base)});
        w.push_back({"terrain\\textures\\lak2.wct", std::vector<uint8_t>{'W', 'C', 'T', 3}});
        w.push_back({"terrain\\textures\\lak3.wct", std::vector<uint8_t>{'W', 'C', 'T', 4}});
        w.push_back({"terrain\\stexture\\lak2.wct", std::vector<uint8_t>{'W', 'C', 'T', 5}});
        w.push_back({"terrain\\stexture\\lak3.wct", std::vector<uint8_t>{'W', 'C', 'T', 6}});
    }
    return w;
}

}  // namespace tpi_lobby_fixture
