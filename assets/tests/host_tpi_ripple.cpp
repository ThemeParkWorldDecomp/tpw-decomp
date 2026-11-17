// Host test of the TPI islands' wave bands and their texture animations (assets/tpi_ripple.h): synthetic outlines,
// meshes and animations built here (tpi_lobby_fixture.h), no game data. Runs alone and repeatedly.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "tpi_island_md2.h"
#include "tpi_lobby_fixture.h"
#include "tpi_ripple.h"

using namespace tpw::assets;
using namespace tpw::assets::tpi_island;
using namespace tpi_lobby_fixture;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond)                                                      \
    do {                                                                 \
        if (cond) ++g_pass;                                              \
        else {                                                           \
            ++g_fail;                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                \
    } while (0)

namespace {

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
bool Same(const Vec3& a, float x, float y, float z) { return Near(a.x, x) && Near(a.y, y) && Near(a.z, z); }

// How far a point lies outside the square 0..10 in x and z (0 inside or on it).
float Outside(const Vec3& p) {
    const float dx = std::max({0.0f, -p.x, p.x - 10}), dz = std::max({0.0f, -p.z, p.z - 10});
    return std::max(dx, dz);
}

void TestRingSquare() {
    for (int way = 0; way < 2; ++way) {
        std::vector<Vec3> square = {{0, 2, 0}, {10, 2, 0}, {10, 2, 10}, {0, 2, 10}};
        if (way == 1) std::reverse(square.begin(), square.end());
        const std::vector<RippleQuad> quads = RippleRing(square, 1.0f, 5.0f);
        CHECK(quads.size() == 8);   // each side of 10 in two pieces of 5
        bool uvs = true, inner = true, outer = true, flat = true, lengths = true, mitre = false;
        for (const RippleQuad& q : quads) {
            const float u[4] = {0, 1, 1, 0}, v[4] = {0, 0, 1, 1};
            for (int k = 0; k < 4; ++k) {
                uvs = uvs && q.u[k] == u[k] && q.v[k] == v[k];
                flat = flat && Near(q.p[k].y, 2);
            }
            inner = inner && Outside(q.p[0]) == 0 && Outside(q.p[1]) == 0;
            outer = outer && Near(Outside(q.p[2]), 1) && Near(Outside(q.p[3]), 1);
            lengths = lengths && Near(std::hypot(q.p[1].x - q.p[0].x, q.p[1].z - q.p[0].z), 5);
            for (int k = 2; k < 4; ++k) mitre = mitre || Same(q.p[k], 11, 2, -1);
        }
        CHECK(uvs);
        CHECK(flat);
        CHECK(inner);
        CHECK(outer);
        CHECK(lengths);
        CHECK(mitre);   // the corner at (10, 0) goes out to (11, -1), so the outer edge stays 1 from both sides
        // The quads follow on round the loop: each one's inner b is the next one's inner a.
        bool joined = true;
        for (size_t i = 0; i < quads.size(); ++i) {
            const RippleQuad& a = quads[i];
            const RippleQuad& b = quads[(i + 1) % quads.size()];
            joined = joined && Same(a.p[1], b.p[0].x, b.p[0].y, b.p[0].z);
        }
        CHECK(joined);
    }
}

void TestRingDegenerate() {
    const std::vector<Vec3> square = {{0, 0, 0}, {10, 0, 0}, {10, 0, 10}, {0, 0, 10}};
    CHECK(RippleRing({}, 1, 5).empty());
    CHECK(RippleRing({{0, 0, 0}, {1, 0, 0}}, 1, 5).empty());
    CHECK(RippleRing({{0, 0, 0}, {5, 0, 0}, {10, 0, 0}}, 1, 5).empty());   // no area
    CHECK(RippleRing(square, 0, 5).empty());
    CHECK(RippleRing(square, 1, 0).empty());
    CHECK(RippleRing(square, -1, 5).empty());
    // A repeated point and the first point repeated at the end change nothing.
    const std::vector<Vec3> repeated = {{0, 0, 0}, {10, 0, 0}, {10, 0, 0}, {10, 0, 10}, {0, 0, 10}, {0, 0, 0}};
    CHECK(RippleRing(repeated, 1, 5).size() == 8);
    // A short side still gets one quad.
    CHECK(RippleRing({{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}, 0.5f, 5).size() == 4);
}

void TestRippleNode() {
    for (int way = 0; way < 2; ++way) {
        std::vector<Vec3> square = {{0, 2, 0}, {10, 2, 0}, {10, 2, 10}, {0, 2, 10}};
        if (way == 1) std::reverse(square.begin(), square.end());
        const Md2Node n = RippleNode(RippleRing(square, 1.0f, 5.0f), 3, "Ripple");
        CHECK(n.name == "Ripple");
        CHECK(n.faces.size() == 16);
        CHECK(n.faceSets.size() == 1 && n.faceSets[0].texture == 3);
        bool up = true;
        for (const Md2Face& f : n.faces) {
            const Vec3& a = n.vertices[n.cornerVertex[f.corner[0]]];
            const Vec3& b = n.vertices[n.cornerVertex[f.corner[1]]];
            const Vec3& c = n.vertices[n.cornerVertex[f.corner[2]]];
            // cross(b - a, c - a).y, the engine's front face
            const float y = (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z);
            up = up && y > 0;
        }
        CHECK(up);
    }
}

void TestUvAnimRoundtrip() {
    const Md2Model isle = MakeIsle();
    const std::vector<uint8_t> mesh = WriteMd2(isle);
    UvAnim anim;
    anim.f00 = 0x14;
    anim.frames = 100;
    UvNodeAnim node;
    node.record[8] = 0x5a;
    node.SetNode(0);
    for (size_t c = 0; c < isle.nodes[0].u.size(); ++c)
        node.tracks.push_back({UvKey{0, 0.25f * static_cast<float>(c), 1}, UvKey{40, 2, -3}, UvKey{100, -1, 0.5f}});
    anim.nodes.push_back(node);
    const std::vector<uint8_t> bytes = WriteUvAnim(mesh, "Jun_isleM1.MD2", anim);
    UvAnim back;
    std::string err;
    CHECK(ReadUvAnim(bytes, back, err));
    CHECK(err.empty());
    CHECK(back.f00 == 0x14);
    CHECK(back.frames == 100);
    CHECK(!back.other);
    CHECK(back.nodes.size() == 1);
    if (back.nodes.size() == 1) {
        const UvNodeAnim& b = back.nodes[0];
        CHECK(b.Node() == 0);
        CHECK(b.record[8] == 0x5a);
        CHECK(b.tracks.size() == node.tracks.size());
        bool same = b.tracks.size() == node.tracks.size();
        for (size_t c = 0; same && c < b.tracks.size(); ++c) {
            same = b.tracks[c].size() == 3;
            for (size_t k = 0; same && k < 3; ++k)
                same = b.tracks[c][k].frame == node.tracks[c][k].frame && b.tracks[c][k].u == node.tracks[c][k].u &&
                       b.tracks[c][k].v == node.tracks[c][k].v;
        }
        CHECK(same);
    }
    // The name in the header is the one given, and writing again gives the same bytes.
    const std::string name = "Jun_isleM1.MD2";
    CHECK(bytes.size() > 0x98 && std::search(bytes.begin(), bytes.begin() + 0x98, name.begin(), name.end()) != bytes.begin() + 0x98);
    CHECK(WriteUvAnim(mesh, "Jun_isleM1.MD2", back) == bytes);
    // Not an animation file.
    UvAnim bad;
    CHECK(!ReadUvAnim(std::vector<uint8_t>(16, 0), bad, err));
    CHECK(!err.empty());
}

void TestFileNodeIndex() {
    // Full nodes first, then minis, each in the model's order.
    Md2Model m;
    const bool mini[5] = {true, false, true, false, false};
    for (int i = 0; i < 5; ++i) {
        Md2Node n;
        n.mini = mini[i];
        m.nodes.push_back(n);
    }
    CHECK(FileNodeIndex(m, 1) == 0);
    CHECK(FileNodeIndex(m, 3) == 1);
    CHECK(FileNodeIndex(m, 4) == 2);
    CHECK(FileNodeIndex(m, 0) == 3);
    CHECK(FileNodeIndex(m, 2) == 4);
    CHECK(FileNodeIndex(m, 5) == -1);
}

void TestMeasureRipple() {
    const Md2Model isle = MakeIsle();
    const std::vector<uint8_t> mesh = WriteMd2(isle);
    const std::vector<uint8_t> m1 = MakeIsleAnim(isle, mesh, "Jun_isleM1.MD2", 100, -1.0f, 0x11);
    const std::vector<uint8_t> m2 = MakeIsleAnim(isle, mesh, "Jun_isleM2.MD2", 200, -2.0f, 0x22);
    RippleStyle style;
    std::string err;
    CHECK(MeasureRipple(mesh, m1, m2, style, err));
    CHECK(Near(style.width, 4.0f, 1e-3f));
    CHECK(Near(style.quadLength, 5.0f, 1e-3f));
    CHECK(style.frames[0] == 100 && style.frames[1] == 200);
    CHECK(Near(style.drift[0][0], -1) && Near(style.drift[0][1], -1));
    CHECK(Near(style.drift[1][0], -2) && Near(style.drift[1][1], -2));
    CHECK(style.texture == "Ripple.tga");
    CHECK(style.anim[0].f00 == 0x14 && style.anim[0].frames == 100 && style.anim[1].frames == 200);
    CHECK(style.record[0][8] == 0x11 && style.record[1][8] == 0x22);

    // No band: the ripple texture renamed.
    Md2Model plain = isle;
    plain.textures[1].frames[0] = "foam.tga";
    CHECK(!MeasureRipple(WriteMd2(plain), m1, m2, style, err));
    CHECK(err.find("no wave band") != std::string::npos);
    // The band not see-through.
    Md2Model opaque = isle;
    opaque.textures[1].material = 0x31;
    CHECK(!MeasureRipple(WriteMd2(opaque), m1, m2, style, err));
    // An animation missing or not moving the band.
    CHECK(!MeasureRipple(mesh, std::vector<uint8_t>(8, 0), m2, style, err));
    CHECK(err.find("M1") != std::string::npos);
    UvAnim still;
    still.frames = 100;
    UvNodeAnim other;
    other.SetNode(7);
    other.tracks.push_back({UvKey{0, 0, 0}, UvKey{100, 1, 1}});
    still.nodes.push_back(other);
    CHECK(!MeasureRipple(mesh, m1, WriteUvAnim(mesh, "Jun_isleM2.MD2", still), style, err));
    CHECK(err.find("M2") != std::string::npos);
    // Corners drifting unlike each other.
    UvAnim uneven;
    CHECK(ReadUvAnim(m1, uneven, err));
    for (auto& track : uneven.nodes[0].tracks) track.back().u += 0.5f * static_cast<float>(&track - &uneven.nodes[0].tracks[0]);
    CHECK(!MeasureRipple(mesh, WriteUvAnim(mesh, "Jun_isleM1.MD2", uneven), m2, style, err));
    CHECK(err.find("alike") != std::string::npos);
    // Not a mesh.
    CHECK(!MeasureRipple(std::vector<uint8_t>(4, 0), m1, m2, style, err));
}

}  // namespace

int main() {
    TestRingSquare();
    TestRingDegenerate();
    TestRippleNode();
    TestUvAnimRoundtrip();
    TestFileNodeIndex();
    TestMeasureRipple();
    std::printf("tpi_ripple: %d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
