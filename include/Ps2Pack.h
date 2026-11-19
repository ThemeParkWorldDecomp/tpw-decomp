// Ps2Pack.h: the PS2 Guest Models mod's model pack, one file per PS2 character (tpw-mods ps2-characters). The
// launcher's converter (assets/ps2_convert.h) writes them from the player's own PS2 disc into the data folder's
// assets/ps2/<character>.p2m, with the textures in assets/ps2/tex/; the game reads them (include/Ps2Characters.h).
// Header only, no game data: the layout and its reader, shared by both sides.
//
// Layout (little endian):
//   header, 64 bytes: "TP2M", u32 version (1), u32 vertex count, u32 triangle count, u32 texture count, u32 clip count,
//     f32 scale (a stored position times scale is the PS2's units), f32 bind bounds[6] (min x y z, max x y z, in the
//     PS2's units), u32 offset of the bind frame, 8 bytes zero;
//   textures: 32 bytes each, the texture's file name in tex/ without ".tga", zero padded;
//   triangles: 32 bytes each, u16 vertex[3], u16 texture, f32 u, v per corner (0..1 across the texture as written,
//     v 0 at the image's top row);
//   clips: 12 bytes each, u8 PS2 animation group, u8 clip in it, u8 flags (kPs2ClipVertex: a vertex animated clip;
//     kPs2ClipShared: the keys were another character's; kPs2ClipRigid: a bone with no keys moved with an ancestor),
//     u8 0, u16 length (frames of 1/30 s), u16 frame count (length + 1, the last the loop's end), u32 offset;
//   frames: per frame, per vertex, s16 x, y, z: model space, x and y on the ground, z up.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace tpw {

constexpr uint32_t kPs2PackVersion = 1;
constexpr uint8_t kPs2ClipVertex = 1;
constexpr uint8_t kPs2ClipShared = 2;
constexpr uint8_t kPs2ClipRigid = 4;

struct Ps2PackTriangle {
    uint16_t vertex[3] = {0, 0, 0};
    uint16_t texture = 0;
    float uv[6] = {0, 0, 0, 0, 0, 0};
};

struct Ps2PackClip {
    uint8_t group = 0, clip = 0, flags = 0;
    uint16_t length = 0, frames = 0;
    uint32_t offset = 0;
};

struct Ps2Pack {
    uint32_t vertices = 0;
    float scale = 1;
    float bounds[6] = {0, 0, 0, 0, 0, 0};
    std::vector<std::string> textures;
    std::vector<Ps2PackTriangle> triangles;
    std::vector<Ps2PackClip> clips;
    std::vector<int16_t> bind;      // vertices * 3
    std::vector<int16_t> frames;    // every clip's frames, clip.offset counted in int16s from the start of this
};

namespace ps2pack_detail {
inline uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }
inline uint16_t U16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
inline float F32(const uint8_t* p) {
    const uint32_t w = U32(p);
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}
inline void Put32(std::vector<uint8_t>& o, uint32_t v) {
    for (int i = 0; i < 4; ++i) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
inline void Put16(std::vector<uint8_t>& o, uint16_t v) {
    o.push_back(static_cast<uint8_t>(v));
    o.push_back(static_cast<uint8_t>(v >> 8));
}
inline void PutF(std::vector<uint8_t>& o, float f) {
    uint32_t w;
    std::memcpy(&w, &f, 4);
    Put32(o, w);
}
}  // namespace ps2pack_detail

// The file's bytes. Clip offsets are taken from the clips' order: each clip's frames follow the previous one's.
inline std::vector<uint8_t> WritePs2Pack(Ps2Pack pack) {
    using namespace ps2pack_detail;
    std::vector<uint8_t> o;
    const size_t head = 64 + 32 * pack.textures.size() + 32 * pack.triangles.size() + 12 * pack.clips.size();
    const size_t bindAt = head;
    size_t at = bindAt + 2 * pack.bind.size(), frame = 0;
    for (Ps2PackClip& c : pack.clips) {
        c.offset = static_cast<uint32_t>(at);
        const size_t n = static_cast<size_t>(c.frames) * pack.vertices * 3;
        at += 2 * n;
        frame += n;
    }
    o.insert(o.end(), {'T', 'P', '2', 'M'});
    Put32(o, kPs2PackVersion);
    Put32(o, pack.vertices);
    Put32(o, static_cast<uint32_t>(pack.triangles.size()));
    Put32(o, static_cast<uint32_t>(pack.textures.size()));
    Put32(o, static_cast<uint32_t>(pack.clips.size()));
    PutF(o, pack.scale);
    for (float b : pack.bounds) PutF(o, b);
    Put32(o, static_cast<uint32_t>(bindAt));
    Put32(o, 0);
    Put32(o, 0);
    for (const std::string& t : pack.textures) {
        char name[32] = {};
        std::memcpy(name, t.data(), t.size() < 31 ? t.size() : 31);
        o.insert(o.end(), name, name + 32);
    }
    for (const Ps2PackTriangle& t : pack.triangles) {
        for (uint16_t v : t.vertex) Put16(o, v);
        Put16(o, t.texture);
        for (float f : t.uv) PutF(o, f);
    }
    for (const Ps2PackClip& c : pack.clips) {
        o.push_back(c.group);
        o.push_back(c.clip);
        o.push_back(c.flags);
        o.push_back(0);
        Put16(o, c.length);
        Put16(o, c.frames);
        Put32(o, c.offset);
    }
    for (int16_t v : pack.bind) Put16(o, static_cast<uint16_t>(v));
    for (size_t i = 0; i < frame && i < pack.frames.size(); ++i) Put16(o, static_cast<uint16_t>(pack.frames[i]));
    return o;
}

