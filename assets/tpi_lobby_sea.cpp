// tpi_lobby_sea.cpp: the island screen's sea, made larger (tpi_lobby_sea.h).
#include "tpi_lobby_sea.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "tpi_ripple.h"

namespace tpw::assets::tpi_island {

namespace {

constexpr double kTau = 6.283185307179586;
constexpr size_t kHeader = 0xb8, kEmbedded = 0x14;

uint16_t L16(const std::vector<uint8_t>& d, size_t at) { return static_cast<uint16_t>(d[at] | (d[at + 1] << 8)); }
uint32_t L32(const std::vector<uint8_t>& d, size_t at) {
    return static_cast<uint32_t>(d[at]) | (static_cast<uint32_t>(d[at + 1]) << 8) | (static_cast<uint32_t>(d[at + 2]) << 16) |
           (static_cast<uint32_t>(d[at + 3]) << 24);
}
void Put16(std::vector<uint8_t>& o, size_t at, uint32_t v) {
    o[at] = static_cast<uint8_t>(v);
    o[at + 1] = static_cast<uint8_t>(v >> 8);
}
void Put32(std::vector<uint8_t>& o, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) o[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
}

// Solves the 3 by 3 system m x = b (Gaussian elimination with pivoting). False when it is singular.
bool Solve3(double m[3][3], double b[3], double x[3]) {
    for (int c = 0; c < 3; ++c) {
        int p = c;
        for (int r = c + 1; r < 3; ++r)
            if (std::fabs(m[r][c]) > std::fabs(m[p][c])) p = r;
        if (std::fabs(m[p][c]) < 1e-12) return false;
        std::swap(m[p], m[c]);
        std::swap(b[p], b[c]);
        for (int r = c + 1; r < 3; ++r) {
            const double f = m[r][c] / m[c][c];
            for (int k = c; k < 3; ++k) m[r][k] -= f * m[c][k];
            b[r] -= f * b[c];
        }
    }
    for (int r = 2; r >= 0; --r) {
        double s = b[r];
        for (int k = r + 1; k < 3; ++k) s -= m[r][k] * x[k];
        x[r] = s / m[r][r];
    }
    return true;
}

// The least-squares plane through the corners' (x, z) to (u, v), and the largest miss.
bool FitPlane(const Md2Node& node, UvPlane& plane, double& worst) {
    double m[3][3] = {}, bu[3] = {}, bv[3] = {};
    for (size_t c = 0; c < node.u.size(); ++c) {
        const Vec3& p = node.vertices[node.cornerVertex[c]];
        const double row[3] = {1, p.x, p.z};
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) m[i][j] += row[i] * row[j];
            bu[i] += row[i] * node.u[c];
            bv[i] += row[i] * node.v[c];
        }
    }
    double m2[3][3];
    std::copy(&m[0][0], &m[0][0] + 9, &m2[0][0]);
    if (!Solve3(m, bu, plane.u) || !Solve3(m2, bv, plane.v)) return false;
    worst = 0;
    for (size_t c = 0; c < node.u.size(); ++c) {
        const Vec3& p = node.vertices[node.cornerVertex[c]];
        worst = std::max(worst, std::fabs(static_cast<double>(plane.U(p.x, p.z)) - node.u[c]));
        worst = std::max(worst, std::fabs(static_cast<double>(plane.V(p.x, p.z)) - node.v[c]));
    }
    return true;
}

float NormalY(const Vec3& a, const Vec3& b, const Vec3& c) {
    return (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z);
}

}  // namespace

float LobbySeaRadius(float centreX, float centreZ) {
    float reach = 0;
    for (float x : {kLobbyReachMinX, kLobbyReachMaxX})
        for (float z : {kLobbyReachMinZ, kLobbyReachMaxZ}) reach = std::max(reach, std::hypot(x - centreX, z - centreZ));
    return reach + kLobbyFarPlane * kLobbyViewSlant + kLobbySeaMargin;
}

