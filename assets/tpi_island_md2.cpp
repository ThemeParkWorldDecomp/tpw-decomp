// tpi_island_md2.cpp: the in-memory M3D2 mesh of the Theme Park Inc island builder (tpi_island_md2.h). The file layout
// is include/m3d2.h's; the writer follows the order ps1_convert.cpp writes (header, textures, texture infos and names,
// vertex sets, points, face sets, faces, corner words, uv sets, nodes, mini nodes, names).
#include "tpi_island_md2.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <tuple>

namespace tpw::assets::tpi_island {

namespace {

constexpr uint32_t kMagic = 0x1cd15d46;
constexpr uint32_t kVersion = 0xdd;
constexpr uint32_t kAnimVersion = 0xcb;
constexpr size_t kHeader = 0xb8, kNode = 0xa0, kMini = 0x58, kName = 0x14;

uint16_t L16(const std::vector<uint8_t>& d, size_t at) { return static_cast<uint16_t>(d[at] | (d[at + 1] << 8)); }
uint32_t L32(const std::vector<uint8_t>& d, size_t at) {
    return static_cast<uint32_t>(d[at]) | (static_cast<uint32_t>(d[at + 1]) << 8) | (static_cast<uint32_t>(d[at + 2]) << 16) |
           (static_cast<uint32_t>(d[at + 3]) << 24);
}
float LF(const std::vector<uint8_t>& d, size_t at) {
    float f;
    std::memcpy(&f, d.data() + at, 4);
    return f;
}
bool In(const std::vector<uint8_t>& d, uint64_t at, uint64_t n) { return at + n <= d.size() && at + n >= at; }
std::string CStr(const std::vector<uint8_t>& d, size_t at, size_t max) {
    std::string s;
    for (size_t i = 0; i < max && at + i < d.size() && d[at + i] != 0; ++i) s.push_back(static_cast<char>(d[at + i]));
    return s;
}

void Put16(std::vector<uint8_t>& o, size_t at, uint32_t v) {
    o[at] = static_cast<uint8_t>(v);
    o[at + 1] = static_cast<uint8_t>(v >> 8);
}
void Put32(std::vector<uint8_t>& o, size_t at, uint32_t v) {
    Put16(o, at, v & 0xffff);
    Put16(o, at + 2, v >> 16);
}
void PutF(std::vector<uint8_t>& o, size_t at, float f) { std::memcpy(o.data() + at, &f, 4); }
void Add16(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(static_cast<uint8_t>(v));
    o.push_back(static_cast<uint8_t>(v >> 8));
}
void Add32(std::vector<uint8_t>& o, uint32_t v) {
    Add16(o, v & 0xffff);
    Add16(o, v >> 16);
}
void AddF(std::vector<uint8_t>& o, float f) {
    size_t at = o.size();
    o.resize(at + 4);
    PutF(o, at, f);
}
void Align4(std::vector<uint8_t>& o) {
    while (o.size() % 4) o.push_back(0);
}

std::string Lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

// ---- matrices --------------------------------------------------------------------------------------------------------

Matrix IdentityMatrix() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }
Matrix Translation(float x, float y, float z) {
    Matrix m = IdentityMatrix();
    m[12] = x;
    m[13] = y;
    m[14] = z;
    return m;
}
Matrix RotationY(float a) {
    // p * M with rows (cos, 0, -sin), (0, 1, 0), (sin, 0, cos): x' = x cos + z sin, z' = z cos - x sin.
    Matrix m = IdentityMatrix();
    const float c = std::cos(a), s = std::sin(a);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
    return m;
}
Matrix Multiply(const Matrix& a, const Matrix& b) {
    Matrix r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[i * 4 + k] * b[k * 4 + j];
            r[i * 4 + j] = s;
        }
    return r;
}
Vec3 Transform(const Vec3& p, const Matrix& m) {
    Vec3 r;
    r.x = p.x * m[0] + p.y * m[4] + p.z * m[8] + m[12];
    r.y = p.x * m[1] + p.y * m[5] + p.z * m[9] + m[13];
    r.z = p.x * m[2] + p.y * m[6] + p.z * m[10] + m[14];
    return r;
}

// ---- reader ----------------------------------------------------------------------------------------------------------

