#pragma once

#include <string>
#include <vector>

namespace thermobench {
namespace geometry {

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    bool operator==(const Vec3& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct Triangle {
    Vec3 normal;
    Vec3 v1;
    Vec3 v2;
    Vec3 v3;
};

struct BoundingBox {
    Vec3 min{1e9, 1e9, 1e9};
    Vec3 max{-1e9, -1e9, -1e9};

    void expand(const Vec3& p) {
        if (p.x < min.x) min.x = p.x;
        if (p.x > max.x) max.x = p.x;
        if (p.y < min.y) min.y = p.y;
        if (p.y > max.y) max.y = p.y;
        if (p.z < min.z) min.z = p.z;
        if (p.z > max.z) max.z = p.z;
    }
};

struct StlMesh {
    std::string name{"solid"};
    std::vector<Triangle> triangles;
    BoundingBox bbox;
    double surface_area{0.0};
};

struct AnsysNode {
    int id{0};
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double temperature_C{0.0};
};

struct PointCloud {
    std::vector<AnsysNode> nodes;
    double min_T{1e9};
    double max_T{-1e9};
    BoundingBox bbox;
};

// 1. ASCII STL Parsers
StlMesh parse_stl_string(const std::string& content);
StlMesh parse_stl_file(const std::string& filepath);
bool validate_stl(const StlMesh& mesh);

// 2. ANSYS FEA CSV Point Cloud Parsers
PointCloud parse_ansys_csv_string(const std::string& content);
PointCloud parse_ansys_csv_file(const std::string& filepath);
bool validate_pointcloud(const PointCloud& pc);

// 3. JSON Serializers
std::string stl_to_json(const StlMesh& mesh);
std::string pointcloud_to_json(const PointCloud& pc);

} // namespace geometry
} // namespace thermobench
