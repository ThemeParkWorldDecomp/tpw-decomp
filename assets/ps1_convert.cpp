// ps1_convert.cpp: PS1 meshes, atlases, skeletons and poses translated to the PC's ride formats.
// All input is static data. The player's PC wads supply only file headers, script tails and signs.
#include "ps1_convert.h"
#include "ps1_disc.h"
#include "source.h"
#include "../platform/path_text.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace tpw::assets {
namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;
using Vec = std::array<double, 3>;
using Quat = std::array<double, 4>;
using Matrix = std::array<double, 16>;
using Transform = std::pair<Quat, Vec>;
// A texture: its VRAM page (low five bits) and its palette; a cell of the ride's own ground has a texture of its own,
// keyed -1 - its cell number (GroundTiles).
using Key = std::pair<int, int>;
// The ride's own ground: a cell's texture key to its theme atlas tile (page, palette, corner u and v, width, height).
using GroundTiles = std::map<Key, std::array<int, 6>>;
// The size of a ground cell's texture: at least the largest procedural ground tile (Ground blending 64), so the engine
// can lay it over the park's ground under the cell.
constexpr int kGroundTexture = 64;
// The mesh header flag (+0x30) the port reads as "the base is a PlayStation ride's ground: blend every cell of it into
// the park's ground" (TPW_MOD_HOOK ps1_ground, src/game/park/SkirtDeform.cpp). No PC mesh has it.
constexpr uint32_t kPs1GroundHeader = 0x40000000;
constexpr double S = 10.0 / 256.0;
const Matrix ident = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
// The output formats store little-endian words and IEEE single precision, regardless of the host's byte order.
void need(bool ok, const char *what) {
    if (!ok)
        throw std::runtime_error(what);
}
uint32_t u32(const Bytes &b, size_t p) {
    need(p <= b.size() && b.size() - p >= 4, "short PS1 record");
    return b[p] | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24;
}
uint16_t u16(const Bytes &b, size_t p) {
    need(p <= b.size() && b.size() - p >= 2, "short PS1 record");
    return b[p] | uint16_t(b[p + 1]) << 8;
}
int16_t i16(const Bytes &b, size_t p) {
    return static_cast<int16_t>(u16(b, p));
}
void put(Bytes &b, size_t p, uint32_t v, size_t n = 4) {
    need(p <= b.size() && n <= b.size() - p, "short output record");
    for (size_t k = 0; k < n; k++)
        b[p + k] = uint8_t(v >> (8 * k));
}
void word(Bytes &b, uint32_t v, size_t n = 4) {
    size_t p = b.size();
    b.resize(p + n);
    put(b, p, v, n);
}
void flt(Bytes &b, double v) {
    float f = static_cast<float>(v);
    uint32_t w;
    std::memcpy(&w, &f, 4);
    word(b, w);
}
template <size_t N> void floats(Bytes &b, const std::array<double, N> &v) {
    for (double x : v)
        flt(b, x);
}
template <size_t N> void putfloats(Bytes &b, size_t p, const std::array<double, N> &v) {
    Bytes t;
    floats(t, v);
    std::copy(t.begin(), t.end(), b.begin() + p);
}
void align(Bytes &b) {
    while (b.size() % 4)
        b.push_back(0);
}
void append(Bytes &a, const Bytes &b) {
    a.insert(a.end(), b.begin(), b.end());
}
Vec sub(Vec a, Vec b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vec add(Vec a, Vec b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
Vec cross(Vec a, Vec b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
template <size_t N> double dot(std::array<double, N> a, std::array<double, N> b) {
    double n = 0;
    for (size_t i = 0; i < N; i++)
        n += a[i] * b[i];
    return n;
}
Vec norm(Vec a) {
    double n = std::sqrt(dot(a, a));
    return n > 1e-9 ? Vec{a[0] / n, a[1] / n, a[2] / n} : Vec{0, 1, 0};
}
Quat qnorm(Quat a) {
    double n = std::sqrt(dot(a, a));
    need(n > 0, "zero PS1 quaternion");
    for (double &c : a)
        c /= n;
    return a;
}
Quat conj(Quat q) {
    return {-q[0], -q[1], -q[2], q[3]};
}
// The PC's ground runs the other way round from the PlayStation's: a half turn about the up axis (x and z both
// reversed), never a mirror, which would show every ride the wrong way round.
Quat halfTurn(Quat q) {
    return {-q[0], q[1], -q[2], q[3]};
}
Vec halfTurnv(Vec v) {
    return {-v[0] * S, v[1] * S, -v[2] * S};
}
Quat mul(Quat a, Quat b) {
    auto [x, y, z, w] = a;
    auto [X, Y, Z, W] = b;
    return {w * X + x * W + y * Z - z * Y, w * Y - x * Z + y * W + z * X, w * Z + x * Y - y * X + z * W,
            w * W - x * X - y * Y - z * Z};
}
Vec rot(Quat q, Vec v) {
    auto [x, y, z, w] = q;
    double tx = 2 * (y * v[2] - z * v[1]), ty = 2 * (z * v[0] - x * v[2]), tz = 2 * (x * v[1] - y * v[0]);
    return {v[0] + w * tx + y * tz - z * ty, v[1] + w * ty + z * tx - x * tz, v[2] + w * tz + x * ty - y * tx};
}
Quat slerp(Quat a, Quat b, double t) {
    double d = dot(a, b);
    if (d < 0) {
        for (double &c : b)
            c = -c;
        d = -d;
    }
    Quat q;
    if (d > 0.9995) {
        for (int i = 0; i < 4; i++)
            q[i] = a[i] + (b[i] - a[i]) * t;
        return qnorm(q);
    }
    double th = std::acos(d), s = std::sin(th);
    for (int i = 0; i < 4; i++)
        q[i] = (a[i] * std::sin((1 - t) * th) + b[i] * std::sin(t * th)) / s;
    return q;
}
Vec lerp(Vec a, Vec b, double t) {
    for (int i = 0; i < 3; i++)
        a[i] += (b[i] - a[i]) * t;
    return a;
}
Matrix matrix(Quat q, Vec p) {
    auto [x, y, z, w] = q;
    return {1 - 2 * (y * y + z * z),
            2 * (x * y + w * z),
            2 * (x * z - w * y),
            0,
            2 * (x * y - w * z),
            1 - 2 * (x * x + z * z),
            2 * (y * z + w * x),
            0,
            2 * (x * z + w * y),
            2 * (y * z - w * x),
            1 - 2 * (x * x + y * y),
            0,
            p[0],
            p[1],
            p[2],
            1};
}
Transform relative(Transform a, Transform b) {
    auto c = conj(a.first);
    return {qnorm(mul(c, b.first)), rot(c, sub(b.second, a.second))};
}
bool same(Transform a, Transform b) {
    auto p = sub(a.second, b.second);
    return std::abs(dot(a.first, b.first)) > 1 - 1e-4 &&
           std::max({std::abs(p[0]), std::abs(p[1]), std::abs(p[2])}) < 0.5;
}
// Python's round uses ties to even; preserve it for mesh corner grouping and packed morph vertices.
double rounded(double x, int places = 0) {
    double s = std::pow(10., places);
    return std::nearbyint(x * s) / s;
}
struct Spec {
    const char *key;
    const char *lookup;
    int reference, faces;
    const char *theme;
    int number, id;
    const char *title;
    const char *a;
    const char *b;
    // A ride without PS1 seat nodes carries its riders on this node (see riderRing); -1 when it has seats.
    int carrier = -1;
    // Where a rider on the carrier faces, in its frame, for the camera a ride without one gets (see riderView); zero
    // for the way the carrier travels.
    Vec ahead{};
};
// The Halloween IDs are free alongside the base game and Gold's coaster types. The reference converter's 2130 to
// 2133 overlap the Devil coaster and its pylon/car types; its geometry and scripts are otherwise preserved byte for byte.
const Spec specs[] = {{"jack", "HALLOW/JACK", 140, 672, "hallow", 201, 2134, "Crazy Clown", "Crazy", "Clown"},
                      {"skulls", "HALLOW/JUMPER", 141, 260, "hallow", 202, 2135, "Jumping Skulls", "Jumping", "Skulls"},
                      {"eyesld", "HALLOW/SLIDE", 147, 275, "hallow", 204, 2136, "Eye Slide", "Eye", "Slide", 3},
                      {"aztec", "JUNGLE/BUNGIE", 218, 352, "jungle", 205, 1187, "Aztec Bounce", "Aztec", "Bounce"},
                      {"diplo", "JUNGLE/DIPLODIP", 221, 286, "jungle", 206, 1130, "Diplo-Dip", "Diplo", "Dip"},
                      {"incapot", "JUNGLE/INCAPOT", 224, 268, "jungle", 207, 1189, "Inca Pot", "Inca", "Pot"},
                      // Romper Stomper faces its body's +Z: its reverse-jointed legs' knees point to -Z, and the +Z end
                      // lifts as it leaps.
                      {"romper", "SPACE/JUMPER", 369, 276, "space", 208, 3130, "Romper Stomper", "Romper", "Stomper", 2,
                       {0, 0, 1}},
                      {"obver", "HALLOW/OBVER", 142, 202, "hallow", 203, 2450, "Tower", "Tower", ""}};
// reference is only the entry number printed in the reference converter's output comments. Disc reads always use
// the recognised disc's ID table and located entries, including on the European and Japanese releases.
struct Tri {
    std::array<int, 3> vi;
    std::array<std::array<int, 2>, 3> uv;
    Key key;
    int mode = 0;
    std::array<Vec, 3> p;
    Vec normal;
    std::array<int, 3> pi;
};
struct Bone {
    int count, start, parent;
    Quat q;
    Vec p;
};
struct Pose {
    int frame;
    Vec p;
    Quat q;
};
struct MeshEntry {
    const Spec &spec;
    const Bytes &raw;
    int frames, last, w, h, d, camera;
    std::vector<Vec> verts;
    std::vector<Tri> faces;
    std::vector<Bone> bones;
    std::map<int, std::pair<int, Vec>> skin;
    std::map<int, std::vector<Pose>> tracks;
    std::vector<int> seats;
    std::array<std::array<uint32_t, 13>, 3> upgrades;
    uint32_t excitement;
    // Walk faces, 40-byte nodes, 12-byte skins and typed poses up to the trailer. No ride-specific byte offsets.
    MeshEntry(const Spec &sp, const Bytes &b) : spec(sp), raw(b) {
        need(u32(b, 0) == 0x96 && u32(b, 4) == 1, "unsupported PS1 mesh");
        need(u32(b, 28) <= 1024, "oversized PS1 mesh header");
        size_t off = u32(b, 0x20 + size_t(4) * u32(b, 28)), trailer = u32(b, 20);
        need(off + 56 <= b.size() && trailer + 188 <= b.size(), "damaged PS1 mesh");
        need(u32(b, off) <= 4096, "oversized PS1 frame count");
        frames = u32(b, off);
        int nn = u32(b, off + 4), nv = u32(b, off + 8), ns = u32(b, off + 12), np = u32(b, off + 28),
            nseat = int32_t(u32(b, off + 40));
        camera = int32_t(u32(b, off + 44));
        need(nn >= 0 && nn <= 128 && nv > 0 && nv <= 8192 && ns >= 0 && ns <= nv && np >= 0 && np <= 128 &&
                 nseat <= 128 && (camera == -1 || (camera >= 0 && camera < nn)),
             "oversized PS1 mesh");
        size_t p = off + 72;
        for (int i = 0; i < nv; i++)
            verts.push_back({double(i16(b, p + 8 * i)), double(i16(b, p + 8 * i + 2)), double(i16(b, p + 8 * i + 4))});
        size_t q = p + 12 * nv, prev = q, after = q, prevend = 0;
        int mode = 0;
        for (size_t at = q; at + 4 <= trailer;) {
            int n = u16(b, at), st = u16(b, at + 2);
            bool ok = n > 0 && n < 5000 && st < 0x4000 && size_t(n) * 14 <= trailer - at - 4 &&
                      (at == prevend || (at >= 4 && u32(b, at - 4) == 0));
            if (ok)
                for (int k = 0; k < n; k++) {
                    int a = u16(b, at + 4 + 14 * k), c = u16(b, at + 6 + 14 * k), d_ = u16(b, at + 8 + 14 * k);
                    if (a >= nv || c >= nv || d_ >= nv || a == c || c == d_ || a == d_) {
                        ok = false;
                        break;
                    }
                }
            if (!ok) {
                at += 2;
                continue;
            }
            if (at - prev >= 8)
                mode = u16(b, at - 6);
            for (int k = 0; k < n; k++) {
                size_t r = at + 4 + 14 * k;
                Tri t;
                t.vi = {u16(b, r), u16(b, r + 2), u16(b, r + 4)};
                for (int j = 0; j < 3; j++)
                    t.uv[j] = {b.at(r + 6 + 2 * j), b.at(r + 7 + 2 * j)};
                t.key = {st & 31, u16(b, r + 12)};
                t.mode = mode;
                faces.push_back(t);
            }
            at += 4 + 14 * n;
            prev = prevend = after = at;
        }
        need(faces.size() == size_t(sp.faces), "PS1 face count changed");
        size_t o = (after + 3) & ~size_t(3);
        for (int i = 0; i < nn; i++) {
            Quat q_;
            bool any = false;
            for (int j = 0; j < 4; j++) {
                q_[j] = i16(b, o + 8 + 2 * j) / 4096.;
                any |= q_[j] != 0;
            }
            bones.push_back({i16(b, o),
                             i16(b, o + 2),
                             i16(b, o + 6),
                             any ? qnorm(q_) : Quat{0, 0, 0, 1},
                             {double(i16(b, o + 24)), double(i16(b, o + 26)), double(i16(b, o + 28))}});
            need(bones.back().parent < i, "cyclic PS1 skeleton");
            o += 40;
        }
        for (int i = 0; i < ns; i++) {
            int v = u16(b, o), owner = -1;
            for (int j = 0; j < nn; j++)
                if (bones[j].start <= i && i < bones[j].start + bones[j].count) {
                    need(owner < 0, "overlapping PS1 skin");
                    owner = j;
                }
            need(owner >= 0 && v < nv && !skin.count(v), "damaged PS1 skin");
            skin[v] = {owner, {double(i16(b, o + 4)), double(i16(b, o + 6)), double(i16(b, o + 8))}};
            o += 12;
        }
        for (int i = 0; i < np; i++) {
            int kind = u16(b, o), node = u16(b, o + 4), count = u16(b, o + 6);
            need(node < nn, "invalid PS1 pose node");
            if (kind == 8) {
                o += 40;
                continue;
            }
            need(kind == 262 || kind == 263, "unsupported PS1 pose");
            o += 8;
            std::vector<Pose> keys;
            for (int k = 0; k <= count; k++) {
                int f = kind == 262 ? u16(b, o) : k;
                // Inca Pot repeats an endpoint. The first matching interval is used below, so a zero-width interval
                // is never interpolated; only a decreasing key is invalid.
                need(keys.empty() || f >= keys.back().frame, "PS1 animation keys are not in order");
                size_t at = o + (kind == 262 ? 4 : 0);
                Vec p_ = {double(i16(b, at)), double(i16(b, at + 2)), double(i16(b, at + 4))};
                Quat q_;
                for (int j = 0; j < 4; j++)
                    q_[j] = i16(b, at + 8 + 2 * j) / 4096.;
                keys.push_back({f, p_, qnorm(q_)});
                o += kind == 262 ? 20 : 16;
            }
            tracks[node] = keys;
        }
        for (int i = 0; i < std::max(0, nseat); i++) {
            int node = int32_t(u32(b, o));
            need(node >= 0 && node < nn, "invalid PS1 seat");
            seats.push_back(node);
            o += 4;
        }
        need(o == trailer, "PS1 pose block length mismatch");
        w = b.at(trailer + 8);
        h = b.at(trailer + 9);
        d = b.at(trailer + 10);
        need(w >= 2 && w <= 32 && d >= 2 && d <= 32, "invalid PS1 footprint");
        excitement = u32(b, trailer + 24);
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 13; j++)
                upgrades[i][j] = u32(b, trailer + 32 + 52 * i + 4 * j);
        for (auto &upgrade : upgrades)
            need(upgrade[4] <= 128, "oversized PS1 ride capacity");
        last = tracks.empty() ? std::max(0, frames - 1) : 0;
        for (auto &t : tracks)
            last = std::max(last, t.second.back().frame);
        // Animated vertex rows are sampled before reduction. Bound the work as well as the input byte ranges so a
        // damaged entry cannot cause an enormous allocation while still keeping all supported discs well below it.
        need(last < 4096 && (size_t(last) + 1) * std::max(size_t(1), skin.size()) <= 4000000,
             "oversized PS1 animation");
    }
    Transform local(int node, int frame) const {
        const auto &n = bones.at(node);
        auto t = tracks.find(node);
        if (t == tracks.end())
            return {n.q, n.p};
        auto &k = t->second;
        if (frame <= k[0].frame)
            return {k[0].q, k[0].p};
        for (size_t i = 1; i < k.size(); i++)
            if (frame <= k[i].frame) {
                auto &a = k[i - 1];
                auto &b = k[i];
                double f = double(frame - a.frame) / (b.frame - a.frame);
                return {slerp(a.q, b.q, f), lerp(a.p, b.p, f)};
            }
        return {k.back().q, k.back().p};
    }
    Transform world(int node, int frame) const {
        auto a = local(node, frame);
        int parent = bones.at(node).parent;
        if (parent < 0)
            return a;
        auto b = world(parent, frame);
        return {qnorm(mul(b.first, a.first)), add(b.second, rot(b.first, a.second))};
    }
    std::vector<int> chain(int node) const {
        std::vector<int> a;
        while (node >= 0) {
            a.push_back(node);
            node = bones.at(node).parent;
        }
        return a;
    }
    std::pair<std::vector<int>, bool> keyframes(int node) const {
        std::set<int> f = {0, last};
        std::vector<int> animated;
        for (int n : chain(node))
            if (tracks.count(n)) {
                animated.push_back(n);
                for (auto &k : tracks.at(n))
                    if (k.frame <= last)
                        f.insert(k.frame);
            }
        return {{f.begin(), f.end()}, !(animated.empty() || (animated.size() == 1 && animated[0] == node))};
    }
    bool moving(int node) const {
        auto a = world(node, 0);
        auto frames_ = keyframes(node).first;
        for (int f = 0; f <= last; f += 7)
            frames_.push_back(f);
        for (int f : frames_)
            if (!same(a, world(node, f)))
                return true;
        return false;
    }
    Vec vertex(int v, int f) const {
        auto s = skin.find(v);
        if (s == skin.end())
            return verts.at(v);
        auto t = world(s->second.first, f);
        return add(t.second, rot(t.first, s->second.second));
    }
    Vec halfTurnp(Vec p) const {
        return {(w * 256 - p[0]) * S, p[1] * S, (d * 256 - p[2]) * S};
    }
};
struct Parts {
    std::vector<int> still;
    std::map<int, std::vector<int>> rigid;
    std::vector<std::pair<std::set<int>, std::vector<int>>> morph;
};
Parts partition(const MeshEntry &e) {
    // A face belongs to the stand, one rigid moving owner, or a connected group of owners that needs morph rows.
    std::set<int> moving;
    for (size_t i = 0; i < e.bones.size(); i++)
        if (e.moving(i))
            moving.insert(i);
    Parts out;
    std::vector<std::pair<int, std::set<int>>> span;
    std::map<int, int> parent;
    std::function<int(int)> find = [&](int x) {
        if (!parent.count(x))
            parent[x] = x;
        while (parent[x] != x)
            x = parent[x];
        return x;
    };
    for (size_t k = 0; k < e.faces.size(); k++) {
        std::set<int> owners;
        for (int v : e.faces[k].vi) {
            auto s = e.skin.find(v);
            owners.insert(s != e.skin.end() && moving.count(s->second.first) ? s->second.first : -1);
        }
        if (owners == std::set<int>{-1})
            out.still.push_back(k);
        else if (owners.size() == 1)
            out.rigid[*owners.begin()].push_back(k);
        else
            span.push_back({int(k), owners});
    }
    for (auto &s : span) {
        std::vector<int> os(s.second.begin(), s.second.end());
        std::sort(os.begin(), os.end(),
                  [](int a, int b) { return (a < 0 ? "S" : std::to_string(a)) < (b < 0 ? "S" : std::to_string(b)); });
        for (size_t i = 1; i < os.size(); i++)
            parent[find(os[i])] = find(os[0]);
    }
    std::map<int, size_t> comps;
    for (auto &s : span) {
        int root = find(*s.second.begin());
        if (!comps.count(root)) {
            comps[root] = out.morph.size();
            out.morph.push_back({{}, {}});
        }
        auto &c = out.morph[comps[root]];
        c.first.insert(s.second.begin(), s.second.end());
        c.second.push_back(s.first);
    }
    return out;
}
std::vector<int> dense(const std::vector<int> &frames) {
    std::set<int> out;
    for (size_t i = 1; i < frames.size(); i++) {
        int a = frames[i - 1], b = frames[i], n = (b - a - 1) / 4;
        out.insert(a);
        for (int k = 1; k <= n; k++)
            out.insert(a + int(rounded(double(k) * (b - a) / (n + 1))));
    }
    out.insert(frames.back());
    return {out.begin(), out.end()};
}
void twoSided(std::vector<Tri> &t) {
    size_t n = t.size();
    for (size_t i = 0; i < n; i++)
        if (t[i].mode & 2) {
            auto x = t[i];
            std::swap(x.vi[1], x.vi[2]);
            std::swap(x.uv[1], x.uv[2]);
            std::swap(x.p[1], x.p[2]);
            t.push_back(x);
        }
}
struct Node {
    std::string name;
    std::map<int, int> posidx;
    std::vector<Vec> positions;
    Bytes verts, points, uvs, faces, words, box;
    std::vector<std::array<int, 5>> fsets;
    int nv, nfs, nf, nc;
    Matrix matrix = ident;
    size_t ov, op, ou, ofs, of, ow;
};
Node buildNode(std::vector<Tri> tris, const std::map<Key, int> &texindex, std::string name) {
    // The PC mesh groups corners by texture, smooths shared positions at 50 degrees, and stores vectors four-wide.
    Node n;
    n.name = std::move(name);
    std::map<Vec, std::vector<size_t>> at;
    std::map<Key, std::vector<size_t>> groups;
    auto roundpos = [](Vec q) {
        for (double &c : q)
            c = rounded(c, 3);
        return q;
    };
    for (size_t i = 0; i < tris.size(); i++) {
        auto &t = tris[i];
        t.normal = norm(cross(sub(t.p[1], t.p[0]), sub(t.p[2], t.p[0])));
        for (int k = 0; k < 3; k++) {
            if (!n.posidx.count(t.vi[k])) {
                n.posidx[t.vi[k]] = n.positions.size();
                n.positions.push_back(t.p[k]);
            }
            t.pi[k] = n.posidx[t.vi[k]];
            at[roundpos(t.p[k])].push_back(i);
        }
        groups[t.key].push_back(i);
    }
    struct Corner {
        Vec normal;
        double u, v;
    };
    std::vector<Corner> corners;
    std::vector<int> words;
    std::vector<std::array<int, 3>> faces;
    std::vector<Vec> normals;
    double limit = std::cos(50 * std::acos(-1.) / 180.);
    for (auto &group : groups) {
        size_t first = faces.size(), firstc = corners.size();
        std::map<std::tuple<int, int, int, double, double, double>, int> seen;
        for (size_t i : group.second) {
            auto &t = tris[i];
            std::array<int, 3> idx;
            for (int k = 0; k < 3; k++) {
                Vec sum = {0, 0, 0};
                for (size_t j : at[roundpos(t.p[k])])
                    if (dot(tris[j].normal, t.normal) >= limit)
                        sum = add(sum, tris[j].normal);
                Vec nn = norm(sum);
                int u = t.uv[k][0], v = t.uv[k][1];
                auto ck = std::make_tuple(t.pi[k], u, v, rounded(nn[0], 3), rounded(nn[1], 3), rounded(nn[2], 3));
                if (!seen.count(ck)) {
                    seen[ck] = corners.size();
                    corners.push_back({nn, u / 256., v / 256.});
                    words.push_back(t.pi[k]);
                }
                idx[k] = seen[ck];
            }
            faces.push_back(idx);
            normals.push_back(t.normal);
        }
        n.fsets.push_back(
            {texindex.at(group.first), int(first), int(faces.size() - first), int(firstc), int(corners.size() - 1)});
    }
    n.nv = n.positions.size();
    n.nfs = n.fsets.size();
    n.nf = faces.size();
    n.nc = corners.size();
    need(n.nv && n.nc, "empty converted node");
    for (size_t i = 0; i < n.positions.size(); i += 4)
        for (int c = 0; c < 3; c++)
            for (int j = 0; j < 4; j++)
                flt(n.verts, n.positions[std::min(i + j, n.positions.size() - 1)][c]);
    for (auto &c : corners)
        floats(n.points, c.normal);
    for (auto &q : normals)
        floats(n.points, q);
    for (size_t i = 0; i < corners.size(); i += 4) {
        for (int j = 0; j < 4; j++)
            flt(n.uvs, corners[std::min(i + j, corners.size() - 1)].u);
        for (int j = 0; j < 4; j++)
            flt(n.uvs, corners[std::min(i + j, corners.size() - 1)].v);
    }
    for (size_t i = 0; i < faces.size(); i++) {
        word(n.faces, n.nc + i, 2);
        for (int v : faces[i])
            word(n.faces, v, 2);
    }
    for (int v : words)
        word(n.words, v, 2);
    for (int hi = 0; hi < 2; hi++)
        for (int c = 0; c < 3; c++) {
            double x = n.positions[0][c];
            for (auto p : n.positions)
                x = hi ? std::max(x, p[c]) : std::min(x, p[c]);
            flt(n.box, x);
        }
    return n;
}
using Pages = std::map<int, Bytes>;
void atlas(const Bytes &b, Pages &pages) {
    // One raw block followed by relative-copy RLE blocks. Four 8192-byte blocks make each VRAM texture page.
    int columns = u16(b, 6), rows = u16(b, 8), base = u16(b, 4);
    need(columns > 0 && columns <= 16 && rows > 0 && rows <= 2, "invalid PS1 atlas");
    size_t pos = 16 + 12 * u16(b, 0) + 8 * u16(b, 10);
    std::vector<Bytes> blocks;
    need(pos <= b.size() && 8192 <= b.size() - pos, "short PS1 atlas");
    blocks.emplace_back(b.begin() + pos, b.begin() + pos + 8192);
    pos += 8192;
    for (int i = 1; i < columns * rows * 4; i++) {
        Bytes block;
        bool done = false;
        while (pos < b.size()) {
            int a = b[pos++];
            if (a < 128) {
                size_t n = a + 1;
                need(pos <= b.size() && n <= b.size() - pos && block.size() + n <= 8192, "damaged PS1 atlas literal");
                block.insert(block.end(), b.begin() + pos, b.begin() + pos + n);
                pos += n;
            } else {
                need(pos < b.size(), "short PS1 atlas backreference");
                int n = b[pos++];
                if (!n) {
                    done = true;
                    break;
                }
                int back = 256 - a;
                need(size_t(back) <= block.size() && block.size() + n <= 8192, "damaged PS1 atlas backreference");
                for (int j = 0; j < n; j++)
                    block.push_back(block[block.size() - back]);
            }
        }
        need(done && block.size() == 8192, "damaged PS1 atlas block");
        blocks.push_back(std::move(block));
    }
    for (int i = 0; i < columns * rows; i++) {
        Bytes pg;
        for (int j = 0; j < 4; j++)
            append(pg, blocks[i * 4 + j]);
        pages[(base + i % columns) | ((i / columns) << 4)] = std::move(pg);
    }
}
uint16_t vram(const Pages &pages, int x, int y) {
    auto p = pages.find((x / 64) | ((y / 256) << 4));
    if (p == pages.end())
        return 0;
    size_t o = (y % 256) * 128 + (x % 64) * 2;
    return u16(p->second, o);
}
using Textures = std::vector<std::pair<int, std::string>>;
void writeFile(const fs::path &path, const Bytes &b) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    need(bool(out), "cannot create converted file");
    out.write(reinterpret_cast<const char *>(b.data()), b.size());
    out.close();
    need(bool(out), "cannot write converted file");
}
void writeText(const fs::path &path, const std::string &s) {
    writeFile(path, Bytes(s.begin(), s.end()));
}
using Pixel = std::array<int, 4>;
std::array<Pixel, 16> palette(const Pages &pages, int clut) {
    std::array<Pixel, 16> pal;
    for (int i = 0; i < 16; i++) {
        uint16_t w = vram(pages, (clut & 63) * 16 + i, (clut >> 6) & 511);
        pal[i] = w ? Pixel{(w & 31) * 255 / 31, ((w >> 5) & 31) * 255 / 31, ((w >> 10) & 31) * 255 / 31, 255}
                   : Pixel{0, 0, 0, 0};
    }
    return pal;
}
void texture(const fs::path &path, const Pages &pages, Key key, const std::set<std::array<int, 4>> &rects) {
    std::vector<Pixel> px(65536, Pixel{-1, -1, -1, -1});
    bool alpha = false;
    std::array<Pixel, 16> pal = palette(pages, key.second);
    auto pg = pages.find(key.first & 31);
    for (auto r : rects)
        for (int v = r[1]; v <= r[3]; v++)
            for (int u = r[0]; u <= r[2]; u++) {
                auto &p = px[v * 256 + u];
                if (p[0] >= 0)
                    continue;
                if (pg == pages.end())
                    p = {0, 0, 0, 0};
                else {
                    int bb = pg->second[(v & 255) * 128 + ((u & 255) >> 1)];
                    p = pal[(u & 1) ? bb >> 4 : bb & 15];
                }
                alpha |= p[3] == 0;
            }
    for (int grow = 0; grow < 2; grow++) {
        auto next = px;
        for (int v = 0; v < 256; v++)
            for (int u = 0; u < 256; u++) {
                size_t i = v * 256 + u;
                if (px[i][0] >= 0 && px[i][3] != 0)
                    continue;
                for (auto d : std::array<std::array<int, 2>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
                    int x = u + d[0], y = v + d[1];
                    if (x >= 0 && x < 256 && y >= 0 && y < 256) {
                        auto p = px[y * 256 + x];
                        if (p[0] >= 0 && p[3] != 0) {
                            p[3] = 0;
                            next[i] = p;
                            break;
                        }
                    }
                }
            }
        px = std::move(next);
    }
    Bytes out(18);
    out[2] = 2;
    put(out, 12, 256, 2);
    put(out, 14, 256, 2);
    out[16] = alpha ? 32 : 24;
    out[17] = alpha ? 0x28 : 0x20;
    for (auto p : px) {
        if (p[0] < 0)
            p = {128, 128, 128, 0};
        out.push_back(p[2]);
        out.push_back(p[1]);
        out.push_back(p[0]);
        if (alpha)
            out.push_back(p[3]);
    }
    writeFile(path, out);
}
// A ground cell's atlas tile, w by h texels.
std::vector<std::array<uint8_t, 4>> tileTexels(const Pages &pages, const std::array<int, 6> &tile) {
    auto [page, clut, u0, v0, w, h] = tile;
    std::array<Pixel, 16> pal = palette(pages, clut);
    auto pg = pages.find(page & 31);
    std::vector<std::array<uint8_t, 4>> px(w * h, {0, 0, 0, 0});
    if (pg != pages.end())
        for (int v = 0; v < h; v++)
            for (int u = 0; u < w; u++) {
                int bb = pg->second[((v0 + v) & 255) * 128 + (((u0 + u) & 255) >> 1)];
                Pixel p = pal[((u0 + u) & 1) ? bb >> 4 : bb & 15];
                px[v * w + u] = {uint8_t(p[0]), uint8_t(p[1]), uint8_t(p[2]), uint8_t(p[3])};
            }
    return px;
}
// The ride's own ground cells' texels with the park's ground keyed out. The jungle's grass is keyed cell by cell
// (KeyPs1Grass); Halloween's and Space's plain ground over the whole patch, laid out as floorTris lays the cells, so
// that only the plain ground the patch's edge reaches is keyed and art it encloses keeps its plain colour (the Skulls'
// stone circles, which run over four cells).
std::map<Key, std::vector<std::array<uint8_t, 4>>> groundTexels(const MeshEntry &e, const Pages &pages,
                                                                const GroundTiles &ground) {
    std::map<Key, std::vector<std::array<uint8_t, 4>>> out;
    for (auto &g : ground)
        out[g.first] = tileTexels(pages, g.second);
    std::string theme = e.spec.theme;
    int n = ground.empty() ? 0 : ground.begin()->second[4];
    bool square = true;
    for (auto &g : ground)
        square = square && g.second[4] == n && g.second[5] == n;
    if ((theme != "hallow" && theme != "space") || !square || int(ground.size()) != e.w * e.d) {
        for (auto &g : ground)
            KeyPs1Grass(out[g.first], g.second[4], g.second[5]);
        return out;
    }
    // Each world texel's cell and tile texel: its centre turned back and mirrored as floorTris turns the corners, in
    // half texels.
    int W = e.w * n, H = e.d * n;
    std::vector<std::array<uint8_t, 4>> patch(size_t(W) * H);
    std::vector<std::pair<Key, int>> from(patch.size());
    size_t map = u32(e.raw, 20) + 192;
    for (int z = 0; z < e.d; z++)
        for (int x = 0; x < e.w; x++) {
            Key key = {-1 - (z * e.w + x), 0};
            int turn = u16(e.raw, map + 4 * size_t((e.d - 1 - z) * e.w + (e.w - 1 - x)) + 2);
            for (int j = 0; j < n; j++)
                for (int i = 0; i < n; i++) {
                    int r = 2 * j + 1, c = 2 * n - 2 * i - 1;
                    for (int k = 0; k < (4 - (turn & 3)) % 4; k++) {
                        int was = r;
                        r = 2 * n - c;
                        c = was;
                    }
                    if (turn & 8)
                        c = 2 * n - c;
                    int t = (2 * n - r - 1) / 2 * n + (c - 1) / 2;
                    size_t at = size_t(z * n + j) * W + x * n + i;
                    patch[at] = out[key][t];
                    from[at] = {key, t};
                }
        }
    KeyPs1Patch(patch, W, H, theme);
    for (size_t i = 0; i < patch.size(); i++)
        out[from[i].first][from[i].second] = patch[i];
    return out;
}
// A ground cell's texture: its keyed texels (groundTexels), each texel repeated to kGroundTexture square, always with
// alpha.
void groundTexture(const fs::path &path, const std::vector<std::array<uint8_t, 4>> &px, int w, int h) {
    Bytes out(18);
    out[2] = 2;
    put(out, 12, kGroundTexture, 2);
    put(out, 14, kGroundTexture, 2);
    out[16] = 32;
    out[17] = 0x28;
    for (int y = 0; y < kGroundTexture; y++)
        for (int x = 0; x < kGroundTexture; x++) {
            auto p = px[(y * h / kGroundTexture) * w + x * w / kGroundTexture];
            out.push_back(p[2]);
            out.push_back(p[1]);
            out.push_back(p[0]);
            out.push_back(p[3]);
        }
    writeFile(path, out);
}
Textures writeTextures(const MeshEntry &e, const Pages &pages, const fs::path &out, const std::vector<Tri> &tris,
                       const GroundTiles &ground, std::map<Key, int> &index) {
    std::map<Key, std::vector<std::array<uint8_t, 4>>> keyed = groundTexels(e, pages, ground);
    std::map<Key, std::set<std::array<int, 4>>> keys;
    for (auto &t : tris) {
        int u0 = 256, v0 = 256, u1 = 0, v1 = 0;
        for (auto p : t.uv) {
            u0 = std::min(u0, p[0]);
            v0 = std::min(v0, p[1]);
            u1 = std::max(u1, p[0]);
            v1 = std::max(v1, p[1]);
        }
        keys[t.key].insert({u0, v0, u1, v1});
    }
    Textures textures;
    for (auto &k : keys) {
        std::ostringstream fn;
        fn << std::string(e.spec.key).substr(0, 4) << std::setw(2) << std::setfill('0') << textures.size() << ".tga";
        for (auto dir : {"textures", "stexture"})
            if (ground.count(k.first))
                groundTexture(out / dir / fn.str(), keyed.at(k.first), ground.at(k.first)[4], ground.at(k.first)[5]);
            else
                texture(out / dir / fn.str(), pages, k.first, k.second);
        index[k.first] = textures.size();
        textures.push_back({1, fn.str()});
    }
    return textures;
}
std::vector<Tri> floorTris(const MeshEntry &e, const Bytes &atlas, GroundTiles &ground) {
    // The ride's own ground, after the trailer's 188 bytes and a 4-byte head: per cell, rows from the exit's side, a
    // tile of the theme atlas (12-byte records after its 16-byte header: page, palette, size, corner) and its turn,
    // quarter turns in the low two bits and a mirror in bit 3. It lies at the ride's own height, as a PC ride's base
    // does: the game hides the park's ground under a placed ride, and the engine takes the ground's edge corners as
    // the skirt it moves onto the park's ground, so its edge meets the park's cells with no gap (raised above them,
    // the sky showed through a line along the edges that face the camera). Each cell has a texture of its own, the
    // whole of it on the cell, as a PC ride's procedural base has, so the engine can blend it into the park's ground
    // cell by cell; its key sorts before the rest, so the cell's own faces come first in their node.
    size_t map = u32(e.raw, 20) + 192;
    std::vector<Tri> out;
    for (int z = 0; z < e.d; z++)
        for (int x = 0; x < e.w; x++) {
            size_t cell = map + 4 * size_t((e.d - 1 - z) * e.w + (e.w - 1 - x));
            int tile = u16(e.raw, cell), turn = u16(e.raw, cell + 2);
            need(tile < u16(atlas, 0), "PS1 ground tile is not in the theme atlas");
            size_t rec = 16 + 12 * size_t(tile);
            int page = u16(atlas, rec), size = u16(atlas, rec + 6), corner = u16(atlas, rec + 8);
            int w = size & 255, h = size >> 8, u = corner & 255, v = corner >> 8;
            need((page >> 7 & 3) == 0 && w > 0 && h > 0 && u + w <= 256 && v + h <= 256, "invalid PS1 ground tile");
            Key key = {-1 - (z * e.w + x), 0};
            ground[key] = {page & 31, u16(atlas, rec + 2), u, v, w, h};
            for (auto corners : std::array<std::array<int, 3>, 2>{{{0, 2, 1}, {3, 1, 2}}}) {
                Tri t;
                t.key = key;
                for (int i = 0; i < 3; i++) {
                    int dx = corners[i] & 1, dz = corners[i] >> 1;
                    // The corner in the tile's picture, whose rows run against z and columns against x: turned back,
                    // then mirrored.
                    int r = dz, c = 1 - dx;
                    for (int k = 0; k < (4 - (turn & 3)) % 4; k++) {
                        int was = r;
                        r = 1 - c;
                        c = was;
                    }
                    if (turn & 8)
                        c = 1 - c;
                    t.vi[i] = -1 - ((z + dz) * (e.w + 1) + x + dx);
                    t.uv[i] = {c * 256, (1 - r) * 256};
                    t.p[i] = {(x + dx) * 10., 0., (z + dz) * 10.};
                }
                out.push_back(t);
            }
        }
    return out;
}
struct Mini {
    std::string name;
    int parent;
    Matrix matrix;
    uint32_t flags, id;
};
Bytes mesh(const std::string &stem, const Bytes &head, std::vector<Node> &nodes, const std::vector<Mini> &minis,
           const Textures &textures, const std::array<double, 6> &box) {
    // Preserve the donor's file header and build fresh texture, mesh, full-node and attachment-node arrays.
    need(head.size() >= 0xb8, "short donor mesh");
    Bytes out(head.begin(), head.begin() + 0xb8);
    std::fill(out.begin() + 0x18, out.begin() + 0x2c, 0);
    std::copy_n(stem.begin(), std::min(stem.size(), size_t(19)), out.begin() + 0x18);
    put(out, 0x30, u32(out, 0x30) | kPs1GroundHeader);
    size_t texoff = out.size();
    for (auto &t : textures) {
        word(out, t.first, 2);
        word(out, 0, 2);
        word(out, 0);
    }
    size_t info = out.size();
    out.resize(out.size() + 16 * textures.size());
    for (size_t i = 0; i < textures.size(); i++) {
        size_t slot = out.size();
        out.resize(slot + 20);
        std::copy_n(textures[i].second.begin(), std::min(textures[i].second.size(), size_t(19)), out.begin() + slot);
        put(out, info + 16 * i + 10, 1, 2);
        put(out, info + 16 * i + 12, slot);
    }
    size_t vo = out.size();
    for (auto &n : nodes) {
        n.ov = out.size();
        append(out, n.verts);
    }
    size_t po = out.size();
    for (auto &n : nodes) {
        n.op = out.size();
        append(out, n.points);
    }
    size_t fso = out.size();
    for (auto &n : nodes) {
        n.ofs = out.size();
        for (auto f : n.fsets) {
            word(out, texoff + 8 * f[0]);
            for (int i = 1; i < 5; i++)
                word(out, f[i], 2);
            word(out, 0);
        }
    }
    size_t fo = out.size();
    for (auto &n : nodes) {
        n.of = out.size();
        append(out, n.faces);
    }
    for (auto &n : nodes) {
        n.ow = out.size();
        append(out, n.words);
        align(out);
    }
    size_t uo = out.size();
    for (auto &n : nodes) {
        n.ou = out.size();
        append(out, n.uvs);
    }
    size_t no = out.size(), mo = no + 0xa0 * nodes.size(), so = mo + 0x58 * minis.size();
    Bytes strings;
    auto name = [&](std::string s) {
        size_t at = so + strings.size();
        strings.insert(strings.end(), s.begin(), s.end());
        strings.push_back(0);
        return at;
    };
    auto full = [&](size_t i) { return no + 0xa0 * i; };
    auto mini = [&](size_t i) { return mo + 0x58 * i; };
    std::vector<std::vector<size_t>> kids(nodes.size());
    for (size_t i = 1; i < nodes.size(); i++)
        kids[0].push_back(full(i));
    for (size_t i = 0; i < minis.size(); i++)
        kids.at(minis[i].parent).push_back(mini(i));
    std::map<size_t, size_t> sibling;
    for (auto &k : kids)
        for (size_t i = 1; i < k.size(); i++)
            sibling[k[i - 1]] = k[i];
    for (size_t i = 0; i < nodes.size(); i++) {
        auto &n = nodes[i];
        Bytes r(0xa0);
        put(r, 0, 1);
        put(r, 4, i ? full(0) : 0);
        put(r, 8, sibling[full(i)]);
        put(r, 12, kids[i].empty() ? 0 : kids[i][0]);
        putfloats(r, 0x10, n.matrix);
        put(r, 0x50, i, 2);
        put(r, 0x54, name(n.name));
        int vals[] = {n.nv, n.nfs, n.nf, n.nc};
        for (int j = 0; j < 4; j++)
            put(r, 0x58 + 2 * j, vals[j], 2);
        size_t offsets[] = {n.ov, n.op, n.ou, n.ofs, n.of};
        for (int j = 0; j < 5; j++)
            put(r, 0x60 + 4 * j, offsets[j]);
        std::copy(n.box.begin(), n.box.end(), r.begin() + 0x78);
        put(r, 0x94, n.ow);
        append(out, r);
    }
    for (size_t i = 0; i < minis.size(); i++) {
        auto &m = minis[i];
        Bytes r(0x58);
        put(r, 0, 0x200);
        put(r, 4, full(m.parent));
        put(r, 8, sibling[mini(i)]);
        putfloats(r, 0x10, m.matrix);
        put(r, 0x50, nodes.size() + i, 2);
        put(r, 0x54, name(m.name));
        append(out, r);
    }
    append(out, strings);
    align(out);
    size_t eo = out.size();
    for (auto &m : minis) {
        word(out, m.flags);
        word(out, m.id);
        out.resize(out.size() + 12);
    }
    int av = 0, au = 0, af = 0, afs = 0, ap = 0;
    for (auto &n : nodes) {
        av += (n.nv + 3) >> 2;
        au += (n.nc + 3) >> 2;
        af += n.nf;
        afs += n.nfs;
        ap += n.nc + n.nf;
    }
    size_t counts[] = {
        textures.size(), size_t(av),   size_t(au),   size_t(afs), size_t(af), 0, nodes.size() + minis.size(),
        nodes.size(),    nodes.size(), minis.size(), size_t(ap)};
    for (int i = 0; i < 11; i++)
        put(out, 0x36 + 2 * i, counts[i], 2);
    size_t offsets[] = {0, texoff, info, vo, po, uo, fso, fo, 0, no, mo, no, eo};
    for (int i = 0; i < 13; i++)
        put(out, 0x4c + 4 * i, offsets[i]);
    putfloats(out, 0x80, box);
    put(out, 0x2c, out.size() - 4);
    return out;
}
struct Rigid {
    int idx;
    std::vector<Pose> keys;
};
struct Morph {
    int idx;
    std::vector<int> frames;
    std::vector<std::vector<Vec>> rows;
    std::vector<int> numbers;
};
std::array<double, 6> bounds(const std::vector<Vec> &p) {
    need(!p.empty(), "empty mesh bounds");
    std::array<double, 6> box;
    for (int hi = 0; hi < 2; hi++)
        for (int c = 0; c < 3; c++) {
            double x = p[0][c];
            for (auto q : p)
                x = hi ? std::max(x, q[c]) : std::min(x, q[c]);
            box[hi * 3 + c] = x;
        }
    return box;
}
Bytes animation(const std::string &stem, const std::string &suffix, const Bytes &donor, const Bytes &main, int last,
                const std::vector<Rigid> &rigid, const std::vector<Morph> &morphs) {
    // Position shapes and quaternion keys are contiguous. Morph coordinates use the loader's signed 10-bit packing.
    need(donor.size() >= 0xb8, "short donor animation");
    Bytes out(donor.begin(), donor.begin() + 0xb8);
    std::fill(out.begin() + 0x18, out.begin() + 0x2c, 0);
    std::string nm = stem + suffix + ".md2";
    std::copy_n(nm.begin(), std::min(nm.size(), size_t(19)), out.begin() + 0x18);
    std::copy(main.begin() + 0x36, main.begin() + 0x4c, out.begin() + 0x36);
    std::fill(out.begin() + 0x4c, out.begin() + 0x80, 0);
    std::copy(main.begin() + 0x80, main.begin() + 0x98, out.begin() + 0x80);
    std::fill(out.begin() + 0x98, out.begin() + 0xb8, 0);
    size_t shapes = out.size();
    out.resize(out.size() + 16 * rigid.size());
    for (size_t i = 0; i < rigid.size(); i++) {
        auto &k = rigid[i].keys;
        size_t pts = out.size();
        for (auto &p : k)
            floats(out, p.p);
        floats(out, k.back().p);
        size_t pairs = out.size();
        for (auto &p : k) {
            word(out, p.frame, 2);
            word(out, 0, 2);
        }
        size_t r = shapes + 16 * i;
        put(out, r, 8);
        put(out, r + 4, k.size() + 1, 2);
        put(out, r + 6, k.size(), 2);
        put(out, r + 8, pts);
        put(out, r + 12, pairs);
    }
    align(out);
    size_t quats = out.size();
    std::vector<size_t> qat, mat;
    for (auto &r : rigid) {
        qat.push_back(out.size());
        for (auto &k : r.keys) {
            word(out, k.frame, 2);
            word(out, 65535, 2);
            floats(out, k.q);
        }
    }
    for (auto &m : morphs) {
        std::vector<Vec> all;
        for (auto &r : m.rows)
            all.insert(all.end(), r.begin(), r.end());
        auto b = bounds(all);
        Vec off, scale;
        for (int c = 0; c < 3; c++) {
            off[c] = (b[c] + b[c + 3]) / 2;
            scale[c] = std::max((b[c + 3] - b[c]) / 1020., 1e-5);
        }
        auto pack = [&](Vec p) {
            uint32_t w = 0;
            for (int c = 0; c < 3; c++) {
                int v = int(rounded((p[c] - off[c]) / scale[c]));
                v = std::max(-511, std::min(511, v));
                w |= (v & 1023) << (10 * c);
            }
            return w;
        };
        align(out);
        size_t frames = out.size();
        for (int f : m.frames)
            word(out, f, 2);
        align(out);
        size_t nbox = out.size();
        word(out, 0, 2);
        word(out, 0, 2);
        size_t numbers = out.size();
        for (int n : m.numbers)
            word(out, n, 2);
        align(out);
        size_t box = out.size();
        for (auto &row : m.rows) {
            auto b_ = bounds(row);
            word(out, pack({b_[0], b_[1], b_[2]}));
            word(out, pack({b_[3], b_[4], b_[5]}));
        }
        size_t verts = out.size();
        for (auto &row : m.rows)
            for (auto p : row)
                word(out, pack(p));
        size_t kd = out.size();
        for (int j = 0; j < 2; j++) {
            word(out, m.frames.size(), 2);
            word(out, j ? m.numbers.size() : 2, 2);
            word(out, j ? numbers : nbox);
            word(out, frames);
            word(out, j ? verts : box);
            word(out, 0);
        }
        mat.push_back(out.size());
        for (int v : {1, 2, 0, 0, 0, 0})
            word(out, v, 2);
        word(out, kd);
        word(out, 0);
        floats(out, off);
        floats(out, scale);
    }
    size_t na = out.size();
    for (size_t i = 0; i < rigid.size(); i++) {
        Bytes r(0x40);
        put(r, 0, i);
        put(r, 4, 9);
        put(r, 0x10, rigid[i].keys.size(), 2);
        put(r, 0x14, rigid[i].idx, 2);
        put(r, 0x18, shapes + 16 * i);
        put(r, 0x1c, qat[i]);
        append(out, r);
    }
    for (size_t i = 0; i < morphs.size(); i++) {
        Bytes r(0x40);
        put(r, 0, rigid.size() + i);
        put(r, 4, 0x1000);
        put(r, 0x14, morphs[i].idx, 2);
        put(r, 0x28, mat[i]);
        append(out, r);
    }
    size_t anim = out.size();
    Bytes h(0x48);
    put(h, 0, 5);
    put(h, 8, last);
    size_t nq = 0;
    for (auto &r : rigid)
        nq += r.keys.size();
    put(h, 12, rigid.size(), 2);
    put(h, 16, nq, 2);
    put(h, 18, rigid.size() + morphs.size(), 2);
    put(h, 0x20, rigid.empty() ? 0 : shapes);
    put(h, 0x28, rigid.empty() ? 0 : quats);
    put(h, 0x2c, rigid.empty() && morphs.empty() ? 0 : na);
    append(out, h);
    put(out, 0x98, anim);
    put(out, 0x2c, out.size() - 4);
    return out;
}
// The bytecode here is the converter's ride behaviour, assembled with labels rather than copied from a game script.
struct Assembler {
    std::vector<uint32_t> code;
    std::map<std::string, size_t> labels;
    std::vector<std::pair<size_t, std::string>> fix;
    void label(const std::string &s) {
        labels[s] = code.size();
    }
    void op(uint32_t n, std::initializer_list<uint32_t> args = {}) {
        code.push_back(0x80000000 | n);
        code.insert(code.end(), args);
    }
    void jump(uint32_t n, const std::string &s) {
        code.push_back(0x80000000 | n);
        fix.push_back({code.size(), s});
        code.push_back(0x20000000);
    }
    std::vector<uint32_t> done() {
        for (auto &f : fix)
            code[f.first] = 0x20000000 | labels.at(f.second);
        return code;
    }
};
constexpr uint32_t variable(int n) {
    return 0x40000000 | n;
}
constexpr uint32_t LETMEON = variable(0), LETMEOFF = variable(1), CAPACITY = variable(2), DURATION = variable(3),
                   BREAKSTAT = variable(4), ONRIDE = variable(5), CLOSED = variable(6), BROKEN = variable(7),
                   WORN = variable(8), RUNNING = variable(9), TEMP = variable(12), P_LEFT = variable(13),
                   P_DEADLINE = variable(14), P_NOW = variable(15);
constexpr int HOLD = 1, YIELD = 2, SET = 3, DIFF = 5, ZEROCOUNT = 6, CLOCK = 7, EVENT = 13, FLUSH = 15, PLAY = 16,
              PLAYWAIT = 17, LOOP = 18, JMP = 31, JZ = 32, JNZ = 33, JNEG = 34, JPOS = 35, NAME = 37, TEST = 38,
              HPUSH = 42, HPOP = 43, ADD = 47, HEADON = 56, HEADOFF = 57, SSTART = 86, SSTOP = 87, REPAIR = 93;
constexpr uint32_t minus1 = 65535;
// Guests board in a batch and ride as heads on the mesh's 0x80 nodes (HEADON), as the PC car rides do. A timed ride
// (one that had PS1 seats) turns its animation until the player's duration has passed; the others turn it once.
std::vector<uint32_t> riderScript(bool timed) {
    Assembler a;
    a.op(NAME, {0x10000000});
    a.op(PLAYWAIT, {0, 0});
    a.op(ZEROCOUNT);
    a.label("closed");
    a.op(TEST, {CLOSED});
    a.jump(JNZ, "closed");
    a.op(LOOP, {2, 0});
    a.label("main");
    a.op(CLOCK, {P_DEADLINE});
    a.op(ADD, {P_DEADLINE, 10000});
    a.op(SET, {P_LEFT, CAPACITY});
    a.label("board");
    a.op(TEST, {P_LEFT});
    a.jump(JZ, "go");
    a.op(TEST, {CLOSED});
    a.jump(JNZ, "unload");
    a.op(TEST, {BREAKSTAT});
    a.jump(JNZ, "break");
    a.op(TEST, {ONRIDE});
    a.jump(JZ, "nobody");
    a.op(CLOCK, {P_NOW});
    a.op(DIFF, {P_NOW, P_DEADLINE, P_NOW});
    a.jump(JNEG, "go");
    a.label("nobody");
    a.op(HOLD);
    a.op(TEST, {LETMEON});
    a.jump(JZ, "nowait");
    a.op(CLOCK, {P_DEADLINE});
    a.op(ADD, {P_DEADLINE, 10000});
    a.op(HPUSH, {LETMEON});
    a.op(HEADON, {LETMEON});
    a.op(SET, {LETMEON, 0});
    a.op(ADD, {ONRIDE, 1});
    a.op(ADD, {P_LEFT, minus1});
    a.label("nowait");
    a.op(YIELD);
    a.jump(JMP, "board");
    a.label("go");
    a.op(SET, {RUNNING, 1});
    a.op(SSTART, {ONRIDE, 20});
    a.op(TEST, {WORN});
    a.jump(JZ, "nowear");
    a.op(EVENT, {2, 1, 22});
    a.label("nowear");
    if (timed) {
        a.op(CLOCK, {P_DEADLINE});
        a.op(SET, {TEMP, DURATION});
        a.label("seconds");
        a.op(ADD, {P_DEADLINE, 1000});
        a.op(DIFF, {TEMP, TEMP, 1});
        a.jump(JPOS, "seconds");
        a.label("turn");
        a.op(PLAYWAIT, {5, 0});
        a.op(CLOCK, {P_NOW});
        a.op(DIFF, {P_NOW, P_DEADLINE, P_NOW});
        a.jump(JPOS, "turn");
    } else
        a.op(PLAYWAIT, {5, 0});
    a.op(SSTOP);
    a.op(SET, {RUNNING, 0});
    a.op(LOOP, {2, 0});
    a.label("unload");
    a.op(TEST, {ONRIDE});
    a.jump(JZ, "empty");
    a.op(HOLD);
    a.op(HPOP, {LETMEOFF});
    a.op(HEADOFF, {LETMEOFF});
    a.op(YIELD);
    a.label("leaving");
    a.op(TEST, {LETMEOFF});
    a.jump(JZ, "left");
    a.op(YIELD);
    a.jump(JMP, "leaving");
    a.label("left");
    a.op(ADD, {ONRIDE, minus1});
    a.jump(JMP, "unload");
    a.label("empty");
    a.op(TEST, {BROKEN});
    a.jump(JNZ, "broken");
    a.op(TEST, {CLOSED});
    a.jump(JZ, "main");
    a.label("shut");
    a.op(TEST, {CLOSED});
    a.jump(JNZ, "shut");
    a.jump(JMP, "main");
    a.label("break");
    a.op(SET, {BROKEN, 1});
    a.op(SET, {RUNNING, 0});
    a.op(FLUSH);
    a.op(EVENT, {5, minus1, 43});
    a.op(PLAY, {9, 0, 0});
    a.op(SSTOP);
    a.jump(JMP, "unload");
    a.label("broken");
    a.op(TEST, {BREAKSTAT});
    a.jump(JNZ, "broken");
    a.op(FLUSH);
    a.op(REPAIR, {1});
    a.op(PLAYWAIT, {10, 0});
    a.op(REPAIR, {0});
    a.op(SET, {BROKEN, 0});
    a.op(LOOP, {2, 0});
    a.jump(JMP, "main");
    return a.done();
}
Bytes script(const Bytes &donor, const std::string &name, const std::vector<uint32_t> &code, int seats) {
    need(donor.size() >= 0x34, "short donor script");
    size_t tail = 0x34 + 4 * uint64_t(u32(donor, 0x30));
    uint32_t oldlen = u32(donor, tail);
    need(tail + 4 <= donor.size() && oldlen <= donor.size() - tail - 4, "damaged donor script");
    Bytes out(donor.begin(), donor.begin() + 0x34);
    put(out, 0x18, seats);
    put(out, 0x30, code.size());
    for (uint32_t w : code)
        word(out, w);
    word(out, name.size() + 1);
    out.insert(out.end(), name.begin(), name.end());
    out.push_back(0);
    out.insert(out.end(), donor.begin() + tail + 4 + oldlen, donor.end());
    return out;
}
std::string crlf(const std::vector<std::string> &lines) {
    std::string s;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i)
            s += "\r\n";
        s += lines[i];
    }
    return s;
}
void names(const fs::path &dir, const Spec &s) {
    std::string text =
        "NAME\r\n" + std::string(s.title) + "\r\n\r\nSIGNA\r\n" + s.a + "\r\n\r\nSIGNB\r\n" + s.b + "\r\n";
    Bytes b = {255, 254};
    for (unsigned char c : text) {
        b.push_back(c);
        b.push_back(0);
    }
    for (const char *lang :
         {"american", "english", "French", "German", "Spanish", "Italian", "Dutch", "Swedish", "Danish", "Polish"})
        writeFile(dir / (std::string(lang) + ".txt"), b);
}
using Donors = std::map<std::string, Bytes>;
std::string lower(std::string s) {
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c += 32;
    return s;
}
fs::path casePath(const fs::path &root, const std::string &path) {
    fs::path result = root;
    std::istringstream parts(path);
    std::string part;
    while (std::getline(parts, part, '/')) {
        std::error_code ec;
        fs::path found;
        for (auto &entry : fs::directory_iterator(result, ec))
            if (lower(tpw::PathText(entry.path().filename())) == lower(part)) {
                found = entry.path();
                break;
            }
        if (found.empty())
            return {};
        result = found;
    }
    return result;
}
bool loadDonors(const fs::path &data, Donors &out) {
    struct Wad {
        const char *path;
        const char *prefix;
        std::vector<std::string> files;
    };
    const Wad wads[] = {
        {"data/levels/jungle/rides/bouncy.wad", "bouncy", {"bouncy.md2", "bouncym1.md2", "bouncy.sgn"}},
        {"data/levels/hallow/rides/pumpkin.wad", "pumpkin", {"pumpkin.rse", "pumpkin.sgn"}},
        {"data/levels/space/rides/bouncy.wad", "spbouncy", {"bouncy.sgn"}},
        {"data/levels/hallow/features/5x5rck.wad", "rock", {"5x5rck.rse"}}};
    for (const auto &wad : wads) {
        bool complete = false;
        for (const char *edition : {"us", "eur", "us-pc", "eur-pc"}) {
            auto path = casePath(data / "assets" / edition / "Theme Park Data", wad.path);
            if (path.empty())
                continue;
            std::string err;
            auto blob = OpenFileBlob(tpw::PathText(path), err);
            if (!blob)
                continue;
            auto source = OpenWad(blob, err);
            if (!source)
                continue;
            Donors found;
            for (const auto &want : wad.files)
                for (auto &entry : source->Entries())
                    if (lower(tpw::PathText(fs::u8path(entry.path).filename())) == want) {
                        auto stream = source->Open(entry, err);
                        Bytes b;
                        if (stream && ReadAll(*stream, 16u << 20, b, err))
                            found[std::string(wad.prefix) + "/" + want] = std::move(b);
                        break;
                    }
            if (found.size() == wad.files.size()) {
                out.insert(found.begin(), found.end());
                complete = true;
                break;
            }
        }
        if (!complete)
            return false;
    }
    return true;
}
void rideSam(const MeshEntry &e, const fs::path &dir, const std::string &stem, int seats) {
    const auto &s = e.spec;
    bool timed = !e.seats.empty();
    int cap = seats;
    for (auto &u : e.upgrades)
        cap = std::max(cap, int(u[4]));
    size_t trailer = u32(e.raw, 20);
    int xx = u16(e.raw, trailer + 12), ex = u16(e.raw, trailer + 16), ez = u16(e.raw, trailer + 18);
    std::vector<std::string> lines = {"#",
                                      "# Converted from PS1 ride entry " + std::to_string(s.reference) +
                                          " by ps1conv.py",
                                      "#",
                                      "Info.Id\t\t\t" + std::to_string(s.id),
                                      "Info.Name\t\t\"" + std::string(s.title) + "\"",
                                      "Info.Shape",
                                      "---"};
    for (int r = 0; r < e.d; r++) {
        std::string row(e.w, '*');
        // The trailer's first door is the exit, always on the first row, and its second the entrance, on the first or
        // the last row. The PC's doors are the way the guests walk: an entrance by its letter ('S' in from the first
        // row's side, 'N' from the last row's) and an exit by its arrow ('8' out past the first row).
        if (r == 0)
            row.at(e.w - 1 - xx) = '8';
        if (r == ez)
            row.at(e.w - 1 - ex) = r ? 'N' : 'S';
        lines.push_back(row);
    }
    lines.insert(lines.end(), {"---", "", "Info.Hoarding", "---"});
    for (int r = 0; r < e.d; r++)
        lines.push_back(r == 0         ? "F" + std::string(e.w - 2, '.') + "7"
                        : r == e.d - 1 ? "L" + std::string(e.w - 2, '.') + "J"
                                       : "[" + std::string(e.w - 2, '.') + "]");
    lines.insert(lines.end(), {"---", "", "Info.DownloadedRideNumber\t" + std::to_string(s.number),
                               "Info.RideTypeStringIndex " + std::to_string(timed ? 7 : 15)});
    lines.insert(lines.end(), {"", "UsageInfo.RequiresTeleport\t1", "UsageInfo.MinCapacity\t1",
                               "UsageInfo.MaxCapacity\t" + std::to_string(cap), "UsageInfo.MinDuration\t10",
                               "UsageInfo.MaxDuration\t60", ""});
    for (int i = 0; i < 3; i++) {
        std::string p = "Upgrades[" + std::to_string(i) + "].";
        auto &u = e.upgrades[i];
        lines.push_back(p + "InitCapacity\t" + std::to_string(u[4]));
        lines.push_back(p + "RedLineCapacity\t" + std::to_string(u[4]));
        if (timed)
            lines.insert(lines.end(), {p + "InitDuration\t30", p + "RedLineDuration\t" + std::to_string(45 + 5 * i),
                                       p + "QueueWaitTimeConstant\t" + std::to_string(130 + 5 * i)});
        else
            lines.push_back(p + "QueueWaitTimeConstant\t" + std::to_string(6 + 3 * i));
        lines.insert(lines.end(), {p + "CostOfUpgrade\t" + std::to_string(u[12]),
                                   p + "CostOfResearch\t" + std::to_string(i ? u[11] : 0), ""});
    }
    lines.insert(lines.end(),
                 {"Info.DurationUnit\t" + std::to_string(timed ? 1 : 3), "", "Info.AttractionValue\t25", "",
                  "Attraction[0].NewBonus\t10", "Attraction[1].NewBonus\t7", "Attraction[2].NewBonus\t4", "",
                  "UsageInfo.ExcitementLevel\t" + std::to_string(e.excitement), ""});
    writeText(dir / (stem + ".sam"), crlf(lines));
    writeText(dir / ("Easy_" + stem + ".sam"),
              crlf({"Upgrades[0].WearRate\t3", "Upgrades[1].WearRate\t2", "Upgrades[2].WearRate\t1",
                    "Upgrades[1].CostOfResearch\t0", "Upgrades[2].CostOfResearch\t0", ""}));
}
std::vector<int> refine(const MeshEntry &e, int node, std::vector<int> frames) {
    double radius = 1;
    for (auto &s : e.skin)
        if (s.second.first == node)
            radius = std::max(radius, std::sqrt(dot(s.second.second, s.second.second)));
    std::vector<Transform> world;
    for (int f = 0; f <= e.last; f++)
        world.push_back(e.world(node, f));
    std::set<int> fr(frames.begin(), frames.end());
    for (;;) {
        std::vector<int> added;
        frames.assign(fr.begin(), fr.end());
        for (size_t i = 1; i < frames.size(); i++) {
            int a = frames[i - 1], b = frames[i];
            auto qa = world[a].first, qb = world[b].first;
            auto pa = world[a].second, pb = world[b].second;
            if (dot(qa, qb) < 0)
                for (double &c : qb)
                    c = -c;
            double worst = 0;
            int at = -1;
            for (int f = a + 1; f < b; f++) {
                double t = double(f - a) / (b - a);
                auto q = slerp(qa, qb, t);
                auto p = lerp(pa, pb, t);
                double d = std::abs(dot(q, world[f].first));
                double err = std::sqrt(dot(sub(p, world[f].second), sub(p, world[f].second))) +
                             2 * radius * std::acos(std::min(1., d)) * 2;
                if (err > worst) {
                    worst = err;
                    at = f;
                }
            }
            if (worst > 2)
                added.push_back(at);
        }
        if (added.empty())
            return frames;
        fr.insert(added.begin(), added.end());
    }
}
Vec scaled(Vec v, double k) {
    return {v[0] * k, v[1] * k, v[2] * k};
}
// The turn that takes local Y to `up`, so a rider's head stands upright however its node is turned.
Quat upright(Vec up) {
    up = norm(up);
    if (up[1] < -0.9999)
        return {1, 0, 0, 0};
    return qnorm({up[2], 0, -up[0], 1 + up[1]});
}
// Where the riders go on a ride the PS1 data gives no seats, in the carrier's frame (PS1 units): a ring about the
// upright axis (world up at frame 0) through the highest point of the parts it carries, three fifths of their mean
// distance from that axis out, as high as their surface reaches at and beyond three quarters of that radius.
std::vector<Vec> riderRing(const MeshEntry &e, int carrier, int count) {
    auto w = e.world(carrier, 0);
    Quat back = conj(w.first);
    Vec up = rot(back, {0, 1, 0});
    std::vector<Vec> pts;
    for (auto &v : e.skin) {
        auto chain = e.chain(v.second.first);
        if (std::find(chain.begin(), chain.end(), carrier) != chain.end())
            pts.push_back(rot(back, sub(e.vertex(v.first, 0), w.second)));
    }
    need(!pts.empty(), "PS1 rider carrier carries nothing");
    Vec axis = pts[0];
    for (auto &p : pts)
        if (dot(p, up) > dot(axis, up))
            axis = p;
    auto height = [&](Vec p) { return dot(sub(p, axis), up); };
    auto across = [&](Vec p) {
        Vec d = sub(sub(p, axis), scaled(up, height(p)));
        return std::sqrt(dot(d, d));
    };
    double mean = 0;
    for (auto &p : pts)
        mean += across(p) / pts.size();
    double radius = 0.6 * mean, top = -1e9;
    for (auto &p : pts)
        if (across(p) >= 0.75 * radius)
            top = std::max(top, height(p));
    Vec side = norm(cross(up, std::abs(up[0]) < 0.9 ? Vec{1, 0, 0} : Vec{0, 0, 1})), ahead = cross(up, side);
    std::vector<Vec> out;
    for (int i = 0; i < count; i++) {
        double a = 2 * std::acos(-1.) * i / count;
        out.push_back(add(add(axis, scaled(up, top)),
                          add(scaled(side, radius * std::cos(a)), scaled(ahead, radius * std::sin(a)))));
    }
    return out;
}
// A seated rider's head is this far above the seat (world units); a PC car's heads are about as far above its rim.
constexpr double kHeadRise = 1.0;
// The ride camera of a ride whose PS1 data has none, placed as the PS1 rides that have one place theirs: on the part
// that carries the riders, at a rider's eye (the middle of their ring, at their heads' height), upright and looking
// `ahead`, or where the carrier travels across its up when that is zero, so it takes every leap, tilt and turn of the
// ride. It is returned as a PS1 camera dummy in the carrier's frame (local Z up, looking along -Y).
Transform riderView(const MeshEntry &e, int carrier, Vec ahead) {
    Vec up = rot(conj(e.world(carrier, 0).first), {0, 1, 0});
    if (dot(ahead, ahead) == 0)
        for (int f = 0; f < e.last; f++) {
            auto a = e.world(carrier, f), b = e.world(carrier, f + 1);
            ahead = add(ahead, rot(conj(a.first), sub(b.second, a.second)));
        }
    ahead = sub(ahead, scaled(up, dot(ahead, up)));
    need(dot(ahead, ahead) > 1e-6, "PS1 rider carrier has no way ahead");
    auto ring = riderRing(e, carrier, 4);
    Vec eye = scaled(add(add(ring[0], ring[1]), add(ring[2], ring[3])), 0.25);
    // Upright, then turned about up to face ahead, then the dummy's axes: its Z up and its -Y ahead.
    Quat u = upright(up);
    Vec a = rot(conj(u), ahead);
    double half = std::atan2(a[0], a[2]) / 2;
    Quat q = mul(mul(u, {0, std::sin(half), 0, std::cos(half)}), {-std::sqrt(0.5), 0, 0, std::sqrt(0.5)});
    return {qnorm(q), add(eye, scaled(up, kHeadRise / S))};
}
void convertRide(const MeshEntry &e, const Pages &pages, const Bytes &atlas, const Donors &donors,
                 const fs::path &dir) {
    const Spec &s = e.spec;
    std::string stem = "_" + std::string(s.key);
    bool feature = s.number == 203;
    fs::create_directories(dir / "textures");
    fs::create_directories(dir / "stexture");
    Parts part = partition(e);
    std::vector<Vec> at0;
    for (size_t i = 0; i < e.verts.size(); i++)
        at0.push_back(e.halfTurnp(e.vertex(i, 0)));
    std::vector<Tri> tris = e.faces;
    double volume = 0;
    for (auto &t : tris)
        volume += dot(at0[t.vi[0]], cross(at0[t.vi[1]], at0[t.vi[2]]));
    if (volume < 0)
        for (auto &t : tris) {
            std::swap(t.vi[1], t.vi[2]);
            std::swap(t.uv[1], t.uv[2]);
        }
    std::vector<Tri> still;
    std::map<int, std::vector<Tri>> rigidT;
    std::vector<std::pair<std::set<int>, std::vector<Tri>>> morphT;
    for (int k : part.still) {
        auto t = tris[k];
        for (int i = 0; i < 3; i++)
            t.p[i] = at0[t.vi[i]];
        still.push_back(t);
    }
    twoSided(still);
    for (auto &r : part.rigid) {
        std::vector<Tri> list;
        for (int k : r.second) {
            auto t = tris[k];
            for (int i = 0; i < 3; i++)
                t.p[i] = halfTurnv(e.skin.at(t.vi[i]).second);
            list.push_back(t);
        }
        twoSided(list);
        rigidT[r.first] = std::move(list);
    }
    for (auto &m : part.morph) {
        std::vector<Tri> list;
        for (int k : m.second) {
            auto t = tris[k];
            for (int i = 0; i < 3; i++)
                t.p[i] = at0[t.vi[i]];
            list.push_back(t);
        }
        twoSided(list);
        morphT.push_back({m.first, std::move(list)});
    }
    // A carrier with no faces of its own (Eye Slide's node that takes the eye down the slide without rolling it) gets a
    // speck of a triangle at its origin, inside the part it carries, so that its riders have a mesh node to move with.
    if (s.carrier >= 0 && !rigidT.count(s.carrier)) {
        need(e.moving(s.carrier) && !rigidT.empty(), "PS1 rider carrier does not move");
        Tri t = rigidT.begin()->second.front();
        t.vi = {-1, -2, -3};
        t.mode = 0;
        t.p = {Vec{0, 0, 0}, Vec{0.01, 0, 0}, Vec{0, 0, 0.01}};
        rigidT[s.carrier] = {t};
    }
    size_t trailer = u32(e.raw, 20);
    int xx = u16(e.raw, trailer + 12), ex = u16(e.raw, trailer + 16), ez = u16(e.raw, trailer + 18);
    need(feature || u16(e.raw, trailer + 14) == 0, "PS1 exit is not on first row");
    need(feature || (ez == 0 && ex != xx) || ez == e.d - 1, "PS1 entrance is not on first or last row");
    need(ex < e.w && xx < e.w, "invalid PS1 entrance or exit");
    GroundTiles ground;
    auto floor = floorTris(e, atlas, ground);
    std::vector<Tri> all = still;
    for (auto &r : rigidT)
        all.insert(all.end(), r.second.begin(), r.second.end());
    for (auto &m : morphT)
        all.insert(all.end(), m.second.begin(), m.second.end());
    all.insert(all.end(), floor.begin(), floor.end());
    std::map<Key, int> texindex;
    Textures textures = writeTextures(e, pages, dir, all, ground, texindex);
    std::vector<Node> nodes;
    std::map<int, int> pcidx;
    std::vector<std::pair<int, std::set<int>>> morphNodes;
    if (feature) {
        need(rigidT.empty() && morphT.empty() && e.seats.empty(), "moving PS1 feature");
        nodes.push_back(buildNode(floor, texindex, "base"));
        nodes.push_back(buildNode(still, texindex, s.key));
    } else {
        still.insert(still.end(), floor.begin(), floor.end());
        nodes.push_back(buildNode(still, texindex, "stand"));
        for (auto &r : rigidT) {
            Node n = buildNode(r.second, texindex, "part" + std::to_string(r.first));
            auto w = e.world(r.first, 0);
            n.matrix = matrix(halfTurn(w.first), e.halfTurnp(w.second));
            pcidx[r.first] = nodes.size();
            nodes.push_back(std::move(n));
        }
        for (size_t j = 0; j < morphT.size(); j++) {
            morphNodes.push_back({int(nodes.size()), morphT[j].first});
            nodes.push_back(buildNode(morphT[j].second, texindex, "morph" + std::to_string(j)));
        }
    }
    std::vector<Mini> minis;
    int heads = 0;
    if (!feature) {
        auto attach = [&](int node) -> std::tuple<int, Matrix, Transform> {
            auto fr = e.keyframes(node).first;
            for (int f = 0; f <= e.last; f += 5)
                fr.push_back(f);
            for (int a : e.chain(node))
                if (pcidx.count(a)) {
                    auto rel = relative(e.world(a, 0), e.world(node, 0));
                    bool ok = true;
                    for (int f : fr)
                        if (!same(rel, relative(e.world(a, f), e.world(node, f)))) {
                            ok = false;
                            break;
                        }
                    if (ok)
                        return {pcidx.at(a), matrix(halfTurn(rel.first), halfTurnv(rel.second)), rel};
                }
            if (!e.moving(node)) {
                auto w = e.world(node, 0);
                return {0, matrix(halfTurn(w.first), e.halfTurnp(w.second)), Transform{}};
            }
            throw std::runtime_error("PS1 attachment moves apart from every mesh part");
        };
        int capacity = 0;
        for (auto &u : e.upgrades)
            capacity = std::max(capacity, int(u[4]));
        // The PS1 node `bone`'s mesh part and the matrix of h (in that node's frame) in that part's frame.
        auto place = [&](int bone, Transform h) {
            auto w = e.world(bone, 0);
            auto [parent, m, rel] = attach(bone);
            if (parent)
                m = matrix(halfTurn(qnorm(mul(rel.first, h.first))),
                           halfTurnv(add(rel.second, rot(rel.first, h.second))));
            else
                m = matrix(halfTurn(qnorm(mul(w.first, h.first))), e.halfTurnp(add(w.second, rot(w.first, h.second))));
            return std::pair<int, Matrix>{parent, m};
        };
        // A rider's head on the PS1 node `bone`, at p in its frame (PS1 units) raised kHeadRise, upright at rest.
        auto head = [&](int bone, Vec p) {
            Vec up = rot(conj(e.world(bone, 0).first), {0, 1, 0});
            auto [parent, m] = place(bone, {upright(up), add(p, scaled(up, kHeadRise / S))});
            int id = minis.size() + 1;
            minis.push_back({"head" + std::to_string(id), parent, m, 0xb1, uint32_t(id)});
        };
        // A seat beyond the PS1 seats repeats one of them, moved sideways along it.
        if (!e.seats.empty())
            for (int k = 0; k < std::max(capacity, int(e.seats.size())); k++) {
                int group = k / int(e.seats.size());
                head(e.seats[k % e.seats.size()], {40. * ((group + 1) / 2) * (group % 2 ? 1 : -1), 0, 0});
            }
        else if (s.carrier >= 0)
            for (auto &p : riderRing(e, s.carrier, capacity))
                head(s.carrier, p);
        need(int(minis.size()) >= capacity, "PS1 ride has no places for its riders");
        heads = minis.size();
        need(e.camera >= 0 || s.carrier >= 0, "PS1 ride has neither a camera nor a rider carrier");
        {
            int parent;
            Matrix m;
            if (e.camera >= 0)
                std::tie(parent, m, std::ignore) = attach(e.camera);
            else
                std::tie(parent, m) = place(s.carrier, riderView(e, s.carrier, s.ahead));
            // After the half turn about Y the PS1 camera dummy has local -Z up and looks along -Y; PC ride cameras
            // have Y up and look along Z. Its rows become (-X, -Z, -Y), which keeps a proper rotation.
            for (int c = 0; c < 3; c++) {
                double x = m[c], y = m[4 + c], z = m[8 + c];
                m[c] = -x;
                m[4 + c] = -z;
                m[8 + c] = -y;
            }
            minis.push_back({"camera", parent, m, 0x1031, 1});
        }
        double top = nodes[0].positions[0][1];
        for (auto p : nodes[0].positions)
            top = std::max(top, p[1]);
        int id = 0;
        for (auto f : std::array<std::array<double, 2>, 3>{{{0.3, 0.5}, {0.7, 0.5}, {0.5, 0.3}}}) {
            ++id;
            minis.push_back({"smoke" + std::to_string(id), 0,
                             matrix({0, 0, 0, 1}, {e.w * 10 * f[0], top * 0.6, e.d * 10 * f[1]}), 0x131, uint32_t(id)});
        }
    }
    std::vector<Vec> allv = nodes[0].positions;
    if (feature)
        allv.insert(allv.end(), nodes[1].positions.begin(), nodes[1].positions.end());
    else {
        std::set<int> sample;
        for (int f = 0; f <= e.last; f += 5)
            sample.insert(f);
        sample.insert(e.last);
        for (int f : sample)
            for (auto &v : e.skin)
                allv.push_back(e.halfTurnp(e.vertex(v.first, f)));
        allv.insert(allv.end(), at0.begin(), at0.end());
    }
    auto box = bounds(allv);
    Bytes main = mesh(stem, donors.at("bouncy/bouncy.md2"), nodes, minis, textures, box);
    writeFile(dir / (stem + ".md2"), main);
    auto &animhead = donors.at("bouncy/bouncym1.md2");
    std::vector<Rigid> rigid;
    std::vector<Morph> morphs;
    if (feature) {
        rigid.push_back({1, {{0, {0, -(box[4] + 1), 0}, {0, 0, 0, 1}}, {90, {0, 0, 0}, {0, 0, 0, 1}}}});
        writeFile(dir / (stem + "c.md2"), animation(stem, "c", animhead, main, 90, rigid, {}));
        Assembler a;
        a.op(NAME, {0x10000000});
        a.op(PLAYWAIT, {0, 0});
        a.label("rest");
        a.op(ZEROCOUNT);
        a.jump(JMP, "rest");
        writeFile(dir / (stem + ".RSE"), script(donors.at("rock/5x5rck.rse"), s.title, a.done(), 0));
        std::vector<std::string> lines = {"#",
                                          "# Converted from PS1 entry " + std::to_string(s.reference) +
                                              " by ps1conv.py, as a feature",
                                          "#",
                                          "Info.Id\t\t\t" + std::to_string(s.id),
                                          "Info.Name\t\t\"" + std::string(s.title) + "\"",
                                          "Info.IsChoosable\t0",
                                          "",
                                          "Info.Shape",
                                          "---"};
        for (int r = 0; r < e.d; r++)
            lines.push_back(std::string(e.w, '*'));
        lines.insert(lines.end(), {"---", "", "Info.OverwritePriority\t0", "", "Info.CreateParticleEffect\t81",
                                   "Info.DestroyParticleEffect\t77", "", "Upgrades[0].CostOfUpgrade\t200",
                                   "Upgrades[0].CostOfResearch\t0", "Research.Group\t0", "",
                                   "Info.DownloadedRideNumber\t" + std::to_string(s.number), ""});
        writeText(dir / (stem + ".sam"), crlf(lines));
        writeText(dir / ("Easy_" + stem + ".sam"), crlf({"Upgrades[0].CostOfResearch\t0", ""}));
        names(dir, s);
        return;
    }
    for (auto &r : rigidT) {
        auto [fr, arcs] = e.keyframes(r.first);
        fr = refine(e, r.first, arcs ? dense(fr) : fr);
        Rigid out;
        out.idx = pcidx.at(r.first);
        Quat prev = {};
        bool hasPrev = false;
        for (int f : fr) {
            auto w = e.world(r.first, f);
            Quat q = halfTurn(w.first);
            if (hasPrev && dot(prev, q) < 0)
                for (double &c : q)
                    c = -c;
            prev = q;
            hasPrev = true;
            out.keys.push_back({f, e.halfTurnp(w.second), q});
        }
        rigid.push_back(std::move(out));
    }
    for (auto &m : morphNodes) {
        auto &n = nodes[m.first];
        std::set<int> fr = {0, e.last};
        for (int o : m.second)
            if (o >= 0) {
                auto k = e.keyframes(o).first;
                fr.insert(k.begin(), k.end());
            }
        std::vector<int> frames(fr.begin(), fr.end());
        frames = dense(frames);
        fr = {frames.begin(), frames.end()};
        std::vector<int> order(n.posidx.size());
        for (auto p : n.posidx)
            order[p.second] = p.first;
        std::vector<std::vector<Vec>> allrows;
        for (int f = 0; f <= e.last; f++) {
            std::vector<Vec> row;
            for (int v : order)
                row.push_back(e.halfTurnp(e.vertex(v, f)));
            allrows.push_back(std::move(row));
        }
        for (;;) {
            frames.assign(fr.begin(), fr.end());
            std::vector<int> added;
            for (size_t i = 1; i < frames.size(); i++) {
                int a = frames[i - 1], b = frames[i];
                double worst = 0;
                int at = -1;
                for (int f = a + 1; f < b; f++) {
                    double t = double(f - a) / (b - a), err = 0;
                    for (size_t v = 0; v < order.size(); v++) {
                        auto delta = sub(lerp(allrows[a][v], allrows[b][v], t), allrows[f][v]);
                        err = std::max(err, std::sqrt(dot(delta, delta)));
                    }
                    if (err > worst) {
                        worst = err;
                        at = f;
                    }
                }
                if (worst > 0.1)
                    added.push_back(at);
            }
            if (added.empty())
                break;
            fr.insert(added.begin(), added.end());
        }
        Morph out;
        out.idx = m.first;
        out.frames = frames;
        for (int f : frames)
            out.rows.push_back(allrows[f]);
        for (int v : order)
            out.numbers.push_back(n.posidx.at(v));
        morphs.push_back(std::move(out));
    }
    writeFile(dir / (stem + "m1.md2"), animation(stem, "m1", animhead, main, e.last, rigid, morphs));
    for (auto suffix : std::array<std::pair<const char *, int>, 4>{{{"i", 10}, {"c", 30}, {"b", 30}, {"r", 60}}}) {
        std::vector<Rigid> rr;
        for (auto &r : rigid) {
            auto k = r.keys[0];
            k.frame = 0;
            auto end = k;
            end.frame = suffix.second;
            rr.push_back({r.idx, {k, end}});
        }
        std::vector<Morph> mm;
        for (auto &m : morphs)
            mm.push_back({m.idx, {0, suffix.second}, {m.rows[0], m.rows[0]}, m.numbers});
        writeFile(dir / (stem + suffix.first + ".md2"),
                  animation(stem, suffix.first, animhead, main, suffix.second, rr, mm));
    }
    writeFile(dir / (stem + ".RSE"),
              script(donors.at("pumpkin/pumpkin.rse"), s.title, riderScript(!e.seats.empty()), 0));
    std::string theme = s.theme;
    writeFile(dir / (stem + ".sgn"), donors.at(theme == "hallow"  ? "pumpkin/pumpkin.sgn"
                                               : theme == "space" ? "spbouncy/bouncy.sgn"
                                                                  : "bouncy/bouncy.sgn"));
    rideSam(e, dir, stem, heads);
    names(dir, s);
}
// Only directories created by this converter are removed. Iterating avoids following a symlink outside staging.
void clearStaging(const fs::path &path) {
    std::error_code ec;
    auto status = fs::symlink_status(path, ec);
    if (ec)
        return;
    if (fs::is_directory(status))
        for (auto &e : fs::directory_iterator(path))
            clearStaging(e.path());
    fs::remove(path, ec);
}
} // namespace

