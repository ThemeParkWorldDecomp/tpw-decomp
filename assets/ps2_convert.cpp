// ps2_convert.cpp: see ps2_convert.h.
#include "ps2_convert.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>

#include "ps2_disc.h"

namespace tpw::assets {
namespace {

using Bytes = std::vector<uint8_t>;

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool EndsWith(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

bool WriteFile(const std::filesystem::path& path, const Bytes& bytes, std::string& reason) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out) {
        reason = "cannot write " + path.u8string();
        return false;
    }
    return true;
}

int Copies(float span) {
    if (span <= 1.0001f) return 1;
    if (span <= 2.0001f) return 2;
    return 4;
}

// A triangle's texture coordinates moved by whole copies so the smallest are in [0, 1).
void Shift(const Ps2Triangle& t, float uv[6]) {
    const float fu = std::floor(std::min({t.corners[0].u, t.corners[1].u, t.corners[2].u}) + 1e-4f);
    const float fv = std::floor(std::min({t.corners[0].v, t.corners[1].v, t.corners[2].v}) + 1e-4f);
    for (int c = 0; c < 3; ++c) {
        uv[2 * c] = t.corners[c].u - fu;
        uv[2 * c + 1] = t.corners[c].v - fv;
    }
}

}  // namespace

std::filesystem::path Ps2ContentPath(const std::filesystem::path& dataRoot) { return dataRoot / "assets" / "ps2"; }

std::filesystem::path Ps2ConverterStampPath(const std::filesystem::path& dataRoot) {
    return Ps2ContentPath(dataRoot) / "converter.txt";
}

int ReadPs2ConverterStamp(const std::filesystem::path& dataRoot) {
    std::ifstream in(Ps2ConverterStampPath(dataRoot));
    int version = 0;
    if (!(in >> version) || version < 1) return 0;
    return version;
}

bool DecodePs2Tga(const Bytes& tga, Ps2Image& out, std::string& reason) {
    out = Ps2Image{};
    if (tga.size() < 18 || tga[1] != 0 || tga[2] != 2 || (tga[16] != 24 && tga[16] != 32)) {
        reason = "not a 24- or 32-bit uncompressed TGA";
        return false;
    }
    const int w = tga[12] | tga[13] << 8, h = tga[14] | tga[15] << 8, bpp = tga[16] / 8;
    const size_t at = 18 + static_cast<size_t>(tga[0]);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || at + static_cast<size_t>(w) * h * bpp > tga.size()) {
        reason = "a TGA cut short";
        return false;
    }
    const bool top = (tga[17] & 0x20) != 0;
    out.width = w;
    out.height = h;
    out.alpha = bpp == 4;
    out.rgba.resize(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = tga.data() + at + static_cast<size_t>(top ? y : h - 1 - y) * w * bpp;
        uint8_t* dst = out.rgba.data() + static_cast<size_t>(y) * w * 4;
        for (int x = 0; x < w; ++x, src += bpp, dst += 4) {
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = bpp == 4 ? src[3] : 255;
        }
    }
    return true;
}

Bytes EncodePs2Tga(const Ps2Image& image) {
    const int bpp = image.alpha ? 4 : 3;
    Bytes o(18, 0);
    o[2] = 2;
    o[12] = static_cast<uint8_t>(image.width);
    o[13] = static_cast<uint8_t>(image.width >> 8);
    o[14] = static_cast<uint8_t>(image.height);
    o[15] = static_cast<uint8_t>(image.height >> 8);
    o[16] = static_cast<uint8_t>(bpp * 8);
    o[17] = image.alpha ? 8 : 0;
    for (int y = image.height - 1; y >= 0; --y) {
        const uint8_t* src = image.rgba.data() + static_cast<size_t>(y) * image.width * 4;
        for (int x = 0; x < image.width; ++x, src += 4) {
            o.push_back(src[2]);
            o.push_back(src[1]);
            o.push_back(src[0]);
            if (bpp == 4) o.push_back(src[3]);
        }
    }
    return o;
}

