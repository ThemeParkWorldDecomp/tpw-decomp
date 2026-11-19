// Host tests of the PS2 Guest Models converter (assets/ps2_disc.h, ps2_model.h, ps2_convert.h, include/Ps2Pack.h): the
// DATA.WAD reader (raw and RefPack entries), the model and animation set readers, the skinning and its fallback for a
// bone with no keys, the vertex animated clips, a shared clip's donor, the texture tiling, the pack's round trip, and a
// whole conversion from a synthetic disc folder. Every file is built here, in code, with made-up shapes; no game data.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "../importer.h"
#include "../ps2_convert.h"
#include "../ps2_disc.h"
#include "../ps2_model.h"
#include "test_installshield.h"

using namespace tpw::assets;
using Bytes = std::vector<uint8_t>;

namespace {

int g_fail = 0, g_pass = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (cond) {                                                     \
            g_pass++;                                                   \
        } else {                                                        \
            g_fail++;                                                   \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

#define CHECK_NEAR(a, b)                                                                                            \
    do {                                                                                                            \
        const double va = (a), vb = (b);                                                                            \
        if (std::fabs(va - vb) < 1e-3 * std::max(1.0, std::fabs(vb))) {                                             \
            g_pass++;                                                                                               \
        } else {                                                                                                    \
            g_fail++;                                                                                               \
            std::printf("FAIL %s:%d: %s = %g, want %g\n", __FILE__, __LINE__, #a, va, vb);                         \
        }                                                                                                           \
    } while (0)

// A little-endian file built in place, with fields patched once their targets are placed.
struct Writer {
    Bytes b;
    uint32_t At() const { return static_cast<uint32_t>(b.size()); }
    void Pad(size_t to) {
        while (b.size() % to) b.push_back(0);
    }
    uint32_t Reserve(size_t n) {
        const uint32_t at = At();
        b.resize(b.size() + n, 0);
        return at;
    }
    void Set16(uint32_t at, uint16_t v) {
        b[at] = static_cast<uint8_t>(v);
        b[at + 1] = static_cast<uint8_t>(v >> 8);
    }
    void Set32(uint32_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) b[at + i] = static_cast<uint8_t>(v >> (8 * i));
    }
    void SetF(uint32_t at, float f) {
        uint32_t w;
        std::memcpy(&w, &f, 4);
        Set32(at, w);
    }
    uint32_t Str(const std::string& s, size_t width = 0) {
        const uint32_t at = At();
        b.insert(b.end(), s.begin(), s.end());
        b.push_back(0);
        while (b.size() - at < width) b.push_back(0);
        return at;
    }
};

uint32_t FloatBits(float f) {
    uint32_t w;
    std::memcpy(&w, &f, 4);
    return w;
}

// The model: one mesh node (a five vertex strip, two triangles, the fifth vertex a second copy of the first with no
// triangle), two bones (the second the first's child), four skin vertices. `triangles` is the header's count.
// Strip positions (x, y up, z): the skin vertex v of the deform list sits at (10 + v, 20 + v, 30 + v).
Bytes MakeMps(uint16_t triangles, const std::string& texture) {
    Writer w;
    w.Reserve(0x50);
    w.Set16(0x22, 1);
    w.Set16(0x2a, triangles);
    w.Set16(0x2e, 3);
    w.Set16(0x30, 1);
    w.Set16(0x32, 2);
    const uint32_t mat = w.Reserve(8);
    w.Set32(0x3c, mat);
    const uint32_t tex = w.Reserve(16);
    w.Set32(0x40, tex);
    w.Set16(tex + 0xa, 1);
    w.Set32(tex + 0xc, w.Str(texture, 20));
    w.Pad(16);
    const uint32_t mesh = w.Reserve(0xa0), bone0 = w.Reserve(0x60), bone1 = w.Reserve(0x60);
    w.Set32(0x48, mesh);
    w.Set32(bone0 + 4, mesh);   // a mesh node's child: a root of the skeleton
    w.Set32(bone0 + 0x54, w.Str("Root"));
    w.Set32(bone1 + 4, bone0);
    w.Set32(bone1 + 0x54, w.Str("Head"));
    w.Pad(16);
    // Strip slot k's skin vertex (the deform list below): k0 -> v1, k1 -> v2, k2 -> v3, k3 -> v0, k4 -> v1.
    const int skinOf[5] = {1, 2, 3, 0, 1};
    const uint32_t pos = w.Reserve(12 * 5);
    for (int k = 0; k < 5; ++k) {
        const int v = skinOf[k];
        uint32_t x = FloatBits(10.0f + v), y = FloatBits(20.0f + v);
        if (k == 4) x |= 1;   // no triangle ends here
        if (k == 2) y |= 1;   // corners (k - 2, k - 1, k)
        w.Set32(pos + 12 * k, x);
        w.Set32(pos + 12 * k + 4, y);
        w.SetF(pos + 12 * k + 8, 30.0f + v);
    }
    // Texture coordinates (4096 = 1.0): the second triangle runs to u 1.5, so the texture is tiled twice across.
    const float uv[5][2] = {{0, 0}, {1, 0}, {0, 1}, {1.5f, 1}, {0, 0}};
    const uint32_t uvs = w.Reserve(4 * 5);
    for (int k = 0; k < 5; ++k) {
        w.Set16(uvs + 4 * k, static_cast<uint16_t>(static_cast<int16_t>(uv[k][0] * 4096)));
        w.Set16(uvs + 4 * k + 2, static_cast<uint16_t>(static_cast<int16_t>(uv[k][1] * 4096)));
    }
    const uint32_t table = w.Reserve(16);
    w.Set32(table, pos);
    w.Set32(table + 4, uvs);
    w.Set32(table + 12, 5);
    const uint32_t group = w.Reserve(0x20);
    w.Set32(group, mat);
    w.Set32(group + 4, table);
    w.Set16(group + 8, 1);
    w.Set16(mesh + 0x64, 1);
    w.Set32(mesh + 0x68, group);
    // Skin: v0 on Root at (1, 0, 0); v1 on Head at (0, 1, 0); v2 half each at (2, 0, 0) and (0, 2, 0); v3 on Head (0, 0, 1).
    struct Inf { uint8_t bone; float weight, x, y, z; };
    const std::vector<std::vector<Inf>> skin = {
        {{0, 1, 1, 0, 0}}, {{1, 1, 0, 1, 0}}, {{0, 0.5f, 2, 0, 0}, {1, 0.5f, 0, 2, 0}}, {{1, 1, 0, 0, 1}}};
    const uint32_t sk = w.Reserve(20), pv = w.Reserve(16), pb = w.Reserve(8), pw = w.Reserve(4 * 5), pp = w.Reserve(12 * 5);
    w.Set16(sk, 4);
    w.Set16(sk + 2, 5);
    w.Set32(sk + 4, pv);
    w.Set32(sk + 8, pb);
    w.Set32(sk + 12, pw);
    w.Set32(sk + 16, pp);
    uint16_t first = 0;
    for (size_t v = 0; v < skin.size(); ++v) {
        w.Set16(pv + 4 * static_cast<uint32_t>(v), first);
        w.b[pv + 4 * v + 2] = static_cast<uint8_t>(skin[v].size());
        for (const Inf& inf : skin[v]) {
            w.b[pb + first] = inf.bone;
            w.SetF(pw + 4 * first, inf.weight);
            w.SetF(pp + 12 * first, inf.x);
            w.SetF(pp + 12 * first + 4, inf.y);
            w.SetF(pp + 12 * first + 8, inf.z);
            ++first;
        }
    }
    // The deform list in skin vertex order, the slots by address (100 + k): v0 = k3; v1 = k0 then k4; v2 = k1; v3 = k2.
    const uint16_t list[5] = {(103 << 2) | 1, (100 << 2) | 2 | 1, (104 << 2) | 1, (101 << 2) | 1, (102 << 2)};
    const uint32_t deform = w.Reserve(2 * 5);
    for (int i = 0; i < 5; ++i) w.Set16(deform + 2 * i, list[i]);
    w.Set32(mesh + 0x90, sk);
    w.Set32(mesh + 0x98, deform);
    return w.b;
}

int16_t Q(float f) { return static_cast<int16_t>(std::lround(f * 32767.0f)); }

// The animation set. Group 0: a vertex animated walk of 1 frame (frame 0 the strips', frame 1 every x + 8). Group 1:
// clip 0 tracks both bones (Root still then a quarter turn about z by frame 2; Head 5 along x from Root); clip 1 tracks
// only Root (Head follows it at clip 0's offset). With `shared`, group 1 clip 0 has no keys of its own (flags 0xa5).
Bytes MakeAps(bool shared) {
    Writer w;
    w.Reserve(0x24);
    w.b[0x1c] = 2;
    const uint32_t groups = w.Reserve(16);
    w.Set32(0x20, groups);
    const uint32_t g0 = w.Reserve(0x1c), g1 = w.Reserve(0x1c * 2);
    w.Set32(groups, 1);
    w.Set32(groups + 4, g0);
    w.Set32(groups + 8, 2);
    w.Set32(groups + 12, g1);
    // Group 0, clip 0.
    w.Set32(g0 + 4, 1);
    w.Set16(g0 + 8, 1);
    const uint32_t track = w.Reserve(48), block = w.Reserve(8), values = w.Reserve(2 * 3 * 4 * 2);
    w.Set32(g0 + 0x10, track);
    w.Set32(track, 0);
    w.Set32(track + 0x20, block);
    w.Set32(block, values);
    w.Set16(block + 4, 8);
    w.Set16(block + 6, 4);
    for (int f = 0; f < 2; ++f)
        for (int v = 0; v < 4; ++v) {
            const uint32_t at = values + 6 * (4 * f + v);
            w.Set16(at, static_cast<uint16_t>(10 + v + 8 * f));
            w.Set16(at + 2, static_cast<uint16_t>(20 + v));
            w.Set16(at + 4, static_cast<uint16_t>(30 + v));
        }
    // Group 1: rotation keys (time, x y z w), position keys (time, x y z).
    const float s = std::sqrt(0.5f);
    auto bone = [&](uint32_t at, uint16_t index, std::vector<std::array<float, 5>> rot, std::vector<std::array<float, 4>> pos) {
        w.Set16(at, index);
        w.b[at + 8] = static_cast<uint8_t>(rot.size());
        w.b[at + 9] = static_cast<uint8_t>(pos.size());
        const uint32_t qp = w.Reserve(10 * rot.size()), pp = w.Reserve(8 * pos.size());
        w.Set32(at + 0xc, qp);
        w.Set32(at + 0x10, pp);
        for (size_t i = 0; i < rot.size(); ++i) {
            w.Set16(qp + 10 * static_cast<uint32_t>(i), static_cast<uint16_t>(rot[i][0]));
            for (int c = 0; c < 4; ++c) w.Set16(qp + 10 * static_cast<uint32_t>(i) + 2 + 2 * c, static_cast<uint16_t>(Q(rot[i][1 + c])));
        }
        for (size_t i = 0; i < pos.size(); ++i) {
            w.Set16(pp + 8 * static_cast<uint32_t>(i), static_cast<uint16_t>(pos[i][0]));
            for (int c = 0; c < 3; ++c) w.Set16(pp + 8 * static_cast<uint32_t>(i) + 2 + 2 * c, static_cast<uint16_t>(static_cast<int16_t>(pos[i][1 + c])));
        }
    };
    for (int k = 0; k < 2; ++k) {
        const uint32_t clip = g1 + 0x1c * k;
        w.Set32(clip + 4, 2);
        if (k == 0 && shared) {
            w.Set32(clip, 0xa5);
            w.Set16(clip + 8, 2);
            continue;
        }
        w.Set32(clip, 0x20);
        const uint16_t n = k == 0 ? 2 : 1;
        w.Set16(clip + 8, n);
        const uint32_t tracks = w.Reserve(20 * n);
        w.Set32(clip + 0x10, tracks);
        bone(tracks, 0, {{0, 0, 0, 0, 1}, {2, 0, 0, s, s}}, {{0, 0, 0, 0}});
        if (n == 2) bone(tracks + 20, 1, {{0, 0, 0, 0, 1}}, {{0, 5, 0, 0}});
    }
    return w.b;
}

// A RefPack stream of literals only.
Bytes RefPack(const Bytes& in) {
    Bytes o = {0x10, 0xfb, static_cast<uint8_t>(in.size() >> 16), static_cast<uint8_t>(in.size() >> 8), static_cast<uint8_t>(in.size())};
    size_t p = 0;
    while (in.size() - p >= 4) {
        const size_t n = std::min<size_t>(112, (in.size() - p) & ~size_t(3));
        o.push_back(static_cast<uint8_t>(0xe0 + n / 4 - 1));
        o.insert(o.end(), in.begin() + p, in.begin() + p + n);
        p += n;
    }
    o.push_back(static_cast<uint8_t>(0xfc + (in.size() - p)));
    o.insert(o.end(), in.begin() + p, in.end());
    return o;
}

// An 'FKNL' linkfile with one folder level under Chars: folder name to its files (name, bytes, packed).
struct WadFile {
    std::string name;
    Bytes bytes;
    bool packed;
};
Bytes MakeWad(const std::vector<std::pair<std::string, std::vector<WadFile>>>& folders) {
    Writer w;
    w.Reserve(24);
    std::memcpy(w.b.data(), "FKNL", 4);
    // root -> Chars -> folders.
    const uint32_t root = w.Reserve(16), rootSub = w.Reserve(8), chars = w.Reserve(16), charsSub = w.Reserve(8 * folders.size());
    w.Set32(20, root);
    w.Set32(root + 4, rootSub);
    w.Set32(root + 12, 1);
    w.Set32(rootSub, w.Str("Chars"));
    w.Set32(rootSub + 4, chars);
    w.Set32(chars + 4, charsSub);
    w.Set32(chars + 12, static_cast<uint32_t>(folders.size()));
    for (size_t i = 0; i < folders.size(); ++i) {
        const auto& [name, files] = folders[i];
        const uint32_t sub = charsSub + 8 * static_cast<uint32_t>(i);
        w.Set32(sub, w.Str(name));
        w.Pad(4);
        const uint32_t node = w.Reserve(16), table = w.Reserve(16 * files.size());
        w.Set32(sub + 4, node);
        w.Set32(node, table);
        w.Set32(node + 8, static_cast<uint32_t>(files.size()));
        for (size_t f = 0; f < files.size(); ++f) {
            const uint32_t at = table + 16 * static_cast<uint32_t>(f);
            w.Set32(at, w.Str(files[f].name));
            const Bytes stored = files[f].packed ? RefPack(files[f].bytes) : files[f].bytes;
            const uint32_t off = w.At();
            w.b.insert(w.b.end(), stored.begin(), stored.end());
            w.Pad(4);
            w.Set32(at + 4, off);
            w.Set32(at + 8, static_cast<uint32_t>(stored.size()));
            w.Set32(at + 12, static_cast<uint32_t>(files[f].bytes.size()));
        }
    }
    return w.b;
}

// A 2 x 1 image, 24-bit, rows from the bottom: red then green.
Bytes MakeTga() {
    Bytes t(18, 0);
    t[2] = 2;
    t[12] = 2;
    t[14] = 1;
    t[16] = 24;
    t.insert(t.end(), {0, 0, 255, 0, 255, 0});
    return t;
}

void WriteBytes(const std::filesystem::path& p, const Bytes& b) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

Bytes ReadBytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void TestModel() {
    Ps2Mps m;
    std::string err;
    CHECK(ParsePs2Mps(MakeMps(2, "Skin.TGA"), m, err));
    CHECK(err.empty());
    CHECK(m.triangles == 2);
    CHECK(m.meshes == 1);
    CHECK(m.boneNames.size() == 2 && m.boneNames[0] == "Root" && m.boneNames[1] == "Head");
    CHECK(m.boneParent.size() == 2 && m.boneParent[0] == -1 && m.boneParent[1] == 0);
    CHECK(m.vertices.size() == 4);
    CHECK(m.meshVertexCount.size() == 1 && m.meshVertexCount[0] == 4);
    // Bind: strip (x, y, z) to model (x, z, y), flag bits gone.
    for (int v = 0; v < 4 && m.vertices.size() == 4; ++v) {
        CHECK_NEAR(m.vertices[v].bind[0], 10 + v);
        CHECK_NEAR(m.vertices[v].bind[1], 30 + v);
        CHECK_NEAR(m.vertices[v].bind[2], 20 + v);
    }
    CHECK(m.vertices.size() == 4 && m.vertices[2].influences.size() == 2 && m.vertices[2].influences[1].bone == 1);
    // Triangles: k2 (0, 1, 2) = (v1, v2, v3); k3 (1, 3, 2) = (v2, v0, v3); k4 skipped.
    CHECK(m.tris.size() == 2);
    if (m.tris.size() == 2) {
        CHECK(m.tris[0].corners[0].vertex == 1 && m.tris[0].corners[1].vertex == 2 && m.tris[0].corners[2].vertex == 3);
        CHECK(m.tris[1].corners[0].vertex == 2 && m.tris[1].corners[1].vertex == 0 && m.tris[1].corners[2].vertex == 3);
        CHECK(m.tris[0].texture == "skin");
        CHECK_NEAR(m.tris[1].corners[1].u, 1.5);
    }
    // Cut short: refused with a reason, not read past the end.
    Bytes cut = MakeMps(2, "skin");
    cut.resize(cut.size() - 4);
    CHECK(!ParsePs2Mps(cut, m, err));
    CHECK(!err.empty());
}

void TestAnimations() {
    Ps2Mps m;
    Ps2Aps a, sharedSet;
    std::string err;
    CHECK(ParsePs2Mps(MakeMps(2, "skin"), m, err));
    CHECK(ParsePs2Aps(MakeAps(false), a, err));
    CHECK(ParsePs2Aps(MakeAps(true), sharedSet, err));
    CHECK(a.clips.size() == 3);
    if (a.clips.size() != 3) return;
    CHECK(!a.clips[0].skeletal && a.clips[0].nodes.size() == 1 && a.clips[0].nodes[0].vertices == 4);
    CHECK(a.clips[1].skeletal && a.clips[1].bones.size() == 2 && !a.clips[1].shared);
    CHECK(sharedSet.clips.size() == 3 && sharedSet.clips[1].shared && sharedSet.clips[1].bones.empty());

    // Vertex animated: frame 1 has every x + 8; half way, + 4; model space (x, z, y).
    const std::vector<float> walk = Ps2Pose(m, a.clips[0], {}, 0.5f);
    CHECK(walk.size() == 12);
    CHECK_NEAR(walk[0], 14);
    CHECK_NEAR(walk[1], 30);
    CHECK_NEAR(walk[2], 20);

    // Skeletal: each key is the bone's whole place in the model, not one relative to its parent. At frame 2 Root has
    // turned a quarter about z, so (1, 0, 0) goes to (0, -1, 0) (row vectors); Head stays 5 along x: (5, 1, 0).
    const std::vector<const Ps2Clip*> others = {&a.clips[1], &a.clips[2]};
    int rigid = -1;
    std::vector<float> p = Ps2Pose(m, a.clips[1], others, 2.0f, &rigid);
    CHECK(rigid == 0);
    CHECK_NEAR(p[0], 0);
    CHECK_NEAR(p[1], -1);
    CHECK_NEAR(p[3], 5);
    CHECK_NEAR(p[4], 1);
    // v2: half Root's (2, 0, 0) -> (0, -2, 0), half Head's (0, 2, 0) -> (5, 2, 0): (2.5, 0, 0).
    CHECK_NEAR(p[6], 2.5);
    CHECK_NEAR(p[7], 0);
    // At frame 0 nothing has turned.
    p = Ps2Pose(m, a.clips[1], others, 0.0f);
    CHECK_NEAR(p[3], 5);
    CHECK_NEAR(p[4], 1);
    // Clip 1 has no Head keys: Head follows Root at clip 0's offset (5 along x), so it turns with Root: (5, 1, 0) turned
    // is (1, -5, 0).
    p = Ps2Pose(m, a.clips[2], others, 2.0f, &rigid);
    CHECK(rigid == 0);
    CHECK_NEAR(p[3], 1);
    CHECK_NEAR(p[4], -5);
    // With no clip tracking both, Head moves rigidly with Root at no offset, and is counted.
    p = Ps2Pose(m, a.clips[2], {&a.clips[2]}, 2.0f, &rigid);
    CHECK(rigid == 1);
    CHECK_NEAR(p[3], 1);
    CHECK_NEAR(p[4], 0);
}

void TestTextures() {
    Ps2Image image;
    std::string err;
    CHECK(DecodePs2Tga(MakeTga(), image, err));
    CHECK(image.width == 2 && image.height == 1 && !image.alpha);
    CHECK(image.rgba.size() == 8 && image.rgba[0] == 255 && image.rgba[1] == 0 && image.rgba[5] == 255);
    const Ps2Image tiled = TilePs2Image(image, 2, 1);
    CHECK(tiled.width == 4 && tiled.height == 1 && tiled.rgba[8] == 255 && tiled.rgba[13] == 255);
    Ps2Image back;
    CHECK(DecodePs2Tga(EncodePs2Tga(tiled), back, err));
    CHECK(back.width == 4 && back.rgba == tiled.rgba);
    // Over 256: halved by averaging until it fits.
    Ps2Image big;
    big.width = 512;
    big.height = 2;
    big.rgba.assign(512 * 2 * 4, 0);
    for (int x = 0; x < 512; ++x) big.rgba[4 * x] = (x % 2) ? 200 : 100;
    const Ps2Image fit = TilePs2Image(big, 1, 1);
    CHECK(fit.width == 256 && fit.height == 1);
    CHECK(fit.rgba.size() == 256 * 4 && fit.rgba[0] == 75);
    CHECK(!DecodePs2Tga(Bytes(10, 0), image, err));
}

void TestPack() {
    Ps2Mps m, donorModel;
    Ps2Aps a, sharedSet;
    std::string err;
    CHECK(ParsePs2Mps(MakeMps(2, "skin"), m, err));
    donorModel = m;
    CHECK(ParsePs2Aps(MakeAps(false), a, err));
    CHECK(ParsePs2Aps(MakeAps(true), sharedSet, err));
    Ps2Tiling tiling;
    MergePs2Tiling(m, tiling);
    CHECK(tiling.size() == 1 && tiling["skin"] == std::make_pair(2, 1));
    tpw::Ps2Pack pack;
    std::vector<std::string> notes;
    CHECK(BuildPs2Pack(m, sharedSet, {{"donor", &donorModel, &a}}, tiling, pack, notes, err));
    CHECK(pack.clips.size() == 3);
    CHECK(pack.clips.size() == 3 && (pack.clips[0].flags & tpw::kPs2ClipVertex) && pack.clips[0].frames == 2);
    CHECK(pack.clips.size() == 3 && (pack.clips[1].flags & tpw::kPs2ClipShared) && pack.clips[1].frames == 3);
    CHECK(!notes.empty() && notes[0].find("keys from donor") != std::string::npos);
    // Without a donor the shared clip is left out, and said so.
    notes.clear();
    tpw::Ps2Pack alone;
    CHECK(BuildPs2Pack(m, sharedSet, {}, tiling, alone, notes, err));
    CHECK(alone.clips.size() == 2);
    // Texture coordinates over the doubled texture: 1.5 copies is 0.75 of it.
    CHECK(pack.triangles.size() == 2);
    CHECK_NEAR(pack.triangles[1].uv[2], 0.75);

    const Bytes file = tpw::WritePs2Pack(pack);
    tpw::Ps2Pack back;
    CHECK(tpw::ReadPs2Pack(file.data(), file.size(), back, err));
    CHECK(back.vertices == 4 && back.textures == pack.textures && back.clips.size() == 3);
    CHECK(back.bind == pack.bind);
    CHECK(back.frames == pack.frames);
    // The shared clip's last frame (the donor's clip 0), stored: v1 at (5, 1, 0) within one step of the scale.
    if (back.clips.size() == 3) {
        const tpw::Ps2PackClip& c = back.clips[1];
        const int16_t* v1 = &back.frames[c.offset + (2 * back.vertices + 1) * 3];
        CHECK(std::fabs(v1[0] * back.scale - 5) <= back.scale && std::fabs(v1[1] * back.scale - 1) <= back.scale);
    }
    CHECK_NEAR(back.bounds[0], 10);
    CHECK_NEAR(back.bounds[3], 13);
    // Damaged files are refused.
    CHECK(!tpw::ReadPs2Pack(file.data(), file.size() - 2, back, err));
    Bytes other = file;
    other[4] = 9;
    CHECK(!tpw::ReadPs2Pack(other.data(), other.size(), back, err));
}

void TestDisc(const std::filesystem::path& scratch) {
    // A WAD of the 24 characters, each the test model with its table triangle count; the animations packed.
    std::vector<std::pair<std::string, std::vector<WadFile>>> folders;
    for (const Ps2Character& ch : Ps2Characters())
        folders.push_back({ch.folder, {{std::string(ch.folder) + ".mps", MakeMps(static_cast<uint16_t>(ch.triangles), "skin"), false},
                                       {std::string(ch.folder) + ".aps", MakeAps(false), true}}});
    folders.push_back({"Textures", {{"skin.tga", MakeTga(), true}}});
    const Bytes wad = MakeWad(folders);
    std::map<std::string, Bytes> files;
    std::string err;
    CHECK(ReadPs2Wad(wad, "Chars/", files, err));
    CHECK(files.size() == 49);
    const Bytes* aps = FindPs2File(files, "chars/boy1a/BOY1A.APS");
    CHECK(aps != nullptr && *aps == MakeAps(false));
    CHECK(!ReadPs2Wad(Bytes(wad.begin(), wad.begin() + 40), "Chars/", files, err));

    const std::filesystem::path disc = scratch / "disc", data = scratch / "data";
    std::filesystem::remove_all(scratch);
    WriteBytes(disc / "SYSTEM.CNF", {'B', 'O', 'O', 'T', '2', ' ', '=', ' ', 'c', 'd', 'r', 'o', 'm', '0', ':', '\\', 'S', 'L', 'U', 'S', '_',
                                     '2', '0', '0', '.', '9', '9', ';', '1', '\n'});
    WriteBytes(disc / "SLUS_200.99", {0x7f, 'E', 'L', 'F', 0, 0, 0, 0});
    WriteBytes(disc / "DATA" / "DATA.WAD", wad);
    Ps2Disc found;
    CHECK(RecognisePs2Path(disc.u8string(), found, err) == Ps2Status::Supported);
    CHECK(found.boot == "SLUS_200.99" && found.chars.size() == 49);

    std::vector<std::string> log;
    CHECK(ReadPs2ConverterStamp(data) == 0);
    CHECK(ConvertPs2(found, data, log, err));
    CHECK(err.empty());
    CHECK(log.size() == 24);
    CHECK(ReadPs2ConverterStamp(data) == kPs2ConverterVersion);
    CHECK(std::filesystem::exists(Ps2ContentPath(data) / "tex" / "skin.tga"));
    CHECK(!std::filesystem::exists(data / "assets" / "ps2.converting"));
    const Bytes boy = ReadBytes(Ps2ContentPath(data) / "boy1a.p2m");
    tpw::Ps2Pack pack;
    CHECK(tpw::ReadPs2Pack(boy.data(), boy.size(), pack, err));
    CHECK(pack.clips.size() == 3 && pack.triangles.size() == 2);
    Ps2Image tex;
    CHECK(DecodePs2Tga(ReadBytes(Ps2ContentPath(data) / "tex" / "skin.tga"), tex, err));
    CHECK(tex.width == 4 && tex.height == 1);

    // Another count for one character: a PS2 disc, but not this one.
    folders[1].second[0].bytes = MakeMps(7, "skin");
    WriteBytes(disc / "DATA" / "DATA.WAD", MakeWad(folders));
    CHECK(RecognisePs2Path(disc.u8string(), found, err) == Ps2Status::Rejected);
    CHECK(err.find("Boy1a") != std::string::npos);
    // Another PS2 game: not ours at all.
    std::filesystem::remove(disc / "DATA" / "DATA.WAD");
    WriteBytes(disc / "SYSTEM.CNF", {'B', 'O', 'O', 'T', '2', '=', 'S', 'L', 'U', 'S', '_', '9', '9', '9', '.', '9', '9', '\n'});
    CHECK(RecognisePs2Path(disc.u8string(), found, err) == Ps2Status::NotPs2);
    std::filesystem::remove_all(scratch);
}

// The launcher's drop (importer.h): the disc's folder is recognised and converted, one line in the report, none of its
// files counted as the game's; a damaged copy, another PS2 game, and a PS2 image without the game's data are refused
// with one error each, naming the drop and the reason, and convert nothing.
void TestImport(const std::filesystem::path& scratch) {
    std::vector<std::pair<std::string, std::vector<WadFile>>> folders;
    for (const Ps2Character& ch : Ps2Characters())
        folders.push_back({ch.folder, {{std::string(ch.folder) + ".mps", MakeMps(static_cast<uint16_t>(ch.triangles), "skin"), false},
                                       {std::string(ch.folder) + ".aps", MakeAps(false), true}}});
    folders.push_back({"Textures", {{"skin.tga", MakeTga(), true}}});
    std::filesystem::remove_all(scratch);
    const std::filesystem::path disc = scratch / "Theme Park Roller Coaster";
    const std::string cnf = "BOOT2 = cdrom0:\\SLUS_200.99;1\nVER = 1.00\nVMODE = NTSC\n";
    WriteBytes(disc / "SYSTEM.CNF", Bytes(cnf.begin(), cnf.end()));
    WriteBytes(disc / "SLUS_200.99", {0x7f, 'E', 'L', 'F', 0, 0, 0, 0});
    WriteBytes(disc / "DATA" / "DATA.WAD", MakeWad(folders));
    // Run one drop into a fresh data folder: its report and progress events.
    auto drop = [&](const std::string& name, const std::filesystem::path& what, std::vector<ImportProgress>& events) {
        ImporterConfig cfg;
        cfg.dataRoot = (scratch / name).u8string();
        Importer imp(cfg);
        std::string err;
        CHECK(imp.Open(err));
        ImportReport r = imp.Import({what.u8string()});
        events = imp.Progress().Drain();
        return r;
    };
    std::vector<ImportProgress> ev;
    ImportReport r = drop("data", disc, ev);
    const std::string line = "PS2 Guest Models: 24 characters converted from the US PlayStation 2 disc (SLUS_200.99).";
    CHECK(r.errors.empty());
    CHECK(r.playStation2.size() == 1 && r.playStation2[0] == line);
    CHECK(r.playStation2Converted && !r.ChangedNothing());
    CHECK(r.written == 0 && r.unknownFiles == 0 && r.playStation.empty() && r.themeParkInc.empty());
    CHECK(r.Summary() == line);
    CHECK(ReadPs2ConverterStamp(scratch / "data") == kPs2ConverterVersion);
    CHECK(std::filesystem::exists(Ps2ContentPath(scratch / "data") / "boy1a.p2m"));
    CHECK(std::any_of(ev.begin(), ev.end(), [&](const ImportProgress& e) {
        return e.kind == ImportProgress::Kind::Content && e.message == line;
    }));

    // Damaged: one character's model is not the disc's.
    folders[3].second[0].bytes = MakeMps(5, "skin");
    WriteBytes(disc / "DATA" / "DATA.WAD", MakeWad(folders));
    r = drop("damaged", disc, ev);
    const std::string refused = "Theme Park Roller Coaster: PlayStation 2 disc refused: ";
    CHECK(r.errors.size() == 1 && r.errors[0].compare(0, refused.size(), refused) == 0);
    CHECK(!r.errors.empty() && r.errors[0].find(Ps2Characters()[3].folder) != std::string::npos);
    CHECK(r.playStation2.empty() && !r.playStation2Converted && r.ChangedNothing());
    CHECK(ReadPs2ConverterStamp(scratch / "damaged") == 0);
    CHECK(std::any_of(ev.begin(), ev.end(), [&](const ImportProgress& e) {
        return e.kind == ImportProgress::Kind::Error && e.message == (r.errors.empty() ? "" : r.errors[0]);
    }));

    // Another PS2 game: named as such.
    std::filesystem::remove(disc / "DATA" / "DATA.WAD");
    const std::string other = "BOOT2 = cdrom0:\\SLUS_999.99;1\n";
    WriteBytes(disc / "SYSTEM.CNF", Bytes(other.begin(), other.end()));
    r = drop("other", disc, ev);
    CHECK(r.errors.size() == 1 && r.errors[0] == refused + "it is another PlayStation 2 game (SYSTEM.CNF boots SLUS_999.99), "
                                                           "not Theme Park Roller Coaster (SLUS_200.99)");
    CHECK(r.playStation2.empty() && r.unknownFiles == 0);

    // An ISO 9660 image of a PS2 disc (the PlayStation system identifier, as a PS1 disc's): not taken for a PS1 disc, and
    // without DATA/DATA.WAD refused as a PS2 disc.
    std::vector<uint8_t> iso = tpwtest::IsoImageOf({{"SYSTEM.CNF", cnf}, {"SLUS_200.99", std::string("\x7f" "ELF\0\0\0\0", 8)}});
    std::memcpy(iso.data() + 16 * 2048 + 8, "PLAYSTATION", 11);
    WriteBytes(scratch / "TPRC.iso", Bytes(iso.begin(), iso.end()));
    r = drop("iso", scratch / "TPRC.iso", ev);
    CHECK(r.errors.size() == 1 && r.errors[0] == "TPRC.iso: PlayStation 2 disc refused: DATA/DATA.WAD is missing");
    CHECK(r.playStation.empty() && r.playStation2.empty());
    if (r.errors.size() != 1) std::printf("  iso: %s\n", r.Summary().c_str());
    for (const std::string& e : r.errors) std::printf("  %s\n", e.c_str());
    std::filesystem::remove_all(scratch);
}

}  // namespace

int main() {
    TestModel();
    TestAnimations();
    TestTextures();
    TestPack();
    const char* env = std::getenv("TPW_TEST_SCRATCH");
    TestDisc(std::filesystem::path(env ? env : TPW_TEST_BUILD_SCRATCH) / "ps2-convert");
    TestImport(std::filesystem::path(env ? env : TPW_TEST_BUILD_SCRATCH) / "ps2-import");
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
