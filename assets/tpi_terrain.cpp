// tpi_terrain.cpp: the terrain repair of the Theme Park Inc converter (tpi_terrain.h). The height field is include/m3d2.h's
// sHField, at the header's 0x6c: the quad grid's width and height at 0x18 and 0x1c, its origin (x, z) and cell size (x, z)
// at 0x08, the (width + 1) x (height + 1) grid point heights at 0x28 and the quads (two words: flags, texture index) at
// 0x2c, row by row; quad (x, y) covers world x [ox + x cx, ox + (x + 1) cx] and z [oz + y cz, oz + (y + 1) cz]
// (src/engine/tb_hfield.cpp).
//
// An obstacle shows before it is cleared what its first clearing animation (<name>m1.MD2, <name>m.MD2 for the ones with
// one) shows at its first frame, as src/game/rides/RideAnim.cpp animates a mesh: the nodes of the animation's list to
// disable (tag_sAnimHeader mNodesToDisable, node ids: the node record's word at 0x50) start hidden, and a node animation
// with flag 0x20000 (tag_sNodeAnim, its node by record index in mF14: full nodes first, then minis) shows its node from
// the last entry of its enable list at or before the frame when that is positive and hides it otherwise. The obstacles'
// meshes are modelled in the world's frame (their .sam files say Info.DontApplyOffset 1), so a node's place in the
// terrain is its world matrix in the obstacle with the obstacle's root matrix left out.
#include "tpi_terrain.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>

#include "tpi_island_md2.h"
#include "tpi_sam.h"

