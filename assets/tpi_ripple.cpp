// tpi_ripple.cpp: the wave band round a Theme Park Inc island and the texture animation files (tpi_ripple.h).
#include "tpi_ripple.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tpw::assets::tpi_island {

namespace {

constexpr uint32_t kMagic = 0x1cd15d46;
constexpr uint32_t kVersion = 0xdd;
constexpr size_t kHeader = 0xb8, kAnimHeader = 0x48, kNodeAnim = 0x40, kUvAnim = 0x14;
constexpr uint32_t kUvFlag = 0x10000;

uint16_t L16(const std::vector<uint8_t>& d, size_t at) { return static_cast<uint16_t>(d[at] | (d[at + 1] << 8)); }
uint32_t L32(const std::vector<uint8_t>& d, size_t at) {
    return static_cast<uint32_t>(d[at]) | (static_cast<uint32_t>(d[at + 1]) << 8) | (static_cast<uint32_t>(d[at + 2]) << 16) |
           (static_cast<uint32_t>(d[at + 3]) << 24);
}
float LF(const std::vector<uint8_t>& d, size_t at) {
    const uint32_t bits = L32(d, at);
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
bool In(const std::vector<uint8_t>& d, uint64_t at, uint64_t n) { return at + n <= d.size() && at + n >= at; }
void Put32(std::vector<uint8_t>& o, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) o[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}
void Add16(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(static_cast<uint8_t>(v));
    o.push_back(static_cast<uint8_t>(v >> 8));
}
void Add32(std::vector<uint8_t>& o, uint32_t v) {
    for (int i = 0; i < 4; ++i) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void AddF(std::vector<uint8_t>& o, float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    Add32(o, bits);
}
void Align4(std::vector<uint8_t>& o) {
    while (o.size() % 4) o.push_back(0);
}

std::string Lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

struct Xz {
    float x, z;
};

}  // namespace

std::vector<RippleQuad> RippleRing(const std::vector<Vec3>& outline, float width, float quadLength) {
    std::vector<Vec3> loop;
    for (const Vec3& p : outline)
        if (loop.empty() || std::fabs(p.x - loop.back().x) + std::fabs(p.z - loop.back().z) > 1e-6f) loop.push_back(p);
    while (loop.size() > 1 && std::fabs(loop.front().x - loop.back().x) + std::fabs(loop.front().z - loop.back().z) <= 1e-6f)
        loop.pop_back();
    const size_t n = loop.size();
    if (n < 3 || !(width > 0) || !(quadLength > 0)) return {};
    double area2 = 0;
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = loop[i];
        const Vec3& b = loop[(i + 1) % n];
        area2 += static_cast<double>(a.x) * b.z - static_cast<double>(b.x) * a.z;
    }
    if (std::fabs(area2) < 1e-9) return {};
    // Side i runs from point i to point i + 1; its outward normal in x, z (for a loop counter-clockwise in x, z, that is
    // positive area, outward is (dz, -dx)).
    const float sign = area2 > 0 ? 1.0f : -1.0f;
    std::vector<Xz> normal(n);
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = loop[i];
        const Vec3& b = loop[(i + 1) % n];
        const float dx = b.x - a.x, dz = b.z - a.z, len = std::sqrt(dx * dx + dz * dz);
        normal[i] = {sign * dz / len, -sign * dx / len};
    }
    // Each point moves out along the mitre of its two sides, far enough that the outer edge lies `width` from both (at
    // most twice `width`, for a sharp corner).
    std::vector<Xz> offset(n);
    for (size_t i = 0; i < n; ++i) {
        const Xz& p = normal[(i + n - 1) % n];
        const Xz& q = normal[i];
        Xz m{p.x + q.x, p.z + q.z};
        const float len = std::sqrt(m.x * m.x + m.z * m.z);
        if (len < 1e-4f) {
            offset[i] = {q.x * width, q.z * width};
            continue;
        }
        m.x /= len;
        m.z /= len;
        const float scale = std::min(2.0f, 1.0f / std::max(0.5f, m.x * q.x + m.z * q.z));
        offset[i] = {m.x * scale * width, m.z * scale * width};
    }
    std::vector<RippleQuad> quads;
    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + 1) % n;
        const Vec3& a = loop[i];
        const Vec3& b = loop[j];
        const float len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.z - a.z) * (b.z - a.z));
        const int pieces = std::max(1, static_cast<int>(std::lround(len / quadLength)));
        auto inner = [&](float t) { return Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; };
        auto outer = [&](float t) {
            const Vec3 p = inner(t);
            return Vec3{p.x + offset[i].x + (offset[j].x - offset[i].x) * t, p.y, p.z + offset[i].z + (offset[j].z - offset[i].z) * t};
        };
        for (int k = 0; k < pieces; ++k) {
            const float t0 = static_cast<float>(k) / static_cast<float>(pieces);
            const float t1 = static_cast<float>(k + 1) / static_cast<float>(pieces);
            RippleQuad q;
            q.p[0] = inner(t0);
            q.p[1] = inner(t1);
            q.p[2] = outer(t1);
            q.p[3] = outer(t0);
            const float u[4] = {0, 1, 1, 0}, v[4] = {0, 0, 1, 1};
            std::copy(u, u + 4, q.u);
            std::copy(v, v + 4, q.v);
            quads.push_back(q);
        }
    }
    return quads;
}

