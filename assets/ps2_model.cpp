// ps2_model.cpp: see ps2_model.h.
#include "ps2_model.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>

namespace tpw::assets {
namespace {

using Bytes = std::vector<uint8_t>;

void Require(bool ok, const std::string& why) {
    if (!ok) throw std::runtime_error(why);
}

void Need(const Bytes& b, size_t at, size_t n) {
    Require(at <= b.size() && n <= b.size() - at, "a field is past the end of the file");
}

uint32_t U32(const Bytes& b, size_t at) {
    Need(b, at, 4);
    return static_cast<uint32_t>(b[at]) | static_cast<uint32_t>(b[at + 1]) << 8 | static_cast<uint32_t>(b[at + 2]) << 16 |
           static_cast<uint32_t>(b[at + 3]) << 24;
}

uint16_t U16(const Bytes& b, size_t at) {
    Need(b, at, 2);
    return static_cast<uint16_t>(b[at] | b[at + 1] << 8);
}

int16_t S16(const Bytes& b, size_t at) { return static_cast<int16_t>(U16(b, at)); }

uint8_t U8(const Bytes& b, size_t at) {
    Need(b, at, 1);
    return b[at];
}

float F32(const Bytes& b, size_t at) {
    const uint32_t w = U32(b, at);
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}

std::string CStr(const Bytes& b, size_t at) {
    Need(b, at, 1);
    const void* end = std::memchr(b.data() + at, 0, b.size() - at);
    Require(end != nullptr, "a name has no end");
    return std::string(reinterpret_cast<const char*>(b.data() + at), static_cast<const uint8_t*>(end) - (b.data() + at));
}

std::string TextureStem(std::string name) {
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const size_t dot = name.rfind('.');
    if (dot != std::string::npos) name.resize(dot);
    return name;
}

// One strip part of a mesh node's group: its positions (with the microcode's flag bits in the low bits of x and y),
// texture coordinates and material.
struct Part {
    uint32_t positions = 0, uvs = 0, count = 0;
    std::string texture;
};

// A rigid transform as the PS2 applies it to row vectors: p * r + t.
struct Xform {
    float r[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    float t[3] = {0, 0, 0};
};

Xform FromQuat(const float q[4], const float t[3]) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    Xform m;
    m.r[0][0] = 1 - 2 * (y * y + z * z); m.r[0][1] = 2 * (x * y - z * w);     m.r[0][2] = 2 * (x * z + y * w);
    m.r[1][0] = 2 * (x * y + z * w);     m.r[1][1] = 1 - 2 * (x * x + z * z); m.r[1][2] = 2 * (y * z - x * w);
    m.r[2][0] = 2 * (x * z - y * w);     m.r[2][1] = 2 * (y * z + x * w);     m.r[2][2] = 1 - 2 * (x * x + y * y);
    for (int i = 0; i < 3; ++i) m.t[i] = t[i];
    return m;
}

void Apply(const Xform& m, const float p[3], float out[3]) {
    for (int c = 0; c < 3; ++c) out[c] = p[0] * m.r[0][c] + p[1] * m.r[1][c] + p[2] * m.r[2][c] + m.t[c];
}

// a then b.
Xform Then(const Xform& a, const Xform& b) {
    Xform m;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) m.r[i][j] = a.r[i][0] * b.r[0][j] + a.r[i][1] * b.r[1][j] + a.r[i][2] * b.r[2][j];
    Apply(b, a.t, m.t);
    return m;
}

Xform Inverse(const Xform& a) {
    Xform m;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) m.r[i][j] = a.r[j][i];
    for (int c = 0; c < 3; ++c) m.t[c] = -(a.t[0] * m.r[0][c] + a.t[1] * m.r[1][c] + a.t[2] * m.r[2][c]);
    return m;
}