void KeyPs1Grass(std::vector<std::array<uint8_t, 4>> &tile, int w, int h) {
    // Grass is a mid green, with red under seven tenths and blue under four tenths of it: the jungle's grass and its
    // blends into the shore are, its water, sand, rocks and olive edges are not, nor Halloween's dark hedges or Space's
    // bright green pad.
    auto grass = [](const std::array<uint8_t, 4> &p) {
        return p[3] == 255 && p[1] >= 96 && p[1] <= 184 && p[0] * 10 <= p[1] * 7 && p[2] * 10 <= p[1] * 4;
    };
    std::vector<bool> ground(tile.size());
    for (size_t i = 0; i < tile.size(); i++)
        ground[i] = grass(tile[i]);
    KeyPs1Ground(tile, w, h, ground);
}

bool Ps1PlainGround(const std::string &theme, const std::array<uint8_t, 4> &p) {
    int r = p[0], g = p[1], b = p[2];
    if (theme == "hallow")
        // Halloween's flagstones (tile 145, the Skulls' whole corner cells, and the margins of every Halloween ride's
        // cells) are a mid grey, green 70 to 110, red up to 20 over it and blue from 10 under it to 40 over it; the
        // Clown's lawn (its corners, outside the ring's dark outline) is a dark olive with no blue, red from green up
        // to 131, green 57 to 90. The Clown's orange ring and outlines, the hedges, planks, bricks, moss and
        // gravestones are none of these.
        return (g >= 70 && g <= 110 && r >= g && r - g <= 20 && b + 10 >= g && b - g <= 40) ||
               (b <= 8 && g >= 57 && g <= 90 && r >= g && r <= 131);
    if (theme == "space")
        // Space's ground is purple, blue over twice green and red over one and a half times it; the Romper's brown
        // crater pad and its red and green lights are not.
        return b > 2 * g && 2 * r > 3 * g;
    return false;
}