bool ReadMd2(const std::vector<uint8_t>& d, Md2Model& model, std::string& err) {
    model = Md2Model();
    if (d.size() < kHeader || L32(d, 0) != kMagic) {
        err = "not an M3D2 mesh";
        return false;
    }
    if (L32(d, 4) > kVersion) {
        err = "mesh version newer than the engine's";
        return false;
    }
    model.name = CStr(d, 0x18, kName);
    model.flags = L32(d, 0x30);
    if ((model.flags & 1) == 0) {
        err = "mesh without geometry";
        return false;
    }
    const uint32_t texCount = L16(d, 0x36), allNodes = L16(d, 0x42), nodeCount = L16(d, 0x44);
    const uint32_t texOff = L32(d, 0x50), infoOff = L32(d, 0x54), nodeOff = L32(d, 0x70), miniOff = L32(d, 0x74),
                   rootOff = L32(d, 0x78);
    for (int i = 0; i < 6; ++i) model.box[i] = LF(d, 0x80 + 4 * i);
    if (nodeCount == 0 || nodeCount > allNodes || !In(d, nodeOff, uint64_t(nodeCount) * kNode) ||
        (allNodes > nodeCount && !In(d, miniOff, uint64_t(allNodes - nodeCount) * kMini)) ||
        !In(d, texOff, uint64_t(texCount) * 8) || !In(d, infoOff, uint64_t(texCount) * 16)) {
        err = "mesh tables run past the end of the file";
        return false;
    }
    for (uint32_t i = 0; i < texCount; ++i) {
        Md2Texture t;
        t.material = L16(d, texOff + 8 * i);
        t.f02 = L16(d, texOff + 8 * i + 2);
        const size_t info = infoOff + 16 * i;
        t.infoFlags = L32(d, info);
        t.f08[0] = d[info + 8];
        t.f08[1] = d[info + 9];
        const uint32_t frames = L16(d, info + 0xa), names = L32(d, info + 0xc);
        if (frames == 0 || frames > 64 || !In(d, names, uint64_t(frames) * kName)) {
            err = "mesh texture names run past the end of the file";
            return false;
        }
        for (uint32_t f = 0; f < frames; ++f) t.frames.push_back(CStr(d, names + kName * f, kName));
        model.textures.push_back(std::move(t));
    }
    // Node records in file order: full nodes, then minis. Offsets name the parents; the root comes first in the model.
    std::vector<size_t> recordOff;
    std::vector<bool> recordMini;
    for (uint32_t i = 0; i < nodeCount; ++i) {
        recordOff.push_back(nodeOff + kNode * i);
        recordMini.push_back(false);
    }
    for (uint32_t i = 0; i < allNodes - nodeCount; ++i) {
        recordOff.push_back(miniOff + kMini * i);
        recordMini.push_back(true);
    }
    std::map<size_t, int> indexOfOffset;
    size_t rootRecord = 0;
    bool rootFound = false;
    for (size_t r = 0; r < recordOff.size(); ++r)
        if (recordOff[r] == rootOff) {
            rootRecord = r;
            rootFound = true;
        }
    if (!rootFound) {
        err = "mesh root node is not in its node tables";
        return false;
    }
    std::vector<size_t> order;
    order.push_back(rootRecord);
    for (size_t r = 0; r < recordOff.size(); ++r)
        if (r != rootRecord) order.push_back(r);
    for (size_t k = 0; k < order.size(); ++k) indexOfOffset[recordOff[order[k]]] = static_cast<int>(k);
    for (size_t k = 0; k < order.size(); ++k) {
        const size_t at = recordOff[order[k]];
        Md2Node n;
        n.mini = recordMini[order[k]];
        n.flags = L32(d, at);
        const uint32_t parentOff = L32(d, at + 4);
        if (parentOff == 0) {
            n.parent = -1;
        } else {
            auto it = indexOfOffset.find(parentOff);
            if (it == indexOfOffset.end()) {
                err = "mesh node with an unknown parent";
                return false;
            }
            n.parent = it->second;
        }
        if (k == 0 && n.parent != -1) {
            err = "mesh root node has a parent";
            return false;
        }
        if (k != 0 && n.parent == -1) {
            err = "mesh with more than one root node";
            return false;
        }
        for (int i = 0; i < 16; ++i) n.matrix[i] = LF(d, at + 0x10 + 4 * i);
        n.f52 = L16(d, at + 0x52);
        const uint32_t nameOff = L32(d, at + 0x54);
        n.name = nameOff && nameOff < d.size() ? CStr(d, nameOff, 64) : std::string();
        if (!n.mini) {
            const uint32_t nv = L16(d, at + 0x58), nfs = L16(d, at + 0x5a), nf = L16(d, at + 0x5c), nc = L16(d, at + 0x5e);
            const uint32_t vp = L32(d, at + 0x60), pp = L32(d, at + 0x64), up = L32(d, at + 0x68), fsp = L32(d, at + 0x6c),
                           fp = L32(d, at + 0x70), wp = L32(d, at + 0x94);
            for (int i = 0; i < 6; ++i) n.box[i] = LF(d, at + 0x78 + 4 * i);
            std::memcpy(n.f90, d.data() + at + 0x90, 4);
            if ((nv && !In(d, vp, uint64_t((nv + 3) / 4) * 0x30)) || (nc && !In(d, up, uint64_t((nc + 3) / 4) * 0x20)) ||
                (nc && !In(d, wp, uint64_t(nc) * 2)) || (nfs && !In(d, fsp, uint64_t(nfs) * 0x10)) ||
                (nf && !In(d, fp, uint64_t(nf) * 8))) {
                err = "mesh node tables run past the end of the file";
                return false;
            }
            for (uint32_t i = 0; i < nv; ++i) {
                const size_t set = vp + (i / 4) * 0x30, lane = (i % 4) * 4;
                n.vertices.push_back({LF(d, set + lane), LF(d, set + 16 + lane), LF(d, set + 32 + lane)});
            }
            for (uint32_t i = 0; i < nc; ++i) {
                const size_t set = up + (i / 4) * 0x20, lane = (i % 4) * 4;
                n.u.push_back(LF(d, set + lane));
                n.v.push_back(LF(d, set + 16 + lane));
                n.cornerVertex.push_back(L16(d, wp + 2 * i));
            }
            uint32_t pointCount = nc + nf;
            for (uint32_t i = 0; i < nf; ++i) {
                Md2Face f;
                f.normal = L16(d, fp + 8 * i);
                for (int k = 0; k < 3; ++k) f.corner[k] = L16(d, fp + 8 * i + 2 + 2 * k);
                pointCount = std::max<uint32_t>(pointCount, (f.normal & 0x7fff) + 1u);
                n.faces.push_back(f);
            }
            if (pointCount && !In(d, pp, uint64_t(pointCount) * 12)) {
                err = "mesh node normals run past the end of the file";
                return false;
            }
            for (uint32_t i = 0; i < pointCount; ++i) n.points.push_back({LF(d, pp + 12 * i), LF(d, pp + 12 * i + 4), LF(d, pp + 12 * i + 8)});
            for (uint32_t i = 0; i < nfs; ++i) {
                const size_t at2 = fsp + 0x10 * i;
                Md2FaceSet s;
                const uint32_t tp = L32(d, at2);
                if (tp != 0 && (tp < texOff || (tp - texOff) % 8 || (tp - texOff) / 8 >= texCount)) {
                    err = "mesh face set with an unknown texture";
                    return false;
                }
                s.texture = tp == 0 ? -1 : static_cast<int>((tp - texOff) / 8);
                s.firstFace = L16(d, at2 + 4);
                s.faceCount = L16(d, at2 + 6);
                s.firstCorner = L16(d, at2 + 8);
                s.lastCorner = L16(d, at2 + 0xa);
                std::memcpy(s.flags, d.data() + at2 + 0xc, 4);
                n.faceSets.push_back(s);
            }
        }
        model.nodes.push_back(std::move(n));
    }
    return true;
}