// A track's transform at time t: rotation keys lerped (the shorter way) and normalised, position keys lerped; before
// the first key the first, after the last the last.
Xform Sample(const Ps2BoneTrack& track, float t) {
    float q[4] = {0, 0, 0, 1}, p[3] = {0, 0, 0};
    if (!track.rot.empty()) {
        const auto& k = track.rot;
        size_t i = 0;
        while (i + 1 < k.size() && k[i + 1].time <= t) ++i;
        if (t <= k[0].time || i + 1 >= k.size()) {
            std::memcpy(q, (t <= k[0].time ? k[0] : k[i]).q, sizeof q);
        } else {
            const float f = (t - k[i].time) / std::max(1e-6f, static_cast<float>(k[i + 1].time - k[i].time));
            float b[4];
            std::memcpy(b, k[i + 1].q, sizeof b);
            if (k[i].q[0] * b[0] + k[i].q[1] * b[1] + k[i].q[2] * b[2] + k[i].q[3] * b[3] < 0)
                for (float& c : b) c = -c;
            float n = 0;
            for (int c = 0; c < 4; ++c) {
                q[c] = k[i].q[c] + (b[c] - k[i].q[c]) * f;
                n += q[c] * q[c];
            }
            n = std::sqrt(n);
            if (n > 0)
                for (float& c : q) c /= n;
        }
    }
    if (!track.pos.empty()) {
        const auto& k = track.pos;
        size_t i = 0;
        while (i + 1 < k.size() && k[i + 1].time <= t) ++i;
        if (t <= k[0].time || i + 1 >= k.size()) {
            std::memcpy(p, (t <= k[0].time ? k[0] : k[i]).p, sizeof p);
        } else {
            const float f = (t - k[i].time) / std::max(1e-6f, static_cast<float>(k[i + 1].time - k[i].time));
            for (int c = 0; c < 3; ++c) p[c] = k[i].p[c] + (k[i + 1].p[c] - k[i].p[c]) * f;
        }
    }
    return FromQuat(q, p);
}

const Ps2BoneTrack* TrackOf(const Ps2Clip& clip, int bone) {
    for (const Ps2BoneTrack& t : clip.bones)
        if (t.bone == bone) return &t;
    return nullptr;
}

}  // namespace