void KeyPs1Patch(std::vector<std::array<uint8_t, 4>> &tile, int w, int h, const std::string &theme) {
    // The plain ground is what the edge reaches through plain texels, side by side only (a diagonal step through a
    // thin outline does not count).
    std::vector<bool> ground(tile.size());
    std::vector<size_t> todo;
    auto reach = [&](int x, int y) {
        size_t i = size_t(y) * w + x;
        if (x >= 0 && x < w && y >= 0 && y < h && !ground[i] && tile[i][3] == 255 && Ps1PlainGround(theme, tile[i])) {
            ground[i] = true;
            todo.push_back(i);
        }
    };
    for (int x = 0; x < w; x++) {
        reach(x, 0);
        reach(x, h - 1);
    }
    for (int y = 0; y < h; y++) {
        reach(0, y);
        reach(w - 1, y);
    }
    while (!todo.empty()) {
        size_t i = todo.back();
        todo.pop_back();
        int x = int(i % w), y = int(i / w);
        reach(x - 1, y);
        reach(x + 1, y);
        reach(x, y - 1);
        reach(x, y + 1);
    }
    // A fleck of up to four other texels with plain ground or the patch's edge all round it is part of the ground (the
    // flagstones' and the lawn's own flecks), not art.
    std::vector<bool> seen(tile.size());
    for (size_t s = 0; s < tile.size(); s++) {
        if (ground[s] || seen[s])
            continue;
        std::vector<size_t> fleck = {s};
        seen[s] = true;
        for (size_t k = 0; k < fleck.size(); k++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int x = int(fleck[k] % w) + dx, y = int(fleck[k] / w) + dy;
                    if (x < 0 || x >= w || y < 0 || y >= h)
                        continue;
                    size_t j = size_t(y) * w + x;
                    if (!ground[j] && !seen[j]) {
                        seen[j] = true;
                        fleck.push_back(j);
                    }
                }
        if (fleck.size() <= 4)
            for (size_t i : fleck)
                ground[i] = tile[i][3] == 255;
    }
    KeyPs1Ground(tile, w, h, ground);
}