Ps2Image TilePs2Image(const Ps2Image& image, int across, int down) {
    Ps2Image out;
    out.alpha = image.alpha;
    out.width = image.width * across;
    out.height = image.height * down;
    out.rgba.resize(static_cast<size_t>(out.width) * out.height * 4);
    for (int y = 0; y < out.height; ++y)
        for (int x = 0; x < out.width; ++x)
            std::copy_n(image.rgba.data() + (static_cast<size_t>(y % image.height) * image.width + x % image.width) * 4, 4,
                        out.rgba.data() + (static_cast<size_t>(y) * out.width + x) * 4);
    while ((out.width > 256 || out.height > 256) && out.width > 1 && out.height > 1) {
        Ps2Image half;
        half.alpha = out.alpha;
        half.width = out.width / 2;
        half.height = out.height / 2;
        half.rgba.resize(static_cast<size_t>(half.width) * half.height * 4);
        for (int y = 0; y < half.height; ++y)
            for (int x = 0; x < half.width; ++x)
                for (int c = 0; c < 4; ++c) {
                    int sum = 0;
                    for (int dy = 0; dy < 2; ++dy)
                        for (int dx = 0; dx < 2; ++dx) sum += out.rgba[(static_cast<size_t>(2 * y + dy) * out.width + 2 * x + dx) * 4 + c];
                    half.rgba[(static_cast<size_t>(y) * half.width + x) * 4 + c] = static_cast<uint8_t>((sum + 2) / 4);
                }
        out = std::move(half);
    }
    return out;
}

void MergePs2Tiling(const Ps2Mps& model, Ps2Tiling& tiling) {
    for (const Ps2Triangle& t : model.tris) {
        float uv[6];
        Shift(t, uv);
        auto& copies = tiling[t.texture];
        copies.first = std::max({copies.first, 1, Copies(std::max({uv[0], uv[2], uv[4]}))});
        copies.second = std::max({copies.second, 1, Copies(std::max({uv[1], uv[3], uv[5]}))});
    }
}