namespace tpw::assets {

namespace {

using namespace tpi_island;
using Bytes = std::vector<uint8_t>;

constexpr uint32_t kQuadHidden = 0x0001;   // tb_hfield.cpp kCellHidden
// Coverage is sampled kSamples x kSamples per quad; a hidden quad with fewer than half its samples under geometry is
// one nothing draws over.
constexpr int kSamples = 4;
constexpr int kCoveredSamples = kSamples * kSamples / 2;
// An obstacle covers a shown quad's sample at ground level when its geometry there is this close to the height field
// (world units; a cell is 10): only then does hiding the quad leave the obstacle drawing the ground in its place.
constexpr float kGroundBelow = 0.5f;
constexpr float kGroundAbove = 1.5f;
// The quads an obstacle's covered cells lose, and the shown quads around them, are drawn again this far below the ground,
// under the obstacle's geometry (below the lowest of it that counts as ground), so that no seam between that geometry
// and the terrain around it, and no part of a cell it leaves uncovered, shows the clear colour. A seam is looked through
// at a slant, so the ring of shown quads catches what passes under the terrain beside it.
constexpr float kUnderGround = kGroundBelow + 0.1f;
constexpr size_t kMaxName = 19;            // a texture name's characters (the mesh's 20-byte names)
constexpr uint32_t kRecord = 0xa0, kMiniRecord = 0x58;

uint16_t L16(const Bytes& d, size_t at) { return static_cast<uint16_t>(d[at] | (d[at + 1] << 8)); }
uint32_t L32(const Bytes& d, size_t at) {
    return static_cast<uint32_t>(d[at]) | (static_cast<uint32_t>(d[at + 1]) << 8) | (static_cast<uint32_t>(d[at + 2]) << 16) |
           (static_cast<uint32_t>(d[at + 3]) << 24);
}
float LF(const Bytes& d, size_t at) {
    float f;
    std::memcpy(&f, d.data() + at, 4);
    return f;
}
void Put16(Bytes& d, size_t at, uint32_t v) {
    d[at] = static_cast<uint8_t>(v);
    d[at + 1] = static_cast<uint8_t>(v >> 8);
}
bool In(const Bytes& d, uint64_t at, uint64_t n) { return at + n <= d.size() && at + n >= at; }

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string Base(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
std::string Dir(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

// A mesh's height field: the quad grid's size, the offsets of its heights and quads, its origin and cell size.
struct Grid {
    uint32_t w = 0, h = 0, heights = 0, quads = 0;
    float ox = 0, oz = 0, cx = 0, cz = 0;
};

bool ReadGrid(const Bytes& d, Grid& g, std::string& err) {
    const uint32_t hf = L32(d, 0x6c);
    if (!In(d, hf, 0x30)) {
        err = "height field past the end of the file";
        return false;
    }
    g.w = L32(d, hf + 0x18);
    g.h = L32(d, hf + 0x1c);
    g.heights = L32(d, hf + 0x28);
    g.quads = L32(d, hf + 0x2c);
    g.ox = LF(d, hf + 0x08);
    g.oz = LF(d, hf + 0x0c);
    g.cx = LF(d, hf + 0x10);
    g.cz = LF(d, hf + 0x14);
    if (g.w == 0 || g.h == 0 || g.w > 4096 || g.h > 4096 || !In(d, g.quads, uint64_t(g.w) * g.h * 4) ||
        !In(d, g.heights, uint64_t(g.w + 1) * (g.h + 1) * 4) || !(g.cx > 0) || !(g.cz > 0)) {
        err = "height field quads past the end of the file";
        return false;
    }
    return true;
}

// The number of samples of each quad that the nodes' geometry covers seen from above (x z), row by row. With `heights`
// (the grid points' heights) only geometry from kGroundBelow below the ground to kGroundAbove above it counts.
std::vector<int> Coverage(const Md2Model& model, const Grid& g, const std::vector<float>* heights) {
    const int sw = static_cast<int>(g.w) * kSamples, sh = static_cast<int>(g.h) * kSamples;
    std::vector<uint8_t> hit(static_cast<size_t>(sw) * sh, 0);
    const std::vector<Matrix> world = WorldMatrices(model);
    auto ground = [&](float x, float z) {
        const float gx = std::clamp((x - g.ox) / g.cx, 0.0f, static_cast<float>(g.w));
        const float gz = std::clamp((z - g.oz) / g.cz, 0.0f, static_cast<float>(g.h));
        const uint32_t i = std::min(static_cast<uint32_t>(gx), g.w - 1), j = std::min(static_cast<uint32_t>(gz), g.h - 1);
        const float fx = gx - static_cast<float>(i), fz = gz - static_cast<float>(j);
        auto at = [&](uint32_t a, uint32_t b) { return (*heights)[static_cast<size_t>(b) * (g.w + 1) + a]; };
        return (at(i, j) * (1 - fx) + at(i + 1, j) * fx) * (1 - fz) + (at(i, j + 1) * (1 - fx) + at(i + 1, j + 1) * fx) * fz;
    };
    for (size_t n = 0; n < model.nodes.size(); ++n) {
        const Md2Node& node = model.nodes[n];
        for (const Md2Face& face : node.faces) {
            Vec3 p[3];
            bool ok = true;
            for (int k = 0; k < 3; ++k) {
                if (face.corner[k] >= node.cornerVertex.size() || node.cornerVertex[face.corner[k]] >= node.vertices.size()) {
                    ok = false;
                    break;
                }
                p[k] = Transform(node.vertices[node.cornerVertex[face.corner[k]]], world[n]);
            }
            if (!ok)
                continue;
            const float minx = std::min({p[0].x, p[1].x, p[2].x}), maxx = std::max({p[0].x, p[1].x, p[2].x});
            const float minz = std::min({p[0].z, p[1].z, p[2].z}), maxz = std::max({p[0].z, p[1].z, p[2].z});
            const int sx0 = std::max(0, static_cast<int>((minx - g.ox) / g.cx * kSamples) - 1);
            const int sx1 = std::min(sw - 1, static_cast<int>((maxx - g.ox) / g.cx * kSamples) + 1);
            const int sz0 = std::max(0, static_cast<int>((minz - g.oz) / g.cz * kSamples) - 1);
            const int sz1 = std::min(sh - 1, static_cast<int>((maxz - g.oz) / g.cz * kSamples) + 1);
            for (int sz = sz0; sz <= sz1; ++sz)
                for (int sx = sx0; sx <= sx1; ++sx) {
                    const float x = g.ox + (sx + 0.5f) * g.cx / kSamples, z = g.oz + (sz + 0.5f) * g.cz / kSamples;
                    const float d0 = (p[1].x - p[0].x) * (z - p[0].z) - (p[1].z - p[0].z) * (x - p[0].x);
                    const float d1 = (p[2].x - p[1].x) * (z - p[1].z) - (p[2].z - p[1].z) * (x - p[1].x);
                    const float d2 = (p[0].x - p[2].x) * (z - p[2].z) - (p[0].z - p[2].z) * (x - p[2].x);
                    const bool neg = d0 < 0 || d1 < 0 || d2 < 0, pos = d0 > 0 || d1 > 0 || d2 > 0;
                    if (neg && pos)
                        continue;
                    if (heights != nullptr) {
                        // The face's height at the sample (d0, d1 and d2 weigh the corners opposite their edges).
                        const float sum = d0 + d1 + d2;
                        if (sum == 0)
                            continue;
                        const float y = (d1 * p[0].y + d2 * p[1].y + d0 * p[2].y) / sum, floor = ground(x, z);
                        if (y < floor - kGroundBelow || y > floor + kGroundAbove)
                            continue;
                    }
                    hit[static_cast<size_t>(sz) * sw + sx] = 1;
                }
        }
    }
    std::vector<int> count(static_cast<size_t>(g.w) * g.h, 0);
    for (int sz = 0; sz < sh; ++sz)
        for (int sx = 0; sx < sw; ++sx)
            count[static_cast<size_t>(sz / kSamples) * g.w + sx / kSamples] += hit[static_cast<size_t>(sz) * sw + sx];
    return count;
}

// The hidden quads that nothing covers, the terrain's own geometry or an obstacle's: they show the clear colour.
std::vector<std::pair<int, int>> StillClear(const Md2Model& model, const Grid& g, const std::vector<uint16_t>& quadFlags) {
    std::vector<std::pair<int, int>> out;
    const std::vector<int> covered = Coverage(model, g, nullptr);
    for (uint32_t y = 0; y < g.h; ++y)
        for (uint32_t x = 0; x < g.w; ++x)
            if ((quadFlags[size_t(y) * g.w + x] & kQuadHidden) != 0 && covered[size_t(y) * g.w + x] < kCoveredSamples)
                out.push_back({static_cast<int>(x), static_cast<int>(y)});
    return out;
}

// ---- an obstacle's files

// The rows of a .sam's shape block (`header`: "Info.Shape", "Obstacle.AfterShape"; between the two lines of dashes after
// it); false when there is none.
bool ShapeRows(const std::string& text, const std::string& header, std::vector<std::string>& rows) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        lines.push_back(line);
        start = end + 1;
    }
    auto dashes = [](const std::string& s) { return !s.empty() && s.find_first_not_of('-') == std::string::npos; };
    for (size_t i = 0; i + 1 < lines.size(); ++i) {
        const size_t first = lines[i].find_first_not_of(" \t");
        if (first == std::string::npos || lines[i].substr(first) != header || !dashes(lines[i + 1]))
            continue;
        rows.clear();
        for (size_t j = i + 2; j < lines.size(); ++j) {
            if (dashes(lines[j]))
                return !rows.empty();
            rows.push_back(lines[j]);
        }
        return false;
    }
    return false;
}

// The clearing crew Theme Park Inc animates beside a waiting obstacle ("boy 01", "girl 03", their tent): they stand
// there while it is cleared, which a fixed piece of scenery never is.
bool IsCrew(const std::string& name) {
    const std::string n = Lower(name);
    return n.compare(0, 4, "boy ") == 0 || n.compare(0, 5, "girl ") == 0 || n == "tent" || n == "tentbase";
}

// The crew's tools and the site's props ("hoard4", "plank 02", "shovel01", the camera, the sparkles), which an obstacle
// shows while it is cleared and not once it is.
bool IsSiteProp(const std::string& name) {
    const std::string n = Lower(name);
    return n.compare(0, 5, "hoard") == 0 || n.compare(0, 5, "plank") == 0 || n.compare(0, 6, "shovel") == 0 ||
           n.find("camera") != std::string::npos || n.find("sprk") != std::string::npos;
}

// The decals ("unbuildable01", "nonbuildable") that mark the land an obstacle closes until it is cleared.
bool IsMarker(const std::string& name) {
    const std::string n = Lower(name);
    return n.compare(0, 11, "unbuildable") == 0 || n.compare(0, 12, "nonbuildable") == 0;
}

// The mesh's node index (as ReadMd2 orders them: the root first, then the other records in file order) for each node
// record, or false when the mesh's node tables cannot be read.
bool RecordNodes(const Bytes& mesh, std::vector<size_t>& node, std::string& err) {
    const uint32_t all = L16(mesh, 0x42), fulls = L16(mesh, 0x44), nodes = L32(mesh, 0x70), minis = L32(mesh, 0x74),
                   root = L32(mesh, 0x78);
    if (fulls > all || !In(mesh, nodes, uint64_t(fulls) * kRecord) || !In(mesh, minis, uint64_t(all - fulls) * kMiniRecord)) {
        err = "mesh nodes past the end of the file";
        return false;
    }
    uint32_t rootRecord = all;
    for (uint32_t r = 0; r < all; ++r)
        if ((r < fulls ? nodes + kRecord * r : minis + kMiniRecord * (r - fulls)) == root)
            rootRecord = r;
    if (rootRecord == all) {
        err = "mesh root node is not in its node tables";
        return false;
    }
    node.resize(all);
    for (uint32_t r = 0; r < all; ++r) node[r] = r == rootRecord ? 0 : (r < rootRecord ? r + 1 : r);
    return true;
}

// Marks the nodes (ReadMd2's order) that an animation moves: a node animation with keys for its place, turn, scale,
// path or shape (tag_sNodeAnim's pointers at 0x18 to 0x28; a texture scroll, 0x2c, does not move it).
void MarkMoved(const Bytes& mesh, const Bytes& d, std::vector<bool>& moved) {
    std::vector<size_t> node;
    std::string ignored;
    if (!RecordNodes(mesh, node, ignored) || !In(d, 0x98, 4) || L32(d, 0x98) == 0)
        return;
    const uint32_t a = L32(d, 0x98);
    if (!In(d, a, 0x48))
        return;
    const uint32_t count = L16(d, a + 0x12), at = L32(d, a + 0x2c);
    if (!In(d, at, uint64_t(count) * 0x40))
        return;
    moved.resize(node.size(), false);
    for (uint32_t k = 0; k < count; ++k) {
        const uint32_t o = at + 0x40 * k, record = L16(d, o + 0x14);
        if (record >= node.size())
            continue;
        for (uint32_t p = 0x18; p <= 0x28; p += 4)
            if (L32(d, o + p) != 0)
                moved[node[record]] = true;
    }
}

// Poses the model (ReadMd2's nodes of `mesh`) as the animation `d` leaves it at its last frame (tag_sAnimHeader mF08),
// as AnimateNode (src/game/rides/RideAnim.cpp) does at that frame: a compressed morph (flag 0x1000 without 0x4000)
// gives the node the vertices its key data hold there (RADecompressMorph: the shared ones, then each moving key data's
// row at or before the frame blended with the next; a word's three 10-bit fields times the morph's scale plus its
// offset), its place along its shape (flag 1: the shape's point of the key at or before the frame, a bezier shape's
// (mF00 flag 2) every third from the second; the obstacles' shapes are in the world's frame, as the matrices are), the
// rotation keys (flag 8) its turn from the key at or before the frame, and the scale keys (flag 0x80) its scale.
// Answers whether it posed a node.
bool PoseAtEnd(const Bytes& mesh, const Bytes& d, Md2Model& model) {
    std::vector<size_t> node;
    std::string ignored;
    if (!RecordNodes(mesh, node, ignored) || !In(d, 0x98, 4) || L32(d, 0x98) == 0)
        return false;
    const uint32_t a = L32(d, 0x98);
    if (!In(d, a, 0x48))
        return false;
    const uint32_t frame = L32(d, a + 8), count = L16(d, a + 0x12), at = L32(d, a + 0x2c);
    if (!In(d, at, uint64_t(count) * 0x40))
        return false;
    auto field = [](uint32_t word, int shift) {
        const int32_t v = static_cast<int32_t>((word >> shift) & 0x3ff);
        return static_cast<float>(v >= 0x200 ? v - 0x400 : v);
    };
    bool posed = false;
    for (uint32_t k = 0; k < count; ++k) {
        const uint32_t o = at + 0x40 * k, record = L16(d, o + 0x14), flags = L32(d, o + 4);
        if (record >= node.size() || node[record] >= model.nodes.size())
            continue;
        Md2Node& n = model.nodes[node[record]];
        const uint32_t morph = L32(d, o + 0x28);
        if ((flags & 0x1000) != 0 && (flags & 0x4000) == 0 && morph != 0 && In(d, morph, 0x2c)) {
            const uint32_t keys = L16(d, morph + 2), keyAt = L32(d, morph + 0xc), shared = L16(d, morph) & 2;
            float offset[3], scale[3];
            for (int c = 0; c < 3; ++c) offset[c] = LF(d, morph + 0x14 + 4 * c), scale[c] = LF(d, morph + 0x20 + 4 * c);
            auto point = [&](uint32_t word) {
                return Vec3{field(word, 0) * scale[0] + offset[0], field(word, 10) * scale[1] + offset[1],
                            field(word, 20) * scale[2] + offset[2]};
            };
            if (In(d, keyAt, uint64_t(keys) * 0x14))
                for (uint32_t i = 1; i < keys; ++i) {
                    const uint32_t ko = keyAt + 0x14 * i, rows = L16(d, ko), words = L16(d, ko + 2), numbers = L32(d, ko + 4),
                                   frames = L32(d, ko + 8), vertices = L32(d, ko + 12);
                    const bool sharedKey = shared != 0 && i == 1;
                    if (rows == 0 || !In(d, numbers, uint64_t(words) * 2) || !In(d, frames, uint64_t(rows) * 2) ||
                        !In(d, vertices, uint64_t(rows) * words * 4))
                        continue;
                    uint32_t row = 0;
                    while (!sharedKey && row + 1 < rows && L16(d, frames + 2 * (row + 1)) <= frame) ++row;
                    float t = 0;
                    if (!sharedKey && row + 1 < rows) {
                        const float first = L16(d, frames + 2 * row), second = L16(d, frames + 2 * (row + 1));
                        t = second > first ? std::clamp((static_cast<float>(frame) - first) / (second - first), 0.0f, 1.0f) : 0.0f;
                    }
                    for (uint32_t w = 0; w < words; ++w) {
                        const uint32_t number = L16(d, numbers + 2 * w);
                        if (number >= n.vertices.size())
                            continue;
                        Vec3 p = point(L32(d, vertices + 4 * (row * words + w)));
                        if (t > 0) {
                            const Vec3 q = point(L32(d, vertices + 4 * ((row + 1) * words + w)));
                            p = Vec3{p.x * (1 - t) + q.x * t, p.y * (1 - t) + q.y * t, p.z * (1 - t) + q.z * t};
                        }
                        n.vertices[number] = p;
                    }
                    posed = true;
                }
        }
        const uint32_t shape = L32(d, o + 0x18);
        if ((flags & 1) != 0 && shape != 0 && In(d, shape, 0x10)) {
            const uint32_t kind = L32(d, shape), points = L16(d, shape + 4), pairs = L16(d, shape + 6),
                           pointAt = L32(d, shape + 8), wordAt = L32(d, shape + 0xc);
            if (points != 0 && pairs != 0 && In(d, pointAt, uint64_t(points) * 12) && In(d, wordAt, uint64_t(pairs) * 4)) {
                uint32_t key = 0;
                while (key + 1 < pairs && L16(d, wordAt + 4 * (key + 1)) <= frame) ++key;
                const uint32_t p = std::min((kind & 2) != 0 ? key * 3 + 1 : key, points - 1);
                for (int c = 0; c < 3; ++c) n.matrix[12 + c] = LF(d, pointAt + 12 * p + 4 * c);
                posed = true;
            }
        }
        const uint32_t quats = L16(d, o + 0x10), quatAt = L32(d, o + 0x1c);
        if ((flags & 8) != 0 && quats != 0 && In(d, quatAt, uint64_t(quats) * 0x14)) {
            uint32_t key = 0;
            while (key + 1 < quats && L16(d, quatAt + 0x14 * (key + 1)) <= frame) ++key;
            const uint32_t q = quatAt + 0x14 * key + 4;
            const float x = LF(d, q), y = LF(d, q + 4), z = LF(d, q + 8), w = LF(d, q + 12);
            const float rows[9] = {1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
                                   2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
                                   2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)};
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c) n.matrix[4 * r + c] = rows[3 * r + c];
            posed = true;
        }
        const uint32_t scales = L16(d, o + 0x12), scaleAt = L32(d, o + 0x20);
        if ((flags & 0x80) != 0 && scales != 0 && In(d, scaleAt, uint64_t(scales) * 0x10)) {
            uint32_t key = 0;
            while (key + 1 < scales && L16(d, scaleAt + 0x10 * (key + 1)) <= frame) ++key;
            for (int r = 0; r < 3; ++r) {
                const float s = LF(d, scaleAt + 0x10 * key + 4 + 4 * r);
                for (int c = 0; c < 3; ++c) n.matrix[4 * r + c] *= s;
            }
            posed = true;
        }
    }
    return posed;
}