void KeyPs1Ground(std::vector<std::array<uint8_t, 4>> &tile, int w, int h, const std::vector<bool> &ground) {
    // A ground texel next to the rest, counting diagonal steps as one, takes the colour of the nearest other texel and
    // alpha 170, two away 85, so the ride's ground fades into the park's instead of ending in a fringe; from three away
    // it keeps its own colour with alpha 0 (drawn without blending, as with Ground blending off, it is the disc's
    // ground). The palette's own transparent texels stay as they are and colour nothing.
    std::vector<int> dist(tile.size(), -1);
    std::vector<size_t> front;
    for (size_t i = 0; i < tile.size(); i++)
        if (!ground[i]) {
            dist[i] = 0;
            if (tile[i][3] == 255)
                front.push_back(i);
        }
    for (int d = 1; !front.empty(); d++) {
        std::vector<size_t> next;
        for (size_t i : front)
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int x = int(i % w) + dx, y = int(i / w) + dy;
                    if (x < 0 || x >= w || y < 0 || y >= h || dist[y * w + x] >= 0)
                        continue;
                    size_t j = y * w + x;
                    dist[j] = d;
                    if (d < 3)
                        tile[j] = {tile[i][0], tile[i][1], tile[i][2], uint8_t(255 - 85 * d)};
                    else
                        tile[j][3] = 0;
                    next.push_back(j);
                }
        front = std::move(next);
    }
    for (size_t i = 0; i < tile.size(); i++)
        if (dist[i] < 0)
            tile[i][3] = 0;
}