// ---- writer ----------------------------------------------------------------------------------------------------------

std::vector<uint8_t> WriteMd2(const Md2Model& model) {
    std::vector<uint8_t> out(kHeader, 0);
    Put32(out, 0, kMagic);
    Put32(out, 4, kVersion);
    Put32(out, 8, kAnimVersion);
    std::memcpy(out.data() + 0x18, model.name.data(), std::min<size_t>(model.name.size(), kName - 1));
    Put32(out, 0x30, model.flags & ~0x20u);
    const size_t texoff = out.size();
    for (const Md2Texture& t : model.textures) {
        Add16(out, t.material);
        Add16(out, t.f02);
        Add32(out, 0);
    }
    const size_t info = out.size();
    out.resize(info + 16 * model.textures.size(), 0);
    for (size_t i = 0; i < model.textures.size(); ++i) {
        const Md2Texture& t = model.textures[i];
        const size_t slot = out.size();
        const size_t frames = std::max<size_t>(1, t.frames.size());
        out.resize(slot + kName * frames, 0);
        for (size_t f = 0; f < t.frames.size(); ++f)
            std::memcpy(out.data() + slot + kName * f, t.frames[f].data(), std::min<size_t>(t.frames[f].size(), kName - 1));
        Put32(out, info + 16 * i, t.infoFlags);
        out[info + 16 * i + 8] = t.f08[0];
        out[info + 16 * i + 9] = t.f08[1];
        Put16(out, info + 16 * i + 0xa, static_cast<uint32_t>(frames));
        Put32(out, info + 16 * i + 0xc, static_cast<uint32_t>(slot));
    }
    std::vector<size_t> fulls, minis;
    for (size_t i = 0; i < model.nodes.size(); ++i) (model.nodes[i].mini ? minis : fulls).push_back(i);
    struct Offsets {
        size_t ov = 0, op = 0, ou = 0, ofs = 0, of = 0, ow = 0;
    };
    std::vector<Offsets> at(model.nodes.size());
    const size_t vo = out.size();
    for (size_t i : fulls) {
        const Md2Node& n = model.nodes[i];
        at[i].ov = n.vertices.empty() ? 0 : out.size();
        for (size_t k = 0; k < n.vertices.size(); k += 4) {
            for (int c = 0; c < 3; ++c)
                for (size_t j = 0; j < 4; ++j) {
                    const Vec3& p = n.vertices[std::min(k + j, n.vertices.size() - 1)];
                    AddF(out, c == 0 ? p.x : c == 1 ? p.y : p.z);
                }
        }
    }
    const size_t po = out.size();
    for (size_t i : fulls) {
        const Md2Node& n = model.nodes[i];
        at[i].op = n.points.empty() ? 0 : out.size();
        for (const Vec3& p : n.points) {
            AddF(out, p.x);
            AddF(out, p.y);
            AddF(out, p.z);
        }
    }
    const size_t fso = out.size();
    for (size_t i : fulls) {
        const Md2Node& n = model.nodes[i];
        at[i].ofs = n.faceSets.empty() ? 0 : out.size();
        for (const Md2FaceSet& s : n.faceSets) {
            Add32(out, s.texture < 0 ? 0u : static_cast<uint32_t>(texoff + 8 * s.texture));
            Add16(out, s.firstFace);
            Add16(out, s.faceCount);
            Add16(out, s.firstCorner);
            Add16(out, s.lastCorner);
            out.insert(out.end(), s.flags, s.flags + 4);
        }
    }
    const size_t fo = out.size();
    for (size_t i : fulls) {
        const Md2Node& n = model.nodes[i];
        at[i].of = n.faces.empty() ? 0 : out.size();
        for (const Md2Face& f : n.faces) {
            Add16(out, f.normal);
            for (int k = 0; k < 3; ++k) Add16(out, f.corner[k]);
        }
    }
    for (size_t i : fulls) {
        const Md2Node& n = model.nodes[i];
        at[i].ow = n.cornerVertex.empty() ? 0 : out.size();
        for (uint16_t w : n.cornerVertex) Add16(out, w);
        Align4(out);
    }
    const size_t uo = out.size();
    for (size_t i : fulls) {
        const Md2Node& n = model.nodes[i];
        at[i].ou = n.u.empty() ? 0 : out.size();
        for (size_t k = 0; k < n.u.size(); k += 4) {
            for (size_t j = 0; j < 4; ++j) AddF(out, n.u[std::min(k + j, n.u.size() - 1)]);
            for (size_t j = 0; j < 4; ++j) AddF(out, n.v[std::min(k + j, n.v.size() - 1)]);
        }
    }
    const size_t no = out.size(), mo = no + kNode * fulls.size(), so = mo + kMini * minis.size();
    std::vector<size_t> pos(model.nodes.size(), 0), id(model.nodes.size(), 0);
    for (size_t r = 0; r < fulls.size(); ++r) {
        pos[fulls[r]] = no + kNode * r;
        id[fulls[r]] = r;
    }
    for (size_t r = 0; r < minis.size(); ++r) {
        pos[minis[r]] = mo + kMini * r;
        id[minis[r]] = fulls.size() + r;
    }
    std::vector<std::vector<size_t>> kids(model.nodes.size());
    for (size_t i = 0; i < model.nodes.size(); ++i)
        if (model.nodes[i].parent >= 0) kids[static_cast<size_t>(model.nodes[i].parent)].push_back(i);
    std::vector<size_t> sibling(model.nodes.size(), 0);
    for (const auto& k : kids)
        for (size_t i = 1; i < k.size(); ++i) sibling[k[i - 1]] = pos[k[i]];
    std::vector<uint8_t> strings;
    auto name = [&](const std::string& s) {
        const size_t where = so + strings.size();
        strings.insert(strings.end(), s.begin(), s.end());
        strings.push_back(0);
        return static_cast<uint32_t>(where);
    };
    auto links = [&](std::vector<uint8_t>& r, size_t i) {
        const Md2Node& n = model.nodes[i];
        Put32(r, 0, n.flags);
        Put32(r, 4, n.parent >= 0 ? static_cast<uint32_t>(pos[static_cast<size_t>(n.parent)]) : 0);
        Put32(r, 8, static_cast<uint32_t>(sibling[i]));
        Put32(r, 12, kids[i].empty() ? 0 : static_cast<uint32_t>(pos[kids[i][0]]));
        for (int k = 0; k < 16; ++k) PutF(r, 0x10 + 4 * k, n.matrix[k]);
        Put16(r, 0x50, static_cast<uint32_t>(id[i]));
        Put16(r, 0x52, n.f52);
        Put32(r, 0x54, name(n.name));
    };
    for (size_t i : fulls) {
        const Md2Node& n = model.nodes[i];
        std::vector<uint8_t> r(kNode, 0);
        links(r, i);
        Put16(r, 0x58, static_cast<uint32_t>(n.vertices.size()));
        Put16(r, 0x5a, static_cast<uint32_t>(n.faceSets.size()));
        Put16(r, 0x5c, static_cast<uint32_t>(n.faces.size()));
        Put16(r, 0x5e, static_cast<uint32_t>(n.cornerVertex.size()));
        Put32(r, 0x60, static_cast<uint32_t>(at[i].ov));
        Put32(r, 0x64, static_cast<uint32_t>(at[i].op));
        Put32(r, 0x68, static_cast<uint32_t>(at[i].ou));
        Put32(r, 0x6c, static_cast<uint32_t>(at[i].ofs));
        Put32(r, 0x70, static_cast<uint32_t>(at[i].of));
        for (int k = 0; k < 6; ++k) PutF(r, 0x78 + 4 * k, n.box[k]);
        std::memcpy(r.data() + 0x90, n.f90, 4);
        Put32(r, 0x94, static_cast<uint32_t>(at[i].ow));
        out.insert(out.end(), r.begin(), r.end());
    }
    for (size_t i : minis) {
        std::vector<uint8_t> r(kMini, 0);
        links(r, i);
        out.insert(out.end(), r.begin(), r.end());
    }
    out.insert(out.end(), strings.begin(), strings.end());
    Align4(out);
    const size_t eo = out.size();
    uint32_t vsets = 0, usets = 0, nfs = 0, nf = 0, np = 0;
    for (size_t i : fulls) {
        const Md2Node& n = model.nodes[i];
        vsets += static_cast<uint32_t>((n.vertices.size() + 3) / 4);
        usets += static_cast<uint32_t>((n.u.size() + 3) / 4);
        nfs += static_cast<uint32_t>(n.faceSets.size());
        nf += static_cast<uint32_t>(n.faces.size());
        np += static_cast<uint32_t>(n.points.size());
    }
    const uint32_t counts[11] = {static_cast<uint32_t>(model.textures.size()), vsets, usets, nfs, nf, 0,
                                 static_cast<uint32_t>(model.nodes.size()), static_cast<uint32_t>(fulls.size()),
                                 static_cast<uint32_t>(fulls.size()), 0, np};
    for (int i = 0; i < 11; ++i) Put16(out, 0x36 + 2 * i, counts[i]);
    const size_t offsets[13] = {0, texoff, info, vo, po, uo, fso, fo, 0, no, mo, model.nodes.empty() ? 0 : pos[0], eo};
    for (int i = 0; i < 13; ++i) Put32(out, 0x4c + 4 * i, static_cast<uint32_t>(offsets[i]));
    for (int i = 0; i < 6; ++i) PutF(out, 0x80 + 4 * i, model.box[i]);
    Put32(out, 0x2c, static_cast<uint32_t>(out.size() - 4));
    return out;
}