bool ParsePs2Mps(const Bytes& b, Ps2Mps& out, std::string& reason) {
    out = Ps2Mps{};
    reason.clear();
    try {
        const uint16_t nmats = U16(b, 0x22);
        out.triangles = U16(b, 0x2a);
        const uint16_t nnodes = U16(b, 0x2e), nmesh = U16(b, 0x30);
        Require(nmesh <= nnodes && nnodes < 1024 && nmats < 1024, "the model's node table is not valid");
        const uint32_t matp = U32(b, 0x3c), texp = U32(b, 0x40), nodep = U32(b, 0x48);
        // Materials: 8 bytes each; their texture lists: 16 bytes each (count at +0xa, names of 20 bytes at +0xc).
        std::map<uint32_t, std::string> materialTexture;
        for (uint32_t i = 0; i < nmats; ++i) {
            const uint32_t info = texp + 16 * i;
            const uint16_t count = U16(b, info + 0xa);
            materialTexture[matp + 8 * i] = count > 0 ? TextureStem(CStr(b, U32(b, info + 0xc))) : std::string();
        }
        std::vector<uint32_t> nodeAt;
        uint32_t p = nodep;
        for (uint32_t i = 0; i < nnodes; ++i) {
            nodeAt.push_back(p);
            p += i < nmesh ? 0xa0 : 0x60;
        }
        Need(b, nodep, p - nodep);
        std::map<uint32_t, int> nodeIndex;
        for (uint32_t i = 0; i < nnodes; ++i) nodeIndex[nodeAt[i]] = static_cast<int>(i);
        out.meshes = nmesh;
        for (uint32_t i = nmesh; i < nnodes; ++i) {
            out.boneNames.push_back(CStr(b, U32(b, nodeAt[i] + 0x54)));
            const auto parent = nodeIndex.find(U32(b, nodeAt[i] + 4));
            out.boneParent.push_back(parent != nodeIndex.end() && parent->second >= static_cast<int>(nmesh)
                                         ? parent->second - static_cast<int>(nmesh) : -1);
        }
        const int nbones = static_cast<int>(out.boneNames.size());
        for (uint32_t ni = 0; ni < nmesh; ++ni) {
            const uint32_t node = nodeAt[ni];
            // Groups at +0x68 (count at +0x64), 0x20 bytes each: material, part table, part count (low byte); the
            // part table's 16-byte rows: positions, texture coordinates, normals, vertex count (low 16 bits).
            const uint16_t ngroups = U16(b, node + 0x64);
            const uint32_t groups = U32(b, node + 0x68);
            std::vector<Part> parts;
            for (uint32_t g = 0; g < ngroups; ++g) {
                const uint32_t q = groups + 0x20 * g;
                const uint32_t material = U32(b, q), table = U32(b, q + 4);
                const uint16_t one = U16(b, q + 8);
                for (uint32_t k = 0; k < (one & 0xffu); ++k) {
                    const uint32_t row = table + 16 * k;
                    Part part;
                    part.positions = U32(b, row);
                    part.uvs = U32(b, row + 4);
                    part.count = U32(b, row + 12) & 0xffff;
                    Need(b, part.positions, 12 * static_cast<size_t>(part.count));
                    Need(b, part.uvs, 4 * static_cast<size_t>(part.count));
                    const auto tex = materialTexture.find(material);
                    if (tex != materialTexture.end()) part.texture = tex->second;
                    parts.push_back(part);
                }
            }
            // The skin (+0x90): vertex count, influence count, then per vertex (first influence u16, count u8), the
            // influences' bones (u8), weights (f32) and bone-space positions (3 f32). The deform list (+0x98): per strip
            // vertex a u16, (address << 2) | bit 1 "this skin vertex has more strip copies after this one".
            const uint32_t skin = U32(b, node + 0x90), deform = U32(b, node + 0x98);
            const uint16_t nv = U16(b, skin);
            const uint32_t pv = U32(b, skin + 4), pb = U32(b, skin + 8), pw = U32(b, skin + 12), pp = U32(b, skin + 16);
            const uint32_t first = static_cast<uint32_t>(out.vertices.size());
            out.meshFirstVertex.push_back(first);
            out.meshVertexCount.push_back(nv);
            for (uint32_t i = 0; i < nv; ++i) {
                const uint16_t f = U16(b, pv + 4 * i);
                const uint8_t n = U8(b, pv + 4 * i + 2);
                Ps2SkinVertex v;
                for (uint32_t j = 0; j < n; ++j) {
                    Ps2Influence inf;
                    inf.bone = U8(b, pb + f + j);
                    Require(inf.bone < nbones, "a skin influence names a bone the model does not have");
                    inf.weight = F32(b, pw + 4 * (f + j));
                    for (int c = 0; c < 3; ++c) inf.local[c] = F32(b, pp + 12 * (f + j) + 4 * c);
                    v.influences.push_back(inf);
                }
                out.vertices.push_back(v);
            }
            size_t strip = 0;
            for (const Part& part : parts) strip += part.count;
            std::vector<uint16_t> list(strip);
            for (size_t i = 0; i < strip; ++i) list[i] = U16(b, deform + 2 * i);
            // The strip slots in address order, part by part, give each address its (part, index).
            std::vector<uint16_t> sorted;
            for (uint16_t x : list) sorted.push_back(static_cast<uint16_t>(x >> 2));
            std::sort(sorted.begin(), sorted.end());
            std::map<uint16_t, std::pair<uint32_t, uint32_t>> slot;
            size_t at = 0;
            for (uint32_t pi = 0; pi < parts.size(); ++pi)
                for (uint32_t k = 0; k < parts[pi].count; ++k) slot[sorted[at++]] = {pi, k};
            std::vector<std::vector<int64_t>> vertexOf(parts.size());
            for (uint32_t pi = 0; pi < parts.size(); ++pi) vertexOf[pi].assign(parts[pi].count, -1);
            uint32_t vi = 0;
            std::vector<bool> placed(nv, false);
            for (uint16_t x : list) {
                Require(vi < nv, "the deform list names more vertices than the skin has");
                const auto [pi, k] = slot[static_cast<uint16_t>(x >> 2)];
                vertexOf[pi][k] = first + vi;
                if (!placed[vi]) {
                    // The bind position: the strip's, without the flag bits; model space is (x, z, y) of the strips'.
                    const uint32_t pos = parts[pi].positions + 12 * k;
                    float xyz[3];
                    for (int c = 0; c < 3; ++c) {
                        uint32_t w = U32(b, pos + 4 * c);
                        if (c < 2) w &= ~1u;
                        std::memcpy(&xyz[c], &w, 4);
                    }
                    Ps2SkinVertex& v = out.vertices[first + vi];
                    v.bind[0] = xyz[0];
                    v.bind[1] = xyz[2];
                    v.bind[2] = xyz[1];
                    placed[vi] = true;
                }
                if (!(x & 2)) ++vi;
            }
            // Triangles: the low bit of a vertex's x word set means no triangle ends there; the low bit of its y word
            // gives the winding.
            for (uint32_t pi = 0; pi < parts.size(); ++pi) {
                const Part& part = parts[pi];
                for (uint32_t k = 2; k < part.count; ++k) {
                    const uint32_t xw = U32(b, part.positions + 12 * k), yw = U32(b, part.positions + 12 * k + 4);
                    if (xw & 1) continue;
                    const uint32_t idx[3] = {k - 2, (yw & 1) ? k - 1 : k, (yw & 1) ? k : k - 1};
                    Ps2Triangle tri;
                    tri.texture = part.texture;
                    for (int c = 0; c < 3; ++c) {
                        Require(vertexOf[pi][idx[c]] >= 0, "a strip vertex has no skin vertex");
                        tri.corners[c].vertex = static_cast<uint32_t>(vertexOf[pi][idx[c]]);
                        tri.corners[c].u = S16(b, part.uvs + 4 * idx[c]) / 4096.0f;
                        tri.corners[c].v = S16(b, part.uvs + 4 * idx[c] + 2) / 4096.0f;
                    }
                    out.tris.push_back(tri);
                }
            }
        }
        return true;
    } catch (const std::exception& e) {
        out = Ps2Mps{};
        reason = std::string("model: ") + e.what();
        return false;
    }
}

