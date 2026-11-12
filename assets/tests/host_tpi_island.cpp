// Host test of the Theme Park Inc island builder (assets/tpi_island.h, tpi_island_md2.h): synthetic meshes, textures
// and wads built here, no game data. Runs alone and repeatedly.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "tpi_fsh.h"
#include "tpi_island.h"
#include "tpi_island_md2.h"
#include "tpi_wad.h"

using namespace tpw::assets;
using namespace tpw::assets::tpi_island;

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

// ---- fixtures

// A SHPI file with one raw 8-bit image and a palette: pixel (x, y) is index (x + y) % 4, palette entries four shades
// of the colour given. The palette is 0x24 (R, G, B per entry, as Theme Park Inc stores it) or 0x2a (B, G, R, A).
std::vector<uint8_t> MakeFsh(uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t code = 0x24, uint8_t a = 0xff) {
    std::vector<uint8_t> f;
    auto put32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) f.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    auto put16 = [&](uint32_t v) {
        f.push_back(static_cast<uint8_t>(v));
        f.push_back(static_cast<uint8_t>(v >> 8));
    };
    f.insert(f.end(), {'S', 'H', 'P', 'I'});
    put32(0);   // length, fixed up below
    put32(1);
    f.insert(f.end(), {'G', 'I', 'M', 'X'});
    f.insert(f.end(), {'0', '0', '0', '0'});
    put32(24);
    // The image block.
    const uint32_t pixels = static_cast<uint32_t>(w) * h;
    const uint32_t blockLen = 16 + pixels;
    f.push_back(0x7b);
    f.push_back(static_cast<uint8_t>(blockLen));
    f.push_back(static_cast<uint8_t>(blockLen >> 8));
    f.push_back(static_cast<uint8_t>(blockLen >> 16));
    put16(w);
    put16(h);
    for (int i = 0; i < 8; ++i) f.push_back(0);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) f.push_back(static_cast<uint8_t>((x + y) % 4));
    // The palette block: 4 entries.
    const uint32_t per = code == 0x24 ? 3 : 4;
    const uint32_t palLen = 16 + 4 * per;
    f.push_back(code);
    f.push_back(static_cast<uint8_t>(palLen));
    f.push_back(static_cast<uint8_t>(palLen >> 8));
    f.push_back(static_cast<uint8_t>(palLen >> 16));
    put16(4);
    put16(1);
    for (int i = 0; i < 8; ++i) f.push_back(0);
    for (int i = 0; i < 4; ++i) {
        const uint8_t rs = static_cast<uint8_t>(r * (i + 1) / 4), gs = static_cast<uint8_t>(g * (i + 1) / 4), bs = static_cast<uint8_t>(b * (i + 1) / 4);
        if (code == 0x24) {
            f.insert(f.end(), {rs, gs, bs});
        } else {
            f.insert(f.end(), {bs, gs, rs, a});
        }
    }
    const uint32_t len = static_cast<uint32_t>(f.size());
    for (int i = 0; i < 4; ++i) f[4 + static_cast<size_t>(i)] = static_cast<uint8_t>(len >> (8 * i));
    return f;
}

// A piece: a box `w` wide, `h` tall, `d` deep standing on y 0 (its front at -z), with the texture names given, under a
// root node whose own matrix has a translation (the builder must ignore it) and a mini node that carries nothing.
Md2Model MakePiece(const std::string& name, float w, float h, float d, const std::vector<std::string>& textures) {
    Md2Model m;
    m.name = name;
    for (const std::string& t : textures) {
        Md2Texture tex;
        tex.frames.push_back(t);
        m.textures.push_back(tex);
    }
    NodeBuilder b;
    b.Box(Vec3{-w / 2, 0, -d / 2}, Vec3{w / 2, h, d / 2}, 4.0f, 0);
    if (textures.size() > 1) b.Box(Vec3{-w / 4, h, -d / 4}, Vec3{w / 4, h + 1.0f, d / 4}, 4.0f, 1);
    Md2Node root = b.Build("root");
    root.parent = -1;
    root.matrix = Translation(100.0f, 200.0f, 300.0f);
    m.nodes.push_back(root);
    Md2Node mini;
    mini.name = "cam";
    mini.mini = true;
    mini.flags = 0x200;
    mini.parent = 0;
    mini.matrix = Translation(1, 2, 3);
    m.nodes.push_back(mini);
    RecomputeBoxes(m);
    return m;
}