// For each node of the mesh as ReadMd2 orders them (the root first, then the other records in file order): shown at the
// animation's first frame (its last with `atEnd`). All are shown without an animation.
bool ShownAtStart(const Bytes& mesh, const Bytes* anim, std::vector<bool>& shown, std::string& err, bool atEnd = false) {
    const uint32_t all = L16(mesh, 0x42), fulls = L16(mesh, 0x44), nodes = L32(mesh, 0x70), minis = L32(mesh, 0x74),
                   root = L32(mesh, 0x78);
    if (fulls > all || !In(mesh, nodes, uint64_t(fulls) * kRecord) || !In(mesh, minis, uint64_t(all - fulls) * kMiniRecord)) {
        err = "mesh nodes past the end of the file";
        return false;
    }
    auto offset = [&](uint32_t r) { return r < fulls ? nodes + kRecord * r : minis + kMiniRecord * (r - fulls); };
    uint32_t rootRecord = all;
    for (uint32_t r = 0; r < all; ++r)
        if (offset(r) == root)
            rootRecord = r;
    if (rootRecord == all) {
        err = "mesh root node is not in its node tables";
        return false;
    }
    std::vector<bool> record(all, true);
    if (anim != nullptr && In(*anim, 0x98, 4) && L32(*anim, 0x98) != 0) {
        const Bytes& d = *anim;
        const uint32_t a = L32(d, 0x98);
        if (!In(d, a, 0x48)) {
            err = "animation past the end of the file";
            return false;
        }
        const uint32_t frame = L32(d, a + (atEnd ? 8 : 4));
        const uint32_t disables = L16(d, a + 0x1a), disableAt = L32(d, a + 0x38);
        if (disableAt != 0 && In(d, disableAt, uint64_t(disables) * 2))
            for (uint32_t i = 0; i < disables; ++i) {
                const uint16_t id = L16(d, disableAt + 2 * i);
                for (uint32_t r = 0; r < all; ++r)
                    if (L16(mesh, offset(r) + 0x50) == id) {
                        record[r] = false;
                        break;
                    }
            }
        const uint32_t count = L16(d, a + 0x12), at = L32(d, a + 0x2c);
        if (!In(d, at, uint64_t(count) * 0x40)) {
            err = "node animations past the end of the file";
            return false;
        }
        for (uint32_t k = 0; k < count; ++k) {
            const uint32_t o = at + 0x40 * k;
            const uint32_t node = L16(d, o + 0x14), entries = L16(d, o + 0x16), enables = L32(d, o + 0x30);
            if ((L32(d, o + 4) & 0x20000) == 0 || enables == 0 || node >= all || !In(d, enables, uint64_t(entries) * 2))
                continue;
            for (uint32_t left = entries; left != 0; --left) {
                const int16_t e = static_cast<int16_t>(L16(d, enables + 2 * (left - 1)));
                const int16_t magnitude = e < 0 ? static_cast<int16_t>(-static_cast<int32_t>(e)) : e;
                if (frame >= static_cast<uint32_t>(static_cast<int32_t>(magnitude))) {
                    record[node] = e > 0;
                    break;
                }
            }
        }
    }
    shown.assign(all, false);
    for (uint32_t r = 0; r < all; ++r) shown[r == rootRecord ? 0 : (r < rootRecord ? r + 1 : r)] = record[r];
    return true;
}