// ---- edits -----------------------------------------------------------------------------------------------------------

namespace {
std::vector<Matrix> Worlds(const Md2Model& model, bool withRoot) {
    std::vector<Matrix> world(model.nodes.size());
    std::vector<char> done(model.nodes.size(), 0);
    std::function<void(size_t)> visit = [&](size_t i) {
        if (done[i]) return;
        done[i] = 1;
        const Md2Node& n = model.nodes[i];
        if (n.parent < 0) {
            world[i] = withRoot ? n.matrix : IdentityMatrix();
        } else {
            visit(static_cast<size_t>(n.parent));
            world[i] = Multiply(n.matrix, world[static_cast<size_t>(n.parent)]);
        }
    };
    for (size_t i = 0; i < model.nodes.size(); ++i) visit(i);
    return world;
}
}  // namespace

std::vector<Matrix> WorldMatrices(const Md2Model& model) { return Worlds(model, true); }

bool GeometryBox(const Md2Model& model, std::array<float, 6>& box) {
    const std::vector<Matrix> world = Worlds(model, false);
    bool any = false;
    for (size_t i = 0; i < model.nodes.size(); ++i)
        for (const Vec3& v : model.nodes[i].vertices) {
            const Vec3 p = Transform(v, world[i]);
            if (!any) {
                box = {p.x, p.y, p.z, p.x, p.y, p.z};
                any = true;
            } else {
                box[0] = std::min(box[0], p.x);
                box[1] = std::min(box[1], p.y);
                box[2] = std::min(box[2], p.z);
                box[3] = std::max(box[3], p.x);
                box[4] = std::max(box[4], p.y);
                box[5] = std::max(box[5], p.z);
            }
        }
    return any;
}