Md2Node RippleNode(const std::vector<RippleQuad>& quads, uint16_t texture, const std::string& name) {
    NodeBuilder b;
    for (const RippleQuad& q : quads) {
        const Vec3& p0 = q.p[0];
        const Vec3& p1 = q.p[1];
        const Vec3& p2 = q.p[2];
        // The engine's front face: cross(b - a, c - a) towards the viewer, here up.
        const float up = (p1.z - p0.z) * (p2.x - p0.x) - (p1.x - p0.x) * (p2.z - p0.z);
        if (up >= 0) b.Quad(q.p[0], q.p[1], q.p[2], q.p[3], q.u[0], q.v[0], q.u[1], q.v[1], q.u[2], q.v[2], q.u[3], q.v[3], texture);
        else b.Quad(q.p[0], q.p[3], q.p[2], q.p[1], q.u[0], q.v[0], q.u[3], q.v[3], q.u[2], q.v[2], q.u[1], q.v[1], texture);
    }
    return b.Build(name);
}

int FileNodeIndex(const Md2Model& model, size_t node) {
    if (node >= model.nodes.size()) return -1;
    size_t full = 0;
    for (const Md2Node& n : model.nodes)
        if (!n.mini) ++full;
    size_t index = model.nodes[node].mini ? full : 0;
    for (size_t i = 0; i < node; ++i)
        if (model.nodes[i].mini == model.nodes[node].mini) ++index;
    return static_cast<int>(index);
}

bool ReadUvAnim(const std::vector<uint8_t>& d, UvAnim& anim, std::string& err) {
    anim = UvAnim{};
    if (d.size() < kHeader || L32(d, 0) != kMagic || L32(d, 4) != kVersion) {
        err = "not a version 0xdd mesh file";
        return false;
    }
    const uint32_t a = L32(d, 0x98);
    if (a == 0 || !In(d, a, kAnimHeader)) {
        err = "no animation header";
        return false;
    }
    anim.f00 = L32(d, a);
    anim.f04 = L32(d, a + 4);
    anim.frames = L32(d, a + 8);
    for (size_t c = 0; c < 9; ++c)
        if (c != 3 && L16(d, a + 0x0c + 2 * c) != 0) anim.other = true;
    const uint16_t count = L16(d, a + 0x12);
    const uint32_t table = L32(d, a + 0x2c);
    if (count != 0 && !In(d, table, static_cast<uint64_t>(count) * kNodeAnim)) {
        err = "node animations out of the file";
        return false;
    }
    for (uint16_t i = 0; i < count; ++i) {
        const size_t at = table + static_cast<size_t>(i) * kNodeAnim;
        const uint32_t uv = L32(d, at + 0x2c);
        bool others = false;
        for (size_t p = 0x18; p < 0x2c; p += 4) others = others || L32(d, at + p) != 0;
        if (others) anim.other = true;
        if ((L32(d, at + 4) & kUvFlag) == 0 || uv == 0) {
            anim.other = true;
            continue;
        }
        if (!In(d, uv, kUvAnim)) {
            err = "texture animation out of the file";
            return false;
        }
        const uint32_t tracks = L32(d, uv), trackTable = L32(d, uv + 4), keys = L32(d, uv + 8), frames = L32(d, uv + 0xc),
                       floats = L32(d, uv + 0x10);
        if (!In(d, trackTable, static_cast<uint64_t>(tracks) * 4) || !In(d, frames, static_cast<uint64_t>(keys) * 2) ||
            !In(d, floats, static_cast<uint64_t>(keys) * 8)) {
            err = "texture animation tracks out of the file";
            return false;
        }
        UvNodeAnim node;
        std::copy(d.begin() + static_cast<std::ptrdiff_t>(at), d.begin() + static_cast<std::ptrdiff_t>(at + kNodeAnim), node.record.begin());
        node.tracks.resize(tracks);
        for (uint32_t t = 0; t < tracks; ++t) {
            const uint32_t first = L16(d, trackTable + 4 * t), n = L16(d, trackTable + 4 * t + 2);
            if (first + n > keys) {
                err = "a texture track past its keys";
                return false;
            }
            for (uint32_t k = first; k < first + n; ++k)
                node.tracks[t].push_back(UvKey{L16(d, frames + 2 * k), LF(d, floats + 8 * k), LF(d, floats + 8 * k + 4)});
        }
        anim.nodes.push_back(std::move(node));
    }
    return true;
}