// ---- textures

const char* const kFolders[2] = {"textures", "stexture"};   // the full size and the small textures (TextureSize)

// A wad's texture folders: lower-case file name to member index, per folder.
struct Folders {
    std::map<std::string, size_t> files[2];
};
Folders TextureFolders(const std::vector<TpiWadMember>& members) {
    Folders f;
    for (size_t i = 0; i < members.size(); ++i) {
        const std::string dir = Lower(Dir(members[i].name)), base = Lower(Base(members[i].name));
        for (int k = 0; k < 2; ++k)
            if (dir == kFolders[k] && base != "qickload.txt")
                f.files[k].emplace(base, i);
    }
    return f;
}

bool SameTexture(const Md2Texture& a, const Md2Texture& b) {
    if (a.material != b.material || a.f02 != b.f02 || a.infoFlags != b.infoFlags || a.f08[0] != b.f08[0] ||
        a.f08[1] != b.f08[1] || a.frames.size() != b.frames.size())
        return false;
    for (size_t i = 0; i < a.frames.size(); ++i)
        if (Lower(a.frames[i]) != Lower(b.frames[i]))
            return false;
    return true;
}

}  // namespace

bool TpiTgaFullyTransparent(const std::vector<uint8_t>& t) {
    if (t.size() < 18 || t[1] != 0 || t[2] != 2 || t[16] != 32)
        return false;
    const size_t w = L16(t, 12), h = L16(t, 14), at = 18 + t[0];
    if (w == 0 || h == 0 || !In(t, at, uint64_t(w) * h * 4))
        return false;
    for (size_t i = 0; i < w * h; ++i)
        if (t[at + 4 * i + 3] != 0)
            return false;
    return true;
}