void BakeScale(Md2Model& model, float s) {
    for (Md2Node& n : model.nodes) {
        for (Vec3& v : n.vertices) {
            v.x *= s;
            v.y *= s;
            v.z *= s;
        }
        for (int k = 12; k < 15; ++k) n.matrix[k] *= s;
        for (float& b : n.box) b *= s;
    }
    for (float& b : model.box) b *= s;
}

void BakeRoot(Md2Model& model) {
    if (model.nodes.empty()) return;
    Md2Node& root = model.nodes[0];
    Matrix lin = root.matrix;
    lin[12] = lin[13] = lin[14] = 0;
    lin[15] = 1;
    if (lin == IdentityMatrix()) {
        root.matrix = IdentityMatrix();
        return;
    }
    for (Vec3& v : root.vertices) v = Transform(v, lin);
    for (Vec3& p : root.points) p = Transform(p, lin);
    for (size_t i = 1; i < model.nodes.size(); ++i)
        if (model.nodes[i].parent == 0) model.nodes[i].matrix = Multiply(model.nodes[i].matrix, lin);
    root.matrix = IdentityMatrix();
    RecomputeBoxes(model);
}

bool AttachPiece(Md2Model& into, const Md2Model& piece, const Matrix& placement, std::string& err) {
    if (piece.nodes.empty() || piece.nodes[0].parent != -1 || piece.nodes[0].mini) {
        err = "piece whose root is not a geometry node";
        return false;
    }
    if (into.nodes.empty() || into.nodes[0].parent != -1) {
        err = "island mesh without a root node";
        return false;
    }
    // Minis stay only where geometry hangs below them.
    std::vector<char> keep(piece.nodes.size(), 0);
    for (size_t i = 0; i < piece.nodes.size(); ++i) {
        if (piece.nodes[i].mini) continue;
        for (int k = static_cast<int>(i); k >= 0; k = piece.nodes[static_cast<size_t>(k)].parent) keep[static_cast<size_t>(k)] = 1;
    }
    std::vector<uint16_t> texMap(piece.textures.size(), 0);
    for (size_t t = 0; t < piece.textures.size(); ++t) {
        const Md2Texture& pt = piece.textures[t];
        size_t found = into.textures.size();
        for (size_t u = 0; u < into.textures.size(); ++u) {
            const Md2Texture& it = into.textures[u];
            if (it.material != pt.material || it.f02 != pt.f02 || it.infoFlags != pt.infoFlags || it.frames.size() != pt.frames.size())
                continue;
            bool same = true;
            for (size_t f = 0; f < pt.frames.size() && same; ++f) same = Lower(it.frames[f]) == Lower(pt.frames[f]);
            if (same) {
                found = u;
                break;
            }
        }
        if (found == into.textures.size()) into.textures.push_back(pt);
        texMap[t] = static_cast<uint16_t>(found);
    }
    // The kept nodes keep their order (a parent may come after its child in the file: the ltree pieces do that).
    std::vector<int> newIndex(piece.nodes.size(), -1);
    size_t next = into.nodes.size();
    for (size_t i = 0; i < piece.nodes.size(); ++i)
        if (keep[i]) newIndex[i] = static_cast<int>(next++);
    for (size_t i = 0; i < piece.nodes.size(); ++i) {
        if (!keep[i]) continue;
        Md2Node n = piece.nodes[i];
        if (n.parent < 0) {
            n.parent = 0;
            n.matrix = placement;   // a ride root's own matrix is overwritten by its placement in the game too
        } else {
            n.parent = newIndex[static_cast<size_t>(n.parent)];
        }
        for (Md2FaceSet& s : n.faceSets)
            if (s.texture >= 0) s.texture = texMap[static_cast<size_t>(s.texture)];
        into.nodes.push_back(std::move(n));
    }
    return true;
}