std::vector<uint8_t> WriteUvAnim(const std::vector<uint8_t>& mesh, const std::string& name, const UvAnim& anim) {
    std::vector<uint8_t> o(mesh.begin(), mesh.begin() + static_cast<std::ptrdiff_t>(std::min(mesh.size(), kHeader)));
    o.resize(kHeader, 0);
    std::fill(o.begin() + 0x18, o.begin() + 0x2c, 0);
    std::memcpy(o.data() + 0x18, name.data(), std::min<size_t>(name.size(), 0x13));
    Put32(o, 0x30, (L32(o, 0x30) & ~1u) | 0x20);   // no geometry of its own, an animation (Basem1 0x22, the isles' 0x20)
    std::fill(o.begin() + 0x4c, o.begin() + 0x80, 0);
    std::fill(o.begin() + 0x98, o.begin() + kHeader, 0);
    // Every node animation's tracks, then their sUVAnim records, then the node animations, then the header, as the
    // originals lay them out.
    struct Tables {
        uint32_t trackTable, keys, frames, floats;
    };
    std::vector<Tables> at;
    for (const UvNodeAnim& node : anim.nodes) {
        const uint32_t trackTable = static_cast<uint32_t>(o.size());
        uint32_t keys = 0;
        for (const auto& track : node.tracks) {
            Add16(o, keys);
            Add16(o, static_cast<uint32_t>(track.size()));
            keys += static_cast<uint32_t>(track.size());
        }
        const uint32_t floats = static_cast<uint32_t>(o.size());
        for (const auto& track : node.tracks)
            for (const UvKey& k : track) {
                AddF(o, k.u);
                AddF(o, k.v);
            }
        const uint32_t frames = static_cast<uint32_t>(o.size());
        for (const auto& track : node.tracks)
            for (const UvKey& k : track) Add16(o, k.frame);
        Align4(o);
        at.push_back({trackTable, keys, frames, floats});
    }
    std::vector<uint32_t> uvAt;
    for (size_t i = 0; i < anim.nodes.size(); ++i) {
        uvAt.push_back(static_cast<uint32_t>(o.size()));
        Add32(o, static_cast<uint32_t>(anim.nodes[i].tracks.size()));
        Add32(o, at[i].trackTable);
        Add32(o, at[i].keys);
        Add32(o, at[i].frames);
        Add32(o, at[i].floats);
    }
    const uint32_t table = static_cast<uint32_t>(o.size());
    for (size_t i = 0; i < anim.nodes.size(); ++i) {
        const size_t at = o.size();
        o.insert(o.end(), anim.nodes[i].record.begin(), anim.nodes[i].record.end());
        Put32(o, at, static_cast<uint32_t>(i));
        std::fill(o.begin() + static_cast<std::ptrdiff_t>(at + 0x18), o.begin() + static_cast<std::ptrdiff_t>(at + 0x30), 0);
        Put32(o, at + 4, L32(o, at + 4) | kUvFlag);
        Put32(o, at + 0x2c, uvAt[i]);
    }
    const uint32_t header = static_cast<uint32_t>(o.size());
    Add32(o, anim.f00);
    Add32(o, anim.f04);
    Add32(o, anim.frames);
    for (size_t c = 0; c < 10; ++c) Add16(o, c == 3 ? static_cast<uint32_t>(anim.nodes.size()) : 0u);   // nine counts, a pad
    for (size_t p = 0; p < 10; ++p) Add32(o, p == 3 && !anim.nodes.empty() ? table : 0u);   // the tables, two spare words
    Put32(o, 0x98, header);
    Put32(o, 0x2c, static_cast<uint32_t>(o.size()));
    return o;
}