bool InstallPs1Ride(const fs::path &staged, const fs::path &destination, std::string &reason) {
    try {
        need(fs::is_directory(fs::symlink_status(staged)), "converted staging folder is missing");
        fs::create_directories(destination.parent_path());
        // Keep the old complete ride outside .staging, which the importer clears on startup. If a process stops between
        // the two renames, the next import restores it before trying the replacement again. Its hidden folder name does
        // not name a ride to content_overlay, so the backup cannot appear as a second downloadable ride.
        fs::path backup =
            destination.parent_path() / fs::u8path(".ps1-previous-" + tpw::PathText(destination.filename()));
        if (fs::exists(fs::symlink_status(backup))) {
            if (fs::exists(fs::symlink_status(destination)))
                clearStaging(backup); // The new complete directory was published before the interruption.
            else
                fs::rename(backup, destination);
            need(!fs::exists(fs::symlink_status(backup)), "previous converted ride needs recovery");
        }
        bool existing = fs::exists(fs::symlink_status(destination));
        if (existing)
            fs::rename(destination, backup);
        try {
            fs::rename(staged, destination);
        } catch (...) {
            if (existing)
                fs::rename(backup, destination);
            throw;
        }
        if (existing)
            clearStaging(backup);
        return true;
    } catch (const std::exception &e) {
        reason = e.what();
        return false;
    }
}