std::vector<SeaRing> SeaRingPlan(float edgeRadius, int edgeSides, float outerRadius, float step, float maxSide) {
    std::vector<SeaRing> plan;
    if (edgeSides < 3 || !(step > 0) || !(maxSide > 0) || !(edgeRadius > 0)) return plan;
    float r = edgeRadius;
    int sides = edgeSides;
    while (r < outerRadius) {
        r += step;
        if (kTau * r / sides > maxSide) sides *= 2;
        plan.push_back({r, sides});
    }
    return plan;
}

bool ExtendSeaNode(Md2Node& node, float outerRadius, UvPlane& plane, size_t& firstNew, std::string& err) {
    if (node.mini || node.faces.empty() || node.faceSets.empty() || node.u.size() != node.cornerVertex.size() ||
        node.points.size() < node.u.size()) {
        err = "node " + node.name + " has no geometry to extend";
        return false;
    }
    // The edge: the sides only one face has.
    std::map<std::pair<uint16_t, uint16_t>, int> uses;
    for (const Md2Face& f : node.faces)
        for (int i = 0; i < 3; ++i) {
            uint16_t a = node.cornerVertex[f.corner[i]], b = node.cornerVertex[f.corner[(i + 1) % 3]];
            if (a > b) std::swap(a, b);
            ++uses[{a, b}];
        }
    std::map<uint16_t, std::vector<uint16_t>> next;
    size_t edgeSides = 0;
    for (const auto& [side, count] : uses)
        if (count == 1) {
            next[side.first].push_back(side.second);
            next[side.second].push_back(side.first);
            ++edgeSides;
        }
    if (next.size() < 3 || next.size() != edgeSides) {
        err = "node " + node.name + ": its edge is not one loop";
        return false;
    }
    std::vector<uint16_t> loop{next.begin()->first};
    uint16_t prev = loop[0], at = next.begin()->second[0];
    while (at != loop[0]) {
        const auto& n = next[at];
        if (n.size() != 2 || loop.size() > edgeSides) {
            err = "node " + node.name + ": its edge is not one loop";
            return false;
        }
        loop.push_back(at);
        const uint16_t to = n[0] == prev ? n[1] : n[0];
        prev = at;
        at = to;
    }
    if (loop.size() != edgeSides) {
        err = "node " + node.name + ": its edge is more than one loop";
        return false;
    }
    // Round the origin anticlockwise (angle atan2(z, x) rising), at one height.
    auto angle = [&](uint16_t v) { return std::atan2(static_cast<double>(node.vertices[v].z), static_cast<double>(node.vertices[v].x)); };
    double turn = 0;
    for (size_t i = 0; i < loop.size(); ++i) {
        double d = angle(loop[(i + 1) % loop.size()]) - angle(loop[i]);
        if (d > kTau / 2) d -= kTau;
        if (d < -kTau / 2) d += kTau;
        if (std::fabs(d) < 1e-6) {
            err = "node " + node.name + ": its edge doubles back";
            return false;
        }
        turn += d;
    }
    if (std::fabs(std::fabs(turn) - kTau) > 1e-3) {
        err = "node " + node.name + ": its edge does not go round its centre";
        return false;
    }
    if (turn < 0) std::reverse(loop.begin(), loop.end());
    float edgeRadius = 0, y = 0;
    for (uint16_t v : loop) {
        edgeRadius = std::max(edgeRadius, std::hypot(node.vertices[v].x, node.vertices[v].z));
        y += node.vertices[v].y;
    }
    y /= static_cast<float>(loop.size());
    for (uint16_t v : loop)
        if (std::fabs(node.vertices[v].y - y) > 0.01f) {
            err = "node " + node.name + ": its edge is not level";
            return false;
        }
    double worst = 0;
    if (!FitPlane(node, plane, worst) || worst > 0.01) {
        err = "node " + node.name + ": its texture is not mapped flat (miss " + std::to_string(worst) + ")";
        return false;
    }
    // Which way the faces turn (seen from above) and which way their normals point.
    const Md2Face& f0 = node.faces[0];
    const float wind = NormalY(node.vertices[node.cornerVertex[f0.corner[0]]], node.vertices[node.cornerVertex[f0.corner[1]]],
                               node.vertices[node.cornerVertex[f0.corner[2]]]);
    const float faceUp = f0.normal < node.points.size() && node.points[f0.normal].y < 0 ? -1.0f : 1.0f;

    const std::vector<SeaRing> plan =
        SeaRingPlan(edgeRadius, static_cast<int>(loop.size()), outerRadius, kLobbySeaStep, kLobbySeaMaxSide);
    const size_t oldCorners = node.u.size();
    std::vector<int> cornerOf(node.vertices.size(), -1);
    for (size_t c = 0; c < oldCorners; ++c)
        if (cornerOf[node.cornerVertex[c]] < 0) cornerOf[node.cornerVertex[c]] = static_cast<int>(c);
    std::vector<uint16_t> ring;
    std::vector<double> angles;
    for (uint16_t v : loop) {
        ring.push_back(static_cast<uint16_t>(cornerOf[v]));
        angles.push_back(angle(v));
    }
    Md2Node out = node;
    std::vector<Vec3> cornerNormals(node.points.begin(), node.points.begin() + static_cast<std::ptrdiff_t>(oldCorners));
    std::vector<Vec3> faceNormals(node.points.begin() + static_cast<std::ptrdiff_t>(oldCorners), node.points.end());
    std::vector<Md2Face> newFaces;
    auto corner = [&](const Vec3& p) {
        out.vertices.push_back(p);
        out.cornerVertex.push_back(static_cast<uint16_t>(out.vertices.size() - 1));
        out.u.push_back(plane.U(p.x, p.z));
        out.v.push_back(plane.V(p.x, p.z));
        cornerNormals.push_back(Vec3{0, 1, 0});
        return static_cast<uint16_t>(out.u.size() - 1);
    };
    auto pos = [&](uint16_t c) { return out.vertices[out.cornerVertex[c]]; };
    auto tri = [&](uint16_t a, uint16_t b, uint16_t c) {
        if ((NormalY(pos(a), pos(b), pos(c)) < 0) != (wind < 0)) std::swap(b, c);
        Md2Face f;
        f.corner[0] = a;
        f.corner[1] = b;
        f.corner[2] = c;
        newFaces.push_back(f);
    };
    for (const SeaRing& r : plan) {
        std::vector<double> nextAngles;
        const bool doubled = static_cast<size_t>(r.sides) == 2 * angles.size();
        for (size_t j = 0; j < angles.size(); ++j) {
            nextAngles.push_back(angles[j]);
            if (doubled) {
                double d = angles[(j + 1) % angles.size()] - angles[j];
                if (d < 0) d += kTau;
                nextAngles.push_back(angles[j] + d / 2);
            }
        }
        std::vector<uint16_t> nextRing;
        for (double a : nextAngles)
            nextRing.push_back(corner(Vec3{static_cast<float>(r.radius * std::cos(a)), y, static_cast<float>(r.radius * std::sin(a))}));
        const size_t n = ring.size();
        for (size_t j = 0; j < n; ++j) {
            const uint16_t a = ring[j], b = ring[(j + 1) % n];
            if (doubled) {
                const uint16_t p = nextRing[2 * j], m = nextRing[2 * j + 1], q = nextRing[(2 * j + 2) % (2 * n)];
                tri(a, b, m);
                tri(a, m, p);
                tri(b, q, m);
            } else {
                const uint16_t p = nextRing[j], q = nextRing[(j + 1) % n];
                tri(a, b, q);
                tri(a, q, p);
            }
        }
        ring = std::move(nextRing);
        angles = std::move(nextAngles);
    }
    if (out.u.size() > 0xffff || out.faces.size() + newFaces.size() > 0xffff ||
        cornerNormals.size() + faceNormals.size() + newFaces.size() > 0xffff) {
        err = "node " + node.name + ": too many corners for one node";
        return false;
    }
    // The points: the corner normals, then the face normals (the old faces' indices move up past the new corners).
    const uint16_t moved = static_cast<uint16_t>(cornerNormals.size() - oldCorners);
    for (Md2Face& f : out.faces)
        if (f.normal >= oldCorners) f.normal = static_cast<uint16_t>(f.normal + moved);
    out.points = cornerNormals;
    out.points.insert(out.points.end(), faceNormals.begin(), faceNormals.end());
    for (Md2Face& f : newFaces) {
        f.normal = static_cast<uint16_t>(out.points.size());
        out.points.push_back(Vec3{0, faceUp, 0});
        out.faces.push_back(f);
    }
    Md2FaceSet& set = out.faceSets.back();
    if (static_cast<size_t>(set.firstFace) + set.faceCount != node.faces.size() || static_cast<size_t>(set.lastCorner) + 1 != oldCorners) {
        err = "node " + node.name + ": its last face set does not end the node";
        return false;
    }
    set.faceCount = static_cast<uint16_t>(set.faceCount + newFaces.size());
    set.lastCorner = static_cast<uint16_t>(out.u.size() - 1);
    const Vec3& v0 = out.vertices[0];
    out.box = {v0.x, v0.y, v0.z, v0.x, v0.y, v0.z};
    for (const Vec3& v : out.vertices) {
        out.box[0] = std::min(out.box[0], v.x);
        out.box[1] = std::min(out.box[1], v.y);
        out.box[2] = std::min(out.box[2], v.z);
        out.box[3] = std::max(out.box[3], v.x);
        out.box[4] = std::max(out.box[4], v.y);
        out.box[5] = std::max(out.box[5], v.z);
    }
    firstNew = oldCorners;
    node = std::move(out);
    return true;
}

