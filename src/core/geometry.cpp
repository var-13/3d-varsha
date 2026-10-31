#include "core/geometry.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace thermobench {
namespace geometry {

namespace {

inline double triangle_area(const Vec3& a, const Vec3& b, const Vec3& c) {
    // AB = b - a, AC = c - a
    double ab_x = b.x - a.x;
    double ab_y = b.y - a.y;
    double ab_z = b.z - a.z;

    double ac_x = c.x - a.x;
    double ac_y = c.y - a.y;
    double ac_z = c.z - a.z;

    // Cross product
    double cp_x = ab_y * ac_z - ab_z * ac_y;
    double cp_y = ab_z * ac_x - ab_x * ac_z;
    double cp_z = ab_x * ac_y - ab_y * ac_x;

    return 0.5 * std::sqrt(cp_x * cp_x + cp_y * cp_y + cp_z * cp_z);
}

inline bool is_valid_vec3(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

} // namespace

StlMesh parse_stl_string(const std::string& content) {
    if (content.empty()) {
        throw std::invalid_argument("STL content is empty.");
    }

    std::istringstream stream(content);
    std::string token;
    stream >> token;

    if (token != "solid") {
        throw std::invalid_argument("Invalid ASCII STL: file must start with 'solid'.");
    }

    StlMesh mesh;
    std::getline(stream, mesh.name);
    // Trim leading whitespace on name
    size_t first = mesh.name.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
        mesh.name = mesh.name.substr(first);
    } else {
        mesh.name = "unnamed";
    }

    while (stream >> token) {
        if (token == "endsolid") {
            break;
        }

        if (token == "facet") {
            Triangle tri;
            std::string normal_kw;
            stream >> normal_kw;
            if (normal_kw != "normal") {
                throw std::invalid_argument("Expected 'normal' after 'facet'.");
            }
            if (!(stream >> tri.normal.x >> tri.normal.y >> tri.normal.z)) {
                throw std::invalid_argument("Failed to parse facet normal coordinates.");
            }
            if (!is_valid_vec3(tri.normal)) {
                throw std::invalid_argument("Facet normal contains non-finite numbers (NaN/Inf).");
            }

            std::string outer_kw, loop_kw;
            stream >> outer_kw >> loop_kw;
            if (outer_kw != "outer" || loop_kw != "loop") {
                throw std::invalid_argument("Expected 'outer loop'.");
            }

            // 3 vertices
            Vec3* verts[3] = {&tri.v1, &tri.v2, &tri.v3};
            for (int i = 0; i < 3; ++i) {
                std::string vertex_kw;
                stream >> vertex_kw;
                if (vertex_kw != "vertex") {
                    throw std::invalid_argument("Expected 'vertex' keyword.");
                }
                if (!(stream >> verts[i]->x >> verts[i]->y >> verts[i]->z)) {
                    throw std::invalid_argument("Failed to parse vertex coordinates.");
                }
                if (!is_valid_vec3(*verts[i])) {
                    throw std::invalid_argument("Vertex contains non-finite numbers (NaN/Inf).");
                }
                mesh.bbox.expand(*verts[i]);
            }

            std::string endloop_kw, endfacet_kw;
            stream >> endloop_kw >> endfacet_kw;
            if (endloop_kw != "endloop" || endfacet_kw != "endfacet") {
                throw std::invalid_argument("Expected 'endloop' and 'endfacet'.");
            }

            mesh.surface_area += triangle_area(tri.v1, tri.v2, tri.v3);
            mesh.triangles.push_back(tri);
        }
    }

    if (mesh.triangles.empty()) {
        throw std::invalid_argument("ASCII STL contains zero triangles.");
    }

    return mesh;
}

StlMesh parse_stl_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open STL file: " + filepath);
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return parse_stl_string(content);
}

bool validate_stl(const StlMesh& mesh) {
    if (mesh.triangles.empty()) return false;
    if (!std::isfinite(mesh.surface_area) || mesh.surface_area <= 0.0) return false;
    if (!is_valid_vec3(mesh.bbox.min) || !is_valid_vec3(mesh.bbox.max)) return false;
    if (mesh.bbox.min.x > mesh.bbox.max.x ||
        mesh.bbox.min.y > mesh.bbox.max.y ||
        mesh.bbox.min.z > mesh.bbox.max.z) {
        return false;
    }
    for (const auto& tri : mesh.triangles) {
        if (!is_valid_vec3(tri.v1) || !is_valid_vec3(tri.v2) || !is_valid_vec3(tri.v3)) return false;
        if (!is_valid_vec3(tri.normal)) return false;
    }
    return true;
}