bool ParsePs2Aps(const Bytes& b, Ps2Aps& out, std::string& reason) {
    out = Ps2Aps{};
    reason.clear();
    // Groups (count u8 at 0x1c, table at 0x20): u32 clip count, u32 clip array. A clip (0x1c bytes): flags (0x20
    // skeletal), length, track count, tracks (0 for a skeletal clip whose keys are another character's).
    // Skeletal tracks (20 bytes): bone u16, rotation key count u8 at +8, position key count u8 at +9, their arrays at
    // +0xc and +0x10 (rotation keys: time u16, x y z w s16 / 32767; position keys: time u16, x y z s16).
    // Vertex animated tracks (48 bytes): mesh node u32, at +0x20 a block: values, u16 value count, u16 vertex count.
    try {
        const uint8_t ngroups = U8(b, 0x1c);
        const uint32_t groups = U32(b, 0x20);
        const float qs = [] {
            const uint32_t w = 0x38000100;
            float f;
            std::memcpy(&f, &w, 4);
            return f;
        }();
        for (uint32_t g = 0; g < ngroups; ++g) {
            const uint32_t n = U32(b, groups + 8 * g), clips = U32(b, groups + 8 * g + 4);
            Require(n < 256, "an animation group has too many clips");
            for (uint32_t k = 0; k < n; ++k) {
                const uint32_t q = clips + 0x1c * k;
                Ps2Clip clip;
                clip.group = static_cast<int>(g);
                clip.clip = static_cast<int>(k);
                clip.flags = U32(b, q);
                clip.length = U32(b, q + 4);
                Require(clip.length < 10000, "a clip is too long");
                const uint16_t ntracks = U16(b, q + 8);
                const uint32_t tracks = U32(b, q + 0x10);
                clip.skeletal = (clip.flags & 0x20) != 0;
                clip.shared = clip.skeletal && tracks == 0;
                if (tracks != 0 && clip.skeletal) {
                    for (uint32_t t = 0; t < ntracks; ++t) {
                        const uint32_t at = tracks + 20 * t;
                        Ps2BoneTrack track;
                        track.bone = U16(b, at);
                        const uint8_t nq = U8(b, at + 8), np = U8(b, at + 9);
                        const uint32_t qp = U32(b, at + 0xc), pp = U32(b, at + 0x10);
                        for (uint32_t i = 0; qp && i < nq; ++i) {
                            Ps2RotKey key;
                            key.time = U16(b, qp + 10 * i);
                            for (int c = 0; c < 4; ++c) key.q[c] = S16(b, qp + 10 * i + 2 + 2 * c) * qs;
                            track.rot.push_back(key);
                        }
                        for (uint32_t i = 0; pp && i < np; ++i) {
                            Ps2PosKey key;
                            key.time = U16(b, pp + 8 * i);
                            for (int c = 0; c < 3; ++c) key.p[c] = S16(b, pp + 8 * i + 2 + 2 * c);
                            track.pos.push_back(key);
                        }
                        clip.bones.push_back(std::move(track));
                    }
                } else if (tracks != 0) {
                    for (uint32_t t = 0; t < ntracks; ++t) {
                        const uint32_t at = tracks + 48 * t;
                        Ps2NodeTrack track;
                        track.node = U32(b, at);
                        const uint32_t block = U32(b, at + 0x20);
                        const uint32_t values = U32(b, block);
                        const uint16_t count = U16(b, block + 4);
                        track.vertices = U16(b, block + 6);
                        Require(track.vertices > 0 && count % track.vertices == 0, "a vertex animation's sizes do not agree");
                        track.values.resize(3 * static_cast<size_t>(count));
                        Need(b, values, 6 * static_cast<size_t>(count));
                        for (size_t i = 0; i < track.values.size(); ++i) track.values[i] = S16(b, values + 2 * i);
                        clip.nodes.push_back(std::move(track));
                    }
                }
                out.clips.push_back(std::move(clip));
            }
        }
        return true;
    } catch (const std::exception& e) {
        out = Ps2Aps{};
        reason = std::string("animation set: ") + e.what();
        return false;
    }
}