bool BuildPs2Pack(const Ps2Mps& model, const Ps2Aps& anims, const std::vector<Ps2Donor>& donors, const Ps2Tiling& tiling,
                  Ps2Pack& out, std::vector<std::string>& notes, std::string& reason) {
    out = Ps2Pack{};
    reason.clear();
    if (model.vertices.empty() || model.vertices.size() > 65535 || model.tris.empty()) {
        reason = "the model has no vertices or triangles, or too many vertices";
        return false;
    }
    out.vertices = static_cast<uint32_t>(model.vertices.size());
    for (const Ps2Triangle& t : model.tris) {
        auto it = std::find(out.textures.begin(), out.textures.end(), t.texture);
        if (it == out.textures.end()) it = out.textures.insert(out.textures.end(), t.texture);
        Ps2PackTriangle p;
        for (int c = 0; c < 3; ++c) p.vertex[c] = static_cast<uint16_t>(t.corners[c].vertex);
        p.texture = static_cast<uint16_t>(it - out.textures.begin());
        Shift(t, p.uv);
        const auto copies = tiling.count(t.texture) ? tiling.at(t.texture) : std::make_pair(1, 1);
        for (int c = 0; c < 3; ++c) {
            p.uv[2 * c] = std::min(1.0f, p.uv[2 * c] / static_cast<float>(copies.first));
            p.uv[2 * c + 1] = std::min(1.0f, p.uv[2 * c + 1] / static_cast<float>(copies.second));
        }
        out.triangles.push_back(p);
    }
    // The clips with keys: a shared one takes the first donor's same clip.
    std::vector<Ps2Clip> clips;
    std::vector<uint8_t> flags;
    for (const Ps2Clip& clip : anims.clips) {
        if (clip.skeletal && clip.shared) {
            const Ps2Clip* from = nullptr;
            std::string name;
            for (const Ps2Donor& d : donors) {
                if (!d.anims || !d.model || d.model->boneNames.size() != model.boneNames.size()) continue;
                for (const Ps2Clip& c : d.anims->clips)
                    if (c.group == clip.group && c.clip == clip.clip && c.skeletal && !c.shared && !c.bones.empty() &&
                        c.length == clip.length)
                        from = &c;
                if (from) {
                    name = d.name;
                    break;
                }
            }
            if (!from) {
                notes.push_back("g" + std::to_string(clip.group) + " c" + std::to_string(clip.clip) + ": shared keys, no donor; left out");
                continue;
            }
            Ps2Clip copy = *from;
            copy.shared = true;
            clips.push_back(copy);
            flags.push_back(kPs2ClipShared);
            notes.push_back("g" + std::to_string(clip.group) + " c" + std::to_string(clip.clip) + ": keys from " + name);
        } else if ((clip.skeletal && !clip.bones.empty()) || (!clip.skeletal && !clip.nodes.empty())) {
            clips.push_back(clip);
            flags.push_back(clip.skeletal ? 0 : kPs2ClipVertex);
        }
    }
    std::vector<const Ps2Clip*> others;
    for (const Ps2Clip& c : clips)
        if (c.skeletal) others.push_back(&c);
    const std::vector<float> bind = Ps2Bind(model);
    std::vector<std::vector<float>> baked;
    float extent = 1;
    for (float v : bind) extent = std::max(extent, std::fabs(v));
    for (size_t i = 0; i < clips.size(); ++i) {
        const Ps2Clip& clip = clips[i];
        Ps2PackClip pc;
        pc.group = static_cast<uint8_t>(clip.group);
        pc.clip = static_cast<uint8_t>(clip.clip);
        pc.length = static_cast<uint16_t>(std::min<uint32_t>(clip.length, 65534));
        pc.frames = static_cast<uint16_t>(pc.length + 1);
        if (!clip.skeletal) {
            size_t frames = 65535;
            for (const Ps2NodeTrack& t : clip.nodes) frames = std::min(frames, t.values.size() / 3 / t.vertices);
            pc.frames = static_cast<uint16_t>(std::max<size_t>(1, frames));
        }
        int rigid = 0;
        std::vector<float> all;
        for (uint32_t f = 0; f < pc.frames; ++f) {
            int r = 0;
            const std::vector<float> pose = Ps2Pose(model, clip, others, static_cast<float>(f), &r);
            rigid = std::max(rigid, r);
            all.insert(all.end(), pose.begin(), pose.end());
        }
        for (float v : all) extent = std::max(extent, std::fabs(v));
        if (rigid > 0) {
            pc.flags |= kPs2ClipRigid;
            notes.push_back("g" + std::to_string(clip.group) + " c" + std::to_string(clip.clip) + ": " + std::to_string(rigid) +
                            " bone(s) with no keys moved rigidly with an ancestor");
        }
        pc.flags |= flags[i];
        out.clips.push_back(pc);
        baked.push_back(std::move(all));
    }
    out.scale = extent / 32767.0f;
    const auto store = [&](float v) { return static_cast<int16_t>(std::lround(std::max(-32767.0f, std::min(32767.0f, v / out.scale)))); };
    for (int c = 0; c < 3; ++c) {
        out.bounds[c] = bind[c];
        out.bounds[3 + c] = bind[c];
    }
    for (size_t i = 0; i < bind.size(); ++i) {
        out.bind.push_back(store(bind[i]));
        out.bounds[i % 3] = std::min(out.bounds[i % 3], bind[i]);
        out.bounds[3 + i % 3] = std::max(out.bounds[3 + i % 3], bind[i]);
    }
    for (const auto& all : baked)
        for (float v : all) out.frames.push_back(store(v));
    return true;
}