bool MeasureRipple(const std::vector<uint8_t>& isle, const std::vector<uint8_t>& m1, const std::vector<uint8_t>& m2,
                   RippleStyle& style, std::string& err) {
    style = RippleStyle{};
    Md2Model model;
    if (!ReadMd2(isle, model, err)) return false;
    if (model.nodes.empty() || model.nodes[0].mini) {
        err = "the island has no root geometry";
        return false;
    }
    const Md2Node& root = model.nodes[0];
    const Md2FaceSet* band = nullptr;
    for (const Md2FaceSet& set : root.faceSets) {
        if (set.texture < 0 || static_cast<size_t>(set.texture) >= model.textures.size()) continue;
        const Md2Texture& t = model.textures[static_cast<size_t>(set.texture)];
        if (t.material != 0x33 || t.frames.empty() || Lower(t.frames[0]).rfind("ripple", 0) != 0) continue;
        if (!band || set.faceCount > band->faceCount) band = &set;
    }
    if (!band) {
        err = "the island has no wave band";
        return false;
    }
    style.texture = model.textures[static_cast<size_t>(band->texture)].frames[0];
    double width = 0, length = 0;
    int widths = 0, lengths = 0;
    // Each quad has one triangle with its inner edge (two corners at v 0, u 0 and 1) and an outer corner (v 1): the
    // inner edge's length is the quad's, the outer corner's distance from the inner edge's line the band's width (so a
    // mitred corner, longer across, counts the same as the rest).
    for (size_t f = band->firstFace; f < static_cast<size_t>(band->firstFace) + band->faceCount && f < root.faces.size(); ++f) {
        const Md2Face& face = root.faces[f];
        bool inRange = true;
        for (uint16_t c : face.corner) inRange = inRange && c < root.u.size() && root.cornerVertex[c] < root.vertices.size();
        if (!inRange) continue;
        for (int i = 0; i < 3; ++i) {
            const uint16_t ci = face.corner[i], cj = face.corner[(i + 1) % 3], ck = face.corner[(i + 2) % 3];
            if (std::fabs(root.v[ci]) > 1e-3f || std::fabs(root.v[cj]) > 1e-3f || std::fabs(root.v[ck] - 1) > 1e-3f ||
                std::fabs(std::fabs(root.u[ci] - root.u[cj]) - 1) > 1e-3f)
                continue;
            const Vec3& pi = root.vertices[root.cornerVertex[ci]];
            const Vec3& pj = root.vertices[root.cornerVertex[cj]];
            const Vec3& pk = root.vertices[root.cornerVertex[ck]];
            const double ex = pj.x - pi.x, ez = pj.z - pi.z, d = std::hypot(ex, ez);
            if (d < 1e-6) continue;
            length += d;
            ++lengths;
            width += std::fabs(ex * (pk.z - pi.z) - ez * (pk.x - pi.x)) / d;
            ++widths;
        }
    }
    if (widths == 0 || lengths == 0) {
        err = "the wave band's quads are not mapped 0 to 1";
        return false;
    }
    style.width = static_cast<float>(width / widths);
    style.quadLength = static_cast<float>(length / lengths);
    const int rootIndex = FileNodeIndex(model, 0);
    const std::vector<uint8_t>* files[2] = {&m1, &m2};
    for (int k = 0; k < 2; ++k) {
        UvAnim anim;
        if (!ReadUvAnim(*files[k], anim, err)) {
            err = "animation M" + std::to_string(k + 1) + ": " + err;
            return false;
        }
        const UvNodeAnim* node = nullptr;
        for (const UvNodeAnim& n : anim.nodes)
            if (n.Node() == rootIndex) node = &n;
        if (!node || node->tracks.size() <= band->lastCorner) {
            err = "animation M" + std::to_string(k + 1) + " does not move the wave band";
            return false;
        }
        bool first = true;
        for (size_t c = band->firstCorner; c <= band->lastCorner; ++c) {
            const auto& track = node->tracks[c];
            if (track.size() < 2) {
                err = "a wave band corner that does not move";
                return false;
            }
            const float du = track.back().u - track.front().u, dv = track.back().v - track.front().v;
            if (first) {
                style.drift[k][0] = du;
                style.drift[k][1] = dv;
                style.frames[k] = track.back().frame;
                first = false;
            } else if (std::fabs(du - style.drift[k][0]) > 1e-3f || std::fabs(dv - style.drift[k][1]) > 1e-3f ||
                       track.back().frame != style.frames[k]) {
                err = "the wave band's corners do not all drift alike";
                return false;
            }
        }
        style.anim[k].f00 = anim.f00;
        style.anim[k].f04 = anim.f04;
        style.anim[k].frames = anim.frames;
        style.record[k] = node->record;
    }
    return true;
}

}  // namespace tpw::assets::tpi_island