void RecomputeBoxes(Md2Model& model) {
    for (Md2Node& n : model.nodes) {
        if (n.vertices.empty()) {
            n.box = {0, 0, 0, 0, 0, 0};
            continue;
        }
        const Vec3& f = n.vertices[0];
        n.box = {f.x, f.y, f.z, f.x, f.y, f.z};
        for (const Vec3& v : n.vertices) {
            n.box[0] = std::min(n.box[0], v.x);
            n.box[1] = std::min(n.box[1], v.y);
            n.box[2] = std::min(n.box[2], v.z);
            n.box[3] = std::max(n.box[3], v.x);
            n.box[4] = std::max(n.box[4], v.y);
            n.box[5] = std::max(n.box[5], v.z);
        }
    }
    std::array<float, 6> box{};
    if (GeometryBox(model, box)) model.box = box;
}

// ---- generated geometry ------------------------------------------------------------------------------------------------

void NodeBuilder::Triangle(const Vec3& a, const Vec3& b, const Vec3& c, float ua, float va, float ub, float vb, float uc,
                           float vc, uint16_t texture) {
    tris_.push_back({{a, b, c}, {ua, ub, uc}, {va, vb, vc}, texture});
}

void NodeBuilder::Quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float ua, float va, float ub, float vb,
                       float uc, float vc, float ud, float vd, uint16_t texture) {
    Triangle(a, b, c, ua, va, ub, vb, uc, vc, texture);
    Triangle(a, c, d, ua, va, uc, vc, ud, vd, texture);
}