// False with a reason when the bytes are not a whole pack of this version. On success each clip's offset is counted in
// int16s into `frames`.
inline bool ReadPs2Pack(const uint8_t* data, size_t size, Ps2Pack& out, std::string& reason) {
    using namespace ps2pack_detail;
    out = Ps2Pack{};
    if (size < 64 || std::memcmp(data, "TP2M", 4) != 0) {
        reason = "not a PS2 model pack";
        return false;
    }
    if (U32(data + 4) != kPs2PackVersion) {
        reason = "a PS2 model pack of another version";
        return false;
    }
    const uint64_t nv = U32(data + 8), nt = U32(data + 12), nx = U32(data + 16), nc = U32(data + 20);
    if (nv == 0 || nv > 65535 || nt > 65535 || nx > 1024 || nc > 1024) {
        reason = "a PS2 model pack with impossible counts";
        return false;
    }
    out.vertices = static_cast<uint32_t>(nv);
    out.scale = F32(data + 24);
    for (int i = 0; i < 6; ++i) out.bounds[i] = F32(data + 28 + 4 * i);
    const uint64_t bindAt = U32(data + 52);
    const uint64_t head = 64 + 32 * nx + 32 * nt + 12 * nc;
    if (head > size || bindAt < head || bindAt + 6 * nv > size) {
        reason = "a PS2 model pack cut short";
        return false;
    }
    const uint8_t* p = data + 64;
    for (uint64_t i = 0; i < nx; ++i, p += 32) {
        size_t n = 0;
        while (n < 31 && p[n] != 0) ++n;
        out.textures.emplace_back(reinterpret_cast<const char*>(p), n);
    }
    for (uint64_t i = 0; i < nt; ++i, p += 32) {
        Ps2PackTriangle t;
        for (int c = 0; c < 3; ++c) t.vertex[c] = U16(p + 2 * c);
        t.texture = U16(p + 6);
        for (int c = 0; c < 6; ++c) t.uv[c] = F32(p + 8 + 4 * c);
        if (t.vertex[0] >= nv || t.vertex[1] >= nv || t.vertex[2] >= nv || t.texture >= nx) {
            reason = "a PS2 model pack's triangle names a vertex or texture it does not have";
            return false;
        }
        out.triangles.push_back(t);
    }
    const uint64_t framesAt = bindAt + 6 * nv;
    for (uint64_t i = 0; i < nc; ++i, p += 12) {
        Ps2PackClip c;
        c.group = p[0];
        c.clip = p[1];
        c.flags = p[2];
        c.length = U16(p + 4);
        c.frames = U16(p + 6);
        const uint64_t at = U32(p + 8);
        if (c.frames == 0 || at < framesAt || (at - framesAt) % 2 != 0 || at + 6 * nv * c.frames > size) {
            reason = "a PS2 model pack's clip is past its end";
            return false;
        }
        c.offset = static_cast<uint32_t>((at - framesAt) / 2);
        out.clips.push_back(c);
    }
    out.bind.resize(3 * nv);
    for (uint64_t i = 0; i < 3 * nv; ++i) out.bind[i] = static_cast<int16_t>(U16(data + bindAt + 2 * i));
    const uint64_t rest = (size - framesAt) / 2;
    out.frames.resize(rest);
    for (uint64_t i = 0; i < rest; ++i) out.frames[i] = static_cast<int16_t>(U16(data + framesAt + 2 * i));
    return true;
}

}  // namespace tpw