bool RepairTpiTerrain(std::vector<uint8_t>& d,
                      const std::function<const std::vector<uint8_t>*(const std::string& name)>& texture,
                      TpiTerrainReport& report, std::string& err) {
    report = TpiTerrainReport();
    Md2Model model;
    if (!ReadMd2(d, model, err))
        return false;
    if ((model.flags & 2) == 0)
        return true;
    Grid g;
    if (!ReadGrid(d, g, err))
        return false;
    const size_t count = static_cast<size_t>(g.w) * g.h;

    // Fully transparent textures: their quads are hidden.
    std::vector<bool> clear(model.textures.size(), false);
    for (size_t i = 0; i < model.textures.size(); ++i) {
        if (model.textures[i].frames.empty())
            continue;
        const std::vector<uint8_t>* bytes = texture(model.textures[i].frames[0]);
        clear[i] = bytes != nullptr && TpiTgaFullyTransparent(*bytes);
    }
    std::vector<bool> used(model.textures.size(), false);
    for (size_t q = 0; q < count; ++q) {
        const uint16_t flags = L16(d, g.quads + 4 * q), t = L16(d, g.quads + 4 * q + 2);
        if (t >= clear.size() || !clear[t])
            continue;
        if (!used[t]) {
            used[t] = true;
            report.clearTextures.push_back(model.textures[t].frames[0]);
        }
        if ((flags & kQuadHidden) == 0) {
            Put16(d, g.quads + 4 * q, flags | kQuadHidden);
            ++report.madeHidden;
        }
    }
    return true;
}