std::vector<float> Ps2Bind(const Ps2Mps& model) {
    std::vector<float> out;
    out.reserve(model.vertices.size() * 3);
    for (const Ps2SkinVertex& v : model.vertices) out.insert(out.end(), v.bind, v.bind + 3);
    return out;
}

std::vector<float> Ps2Pose(const Ps2Mps& model, const Ps2Clip& clip, const std::vector<const Ps2Clip*>& others, float t,
                           int* rigid) {
    std::vector<float> out = Ps2Bind(model);
    if (!clip.skeletal) {
        for (const Ps2NodeTrack& track : clip.nodes) {
            if (track.node >= model.meshes || track.vertices != model.meshVertexCount[track.node]) continue;
            const size_t frames = track.values.size() / 3 / track.vertices;
            if (frames == 0) continue;
            const float clamped = std::min(std::max(t, 0.0f), static_cast<float>(frames - 1));
            const size_t f0 = static_cast<size_t>(clamped), f1 = std::min(f0 + 1, frames - 1);
            const float f = clamped - static_cast<float>(f0);
            for (uint32_t i = 0; i < track.vertices; ++i) {
                const int16_t* a = &track.values[(f0 * track.vertices + i) * 3];
                const int16_t* c = &track.values[(f1 * track.vertices + i) * 3];
                float* dst = &out[(model.meshFirstVertex[track.node] + i) * 3];
                // Strip space (y up) to model space (z up).
                const float v[3] = {a[0] + (c[0] - a[0]) * f, a[1] + (c[1] - a[1]) * f, a[2] + (c[2] - a[2]) * f};
                dst[0] = v[0];
                dst[1] = v[2];
                dst[2] = v[1];
            }
        }
        return out;
    }
    const int nbones = static_cast<int>(model.boneNames.size());
    std::vector<Xform> pose(nbones);
    std::vector<bool> have(nbones, false);
    int fellBack = 0;
    std::vector<bool> used(nbones, false);
    for (const Ps2SkinVertex& v : model.vertices)
        for (const Ps2Influence& inf : v.influences) used[inf.bone] = true;
    for (int bone = 0; bone < nbones; ++bone) {
        if (const Ps2BoneTrack* track = TrackOf(clip, bone)) {
            pose[bone] = Sample(*track, t);
            have[bone] = true;
        }
    }
    for (int bone = 0; bone < nbones; ++bone) {
        if (have[bone] || !used[bone]) continue;
        int ancestor = model.boneParent[bone];
        for (int guard = 0; ancestor >= 0 && !TrackOf(clip, ancestor) && guard < nbones; ++guard)
            ancestor = model.boneParent[ancestor];
        if (ancestor < 0 || !TrackOf(clip, ancestor)) continue;   // nothing to follow: its vertices keep their bind place
        const Ps2BoneTrack* ancestorTrack = TrackOf(clip, ancestor);
        Xform offset;
        bool found = false;
        for (const Ps2Clip* other : others) {
            const Ps2BoneTrack* a = other ? TrackOf(*other, ancestor) : nullptr;
            const Ps2BoneTrack* own = other ? TrackOf(*other, bone) : nullptr;
            if (!a || !own) continue;
            offset = Then(Sample(*own, 0), Inverse(Sample(*a, 0)));
            found = true;
            break;
        }
        if (!found) ++fellBack;
        pose[bone] = Then(offset, Sample(*ancestorTrack, t));
        have[bone] = true;
    }
    for (size_t vi = 0; vi < model.vertices.size(); ++vi) {
        const Ps2SkinVertex& v = model.vertices[vi];
        float acc[3] = {0, 0, 0}, sum = 0;
        for (const Ps2Influence& inf : v.influences) {
            float p[3];
            if (have[inf.bone]) Apply(pose[inf.bone], inf.local, p);
            else std::memcpy(p, v.bind, sizeof p);
            for (int c = 0; c < 3; ++c) acc[c] += inf.weight * p[c];
            sum += inf.weight;
        }
        if (sum > 0)
            for (int c = 0; c < 3; ++c) out[vi * 3 + c] = acc[c] / sum;
    }
    if (rigid) *rigid = fellBack;
    return out;
}

}  // namespace tpw::assets