bool ConvertPs1(const Ps1Disc &disc, const fs::path &dataRoot, std::string &reason) {
    fs::path staging;
    try {
        Donors donors;
        if (!loadDonors(dataRoot, donors)) {
            reason = "drop your US or European PC disc too, then drop the PlayStation disc again.";
            return false;
        }
        fs::path assets = fs::canonical(dataRoot / "assets");
        // The content names are fixed by the converter. Refuse a pre-existing link in that output tree rather than
        // following it out of the player's selected asset folder.
        for (const Spec &s : specs) {
            fs::path relative =
                fs::path("ps1") / "data" / "levels" / s.theme / (s.number == 203 ? "features" : "rides");
            fs::path path = assets;
            for (const auto &part : relative) {
                path /= part;
                std::error_code ec;
                auto status = fs::symlink_status(path, ec);
                need(!fs::is_symlink(status), "PlayStation output folder contains a symbolic link");
            }
        }
        fs::path top = assets / ".staging";
        need(!fs::is_symlink(fs::symlink_status(top)), "PlayStation staging folder contains a symbolic link");
        fs::create_directories(top);
        for (unsigned n = 0;; n++) {
            staging = top / ("ps1-" + std::to_string(n));
            std::error_code ec;
            if (fs::create_directory(staging, ec))
                break;
            need(!ec, "cannot create PlayStation staging folder");
        }
        std::vector<std::pair<fs::path, fs::path>> pending;
        for (const Spec &s : specs) {
            auto found = disc.rides.find(s.lookup);
            need(found != disc.rides.end(), "PlayStation ride is missing");
            const auto &entry = found->second;
            MeshEntry e(s, disc.entries.at(entry.entry));
            Pages pages;
            atlas(disc.entries.at(entry.rideAtlas), pages);
            atlas(disc.entries.at(entry.themeAtlas), pages);
            std::string folder = "_" + std::string(s.key) + "_" + std::to_string(s.number);
            fs::path dir = staging / folder;
            convertRide(e, pages, disc.entries.at(entry.themeAtlas), donors, dir);
            fs::path target =
                assets / "ps1" / "data" / "levels" / s.theme / (s.number == 203 ? "features" : "rides") / folder;
            pending.push_back({dir, target});
        }
        for (auto &p : pending)
            if (!InstallPs1Ride(p.first, p.second, reason))
                throw std::runtime_error(reason);
        clearStaging(staging);
        return true;
    } catch (const std::exception &e) {
        if (!staging.empty())
            clearStaging(staging);
        reason = std::string("PlayStation conversion failed: ") + e.what();
        return false;
    }
}
} // namespace tpw::assets