bool MergeTpiObstacles(std::vector<TpiWadMember>& terrain, const std::vector<TpiObstacle>& obstacles,
                       const std::function<const std::vector<uint8_t>*(const std::string& path)>& shared,
                       TpiObstacleReport& report, std::string& err) {
    report = TpiObstacleReport();
    std::vector<TpiWadMember> out = terrain;
    size_t baseAt = out.size();
    for (size_t i = 0; i < out.size(); ++i)
        if (Dir(out[i].name).empty() && Lower(out[i].name) == "base.md2")
            baseAt = i;
    if (baseAt == out.size()) {
        err = "terrain without base.MD2";
        return false;
    }
    Md2Model base;
    if (!ReadMd2(out[baseAt].data, base, err, true))
        return false;
    if ((base.flags & 2) == 0 || base.heightField.empty()) {
        err = "base.MD2 has no height field";
        return false;
    }
    if (base.nodes.empty() || base.nodes[0].matrix != IdentityMatrix()) {
        err = "base.MD2's root is not at the world's origin";
        return false;
    }
    Grid g;
    if (!ReadGrid(out[baseAt].data, g, err))
        return false;
    std::vector<uint16_t> quadFlags(static_cast<size_t>(g.w) * g.h);
    for (size_t q = 0; q < quadFlags.size(); ++q) quadFlags[q] = L16(out[baseAt].data, g.quads + 4 * q);
    std::vector<float> heights(static_cast<size_t>(g.w + 1) * (g.h + 1));
    for (size_t i = 0; i < heights.size(); ++i) heights[i] = LF(out[baseAt].data, g.heights + 4 * i);
    const size_t terrainTextures = base.textures.size();
    std::vector<bool> madeHidden(quadFlags.size());
    std::vector<uint16_t> quadTexture(quadFlags.size());
    for (size_t q = 0; q < quadTexture.size(); ++q) quadTexture[q] = L16(out[baseAt].data, g.quads + 4 * q + 2);

    // The terrain's texture folders as they grow, and the names their lists gain.
    Folders folders = TextureFolders(out);
    std::vector<std::string> listed[2];
    auto terrainTexture = [&](int k, const std::string& lower) -> const Bytes* {
        auto it = folders.files[k].find(lower);
        return it == folders.files[k].end() ? nullptr : &out[it->second].data;
    };
    // What the terrain's mesh gets for a name: its own folder's file, else the shared one (MeshLoad.cpp
    // OverrideTexturePaths).
    auto terrainSees = [&](int k, const std::string& lower) -> const Bytes* {
        const Bytes* own = terrainTexture(k, lower);
        return own != nullptr ? own : shared(std::string(kFolders[k]) + "/" + lower);
    };
    auto nameTaken = [&](const std::string& lower) {
        for (int k = 0; k < 2; ++k)
            if (terrainSees(k, lower) != nullptr)
                return true;
        return false;
    };
    auto addTexture = [&](int k, const std::string& name, const Bytes& bytes) {
        out.push_back({std::string(kFolders[k]) + "/" + name, bytes});
        folders.files[k].emplace(Lower(name), out.size() - 1);
        listed[k].push_back(name);
        report.addedTextures.push_back(std::string(kFolders[k]) + "/" + name);
    };

    for (const TpiObstacle& obstacle : obstacles) {
        TpiObstacleResult result;
        result.name = obstacle.name;
        auto skip = [&](const std::string& why) {
            result.skipped = why;
            report.obstacles.push_back(result);
        };
        const TpiWadMember* sam = nullptr;
        for (const TpiWadMember& m : obstacle.members) {
            const std::string n = Lower(m.name);
            if (Dir(n).empty() && n.size() > 4 && n.compare(n.size() - 4, 4, ".sam") == 0)
                sam = &m;
        }
        if (sam == nullptr) {
            skip("no .sam");
            continue;
        }
        const std::string text(sam->data.begin(), sam->data.end());
        int32_t isObstacle = 0, ox = 0, oy = 0;
        std::vector<std::string> rows;
        if (!TpiSamInteger(text, "Info", "IsObstacle", isObstacle) || isObstacle != 1) {
            skip("not an obstacle");
            continue;
        }
        if (!ShapeRows(text, "Info.Shape", rows) || !TpiSamInteger(text, "Obstacle", "ShapeOffsetX", ox) ||
            !TpiSamInteger(text, "Obstacle", "ShapeOffsetY", oy)) {
            skip("no shape");
            continue;
        }
        const std::string stem = Lower(sam->name.substr(0, sam->name.size() - 4));
        // The obstacle's mesh and its clearing animations, in order (<stem>m1.MD2, m2, ...; <stem>m.MD2 for one).
        auto files = [](const TpiObstacle& o, const std::string& s, const TpiWadMember*& mesh,
                         std::vector<const TpiWadMember*>& anims) {
            mesh = nullptr;
            anims.clear();
            std::map<int, const TpiWadMember*> numbered;
            for (const TpiWadMember& m : o.members) {
                if (!Dir(m.name).empty())
                    continue;
                const std::string n = Lower(m.name);
                if (n == s + ".md2") mesh = &m;
                if (n == s + "m.md2") numbered[0] = &m;
                if (n.size() > s.size() + 5 && n.compare(0, s.size() + 1, s + "m") == 0 &&
                    n.compare(n.size() - 4, 4, ".md2") == 0) {
                    const std::string digits = n.substr(s.size() + 1, n.size() - s.size() - 5);
                    if (!digits.empty() && digits.size() < 4 && digits.find_first_not_of("0123456789") == std::string::npos)
                        numbered[std::stoi(digits)] = &m;
                }
            }
            if (numbered.size() > 1)
                numbered.erase(0);   // <stem>m.MD2 is the one animation only of the obstacles without numbered ones
            for (const auto& [n, m] : numbered) anims.push_back(m);
        };
        const TpiWadMember* mesh = nullptr;
        std::vector<const TpiWadMember*> anims;
        files(obstacle, stem, mesh, anims);
        if (mesh == nullptr) {
            skip("no mesh");
            continue;
        }

        // What it looks like once cleared, which is how the converted world has it (Theme Park World's scenery is never
        // cleared). Theme Park Inc swaps a cleared obstacle that names one (Obstacle.ReplaceWithID: the lake's 1704)
        // for that object (the drained lake, dlake: Info.IsObstacle 0, Info.IsSupportingObstacle 1). Another shows
        // what its last clearing animation shows at its first frame; that is taken when the nodes the animations move
        // are at most a tenth of its faces, and those nodes are left out. One whose animations move more and whose
        // cleared shape leaves walkways (the bridges: their decks are raised and unfolded by them) is posed as each of
        // its animations in turn leaves it at its last frame (PoseAtEnd), shown as the last one shows it there. Else it
        // stays as it shows before it is cleared (its first animation's first frame).
        const TpiObstacle* source = &obstacle;
        const TpiWadMember* drawn = mesh;
        const TpiWadMember* visibility = anims.empty() ? nullptr : anims.front();
        std::vector<bool> leaveMoved;
        bool cleared = false, posedAtEnd = false;
        int32_t replaceId = 0;
        if (TpiSamInteger(text, "Obstacle", "ReplaceWithID", replaceId) && replaceId != 0) {
            for (const TpiObstacle& other : obstacles) {
                for (const TpiWadMember& m : other.members) {
                    const std::string n = Lower(m.name);
                    int32_t id = 0;
                    if (&other == &obstacle || !Dir(n).empty() || n.size() <= 4 || n.compare(n.size() - 4, 4, ".sam") != 0 ||
                        !TpiSamInteger(std::string(m.data.begin(), m.data.end()), "Info", "Id", id) || id != replaceId)
                        continue;
                    const TpiWadMember* otherMesh = nullptr;
                    std::vector<const TpiWadMember*> otherAnims;
                    files(other, n.substr(0, n.size() - 4), otherMesh, otherAnims);
                    if (otherMesh == nullptr)
                        continue;
                    source = &other;
                    drawn = otherMesh;
                    visibility = otherAnims.empty() ? nullptr : otherAnims.front();
                    result.state = "replaced by " + other.name;
                    cleared = true;
                }
                if (cleared)
                    break;
            }
        }
        if (!cleared && anims.size() >= 2) {
            Md2Model model;
            std::vector<bool> shown, moved;
            if (!ReadMd2(mesh->data, model, err) || !ShownAtStart(mesh->data, &anims.back()->data, shown, err)) {
                err = obstacle.name + "/" + mesh->name + ": " + err;
                return false;
            }
            for (const TpiWadMember* a : anims) MarkMoved(mesh->data, a->data, moved);
            moved.resize(model.nodes.size(), false);
            size_t faces = 0, movedFaces = 0;
            for (size_t n = 0; n < model.nodes.size(); ++n) {
                const Md2Node& node = model.nodes[n];
                if (node.mini || !shown[n] || IsCrew(node.name) || IsSiteProp(node.name) || IsMarker(node.name))
                    continue;
                faces += node.faces.size();
                if (moved[n])
                    movedFaces += node.faces.size();
            }
            std::vector<std::string> afterRows;
            bool walkways = false;
            if (ShapeRows(text, "Obstacle.AfterShape", afterRows))
                for (const std::string& row : afterRows) walkways = walkways || row.find_first_of(",@") != std::string::npos;
            if (faces != 0 && movedFaces * 10 <= faces) {
                visibility = anims.back();
                leaveMoved = moved;
                result.state = "cleared";
                cleared = true;
            } else if (walkways) {
                visibility = anims.back();
                posedAtEnd = true;
                result.state = "cleared, posed";
                cleared = true;
            }
        }
        if (!cleared)
            result.state = "before clearing";

        // The cells it closes: once cleared, its Obstacle.AfterShape ('X' and 'H' closed; '.' open; ',' and '@' the
        // paths it leaves, the bridges' and tunnels' walkways); before, its Info.Shape.
        std::vector<std::string> after;
        const bool useAfter = ShapeRows(text, "Obstacle.AfterShape", after) && cleared;
        if (useAfter)
            rows = after;

        Md2Model model;
        std::vector<bool> shown;
        if (!ReadMd2(drawn->data, model, err) ||
            !ShownAtStart(drawn->data, visibility != nullptr ? &visibility->data : nullptr, shown, err, posedAtEnd)) {
            err = source->name + "/" + drawn->name + ": " + err;
            return false;
        }
        if (posedAtEnd)
            for (const TpiWadMember* a : anims) PoseAtEnd(drawn->data, a->data, model);
        leaveMoved.resize(model.nodes.size(), false);

        // Each texture as Theme Park Inc gave it to the obstacle (its own folders first, then the shared ones), under a
        // name that gives the terrain the same picture: its own when the terrain sees no other picture by it, else a
        // new one.
        const Folders own = TextureFolders(source->members);
        std::map<std::string, std::string> placedName;   // lower-case name: the name the terrain has it by
        auto place = [&](const std::string& frame) {
            const std::string lower = Lower(frame);
            auto done = placedName.find(lower);
            if (done != placedName.end())
                return done->second;
            const Bytes* tpi[2];
            bool clash = false, any = false;
            for (int k = 0; k < 2; ++k) {
                auto it = own.files[k].find(lower);
                tpi[k] = it != own.files[k].end() ? &source->members[it->second].data
                                                  : shared(std::string(kFolders[k]) + "/" + lower);
                const Bytes* sees = terrainSees(k, lower);
                any = any || tpi[k] != nullptr;
                clash = clash || (tpi[k] != nullptr && sees != nullptr && *sees != *tpi[k]);
            }
            std::string name = frame;
            if (!any) {
                report.missingTextures.push_back(source->name + ": " + frame);
            } else if (clash) {
                const size_t dot = frame.find_last_of('.');
                const std::string s = dot == std::string::npos ? frame : frame.substr(0, dot);
                const std::string ext = dot == std::string::npos ? std::string() : frame.substr(dot);
                for (int n = 1;; ++n) {
                    const std::string suffix = "_" + std::to_string(n);
                    name = s.substr(0, std::min(s.size(), kMaxName - suffix.size() - ext.size())) + suffix + ext;
                    if (!nameTaken(Lower(name)))
                        break;
                }
                report.renamedTextures.push_back(source->name + ": " + frame + " -> " + name);
                for (int k = 0; k < 2; ++k)
                    if (tpi[k] != nullptr) addTexture(k, name, *tpi[k]);
            } else {
                for (int k = 0; k < 2; ++k)
                    if (tpi[k] != nullptr && terrainSees(k, lower) == nullptr) addTexture(k, frame, *tpi[k]);
            }
            placedName.emplace(lower, name);
            return name;
        };
        std::vector<int> slot(model.textures.size(), -1);
        for (size_t t = 0; t < model.textures.size(); ++t) {
            Md2Texture tex = model.textures[t];
            for (std::string& f : tex.frames) f = place(f);
            for (size_t b = 0; b < base.textures.size() && slot[t] < 0; ++b)
                if (SameTexture(base.textures[b], tex))
                    slot[t] = static_cast<int>(b);
            if (slot[t] < 0) {
                slot[t] = static_cast<int>(base.textures.size());
                base.textures.push_back(std::move(tex));
            }
        }

        // The shown nodes with geometry, each under the terrain's root at its place in the world.
        Md2Model placed;
        placed.nodes.push_back(Md2Node());
        Md2Model unrooted = model;
        unrooted.nodes[0].matrix = IdentityMatrix();
        const std::vector<Matrix> world = WorldMatrices(unrooted);
        for (size_t n = 0; n < model.nodes.size(); ++n) {
            const Md2Node& node = model.nodes[n];
            if (node.mini || !shown[n] || node.faces.empty())
                continue;
            if (IsCrew(node.name) || (cleared && (IsSiteProp(node.name) || leaveMoved[n])) ||
                (useAfter && IsMarker(node.name))) {
                result.leftOut.push_back(node.name);
                continue;
            }
            Md2Node copy = node;
            copy.parent = 0;
            copy.matrix = world[n];
            for (Md2FaceSet& fs : copy.faceSets)
                if (fs.texture >= 0) fs.texture = fs.texture < static_cast<int>(slot.size()) ? slot[fs.texture] : -1;
            result.faces += static_cast<int>(copy.faces.size());
            ++result.nodes;
            placed.nodes.push_back(std::move(copy));
        }

        // Its blocked cells become hidden quads where its geometry takes the ground's place.
        const std::vector<int> atGround = Coverage(placed, g, &heights);
        for (size_t j = 0; j < rows.size(); ++j)
            for (size_t i = 0; i < rows[j].size(); ++i) {
                if (rows[j][i] != 'H' && rows[j][i] != 'X')
                    continue;
                const int64_t x = ox + static_cast<int64_t>(i), y = oy + static_cast<int64_t>(j);
                if (x < 0 || y < 0 || x >= int64_t(g.w) || y >= int64_t(g.h))
                    continue;
                const size_t q = static_cast<size_t>(y) * g.w + static_cast<size_t>(x);
                if ((quadFlags[q] & kQuadHidden) != 0) {
                    ++result.alreadyHidden;
                } else if (atGround[q] >= kCoveredSamples) {
                    quadFlags[q] |= kQuadHidden;
                    madeHidden[q] = true;
                    ++result.blocked;
                } else {
                    ++result.open;
                }
            }

        for (size_t n = 1; n < placed.nodes.size(); ++n) base.nodes.push_back(std::move(placed.nodes[n]));
        report.obstacles.push_back(result);
    }

    // The quads the obstacles hid, and the shown ones beside them, lowered under the obstacles, each with its own texture
    // (the one the height field draws it with from afar; its turn, quad flags 0x78, is left out). Quads hidden before
    // (the river's, the transparent ones) are left out: their texture is not the ground's.
    NodeBuilder underGround;
    for (int64_t y = 0; y < int64_t(g.h); ++y)
        for (int64_t x = 0; x < int64_t(g.w); ++x) {
            const size_t q = static_cast<size_t>(y) * g.w + static_cast<size_t>(x);
            bool near = false;
            for (int64_t ny = std::max<int64_t>(y - 1, 0); ny <= std::min<int64_t>(y + 1, int64_t(g.h) - 1) && !near; ++ny)
                for (int64_t nx = std::max<int64_t>(x - 1, 0); nx <= std::min<int64_t>(x + 1, int64_t(g.w) - 1) && !near; ++nx)
                    near = madeHidden[static_cast<size_t>(ny) * g.w + static_cast<size_t>(nx)];
            if (!near || ((quadFlags[q] & kQuadHidden) != 0 && !madeHidden[q]))
                continue;
            const uint16_t texture = quadTexture[q];
            if (texture >= terrainTextures)
                continue;
            auto corner = [&](int64_t cx, int64_t cz) {
                return Vec3{g.ox + cx * g.cx, heights[size_t(cz) * (g.w + 1) + size_t(cx)] - kUnderGround,
                            g.oz + cz * g.cz};
            };
            underGround.Quad(corner(x, y), corner(x, y + 1), corner(x + 1, y + 1), corner(x + 1, y), 0, 0, 0, 1, 1, 1, 1,
                             0, texture);
        }
    if (!underGround.Empty()) {
        base.nodes.push_back(underGround.Build("tpi underground"));
        base.nodes.back().parent = 0;
    }

    // Nothing merged (no obstacles, or none that shows or blocks anything): the terrain stays as it is.
    bool changed = !report.addedTextures.empty();
    for (const TpiObstacleResult& r : report.obstacles)
        changed = changed || r.nodes != 0 || r.blocked != 0;
    if (!changed) {
        report.stillClear = StillClear(base, g, quadFlags);
        return true;
    }

    // The merged mesh must fit the format's 16-bit counts (the point count excepted: tpi_island_md2.cpp WriteMd2).
    uint64_t vsets = 0, usets = 0, faceSets = 0, faces = 0;
    for (const Md2Node& n : base.nodes) {
        vsets += (n.vertices.size() + 3) / 4;
        usets += (n.u.size() + 3) / 4;
        faceSets += n.faceSets.size();
        faces += n.faces.size();
    }
    if (std::max({vsets, usets, faceSets, faces, uint64_t(base.nodes.size()), uint64_t(base.textures.size())}) > 0xffff) {
        err = "the terrain with its obstacles has more than 65535 of a kind of mesh record";
        return false;
    }
    std::array<float, 6> box{};
    if (GeometryBox(base, box))
        for (int k = 0; k < 3; ++k) {
            base.box[k] = std::min(base.box[k], box[k]);
            base.box[k + 3] = std::max(base.box[k + 3], box[k + 3]);
        }
    Bytes merged = WriteMd2(base);
    Grid mg;
    if (!ReadGrid(merged, mg, err))
        return false;
    for (size_t q = 0; q < quadFlags.size(); ++q) {
        Put16(merged, mg.quads + 4 * q, quadFlags[q]);
        Put16(merged, mg.quads + 4 * q + 2, quadTexture[q]);
    }

    report.stillClear = StillClear(base, g, quadFlags);

    // The texture lists gain the added files, as the loader looks a texture up there before it opens the folder.
    for (int k = 0; k < 2; ++k) {
        if (listed[k].empty())
            continue;
        size_t at = out.size();
        for (size_t i = 0; i < out.size(); ++i)
            if (Lower(Dir(out[i].name)) == kFolders[k] && Lower(Base(out[i].name)) == "qickload.txt")
                at = i;
        if (at == out.size())
            continue;
        Bytes& list = out[at].data;
        if (!list.empty() && list.back() != '\n')
            list.push_back('\n');
        for (const std::string& name : listed[k]) {
            list.insert(list.end(), name.begin(), name.end());
            list.push_back('\n');
        }
    }
    out[baseAt].data = std::move(merged);
    terrain = std::move(out);
    return true;
}

}  // namespace tpw::assets