bool ReadEmbeddedRecords(const std::vector<uint8_t>& mesh, std::vector<uint8_t>& records, uint16_t& count, std::string& err) {
    records.clear();
    count = 0;
    if (mesh.size() < kHeader) {
        err = "too short for a mesh";
        return false;
    }
    const uint16_t n = L16(mesh, 0x48);
    const uint32_t at = L32(mesh, 0x7c);
    if (n == 0 || at == 0) return true;
    if (static_cast<uint64_t>(at) + uint64_t(n) * kEmbedded > mesh.size()) {
        err = "its embedded records run past its end";
        return false;
    }
    for (uint16_t i = 0; i < n; ++i)
        if (L32(mesh, at + i * kEmbedded + 0x0c) != 0 || L32(mesh, at + i * kEmbedded + 0x10) != 0) {
            err = "an embedded record points to more data";
            return false;
        }
    records.assign(mesh.begin() + at, mesh.begin() + at + n * kEmbedded);
    count = n;
    return true;
}

void AppendEmbeddedRecords(std::vector<uint8_t>& mesh, const std::vector<uint8_t>& records, uint16_t count) {
    if (count == 0 || mesh.size() < kHeader) return;
    while (mesh.size() % 4) mesh.push_back(0);
    const uint32_t at = static_cast<uint32_t>(mesh.size());
    mesh.insert(mesh.end(), records.begin(), records.end());
    Put16(mesh, 0x48, count);
    Put32(mesh, 0x7c, at);
    Put32(mesh, 0x2c, static_cast<uint32_t>(mesh.size() - 4));   // as WriteMd2 sets it
}