// Moves the root's first face set's faces to the end, so that its face sets no longer run in face order (the tesla's
// run in texture name order).
Md2Model FirstSetLast(Md2Model m) {
    Md2Node& n = m.nodes[0];
    Md2FaceSet& a = n.faceSets[0];
    std::vector<Md2Face> faces(n.faces.begin() + a.faceCount, n.faces.end());
    faces.insert(faces.end(), n.faces.begin(), n.faces.begin() + a.faceCount);
    for (size_t i = 1; i < n.faceSets.size(); ++i) n.faceSets[i].firstFace = static_cast<uint16_t>(n.faceSets[i].firstFace - a.faceCount);
    a.firstFace = static_cast<uint16_t>(n.faces.size() - a.faceCount);
    n.faces = std::move(faces);
    return m;
}

// An animation file: an M3D2 header without geometry whose anim block has one shape of three points.
std::vector<uint8_t> MakeAnim(const std::vector<Vec3>& points) {
    std::vector<uint8_t> d(0xb8, 0);
    auto put32 = [&](size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) d[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
    };
    put32(0, 0x1cd15d46);
    put32(4, 0xdd);
    put32(8, 0xcb);
    put32(0x30, 0x20);
    const size_t anim = d.size();
    d.resize(anim + 0x24, 0);
    d[anim + 0x0c] = 1;
    const size_t shape = d.size();
    d.resize(shape + 0x10, 0);
    put32(anim + 0x20, static_cast<uint32_t>(shape));
    d[shape + 4] = static_cast<uint8_t>(points.size());
    const size_t pts = d.size();
    put32(shape + 8, static_cast<uint32_t>(pts));
    for (const Vec3& p : points) {
        d.resize(d.size() + 12);
        std::memcpy(d.data() + d.size() - 12, &p.x, 4);
        std::memcpy(d.data() + d.size() - 8, &p.y, 4);
        std::memcpy(d.data() + d.size() - 4, &p.z, 4);
    }
    put32(0x98, static_cast<uint32_t>(anim));
    put32(0x2c, static_cast<uint32_t>(d.size() - 4));
    return d;
}

// A Source over files in memory.
class MemorySource : public Source {
public:
    void Add(const std::string& path, std::vector<uint8_t> bytes) {
        Entry e;
        e.path = path;
        e.size = bytes.size();
        entries_.push_back(e);
        data_[path] = std::move(bytes);
    }
    const char* Kind() const override { return "memory"; }
    const std::vector<Entry>& Entries() const override { return entries_; }
    std::unique_ptr<Stream> Open(const Entry& entry, std::string& err) override {
        auto it = data_.find(entry.path);
        if (it == data_.end()) {
            err = "no such entry";
            return nullptr;
        }
        return std::make_unique<MemoryStream>(it->second);
    }

private:
    class MemoryStream : public Stream {
    public:
        explicit MemoryStream(const std::vector<uint8_t>& d) : d_(d) {}
        uint64_t Size() const override { return d_.size(); }
        int64_t Read(void* buf, size_t n, std::string&) override {
            const size_t take = std::min(n, d_.size() - at_);
            std::memcpy(buf, d_.data() + at_, take);
            at_ += take;
            return static_cast<int64_t>(take);
        }

    private:
        const std::vector<uint8_t>& d_;
        size_t at_ = 0;
    };
    std::vector<Entry> entries_;
    std::map<std::string, std::vector<uint8_t>> data_;
};

std::vector<uint8_t> PieceWad(const std::string& piece, const Md2Model& model, const std::vector<std::string>& textureStems) {
    std::vector<TpiWadMember> members;
    members.push_back({piece + ".MD2", WriteMd2(model)});
    members.push_back({"qickload.txt", std::vector<uint8_t>{'x', '\r', '\n'}});
    for (const std::string& stem : textureStems) members.push_back({"textures\\" + stem + ".fsh", MakeFsh(8, 8, 200, 100, 50)});
    return BuildTpiWad(members);
}

