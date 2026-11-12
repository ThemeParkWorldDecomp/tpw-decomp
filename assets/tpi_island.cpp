// tpi_island.cpp: the lobby islands of the Theme Park Inc worlds (tpi_island.h). The recipe per world is a table at
// the top; the geometry we generate is the slab and the fence (tpi_island_md2.h NodeBuilder), everything else is the
// world's own pieces and textures, scaled and placed.
#include "tpi_island.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>

#include "../platform/path_text.h"
#include "tpi_fsh.h"
#include "tpi_island_md2.h"
#include "tpi_wad.h"

namespace tpw::assets {

namespace {

using namespace tpi_island;

constexpr float kPi = 3.14159265f;
// The slab: the top at kTop, a ledge between the two cliff bands, the foot under the water line. The outline is an
// ellipse with a little waviness, deeper towards the camera (-z) as the originals' islands are, centred at z kCentreZ.
constexpr float kTop = 9.0f, kLedge = 5.5f, kFoot = 2.0f, kCentreZ = -9.0f, kHalfX = 25.0f, kHalfZ = 31.0f;
constexpr int kSegments = 16;
// The gate stands at the front, at least this wide and this tall (the originals' gates are 5 to 6 units each way).
constexpr float kGateWidth = 7.0f, kGateHeight = 6.0f, kGateZ = -14.0f;
// Each piece stands this far above the slab top, and this far again above each earlier piece its footprint overlaps
// (TPW_MOD_HOOK tpi_islands, PlacePiece). Theme Park Inc's pieces carry their own ground tiles at their lowest point
// (the yeti's base4x5, the fir tree's snow square, the fountain's base), so a piece set right on the top lays them in
// the slab's own plane, and two overlapping pieces in each other's: the depth test then picks a different face each
// frame and the ground flashes (bug H8). The originals' islands never stack ground: their tiles lie side by side in
// one plane. The lobby's projection has its near plane at 0.1 (LBLobbyView.cpp) and a 24-bit depth buffer, so one
// depth step is about d * d / (0.1 * 2^24) units at distance d: 0.003 at the camera's orbit (70), 0.037 where the fog
// ends (250). 0.06 clears that everywhere the island can be seen and is too small to see as a gap.
constexpr float kGroundLift = 0.06f;

struct Pick {
    const char* piece;   // the wad's name under features/ or rides/ (case does not matter); nullptr ends a list
    float height;        // the piece is scaled to stand this tall
    float x, z;          // where its footprint's centre goes on the slab
    float facing;        // turn about y, degrees (0 keeps the piece's own -z front)
};

struct Recipe {
    const char* world;    // the mod's world name (the descriptor's file name and folder)
    const char* zone;     // the folder under Data/levels in the player's copy
    const char* prefix;   // the meshes' prefix: <prefix>_isle, <prefix>_gate
    const char* title;    // the island's title in the lobby
    const char* ground, *cliff, *rock, *post, *rail;   // texture stems from the zone's terrain.wad
    Pick landmark;
    Pick scenery[6];
    uint8_t sky[3];
    int lightning, rainy;
    float y;              // the descriptor's last number (the originals use 12.5 to 38)
};

const Recipe kRecipes[] = {
    {"arabian", "arabian", "ara", "Arabian Nights", "sanded01", "cliffblend", "rock1", "post1", "post1",
     {"kingtut", 22.0f, 0.0f, 6.0f, 0.0f},
     {{"ltree", 12.0f, -16.0f, 10.0f, 0.0f}, {"ltree", 12.0f, 16.0f, -4.0f, 40.0f}, {"lrock", 4.0f, 14.0f, -22.0f, 0.0f},
      {"fountain", 6.0f, -12.0f, -20.0f, 0.0f}, {nullptr, 0, 0, 0, 0}, {nullptr, 0, 0, 0, 0}},
     {236, 178, 120}, 0, 0, 15.0f},
    {"water", "water", "pol", "Polar Zone", "top1", "rock2", "rock1", "logs02", "logs02",
     {"yeti", 22.0f, 0.0f, 6.0f, 0.0f},
     {{"penguins", 10.0f, -12.0f, -20.0f, 0.0f}, {"ltree", 12.0f, -16.0f, 10.0f, 0.0f}, {"ltree", 12.0f, 16.0f, -4.0f, 40.0f},
      {"sealout", 9.0f, 14.0f, -22.0f, 20.0f}, {nullptr, 0, 0, 0, 0}, {nullptr, 0, 0, 0, 0}},
     {150, 190, 230}, 40, 1, 15.0f},
    {"science", "science", "inv", "Land of Invention", "cobbles04", "cliffblend", "rock1", "fence01", "fence01",
     {"tesla", 24.0f, 0.0f, 6.0f, 0.0f},
     {{"instal2", 8.0f, -12.0f, -20.0f, 0.0f}, {"rock_l", 5.0f, 14.0f, -22.0f, 0.0f}, {"fount", 6.0f, 12.0f, -10.0f, 0.0f},
      {"install1", 10.0f, -16.0f, 10.0f, 0.0f}, {"wcumb", 5.0f, 16.0f, -4.0f, 0.0f}, {nullptr, 0, 0, 0, 0}},
     {90, 60, 110}, 0, 0, 15.0f},
};

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
std::string Stem(const std::string& fileName) {
    const size_t slash = fileName.find_last_of('/');
    const std::string base = slash == std::string::npos ? fileName : fileName.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    return dot == std::string::npos ? base : base.substr(0, dot);
}
std::string Folder(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}
std::string Num(float f) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", static_cast<double>(f));
    return buf;
}

// ---- the zone's files -------------------------------------------------------------------------------------------------

// Texture layers in priority order: each maps a lower-case stem to a member's bytes.
class TextureLayers {
public:
    void Add(const std::vector<TpiWadMember>& members, const std::string& folderLower) {
        std::map<std::string, const std::vector<uint8_t>*> layer;
        for (const TpiWadMember& m : members) {
            const std::string lower = Lower(m.name);
            if (!(EndsWith(lower, ".fsh") || EndsWith(lower, ".tga"))) continue;
            if (Folder(lower) != folderLower) continue;
            layer.emplace(Stem(lower), &m.data);
        }
        layers_.push_back(std::move(layer));
    }
    const std::vector<uint8_t>* Find(const std::string& stemLower) const {
        for (const auto& layer : layers_) {
            auto it = layer.find(stemLower);
            if (it != layer.end()) return it->second;
        }
        return nullptr;
    }
    void AddWad(const std::vector<TpiWadMember>& members) {
        Add(members, "textures");
        Add(members, "");
        Add(members, "stexture");
        Add(members, "pathtex");
        Add(members, "proc_tex");   // the terrain's own height band textures (base, mid, top)
    }

private:
    std::vector<std::map<std::string, const std::vector<uint8_t>*>> layers_;
};

struct Zone {
    const Recipe* recipe = nullptr;
    std::shared_ptr<Source> source;
    std::string prefixLower;                            // "<root>data/levels/<zone>/"
    std::map<std::string, const Entry*> entries;        // lower-case path under the zone folder
    std::vector<TpiWadMember> terrain, sharetex, gates;  // kept alive for the layers
    std::vector<std::unique_ptr<std::vector<TpiWadMember>>> keep;   // the pieces' members, referenced by resolved
    TextureLayers layers;                                // the zone's own: sharetex, terrain
    // Stem (lower case, as the meshes will name it) to the bytes found for it (nullptr: not found).
    std::map<std::string, const std::vector<uint8_t>*> resolved;
    std::vector<std::string>* log = nullptr;
    // The pieces placed so far: footprint (x0, z0, x1, z1) and how many kGroundLift steps above the slab top.
    struct Footprint {
        float x0, z0, x1, z1;
        int layer;
    };
    std::vector<Footprint> placed;