bool ExtendLobbySea(const std::vector<uint8_t>& base, const std::vector<uint8_t>& anim, std::vector<uint8_t>& outBase,
                    std::vector<uint8_t>& outAnim, std::string& err) {
    Md2Model model;
    if (!ReadMd2(base, model, err, true)) {
        err = "Base.MD2: " + err;
        return false;
    }
    std::vector<uint8_t> records;
    uint16_t recordCount = 0;
    if (!ReadEmbeddedRecords(base, records, recordCount, err)) {
        err = "Base.MD2: " + err;
        return false;
    }
    UvAnim slide;
    if (!ReadUvAnim(anim, slide, err)) {
        err = "Basem1.MD2: " + err;
        return false;
    }
    if (slide.other) {
        err = "Basem1.MD2 holds more than texture slides";
        return false;
    }
    const std::vector<Matrix> world = WorldMatrices(model);
    size_t extended = 0;
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        Md2Node& node = model.nodes[i];
        if (node.mini || node.faces.empty()) continue;
        const Matrix& m = world[i];
        // The node's frame may be moved and turned about y (Sea_Under is), not tilted or scaled, so that a distance in
        // its x and z is one in the lobby's.
        if (std::fabs(m[5] - 1) > 1e-4f || std::fabs(m[1]) > 1e-4f || std::fabs(m[4]) > 1e-4f || std::fabs(m[6]) > 1e-4f ||
            std::fabs(m[9]) > 1e-4f || std::fabs(m[0] * m[0] + m[2] * m[2] - 1) > 1e-3f) {
            err = "Base.MD2 node " + node.name + " is tilted or scaled";
            return false;
        }
        const Vec3 centre = Transform(Vec3{0, 0, 0}, m);
        const size_t oldCorners = node.u.size();
        UvPlane plane;
        size_t firstNew = 0;
        if (!ExtendSeaNode(node, LobbySeaRadius(centre.x, centre.z), plane, firstNew, err)) {
            err = "Base.MD2 " + err;
            return false;
        }
        const int fileIndex = FileNodeIndex(model, i);
        for (UvNodeAnim& track : slide.nodes) {
            if (track.Node() != fileIndex) continue;
            if (track.tracks.size() != oldCorners) {
                err = "Basem1.MD2: node " + node.name + " has " + std::to_string(track.tracks.size()) + " tracks for " +
                      std::to_string(oldCorners) + " corners";
                return false;
            }
            // Every old corner's keys: the same frames, each the corner's own (u, v) plus the same slide.
            const std::vector<UvKey> first = track.tracks[0];
            for (size_t c = 0; c < oldCorners; ++c) {
                const std::vector<UvKey>& keys = track.tracks[c];
                if (keys.size() != first.size() || keys.empty()) {
                    err = "Basem1.MD2: node " + node.name + "'s corners slide differently";
                    return false;
                }
                for (size_t k = 0; k < keys.size(); ++k)
                    if (keys[k].frame != first[k].frame || std::fabs((keys[k].u - keys[0].u) - (first[k].u - first[0].u)) > 1e-3f ||
                        std::fabs((keys[k].v - keys[0].v) - (first[k].v - first[0].v)) > 1e-3f ||
                        std::fabs(keys[0].u - node.u[c]) > 0.01f || std::fabs(keys[0].v - node.v[c]) > 0.01f) {
                        err = "Basem1.MD2: node " + node.name + "'s corners slide differently";
                        return false;
                    }
            }
            for (size_t c = firstNew; c < node.u.size(); ++c) {
                std::vector<UvKey> keys;
                for (const UvKey& k : first) keys.push_back({k.frame, node.u[c] + (k.u - first[0].u), node.v[c] + (k.v - first[0].v)});
                track.tracks.push_back(std::move(keys));
            }
        }
        ++extended;
    }
    if (extended == 0) {
        err = "Base.MD2 has no sea to extend";
        return false;
    }
    std::array<float, 6> box{};
    if (GeometryBox(model, box)) model.box = box;
    outBase = WriteMd2(model);
    AppendEmbeddedRecords(outBase, records, recordCount);
    std::string name(reinterpret_cast<const char*>(anim.data() + 0x18), 0x13);
    name = name.c_str();
    outAnim = WriteUvAnim(outBase, name, slide);
    return true;
}

}  // namespace tpw::assets::tpi_island