void NodeBuilder::Box(const Vec3& lo, const Vec3& hi, float s, uint16_t t) {
    // Six faces wound so that cross(b - a, c - a) points outwards (the engine's convention).
    const Vec3 a{lo.x, lo.y, lo.z}, b{hi.x, lo.y, lo.z}, c{hi.x, lo.y, hi.z}, d{lo.x, lo.y, hi.z};
    const Vec3 e{lo.x, hi.y, lo.z}, f{hi.x, hi.y, lo.z}, g{hi.x, hi.y, hi.z}, h{lo.x, hi.y, hi.z};
    Quad(e, h, g, f, lo.x / s, lo.z / s, lo.x / s, hi.z / s, hi.x / s, hi.z / s, hi.x / s, lo.z / s, t);   // top (+y)
    Quad(a, b, c, d, lo.x / s, lo.z / s, hi.x / s, lo.z / s, hi.x / s, hi.z / s, lo.x / s, hi.z / s, t);   // bottom (-y)
    Quad(a, e, f, b, lo.x / s, lo.y / s, lo.x / s, hi.y / s, hi.x / s, hi.y / s, hi.x / s, lo.y / s, t);   // front (-z)
    Quad(d, c, g, h, lo.x / s, lo.y / s, hi.x / s, lo.y / s, hi.x / s, hi.y / s, lo.x / s, hi.y / s, t);   // back (+z)
    Quad(a, d, h, e, lo.z / s, lo.y / s, hi.z / s, lo.y / s, hi.z / s, hi.y / s, lo.z / s, hi.y / s, t);   // left (-x)
    Quad(b, f, g, c, lo.z / s, lo.y / s, lo.z / s, hi.y / s, hi.z / s, hi.y / s, hi.z / s, lo.y / s, t);   // right (+x)
}