// The synthetic copy: three zones with a gate, a landmark and one scenery piece each, a terrain.wad with the ground
// textures and a Sharetex.wad with one shared texture.
std::shared_ptr<MemorySource> MakeCopy(const std::string& root) {
    auto src = std::make_shared<MemorySource>();
    const char* zones[3] = {"Arabian", "water", "SCIENCE"};
    const char* landmarks[3] = {"kingtut", "yeti", "tesla"};
    const char* scenery[3] = {"ltree", "penguins", "rock_l"};
    for (int z = 0; z < 3; ++z) {
        const std::string dir = root + "Data/levels/" + zones[z] + "/";
        // The gate: 40 wide, 30 tall, 10 deep, with sign textures, standing away from the origin.
        Md2Model gate = MakePiece("gates.MD2", 40, 30, 10, {"stone.TGA", "sign1.TGA"});
        {
            Md2Texture sign2;
            sign2.frames.push_back("sign2.tga");
            gate.textures.push_back(sign2);
            for (Vec3& v : gate.nodes[0].vertices) {
                v.x += 500;
                v.z += 20;
            }
            RecomputeBoxes(gate);
        }
        std::vector<TpiWadMember> gateMembers;
        gateMembers.push_back({"gates.md2", WriteMd2(gate)});
        gateMembers.push_back({"gatesM1.md2", MakeAnim({{1, 2, 3}, {4, 5, 6}, {7, 8, 9}})});
        gateMembers.push_back({"gatesM2.md2", MakeAnim({{10, 20, 30}})});
        gateMembers.push_back({"gates.sgn", std::vector<uint8_t>{'s', 'g', 'n'}});
        gateMembers.push_back({"textures\\stone.fsh", MakeFsh(16, 8, 90, 90, 90)});
        gateMembers.push_back({"textures\\sign1.fsh", MakeFsh(8, 8, 255, 255, 255)});
        src->Add(dir + "features/Gates.wad", BuildTpiWad(gateMembers));
        // The landmark: 40 tall, with sign boards of its own that must be left out (a face set on its body, as the
        // kingtut's, and a node of its own, as the tesla's "sign"), a shared texture, and an animating texture whose
        // last name is the group's name (no file of that name exists or gets written); and, as the tesla has, an
        // animating group named sign1/sign2 that is a picture of its own, not a sign.
        Md2Model landmark = FirstSetLast(MakePiece(landmarks[z], 20, 40, 15, {"body.tga", "sign1.tga", "Sign2.TGA"}));
        NodeBuilder board;
        board.Box(Vec3{-5, 20, -7.5f}, Vec3{5, 26, -7.0f}, 4.0f, 2);
        Md2Node boardNode = board.Build("sign");
        boardNode.parent = 0;
        landmark.nodes.push_back(boardNode);
        Md2Texture glow;
        glow.infoFlags = 1;
        glow.frames = {"GLOW1.tga", "glow2.tga", "glow.tga"};
        landmark.textures.push_back(glow);
        Md2Texture field;
        field.infoFlags = 1;
        field.frames = {"SIGN1.tga", "sign2.tga", "sign.tga"};
        landmark.textures.push_back(field);
        src->Add(dir + "rides/" + landmarks[z] + ".wad",
                 PieceWad(landmarks[z], landmark, {"body", "glow1", "glow2", "sign1", "sign2"}));
        src->Add(dir + "features/" + scenery[z] + ".wad",
                 PieceWad(scenery[z], FirstSetLast(MakePiece(scenery[z], 5, 8, 5, {"leaf.TGA", "shared.tga"})), {"leaf"}));
        std::vector<TpiWadMember> terrain;
        for (const char* stem : {"sanded01", "cliffblend", "rock1", "post1", "top1", "rock2", "logs02", "cobbles04", "fence01"})
            terrain.push_back({std::string("textures\\") + stem + ".fsh", MakeFsh(8, 8, 120, 160, 80)});
        src->Add(dir + "terrain.wad", BuildTpiWad(terrain));
        std::vector<TpiWadMember> share;
        share.push_back({"textures\\shared.fsh", MakeFsh(8, 8, 10, 20, 30)});
        src->Add(dir + "Sharetex.wad", BuildTpiWad(share));
    }
    return src;
}

bool Has(const TpiIslandFiles& f, const std::string& path) { return f.files.count(path) != 0; }