PointCloud parse_ansys_csv_string(const std::string& content) {
    if (content.empty()) {
        throw std::invalid_argument("CSV content is empty.");
    }

    std::istringstream stream(content);
    std::string line;

    // Header check
    if (!std::getline(stream, line)) {
        throw std::invalid_argument("Empty CSV file.");
    }

    // Clean carriage return
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::transform(line.begin(), line.end(), line.begin(), ::tolower);
    if (line.find("node") == std::string::npos || line.find("temperature") == std::string::npos) {
        throw std::invalid_argument("CSV header does not match required schema: node_id,x_m,y_m,z_m,temperature_C");
    }

    PointCloud pc;

    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::istringstream liness(line);
        std::string s_id, s_x, s_y, s_z, s_t;

        if (!std::getline(liness, s_id, ',') ||
            !std::getline(liness, s_x, ',') ||
            !std::getline(liness, s_y, ',') ||
            !std::getline(liness, s_z, ',') ||
            !std::getline(liness, s_t, ',')) {
            throw std::invalid_argument("Malformed row in CSV: " + line);
        }

        AnsysNode node;
        try {
            node.id = std::stoi(s_id);
            node.x = std::stod(s_x);
            node.y = std::stod(s_y);
            node.z = std::stod(s_z);
            node.temperature_C = std::stod(s_t);
        } catch (const std::exception&) {
            throw std::invalid_argument("Non-numeric value in CSV line: " + line);
        }

        if (!std::isfinite(node.x) || !std::isfinite(node.y) || !std::isfinite(node.z) || !std::isfinite(node.temperature_C)) {
            throw std::invalid_argument("Non-finite numerical value in node line: " + line);
        }

        if (node.temperature_C < pc.min_T) pc.min_T = node.temperature_C;
        if (node.temperature_C > pc.max_T) pc.max_T = node.temperature_C;

        Vec3 pos{node.x, node.y, node.z};
        pc.bbox.expand(pos);

        pc.nodes.push_back(node);
    }

    if (pc.nodes.empty()) {
        throw std::invalid_argument("CSV contained no data rows.");
    }

    return pc;
}

PointCloud parse_ansys_csv_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open ANSYS CSV file: " + filepath);
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return parse_ansys_csv_string(content);
}

bool validate_pointcloud(const PointCloud& pc) {
    if (pc.nodes.empty()) return false;
    if (!std::isfinite(pc.min_T) || !std::isfinite(pc.max_T)) return false;
    if (pc.min_T > pc.max_T) return false;
    if (!is_valid_vec3(pc.bbox.min) || !is_valid_vec3(pc.bbox.max)) return false;
    return true;
}

std::string stl_to_json(const StlMesh& mesh) {
    std::ostringstream oss;
    oss << "{\"name\":\"" << mesh.name << "\""
        << ",\"triangle_count\":" << mesh.triangles.size()
        << ",\"surface_area\":" << mesh.surface_area
        << ",\"bbox\":{\"min\":[" << mesh.bbox.min.x << "," << mesh.bbox.min.y << "," << mesh.bbox.min.z << "]"
        << ",\"max\":[" << mesh.bbox.max.x << "," << mesh.bbox.max.y << "," << mesh.bbox.max.z << "]}"
        << ",\"triangles\":[";

    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& t = mesh.triangles[i];
        if (i > 0) oss << ",";
        oss << "[" << t.normal.x << "," << t.normal.y << "," << t.normal.z << ","
            << t.v1.x << "," << t.v1.y << "," << t.v1.z << ","
            << t.v2.x << "," << t.v2.y << "," << t.v2.z << ","
            << t.v3.x << "," << t.v3.y << "," << t.v3.z << "]";
    }
    oss << "]}";
    return oss.str();
}

std::string pointcloud_to_json(const PointCloud& pc) {
    std::ostringstream oss;
    oss << "{\"node_count\":" << pc.nodes.size()
        << ",\"min_temperature_C\":" << pc.min_T
        << ",\"max_temperature_C\":" << pc.max_T
        << ",\"bbox\":{\"min\":[" << pc.bbox.min.x << "," << pc.bbox.min.y << "," << pc.bbox.min.z << "]"
        << ",\"max\":[" << pc.bbox.max.x << "," << pc.bbox.max.y << "," << pc.bbox.max.z << "]}"
        << ",\"nodes\":[";

    for (size_t i = 0; i < pc.nodes.size(); ++i) {
        const auto& n = pc.nodes[i];
        if (i > 0) oss << ",";
        oss << "[" << n.id << "," << n.x << "," << n.y << "," << n.z << "," << n.temperature_C << "]";
    }
    oss << "]}";
    return oss.str();
}

} // namespace geometry
} // namespace thermobench