Md2Node NodeBuilder::Build(const std::string& name) const {
    Md2Node n;
    n.name = name;
    n.flags = 1;
    std::map<std::tuple<float, float, float>, uint16_t> vertexOf;
    std::vector<uint16_t> textures;
    for (const Tri& t : tris_)
        if (std::find(textures.begin(), textures.end(), t.texture) == textures.end()) textures.push_back(t.texture);
    std::sort(textures.begin(), textures.end());
    std::vector<Vec3> faceNormals;
    std::vector<Vec3> cornerNormals;
    for (uint16_t tex : textures) {
        Md2FaceSet set;
        set.texture = tex;
        set.firstFace = static_cast<uint16_t>(n.faces.size());
        set.firstCorner = static_cast<uint16_t>(n.cornerVertex.size());
        for (const Tri& t : tris_) {
            if (t.texture != tex) continue;
            const Vec3 e1{t.p[1].x - t.p[0].x, t.p[1].y - t.p[0].y, t.p[1].z - t.p[0].z};
            const Vec3 e2{t.p[2].x - t.p[0].x, t.p[2].y - t.p[0].y, t.p[2].z - t.p[0].z};
            Vec3 nrm{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
            const float len = std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
            if (len > 1e-12f) {
                nrm.x /= len;
                nrm.y /= len;
                nrm.z /= len;
            } else {
                nrm = {0, 1, 0};
            }
            Md2Face f;
            for (int k = 0; k < 3; ++k) {
                const auto key = std::make_tuple(t.p[k].x, t.p[k].y, t.p[k].z);
                auto it = vertexOf.find(key);
                if (it == vertexOf.end()) {
                    it = vertexOf.emplace(key, static_cast<uint16_t>(n.vertices.size())).first;
                    n.vertices.push_back(t.p[k]);
                }
                f.corner[k] = static_cast<uint16_t>(n.cornerVertex.size());
                n.cornerVertex.push_back(it->second);
                n.u.push_back(t.u[k]);
                n.v.push_back(t.v[k]);
                cornerNormals.push_back(nrm);
            }
            faceNormals.push_back(nrm);
            n.faces.push_back(f);
        }
        set.faceCount = static_cast<uint16_t>(n.faces.size() - set.firstFace);
        set.lastCorner = static_cast<uint16_t>(n.cornerVertex.empty() ? 0 : n.cornerVertex.size() - 1);
        n.faceSets.push_back(set);
    }
    const uint16_t nc = static_cast<uint16_t>(cornerNormals.size());
    for (size_t i = 0; i < n.faces.size(); ++i) n.faces[i].normal = static_cast<uint16_t>(nc + i);
    n.points = cornerNormals;
    n.points.insert(n.points.end(), faceNormals.begin(), faceNormals.end());
    if (!n.vertices.empty()) {
        const Vec3& f = n.vertices[0];
        n.box = {f.x, f.y, f.z, f.x, f.y, f.z};
        for (const Vec3& v : n.vertices) {
            n.box[0] = std::min(n.box[0], v.x);
            n.box[1] = std::min(n.box[1], v.y);
            n.box[2] = std::min(n.box[2], v.z);
            n.box[3] = std::max(n.box[3], v.x);
            n.box[4] = std::max(n.box[4], v.y);
            n.box[5] = std::max(n.box[5], v.z);
        }
    }
    return n;
}

}  // namespace tpw::assets::tpi_island