const std::vector<uint8_t>& File(const TpiIslandFiles& f, const std::string& path) {
    static const std::vector<uint8_t> none;
    auto it = f.files.find(path);
    return it == f.files.end() ? none : it->second;
}

float ReadF(const std::vector<uint8_t>& d, size_t at) {
    float f;
    std::memcpy(&f, d.data() + at, 4);
    return f;
}
uint32_t Read32(const std::vector<uint8_t>& d, size_t at) {
    return static_cast<uint32_t>(d[at]) | (static_cast<uint32_t>(d[at + 1]) << 8) | (static_cast<uint32_t>(d[at + 2]) << 16) |
           (static_cast<uint32_t>(d[at + 3]) << 24);
}

// ---- tests

void TestMatrices() {
    const Matrix m = Multiply(RotationY(3.14159265f / 2), Translation(10, 0, 0));
    const Vec3 p = Transform(Vec3{0, 0, -1}, m);
    // A point at -z turned by +90 degrees about y lands at -x (the game's camera for angle 90 sits at -x).
    CHECK(std::fabs(p.x - 9.0f) < 1e-4f && std::fabs(p.z) < 1e-4f);
    const Vec3 q = Transform(Vec3{1, 2, 3}, Multiply(Translation(1, 1, 1), Translation(2, 2, 2)));
    CHECK(std::fabs(q.x - 4) < 1e-6f && std::fabs(q.y - 5) < 1e-6f && std::fabs(q.z - 6) < 1e-6f);
}