bool ConvertPs2(const Ps2Disc& disc, const std::filesystem::path& dataRoot, std::vector<std::string>& log, std::string& reason) {
    reason.clear();
    struct Character {
        std::string name;
        Ps2Mps model;
        Ps2Aps anims;
    };
    std::vector<Character> characters;
    for (const Ps2Character& ch : Ps2Characters()) {
        Character c;
        c.name = Lower(ch.folder);
        const std::string folder = "chars/" + c.name + "/";
        const Bytes *mps = nullptr, *aps = nullptr;
        for (const auto& [path, bytes] : disc.chars) {
            const std::string low = Lower(path);
            if (low.compare(0, folder.size(), folder) != 0) continue;
            if (EndsWith(low, ".mps")) mps = &bytes;
            if (EndsWith(low, ".aps")) aps = &bytes;
        }
        if (!mps || !aps) {
            reason = ch.folder + std::string(": the model or its animations are missing");
            return false;
        }
        std::string err;
        if (!ParsePs2Mps(*mps, c.model, err) || !ParsePs2Aps(*aps, c.anims, err)) {
            reason = ch.folder + std::string(": ") + err;
            return false;
        }
        characters.push_back(std::move(c));
    }
    Ps2Tiling tiling;
    for (const Character& c : characters) MergePs2Tiling(c.model, tiling);
    std::vector<Ps2Donor> donors;
    for (const Character& c : characters) donors.push_back({c.name, &c.model, &c.anims});

    const std::filesystem::path content = Ps2ContentPath(dataRoot);
    const std::filesystem::path staging = content.parent_path() / "ps2.converting";
    std::error_code ec;
    std::filesystem::remove_all(staging, ec);
    std::filesystem::create_directories(staging / "tex", ec);
    if (ec) {
        reason = "cannot make " + staging.u8string() + ": " + ec.message();
        return false;
    }
    for (const auto& [texture, copies] : tiling) {
        // The disc's TGA of the same stem: Chars/Textures/<stem>.tga, or in a character's own folder (the dinosaur's).
        const Bytes* tga = FindPs2File(disc.chars, "Chars/Textures/" + texture + ".tga");
        for (const auto& [path, bytes] : disc.chars)
            if (!tga && EndsWith(Lower(path), "/" + texture + ".tga")) tga = &bytes;
        Ps2Image image;
        std::string err;
        if (!tga) {
            // A few materials name a texture the disc does not have (the game draws them untextured): plain grey.
            image.width = image.height = 1;
            image.rgba = {160, 160, 160, 255};
            log.push_back("texture " + texture + ": not on the disc, plain grey");
        } else if (!DecodePs2Tga(*tga, image, err)) {
            reason = "texture " + texture + ": " + err;
            return false;
        }
        if (!WriteFile(staging / "tex" / (texture + ".tga"), EncodePs2Tga(TilePs2Image(image, copies.first, copies.second)), reason))
            return false;
    }
    for (const Character& c : characters) {
        Ps2Pack pack;
        std::vector<std::string> notes;
        if (!BuildPs2Pack(c.model, c.anims, donors, tiling, pack, notes, reason)) {
            reason = c.name + ": " + reason;
            return false;
        }
        if (!WriteFile(staging / (c.name + ".p2m"), WritePs2Pack(pack), reason)) return false;
        std::string line = c.name + ": " + std::to_string(pack.vertices) + " vertices, " + std::to_string(pack.triangles.size()) +
                           " triangles, " + std::to_string(pack.textures.size()) + " textures, " + std::to_string(pack.clips.size()) + " clips";
        for (const std::string& n : notes) line += "; " + n;
        log.push_back(line);
    }
    std::filesystem::remove_all(content, ec);
    std::filesystem::rename(staging, content, ec);
    if (ec) {
        reason = "cannot move the converted models into " + content.u8string() + ": " + ec.message();
        return false;
    }
    const std::filesystem::path stamp = Ps2ConverterStampPath(dataRoot);
    const std::string text = std::to_string(kPs2ConverterVersion) + "\n";
    if (!WriteFile(stamp.u8string() + ".tmp", Bytes(text.begin(), text.end()), reason)) return false;
    std::filesystem::rename(stamp.u8string() + ".tmp", stamp, ec);
    if (ec) {
        reason = "cannot write " + stamp.u8string() + ": " + ec.message();
        return false;
    }
    return true;
}

}  // namespace tpw::assets