    void Log(const std::string& line) const { log->push_back(std::string(recipe->world) + ": " + line); }

    const Entry* Find(const std::string& relativeLower) const {
        auto it = entries.find(relativeLower);
        return it == entries.end() ? nullptr : it->second;
    }
    const Entry* FindWad(const std::string& nameLower) const {
        if (const Entry* e = Find("features/" + nameLower + ".wad")) return e;
        if (const Entry* e = Find("rides/" + nameLower + ".wad")) return e;
        for (const auto& [path, entry] : entries)
            if (EndsWith(path, "/" + nameLower + ".wad")) return entry;
        return nullptr;
    }
    bool ReadWad(const Entry* entry, std::vector<TpiWadMember>& members, std::string& err) const {
        std::unique_ptr<Stream> stream = source->Open(*entry, err);
        if (!stream) return false;
        std::vector<uint8_t> bytes;
        if (!ReadAll(*stream, 512ull << 20, bytes, err)) return false;
        return ReadTpiWad(bytes, members, err);
    }
    // Resolves a texture stem for a mesh, looking in the piece's own layers first; records the outcome once.
    void Resolve(const std::string& outStem, const std::string& sourceStem, const TextureLayers* own) {
        if (resolved.count(outStem)) return;
        const std::vector<uint8_t>* bytes = own ? own->Find(sourceStem) : nullptr;
        if (!bytes) bytes = layers.Find(sourceStem);
        resolved[outStem] = bytes;
        if (!bytes) Log("texture " + sourceStem + " not found, grey stand-in written as " + outStem + ".tga");
    }
};

// Finds the mesh of a piece's wad: the root member named after the piece, else the first root .md2 that reads.
const TpiWadMember* FindMesh(const std::vector<TpiWadMember>& members, const std::string& pieceLower) {
    const TpiWadMember* fallback = nullptr;
    for (const TpiWadMember& m : members) {
        const std::string lower = Lower(m.name);
        if (lower.find('/') != std::string::npos || !EndsWith(lower, ".md2")) continue;
        if (Stem(lower) == pieceLower) return &m;
        if (!fallback && !EndsWith(Stem(lower), "m1") && !EndsWith(Stem(lower), "m2")) fallback = &m;
    }
    return fallback;
}
const TpiWadMember* FindMember(const std::vector<TpiWadMember>& members, const std::string& nameLower) {
    for (const TpiWadMember& m : members)
        if (Lower(m.name) == nameLower) return &m;
    return nullptr;
}

// ---- the slab and the fence ----------------------------------------------------------------------------------------

// Adds a quad wound so that its normal points away from `awayFrom`.
void QuadOut(NodeBuilder& b, const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const float (&u)[4],
             const float (&v)[4], uint16_t tex, const Vec3& awayFrom) {
    const Vec3 e1{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z}, e2{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
    const Vec3 n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
    const Vec3 c{(p0.x + p1.x + p2.x + p3.x) / 4 - awayFrom.x, (p0.y + p1.y + p2.y + p3.y) / 4 - awayFrom.y,
                 (p0.z + p1.z + p2.z + p3.z) / 4 - awayFrom.z};
    if (n.x * c.x + n.y * c.y + n.z * c.z >= 0) b.Quad(p0, p1, p2, p3, u[0], v[0], u[1], v[1], u[2], v[2], u[3], v[3], tex);
    else b.Quad(p0, p3, p2, p1, u[0], v[0], u[3], v[3], u[2], v[2], u[1], v[1], tex);
}

Vec3 Outline(int i, float scale, float y) {
    const float a = 2 * kPi * static_cast<float>(i) / kSegments;
    const float wave = 1.0f + 0.10f * std::sin(3 * a + 1.3f) + 0.06f * std::cos(5 * a + 0.4f);
    return {kHalfX * std::sin(a) * wave * scale, y, kCentreZ - kHalfZ * std::cos(a) * wave * scale};
}

// A bar from `from` to `to` with a square section of half size `half`, four long faces.
void Bar(NodeBuilder& b, const Vec3& from, const Vec3& to, float half, uint16_t tex) {
    Vec3 d{to.x - from.x, 0, to.z - from.z};
    const float len = std::sqrt(d.x * d.x + d.z * d.z);
    if (len < 1e-6f) return;
    d.x /= len;
    d.z /= len;
    const Vec3 side{d.z * half, 0, -d.x * half}, up{0, half, 0};
    const Vec3 centre{(from.x + to.x) / 2, (from.y + to.y) / 2, (from.z + to.z) / 2};
    auto P = [&](const Vec3& base, float s, float u) { return Vec3{base.x + side.x * s, base.y + up.y * u, base.z + side.z * s}; };
    const float u[4] = {0, len / 4, len / 4, 0}, v[4] = {0, 0, 1, 1};
    QuadOut(b, P(from, 1, 1), P(to, 1, 1), P(to, -1, 1), P(from, -1, 1), u, v, tex, centre);       // top
    QuadOut(b, P(from, 1, -1), P(to, 1, -1), P(to, -1, -1), P(from, -1, -1), u, v, tex, centre);   // bottom
    QuadOut(b, P(from, 1, -1), P(to, 1, -1), P(to, 1, 1), P(from, 1, 1), u, v, tex, centre);       // one side
    QuadOut(b, P(from, -1, -1), P(to, -1, -1), P(to, -1, 1), P(from, -1, 1), u, v, tex, centre);   // other side
}

// Texture slots of the slab model: 0 ground, 1 cliff, 2 rock, 3 post, 4 rail.
Md2Model MakeSlab(const Recipe& r) {
    Md2Model model;
    model.name = std::string(r.prefix) + "_isle.md2";
    model.flags = 1;
    const char* stems[5] = {r.ground, r.cliff, r.rock, r.post, r.rail};
    uint16_t slot[5];
    for (int i = 0; i < 5; ++i) {
        const std::string name = Lower(stems[i]) + ".tga";
        size_t found = model.textures.size();
        for (size_t t = 0; t < model.textures.size(); ++t)
            if (model.textures[t].frames[0] == name) found = t;
        if (found == model.textures.size()) {
            Md2Texture tex;
            tex.frames.push_back(name);
            model.textures.push_back(tex);
        }
        slot[i] = static_cast<uint16_t>(found);
    }
    NodeBuilder b;
    const Vec3 axisTop{0, kTop, kCentreZ}, below{0, -100.0f, kCentreZ};
    for (int i = 0; i < kSegments; ++i) {
        const Vec3 p = Outline(i, 1.0f, kTop), q = Outline(i + 1, 1.0f, kTop);
        // The top: a fan from the centre, the ground texture tiled every 10 units.
        const float uT[4] = {axisTop.x / 10, p.x / 10, q.x / 10, q.x / 10}, vT[4] = {axisTop.z / 10, p.z / 10, q.z / 10, q.z / 10};
        QuadOut(b, axisTop, p, q, q, uT, vT, slot[0], below);
        // The cliff bands: three tiles of texture per segment.
        const Vec3 pl = Outline(i, 0.96f, kLedge), ql = Outline(i + 1, 0.96f, kLedge);
        const Vec3 pf = Outline(i, 0.90f, kFoot), qf = Outline(i + 1, 0.90f, kFoot);
        const float u[4] = {3.0f * i, 3.0f * (i + 1), 3.0f * (i + 1), 3.0f * i}, v[4] = {0, 0, 1, 1};
        QuadOut(b, p, q, ql, pl, u, v, slot[1], Vec3{0, (kTop + kLedge) / 2, kCentreZ});
        QuadOut(b, pl, ql, qf, pf, u, v, slot[2], Vec3{0, (kLedge + kFoot) / 2, kCentreZ});
        // The ledge between the bands, in the cliff texture.
        const float vL[4] = {0, 0, 0.3f, 0.3f};
        QuadOut(b, pl, ql, Outline(i + 1, 0.96f, kLedge - 0.001f), Outline(i, 0.96f, kLedge - 0.001f), u, vL, slot[1], below);
    }
    // The fence: posts inset from the edge, two rails, a gap of two segments at the front for the gate.
    std::vector<Vec3> posts;
    for (int i = 0; i < kSegments; ++i) posts.push_back(Outline(i, 0.93f, kTop));
    for (int i = 0; i < kSegments; ++i) {
        if (i == 0) continue;   // the gate's place
        const Vec3& p = posts[static_cast<size_t>(i)];
        b.Box(Vec3{p.x - 0.25f, kTop - 0.3f, p.z - 0.25f}, Vec3{p.x + 0.25f, kTop + 2.2f, p.z + 0.25f}, 2.5f, slot[3]);
    }
    for (int i = 0; i < kSegments; ++i) {
        const int j = (i + 1) % kSegments;
        if (i == 0 || j == 0) continue;
        for (float h : {0.9f, 1.8f}) {
            const Vec3 a{posts[static_cast<size_t>(i)].x, kTop + h, posts[static_cast<size_t>(i)].z};
            const Vec3 c{posts[static_cast<size_t>(j)].x, kTop + h, posts[static_cast<size_t>(j)].z};
            Bar(b, a, c, 0.12f, slot[4]);
        }
    }
    Md2Node root = b.Build("Island");
    root.parent = -1;
    root.matrix = IdentityMatrix();
    model.nodes.push_back(std::move(root));
    RecomputeBoxes(model);
    return model;
}

// ---- pieces -----------------------------------------------------------------------------------------------------------

// A piece's own sign1/sign2 faces are placeholders the game paints with the ride's name at run time (tesla.wad's
// "sign" node, the boards on the kingtut and the yeti). The island paints no names, so they are left out: their faces,
// face sets and textures go, and a node left without faces becomes a mini (dropped unless geometry hangs below it).
// An animating group whose frames are named sign1/sign2 is a picture, not a board, and stays (RenameTextures).
void DropSignBoards(Md2Model& model) {
    std::vector<int> newTexture(model.textures.size(), -1);
    std::vector<Md2Texture> textures;
    for (size_t t = 0; t < model.textures.size(); ++t) {
        const Md2Texture& tex = model.textures[t];
        const std::string stem = tex.frames.empty() ? std::string() : Lower(Stem(tex.frames[0]));
        const bool animated = (tex.infoFlags & 1) != 0 && tex.frames.size() > 1;
        if (!animated && (stem == "sign1" || stem == "sign2" || stem == "sign")) continue;
        newTexture[t] = static_cast<int>(textures.size());
        textures.push_back(tex);
    }
    for (Md2Node& n : model.nodes) {
        if (n.mini) continue;
        // Face sets need not run in face order (the tesla's run in texture name order), so the faces keep theirs: the
        // dropped sets' faces go and the kept sets' first faces move down past them.
        std::vector<char> gone(n.faces.size(), 0);
        bool dropped = false;
        for (const Md2FaceSet& s : n.faceSets) {
            if (s.texture < 0 || newTexture[static_cast<size_t>(s.texture)] >= 0) continue;
            dropped = true;
            for (size_t f = s.firstFace; f < n.faces.size() && f < size_t(s.firstFace) + s.faceCount; ++f) gone[f] = 1;
        }
        std::vector<uint16_t> newFace(n.faces.size() + 1, 0);
        std::vector<Md2Face> faces;
        for (size_t f = 0; f < n.faces.size(); ++f) {
            newFace[f] = static_cast<uint16_t>(faces.size());
            if (!gone[f]) faces.push_back(n.faces[f]);
        }
        newFace[n.faces.size()] = static_cast<uint16_t>(faces.size());
        std::vector<Md2FaceSet> sets;
        for (Md2FaceSet s : n.faceSets) {
            if (s.texture >= 0 && newTexture[static_cast<size_t>(s.texture)] < 0) continue;
            if (s.texture >= 0) s.texture = newTexture[static_cast<size_t>(s.texture)];
            if (dropped) s.firstFace = newFace[std::min<size_t>(s.firstFace, n.faces.size())];
            sets.push_back(s);
        }
        n.faceSets = std::move(sets);
        if (!dropped) continue;
        n.faces = std::move(faces);
        if (n.faceSets.empty()) {
            n.mini = true;
            n.flags = 0x200;
            n.vertices.clear();
            n.points.clear();
            n.u.clear();
            n.v.clear();
            n.cornerVertex.clear();
            n.faces.clear();
            n.box = {0, 0, 0, 0, 0, 0};
        }
    }
    model.textures = std::move(textures);
}

// Renames a mesh's textures to lower-case ".tga" names and resolves them. A piece's animating group whose frames are
// named sign1/sign2 is a picture of its own, not a sign (the tesla's force field): its frames become
// "<piece>_sign1"/"<piece>_sign2" with the wad's own textures, so that only the gate carries sign1/sign2 (the game
// wants exactly two or none in a mesh).
// An animating texture (info flag bit 0, several frames) ends with the group's name, which is not a file: Theme Park
// World's own meshes name frames "mface1".."mface4" and then "face" with no such file in the wad. That name is
// lower-cased and left alone, no file is written for it.
void RenameTextures(Zone& zone, Md2Model& model, const std::string& pieceLower, const TextureLayers* own, bool keepSigns) {
    for (Md2Texture& t : model.textures) {
        const bool animated = (t.infoFlags & 1) != 0 && t.frames.size() > 1;
        for (size_t f = 0; f < t.frames.size(); ++f) {
            std::string& frame = t.frames[f];
            const std::string stem = Lower(Stem(frame));
            std::string out = stem;
            if (animated && f + 1 == t.frames.size()) {
                frame = out + ".tga";
                continue;
            }
            const bool signName = stem == "sign1" || stem == "sign2" || stem == "sign";
            if (!keepSigns && signName && animated) {
                out = pieceLower + "_" + stem;
                zone.Resolve(out, stem, own);
                frame = out + ".tga";
                continue;
            }
            zone.Resolve(out, stem, own);
            frame = out + ".tga";
        }
    }
}

bool PlacePiece(Zone& zone, Md2Model& isle, const Pick& pick) {
    const std::string pieceLower = Lower(pick.piece);
    const Entry* entry = zone.FindWad(pieceLower);
    if (!entry) {
        zone.Log("piece " + pieceLower + " not found in the zone, left out");
        return false;
    }
    std::string err;
    std::unique_ptr<std::vector<TpiWadMember>> keep(new std::vector<TpiWadMember>());
    std::vector<TpiWadMember>* members = keep.get();
    if (!zone.ReadWad(entry, *members, err)) {
        zone.Log("piece " + pieceLower + ": " + err + ", left out");
        return false;
    }
    const TpiWadMember* mesh = FindMesh(*members, pieceLower);
    Md2Model piece;
    if (!mesh || !ReadMd2(mesh->data, piece, err)) {
        zone.Log("piece " + pieceLower + ": " + (mesh ? err : std::string("no mesh in its wad")) + ", left out");
        return false;
    }
    if (piece.nodes.empty() || piece.nodes[0].mini) {
        zone.Log("piece " + pieceLower + ": its root is not a geometry node, left out");
        return false;
    }
    BakeRoot(piece);
    std::array<float, 6> box{};
    if (!GeometryBox(piece, box) || box[4] - box[1] <= 0.01f) {
        zone.Log("piece " + pieceLower + ": no geometry, left out");
        return false;
    }
    const float s = pick.height / (box[4] - box[1]);
    BakeScale(piece, s);
    GeometryBox(piece, box);
    const float cx = (box[0] + box[3]) / 2, cz = (box[2] + box[5]) / 2;
    const Matrix turn = Multiply(Translation(-cx, -box[1], -cz), RotationY(pick.facing * kPi / 180.0f));
    // Its footprint on the slab, and the lowest layer above every earlier piece it overlaps (kGroundLift).
    Zone::Footprint foot{1e30f, 1e30f, -1e30f, -1e30f, 1};
    for (float x : {box[0], box[3]})
        for (float z : {box[2], box[5]}) {
            const Vec3 p = Transform(Vec3{x, 0, z}, turn);
            foot.x0 = std::min(foot.x0, pick.x + p.x);
            foot.x1 = std::max(foot.x1, pick.x + p.x);
            foot.z0 = std::min(foot.z0, pick.z + p.z);
            foot.z1 = std::max(foot.z1, pick.z + p.z);
        }
    for (const Zone::Footprint& f : zone.placed)
        if (f.x0 < foot.x1 && foot.x0 < f.x1 && f.z0 < foot.z1 && foot.z0 < f.z1) foot.layer = std::max(foot.layer, f.layer + 1);
    const float y = kTop + kGroundLift * static_cast<float>(foot.layer);
    const Matrix placement = Multiply(turn, Translation(pick.x, y, pick.z));
    TextureLayers own;
    own.AddWad(*members);
    DropSignBoards(piece);
    RenameTextures(zone, piece, pieceLower, &own, false);
    if (!AttachPiece(isle, piece, placement, err)) {
        zone.Log("piece " + pieceLower + ": " + err + ", left out");
        return false;
    }
    zone.placed.push_back(foot);
    char line[160];
    std::snprintf(line, sizeof line, "%s placed at (%.0f, %.2f, %.0f), scale %.3f, %zu nodes, %zu textures", pieceLower.c_str(),
                  static_cast<double>(pick.x), static_cast<double>(y), static_cast<double>(pick.z), static_cast<double>(s),
                  piece.nodes.size(), piece.textures.size());
    zone.Log(line);
    zone.keep.push_back(std::move(keep));   // zone.resolved points into the members
    return true;
}

// ---- the gate ----------------------------------------------------------------------------------------------------------

uint16_t R16(const std::vector<uint8_t>& d, size_t at) { return static_cast<uint16_t>(d[at] | (d[at + 1] << 8)); }
uint32_t R32(const std::vector<uint8_t>& d, size_t at) {
    return static_cast<uint32_t>(d[at]) | (static_cast<uint32_t>(d[at + 1]) << 8) | (static_cast<uint32_t>(d[at + 2]) << 16) |
           (static_cast<uint32_t>(d[at + 3]) << 24);
}
bool Inside(const std::vector<uint8_t>& d, uint64_t at, uint64_t n) { return at + n <= d.size() && at + n >= at; }
void MulF(std::vector<uint8_t>& d, size_t at, float s, float add = 0.0f) {
    float f;
    std::memcpy(&f, d.data() + at, 4);
    f = f * s + add;
    std::memcpy(d.data() + at, &f, 4);
}

// Scales a mesh file in place: every vertex, every node's translation and box, the animation's shape points. The root's
// matrix is folded into its children and its own geometry (rotation only, as BakeRoot does) and becomes the identity
// (the game writes the placement into it), and the root's children are moved by `offset`. Texture names become lower
// case. Tables whose pointer is zero are absent (the animation files carry the mesh's counts with no tables). False
// when a table runs past the end of the file.
bool ScaleMeshBytes(std::vector<uint8_t>& d, float s, const Vec3& offset) {
    if (d.size() < 0xb8 || R32(d, 0) != 0x1cd15d46) return false;
    const uint32_t vsets = R16(d, 0x38), allNodes = R16(d, 0x42), nodes = R16(d, 0x44), texCount = R16(d, 0x36);
    const uint32_t vOff = R32(d, 0x58), nodeOff = R32(d, 0x70), miniOff = R32(d, 0x74), rootOff = R32(d, 0x78), infoOff = R32(d, 0x54);
    const uint32_t minis = allNodes > nodes ? allNodes - nodes : 0;
    if (vsets && vOff && !Inside(d, vOff, uint64_t(vsets) * 0x30)) return false;
    if (nodes && nodeOff && !Inside(d, nodeOff, uint64_t(nodes) * 0xa0)) return false;
    if (minis && miniOff && !Inside(d, miniOff, uint64_t(minis) * 0x58)) return false;
    auto matrixAt = [&](size_t at) {
        tpi_island::Matrix m;
        for (int k = 0; k < 16; ++k) std::memcpy(&m[static_cast<size_t>(k)], d.data() + at + 4 * k, 4);
        return m;
    };
    auto putMatrix = [&](size_t at, const tpi_island::Matrix& m) {
        for (int k = 0; k < 16; ++k) std::memcpy(d.data() + at + 4 * k, &m[static_cast<size_t>(k)], 4);
    };
    // The root's rotation, folded into its children and its own geometry first.
    tpi_island::Matrix lin = tpi_island::IdentityMatrix();
    const bool haveRoot = rootOff && Inside(d, rootOff, 0x58);
    if (haveRoot) {
        lin = matrixAt(rootOff + 0x10);
        lin[12] = lin[13] = lin[14] = 0;
        lin[15] = 1;
    }
    const bool turned = lin != tpi_island::IdentityMatrix();
    auto record = [&](size_t at, bool full) {
        if (at == rootOff) {
            if (full && turned) {
                // A root carrying geometry: its vertices and normals turn with the root's matrix.
                const uint32_t nv = R16(d, at + 0x58), vp = R32(d, at + 0x60), sets = (nv + 3) / 4;
                const uint32_t np = uint32_t(R16(d, at + 0x5e)) + R16(d, at + 0x5c), pp = R32(d, at + 0x64);
                if (sets && Inside(d, vp, uint64_t(sets) * 0x30))
                    for (uint32_t i = 0; i < sets; ++i)
                        for (int lane = 0; lane < 4; ++lane) {
                            const size_t x = vp + 0x30 * i + 4 * lane, y = x + 16, z = x + 32;
                            Vec3 v;
                            std::memcpy(&v.x, d.data() + x, 4);
                            std::memcpy(&v.y, d.data() + y, 4);
                            std::memcpy(&v.z, d.data() + z, 4);
                            v = tpi_island::Transform(v, lin);
                            std::memcpy(d.data() + x, &v.x, 4);
                            std::memcpy(d.data() + y, &v.y, 4);
                            std::memcpy(d.data() + z, &v.z, 4);
                        }
                if (np && Inside(d, pp, uint64_t(np) * 12))
                    for (uint32_t i = 0; i < np; ++i) {
                        Vec3 v;
                        std::memcpy(&v, d.data() + pp + 12 * i, 12);
                        v = tpi_island::Transform(v, lin);
                        std::memcpy(d.data() + pp + 12 * i, &v, 12);
                    }
            }
            putMatrix(at + 0x10, tpi_island::IdentityMatrix());
        } else {
            const bool child = haveRoot && R32(d, at + 4) == rootOff;
            if (child && turned) putMatrix(at + 0x10, tpi_island::Multiply(matrixAt(at + 0x10), lin));
            MulF(d, at + 0x40, s, child ? offset.x : 0.0f);
            MulF(d, at + 0x44, s, child ? offset.y : 0.0f);
            MulF(d, at + 0x48, s, child ? offset.z : 0.0f);
        }
        if (full)
            for (int k = 0; k < 6; ++k) MulF(d, at + 0x78 + 4 * k, s);
    };
    if (nodeOff)
        for (uint32_t i = 0; i < nodes; ++i) record(nodeOff + 0xa0 * i, true);
    if (miniOff)
        for (uint32_t i = 0; i < minis; ++i) record(miniOff + 0x58 * i, false);
    // A root carrying geometry moves by the offset too (its vertices are in the model's space now).
    if (haveRoot && nodeOff && rootOff >= nodeOff && rootOff < nodeOff + 0xa0 * uint64_t(nodes)) {
        const uint32_t nv = R16(d, rootOff + 0x58), vp = R32(d, rootOff + 0x60), sets = (nv + 3) / 4;
        if (sets && Inside(d, vp, uint64_t(sets) * 0x30))
            for (uint32_t i = 0; i < sets; ++i)
                for (int lane = 0; lane < 4; ++lane) {
                    MulF(d, vp + 0x30 * i + 4 * lane, s, offset.x);
                    MulF(d, vp + 0x30 * i + 16 + 4 * lane, s, offset.y);
                    MulF(d, vp + 0x30 * i + 32 + 4 * lane, s, offset.z);
                }
        // Its vertex sets are scaled here, so the general pass below skips them.
        for (uint32_t i = 0; i < vsets && vOff; ++i) {
            const size_t at = vOff + 0x30 * i;
            if (sets && at >= vp && at < vp + 0x30 * uint64_t(sets)) continue;
            for (int k = 0; k < 12; ++k) MulF(d, at + 4 * k, s);
        }
    } else if (vOff) {
        for (uint32_t i = 0; i < vsets; ++i)
            for (int k = 0; k < 12; ++k) MulF(d, vOff + 0x30 * i + 4 * k, s);
    }
    const uint32_t anim = R32(d, 0x98);
    if (anim) {
        if (!Inside(d, anim, 0x24)) return false;
        const uint32_t shapes = R16(d, anim + 0x0c), shapesOff = R32(d, anim + 0x20);
        if (shapes && !Inside(d, shapesOff, uint64_t(shapes) * 0x10)) return false;
        for (uint32_t i = 0; i < shapes; ++i) {
            const size_t shape = shapesOff + 0x10 * i;
            const uint32_t points = R16(d, shape + 4), pointsOff = R32(d, shape + 8);
            if (points && !Inside(d, pointsOff, uint64_t(points) * 12)) return false;
            for (uint32_t p = 0; p < points * 3; ++p) MulF(d, pointsOff + 4 * p, s);
        }
    }
    if (texCount && infoOff) {
        if (!Inside(d, infoOff, uint64_t(texCount) * 16)) return false;
        for (uint32_t i = 0; i < texCount; ++i) {
            const uint32_t frames = R16(d, infoOff + 16 * i + 0xa), names = R32(d, infoOff + 16 * i + 0xc);
            if (!Inside(d, names, uint64_t(frames) * 0x14)) return false;
            for (uint32_t f = 0; f < frames * 0x14; ++f) d[names + f] = static_cast<uint8_t>(std::tolower(d[names + f]));
        }
    }
    return true;
}

// Writes the gate's four files; the gate's textures join the zone's resolved set.
bool BuildGate(Zone& zone, TpiIslandFiles& out, const std::string& folder, std::string& reason) {
    const Recipe& r = *zone.recipe;
    const Entry* entry = zone.FindWad("gates");
    if (!entry) {
        reason = std::string("the ") + r.zone + " zone has no features/Gates.wad";
        return false;
    }
    std::string err;
    if (!zone.ReadWad(entry, zone.gates, err)) {
        reason = std::string(r.zone) + " features/Gates.wad: " + err;
        return false;
    }
    const TpiWadMember* mesh = FindMember(zone.gates, "gates.md2");
    Md2Model gate;
    if (!mesh || !ReadMd2(mesh->data, gate, err)) {
        reason = std::string(r.zone) + " features/Gates.wad: " + (mesh ? err : std::string("no gates.md2"));
        return false;
    }
    // The gate is at least kGateWidth wide and kGateHeight tall: the zones' gates differ in shape (a tall arch, a wide
    // wall with buttresses), and the smaller of the two rules alone would shrink one of them to nothing.
    BakeRoot(gate);
    std::array<float, 6> box{};
    if (!GeometryBox(gate, box) || box[4] - box[1] <= 0.01f) {
        reason = std::string(r.zone) + " gates.md2 has no geometry";
        return false;
    }
    const float s = std::max(kGateWidth / (box[3] - box[0]), kGateHeight / (box[4] - box[1]));
    const Vec3 offset{-(box[0] + box[3]) / 2 * s, kTop - box[1] * s, kGateZ - (box[2] + box[5]) / 2 * s};
    std::vector<uint8_t> bytes = mesh->data;
    if (!ScaleMeshBytes(bytes, s, offset)) {
        reason = std::string(r.zone) + " gates.md2 could not be scaled";
        return false;
    }
    // The header's box from the scaled geometry.
    Md2Model scaled;
    if (ReadMd2(bytes, scaled, err) && GeometryBox(scaled, box))
        for (int k = 0; k < 6; ++k) std::memcpy(bytes.data() + 0x80 + 4 * k, &box[k], 4);
    const std::string prefix = r.prefix;
    out.files[folder + prefix + "_gate.md2"] = std::move(bytes);
    for (const char* suffix : {"m1", "m2"}) {
        const TpiWadMember* anim = FindMember(zone.gates, std::string("gates") + suffix + ".md2");
        if (!anim) {
            zone.Log(std::string("gates") + suffix + ".md2 not in Gates.wad, the gate has no animation");
            continue;
        }
        std::vector<uint8_t> a = anim->data;
        if (!ScaleMeshBytes(a, s, Vec3{0, 0, 0})) {
            zone.Log(std::string("gates") + suffix + ".md2 could not be scaled, left out");
            continue;
        }
        out.files[folder + prefix + "_gate" + (suffix[1] == '1' ? "M1" : "M2") + ".md2"] = std::move(a);
    }
    if (const TpiWadMember* sgn = FindMember(zone.gates, "gates.sgn")) out.files[folder + prefix + "_gate.sgn"] = sgn->data;
    else zone.Log("gates.sgn not in Gates.wad, the gate has no sign description");
    TextureLayers own;
    own.AddWad(zone.gates);
    RenameTextures(zone, gate, "gates", &own, true);
    char line[160];
    std::snprintf(line, sizeof line, "gate scaled %.3f (%.1f wide, %.1f tall) at z %.0f, %zu textures", static_cast<double>(s),
                  static_cast<double>((box[3] - box[0])), static_cast<double>(box[4] - box[1]), static_cast<double>(kGateZ),
                  gate.textures.size());
    zone.Log(line);
    return true;
}

// ---- textures and the descriptor ----------------------------------------------------------------------------------------

std::vector<uint8_t> SolidTga(uint8_t r, uint8_t g, uint8_t b) {
    TpiImage image;
    image.width = image.height = 16;
    for (uint32_t i = 0; i < 256; ++i) {
        image.bgra.push_back(b);
        image.bgra.push_back(g);
        image.bgra.push_back(r);
        image.bgra.push_back(255);
    }
    return EncodeTpiTga32(image);
}

bool LooksLikeTga(const std::vector<uint8_t>& bytes) {
    return bytes.size() > 18 && (bytes[2] == 2 || bytes[2] == 10) && (bytes[16] == 24 || bytes[16] == 32);
}

void WriteTextures(Zone& zone, TpiIslandFiles& out, const std::string& folder) {
    std::string list;
    for (const auto& [stem, bytes] : zone.resolved) {
        std::vector<uint8_t> tga;
        std::string err;
        if (bytes && IsTpiFsh(*bytes)) {
            if (!TpiFshToTga(*bytes, tga, err)) {
                zone.Log("texture " + stem + ": " + err + ", grey stand-in written");
                tga.clear();
            }
        } else if (bytes && LooksLikeTga(*bytes)) {
            tga = *bytes;
        }
        if (tga.empty()) tga = SolidTga(128, 128, 128);
        out.files[folder + "stexture/" + stem + ".tga"] = tga;
        out.files[folder + "textures/" + stem + ".tga"] = std::move(tga);
        list += stem + ".tga\r\n";
    }
    std::vector<uint8_t> listBytes(list.begin(), list.end());
    out.files[folder + "qickload.txt"] = listBytes;
    out.files[folder + "textures/qickload.txt"] = listBytes;
    out.files[folder + "stexture/qickload.txt"] = listBytes;
}

std::string Descriptor(const Recipe& r, int slot) {
    std::string t = "ISLAND(" + std::to_string(slot) + ",\"data\\lobby\\" + r.world + "\",\"" + r.prefix + "_isle\",\"" + r.prefix +
                    "_gate\",\"" + r.title + "\",0.0," + Num(r.y) + ")\r\n";
    if (r.lightning > 0) t += "LIGHTNING(" + std::to_string(r.lightning) + ")\r\n";
    if (r.rainy > 0) t += "RAINY(" + std::to_string(r.rainy) + ")\r\n";
    t += "SKYCOLOUR(" + std::to_string(r.sky[0]) + "," + std::to_string(r.sky[1]) + "," + std::to_string(r.sky[2]) + ")\r\n";
    return t;
}

bool BuildWorld(const TpiDisc& disc, const Recipe& r, int slot, TpiIslandFiles& out, std::string& reason) {
    Zone zone;
    zone.recipe = &r;
    zone.source = disc.source;
    zone.log = &out.log;
    zone.prefixLower = Lower(disc.root + "data/levels/" + r.zone + "/");
    for (const Entry& e : disc.source->Entries()) {
        if (e.kind != EntryKind::Data) continue;
        const std::string lower = Lower(e.path);
        if (lower.compare(0, zone.prefixLower.size(), zone.prefixLower) == 0) zone.entries.emplace(lower.substr(zone.prefixLower.size()), &e);
    }
    if (zone.entries.empty()) {
        reason = std::string("the copy has no Data/levels/") + r.zone + " folder";
        return false;
    }
    std::string err;
    const Entry* terrain = zone.Find("terrain.wad");
    if (!terrain || !zone.ReadWad(terrain, zone.terrain, err)) {
        reason = std::string(r.zone) + " terrain.wad: " + (terrain ? err : std::string("missing"));
        return false;
    }
    if (const Entry* share = zone.Find("sharetex.wad")) {
        if (!zone.ReadWad(share, zone.sharetex, err)) zone.Log("Sharetex.wad: " + err + ", its textures are not used");
    }
    zone.layers.AddWad(zone.sharetex);
    zone.layers.AddWad(zone.terrain);
    const std::string folder = std::string("data/lobby/") + r.world + "/";
    if (!BuildGate(zone, out, folder, reason)) return false;
    Md2Model isle = MakeSlab(r);
    RenameTextures(zone, isle, "island", nullptr, true);
    PlacePiece(zone, isle, r.landmark);
    for (const Pick& pick : r.scenery)
        if (pick.piece) PlacePiece(zone, isle, pick);
    RecomputeBoxes(isle);
    out.files[folder + r.prefix + "_isle.md2"] = WriteMd2(isle);
    WriteTextures(zone, out, folder);
    const std::string text = Descriptor(r, slot);
    out.files[std::string("data/lobby/") + r.world + ".txt"] = std::vector<uint8_t>(text.begin(), text.end());
    char line[160];
    std::snprintf(line, sizeof line, "island %zu nodes, %zu textures, box x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f", isle.nodes.size(),
                  isle.textures.size(), static_cast<double>(isle.box[0]), static_cast<double>(isle.box[3]),
                  static_cast<double>(isle.box[1]), static_cast<double>(isle.box[4]), static_cast<double>(isle.box[2]),
                  static_cast<double>(isle.box[5]));
    zone.Log(line);
    return true;
}

}  // namespace

bool BuildTpiIslandFiles(const TpiDisc& disc, TpiIslandFiles& out, std::string& reason) {
    if (!disc.source) {
        reason = "no source to read the copy from";
        return false;
    }
    int slot = 4;   // the originals use 0 to 3; with the mod on the ring code places the three worlds and ignores this
    for (const Recipe& r : kRecipes)
        if (!BuildWorld(disc, r, slot++, out, reason)) return false;
    return true;
}

bool BuildTpiIslands(const TpiDisc& disc, const std::string& outRoot, std::string& reason) {
    TpiIslandFiles built;
    if (!BuildTpiIslandFiles(disc, built, reason)) return false;
    namespace fs = std::filesystem;
    for (const auto& [path, bytes] : built.files) {
        const fs::path target = fs::path(outRoot) / path;
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        std::ofstream f(target, std::ios::binary | std::ios::trunc);
        if (!f || !f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
            reason = "could not write " + tpw::PathText(target);
            return false;
        }
    }
    return true;
}

}  // namespace tpw::assets