void TestRoundTrip() {
    Md2Model piece = MakePiece("piece.MD2", 4, 6, 2, {"a.tga", "b.tga"});
    const std::vector<uint8_t> bytes = WriteMd2(piece);
    CHECK(Read32(bytes, 0x2c) == bytes.size() - 4);
    Md2Model back;
    std::string err;
    CHECK(ReadMd2(bytes, back, err));
    CHECK(back.name == "piece.MD2");
    CHECK(back.textures.size() == 2 && back.textures[1].frames[0] == "b.tga");
    CHECK(back.nodes.size() == 2 && !back.nodes[0].mini && back.nodes[1].mini && back.nodes[1].parent == 0);
    CHECK(back.nodes[0].vertices.size() == piece.nodes[0].vertices.size());
    CHECK(back.nodes[0].faces.size() == 24 && back.nodes[0].faceSets.size() == 2);
    CHECK(back.nodes[0].cornerVertex == piece.nodes[0].cornerVertex);
    CHECK(back.nodes[0].points.size() == piece.nodes[0].points.size());
    CHECK(std::fabs(back.nodes[0].matrix[12] - 100.0f) < 1e-6f && std::fabs(back.nodes[1].matrix[14] - 3.0f) < 1e-6f);
    // Writing again gives the same bytes.
    CHECK(WriteMd2(back) == bytes);
    // Every face's stored normal agrees with its corners' winding.
    const Md2Node& n = back.nodes[0];
    int agree = 0;
    for (const Md2Face& f : n.faces) {
        const Vec3 a = n.vertices[n.cornerVertex[f.corner[0]]], b = n.vertices[n.cornerVertex[f.corner[1]]], c = n.vertices[n.cornerVertex[f.corner[2]]];
        const Vec3 e1{b.x - a.x, b.y - a.y, b.z - a.z}, e2{c.x - a.x, c.y - a.y, c.z - a.z};
        const Vec3 cr{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        const Vec3& nm = n.points[f.normal];
        if (cr.x * nm.x + cr.y * nm.y + cr.z * nm.z > 0) ++agree;
    }
    CHECK(agree == 24);
    // Garbage is refused.
    std::vector<uint8_t> junk(0x200, 0x55);
    CHECK(!ReadMd2(junk, back, err));
    std::vector<uint8_t> cut(bytes.begin(), bytes.begin() + 0x100);
    CHECK(!ReadMd2(cut, back, err));
}

// The channel order the islands' textures depend on: a 0x24 palette entry is R, G, B in the file and a 0x2a entry B, G,
// R, A, and the TGA the game reads is B, G, R, A. Pinned with one pair Theme Park Inc ships both ways (its flag texture:
// 0x24 entry 255, 78, 0 beside a .tga whose pixel bytes are 0, 78, 255), so a swap on either side shows here.
bool TgaPixelsAre(const std::vector<uint8_t>& tga, uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
    if (tga.size() < 18 + 4 || tga[2] != 2 || tga[16] != 32) return false;
    for (size_t at = 18; at + 4 <= tga.size() && at < 18 + 4 * 64; at += 4)
        if (tga[at] != b || tga[at + 1] != g || tga[at + 2] != r || tga[at + 3] != a) return false;
    return true;
}

void TestChannelOrder() {
    // Every pixel index 3 (the full shade), so every TGA pixel is the colour itself.
    auto solid = [](std::vector<uint8_t> fsh) {
        for (size_t at = 24 + 16; at < 24 + 16 + 8 * 8; ++at) fsh[at] = 3;
        return fsh;
    };
    std::vector<uint8_t> tga;
    std::string err;
    CHECK(TpiFshToTga(solid(MakeFsh(8, 8, 255, 78, 0)), tga, err));
    CHECK(TgaPixelsAre(tga, 0, 78, 255, 0xff));
    CHECK(TpiFshToTga(solid(MakeFsh(8, 8, 217, 163, 1, 0x2a, 153)), tga, err));
    CHECK(TgaPixelsAre(tga, 1, 163, 217, 153));
}

void TestAttachAndScale() {
    Md2Model into = MakePiece("isle", 10, 1, 10, {"ground.tga"});
    Md2Model piece = MakePiece("tree", 2, 8, 2, {"leaf.tga", "ground.tga"});
    std::array<float, 6> box{};
    CHECK(GeometryBox(piece, box) && std::fabs(box[4] - 9.0f) < 1e-5f && std::fabs(box[1]) < 1e-5f);
    BakeScale(piece, 0.5f);
    CHECK(GeometryBox(piece, box) && std::fabs(box[4] - 4.5f) < 1e-5f);
    std::string err;
    CHECK(AttachPiece(into, piece, Translation(3, 1, -2), err));
    // The shared texture name merged, the empty mini dropped, the root's own translation replaced by the placement.
    CHECK(into.textures.size() == 2);
    CHECK(into.nodes.size() == 3);
    CHECK(into.nodes[2].parent == 0 && std::fabs(into.nodes[2].matrix[12] - 3.0f) < 1e-6f);
    const std::vector<Matrix> world = WorldMatrices(into);
    CHECK(std::fabs(world[2][13] - 201.0f) < 1e-4f);   // the isle root's own translation (200) still counts here
    RecomputeBoxes(into);
    CHECK(std::fabs(into.box[4] - 5.5f) < 1e-4f);
}

void TestBuild() {
    const std::string root = "Game/";
    auto src = MakeCopy(root);
    TpiDisc disc;
    disc.source = src;
    disc.root = root;
    TpiIslandFiles out;
    std::string reason;
    CHECK(BuildTpiIslandFiles(disc, out, reason));
    if (!reason.empty()) std::printf("reason: %s\n", reason.c_str());
    const char* worlds[3] = {"arabian", "water", "science"};
    const char* prefixes[3] = {"ara", "pol", "inv"};
    for (int w = 0; w < 3; ++w) {
        const std::string folder = std::string("data/lobby/") + worlds[w] + "/", p = prefixes[w];
        CHECK(Has(out, std::string("data/lobby/") + worlds[w] + ".txt"));
        CHECK(Has(out, folder + p + "_isle.md2"));
        CHECK(Has(out, folder + p + "_gate.md2"));
        CHECK(Has(out, folder + p + "_gateM1.md2"));
        CHECK(Has(out, folder + p + "_gateM2.md2"));
        CHECK(Has(out, folder + p + "_gate.sgn"));
        CHECK(Has(out, folder + "qickload.txt") && Has(out, folder + "textures/qickload.txt") && Has(out, folder + "stexture/qickload.txt"));
        CHECK(Has(out, folder + "textures/stone.tga") && Has(out, folder + "stexture/stone.tga"));
        CHECK(Has(out, folder + "textures/sign1.tga") && Has(out, folder + "textures/sign2.tga"));
        CHECK(Has(out, folder + "textures/body.tga") && Has(out, folder + "textures/leaf.tga") && Has(out, folder + "textures/shared.tga"));
        CHECK(Has(out, folder + (w == 1 ? "textures/rock2.tga" : "textures/cliffblend.tga")) && Has(out, folder + "textures/rock1.tga"));
        CHECK(Has(out, folder + "textures/glow1.tga") && Has(out, folder + "textures/glow2.tga") && !Has(out, folder + "textures/glow.tga"));
        CHECK(!Has(out, folder + "stexture/glow.tga"));
        // The isle: textures renamed to lower case, the landmark's sign boards left out, pieces under the root.
        Md2Model isle;
        std::string err;
        CHECK(ReadMd2(File(out, folder + p + "_isle.md2"), isle, err));
        bool anySign = false, pieceSign = false;
        for (const Md2Texture& t : isle.textures)
            for (const std::string& f : t.frames) {
                if (f == "sign1.tga" || f == "sign2.tga") anySign = true;
                if (f.find("_sign.tga") != std::string::npos) pieceSign = true;
                CHECK(f.find(".tga") != std::string::npos && f.find("TGA") == std::string::npos);
            }
        CHECK(!anySign && !pieceSign);
        // The sign-named animating group keeps the wad's own pictures under the piece's names, not the plain board.
        const std::string lm = w == 0 ? "kingtut" : w == 1 ? "yeti" : "tesla";
        bool fieldKept = false;
        for (const Md2Texture& t : isle.textures)
            fieldKept = fieldKept || ((t.infoFlags & 1) && t.frames.size() == 3 && t.frames[0] == lm + "_sign1.tga" &&
                                      t.frames[1] == lm + "_sign2.tga" && t.frames[2] == "sign.tga");
        CHECK(fieldKept);
        CHECK(!Has(out, folder + "textures/sign.tga") && !Has(out, folder + "textures/" + lm + "_sign.tga"));
        // The boards' faces are gone: the landmark keeps only its body's face set, and its board node is dropped.
        CHECK(isle.nodes[1].faceSets.size() == 1 && isle.nodes[1].faces.size() == isle.nodes[1].faceSets[0].faceCount);
        for (const Md2Node& n : isle.nodes) CHECK(n.name != "sign");
        // Every face set still names its own faces (their corners are the set's), face sets out of face order included.
        for (const Md2Node& n : isle.nodes)
            for (const Md2FaceSet& s : n.faceSets) {
                bool own = size_t(s.firstFace) + s.faceCount <= n.faces.size();
                for (size_t f = s.firstFace; own && f < size_t(s.firstFace) + s.faceCount; ++f)
                    for (uint16_t c : n.faces[f].corner) own = own && c >= s.firstCorner && c <= s.lastCorner;
                CHECK(own);
            }
        for (const char* frame : {"_sign1.tga", "_sign2.tga"}) {
            const std::vector<uint8_t>& field = File(out, folder + "textures/" + lm + frame);
            CHECK(field.size() > 21 && field[18] == 50 && field[19] == 100 && field[20] == 200 && field[21] == 0xff);
        }
        CHECK(isle.nodes.size() == (w == 0 ? 4u : 3u));   // slab, landmark, scenery (arabian places ltree twice; minis dropped)
        CHECK(isle.nodes[0].name == "Island" && isle.nodes[1].parent == 0 && isle.nodes[2].parent == 0);
        // The landmark stands this tall on the slab top (9) and the scenery too.
        std::array<float, 6> box{};
        CHECK(GeometryBox(isle, box));
        CHECK(std::fabs(box[4] - (9.0f + (w == 2 ? 24.0f : 22.0f))) < 0.05f);
        CHECK(box[1] < 2.5f && box[1] > 1.5f);
        CHECK(box[0] < -22.0f && box[3] > 22.0f && box[2] < -38.0f && box[5] > 18.0f);   // the slab, waved coast included
        // The gate: at least 7 wide and 6 tall, standing on the top at z -14, root matrix identity, scaled shapes, sign textures kept.
        Md2Model gate;
        const std::vector<uint8_t>& gateBytes = File(out, folder + p + "_gate.md2");
        CHECK(ReadMd2(gateBytes, gate, err));
        CHECK(GeometryBox(gate, box));
        CHECK(std::fabs(box[4] - 15.0f) < 0.01f && std::fabs(box[1] - 9.0f) < 0.01f);   // 6 tall (the taller rule wins for a 40 by 31 gate)
        CHECK(std::fabs((box[0] + box[3]) / 2) < 0.01f && std::fabs((box[2] + box[5]) / 2 + 14.0f) < 0.01f);
        CHECK(std::fabs(box[3] - box[0] - 40.0f * 6.0f / 31.0f) < 0.01f);   // 40 wide at 6/31
        CHECK(std::fabs(ReadF(gateBytes, 0x80 + 16) - 15.0f) < 0.01f);   // the header's box follows   // the header's box follows
        CHECK(gate.nodes[0].matrix == IdentityMatrix());
        int signs = 0;
        for (const Md2Texture& t : gate.textures)
            for (const std::string& f : t.frames) signs += (f == "sign1.tga" || f == "sign2.tga");
        CHECK(signs == 2);
        const std::vector<uint8_t>& m1 = File(out, folder + p + "_gateM1.md2");
        const uint32_t anim = Read32(m1, 0x98), shape = Read32(m1, anim + 0x20), pts = Read32(m1, shape + 8);
        CHECK(std::fabs(ReadF(m1, pts) - 6.0f / 31.0f) < 1e-5f && std::fabs(ReadF(m1, pts + 32) - 9.0f * 6.0f / 31.0f) < 1e-5f);
        CHECK(File(out, folder + p + "_gate.sgn") == std::vector<uint8_t>({'s', 'g', 'n'}));
        // Textures are 32-bit TGAs of the decoded size; the quick-load list names every one, CRLF.
        const std::vector<uint8_t>& stone = File(out, folder + "textures/stone.tga");
        CHECK(stone.size() == 18 + 16 * 8 * 4 && stone[2] == 2 && stone[16] == 32);
        // A piece's texture is the decoder's bytes, no swap of the builder's own: the first stored pixel (the bottom row,
        // rows bottom-up) is index 3, the fixture's full colour R 200 G 100 B 50, so B, G, R, A = 50, 100, 200, 255.
        const std::vector<uint8_t>& body = File(out, folder + "textures/body.tga");
        CHECK(body.size() == 18 + 8 * 8 * 4 && body[18] == 50 && body[19] == 100 && body[20] == 200 && body[21] == 0xff);
        const std::vector<uint8_t>& list = File(out, folder + "qickload.txt");
        const std::string text(list.begin(), list.end());
        CHECK(text.find("stone.tga\r\n") != std::string::npos && text.find("sanded01.tga\r\n") != (w == 0 ? std::string::npos : 0));
        CHECK(text.find("\n") != std::string::npos && text.find("\r\n") != std::string::npos);
        // The descriptor.
        const std::vector<uint8_t>& desc = File(out, std::string("data/lobby/") + worlds[w] + ".txt");
        const std::string d(desc.begin(), desc.end());
        CHECK(d.find(std::string("ISLAND(") + std::to_string(4 + w) + ",\"data\\lobby\\" + worlds[w] + "\",\"" + p + "_isle\",\"" + p + "_gate\",\"") == 0);
        CHECK(d.find("SKYCOLOUR(") != std::string::npos);
        CHECK((d.find("RAINY(") != std::string::npos) == (w == 1));
    }
    // Missing ground textures (water's "top1" exists here, but a stem no fixture has) give a stand-in, never a failure:
    // the water and science recipes name "ltree" which the fixture lacks, so the log says so.
    bool leftOut = false;
    for (const std::string& line : out.log)
        if (line.find("ltree not found") != std::string::npos) leftOut = true;
    CHECK(leftOut);
    // Deterministic: a second build is byte for byte the same.
    TpiIslandFiles again;
    CHECK(BuildTpiIslandFiles(disc, again, reason));
    CHECK(again.files == out.files);
    // A copy without a gate fails with a reason naming the zone.
    auto bare = std::make_shared<MemorySource>();
    bare->Add("Data/levels/arabian/terrain.wad", BuildTpiWad({}));
    TpiDisc broken;
    broken.source = bare;
    broken.root = "";
    TpiIslandFiles none;
    CHECK(!BuildTpiIslandFiles(broken, none, reason));
    CHECK(reason.find("arabian") != std::string::npos);
}

}  // namespace

int main() {
    TestMatrices();
    TestRoundTrip();
    TestAttachAndScale();
    TestChannelOrder();
    TestBuild();
    std::printf("tpi_island: %d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
