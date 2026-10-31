#include "test_runner.hpp"
#include "core/geometry.hpp"
#include <cmath>
#include <string>

using namespace thermobench;

namespace {

TEST("Phase 4 - parse_stl: parses sample.stl with 12 triangles and exact bbox") {
    auto mesh = geometry::parse_stl_file("data/sample.stl");
    ASSERT(mesh.name == "heatsink_block");
    ASSERT(mesh.triangles.size() == 12);
    ASSERT(geometry::validate_stl(mesh));

    // Bounding box: [0, 0.05] x [0, 0.05] x [0, 0.01]
    ASSERT_NEAR(mesh.bbox.min.x, 0.0, 1e-6);
    ASSERT_NEAR(mesh.bbox.max.x, 0.05, 1e-6);
    ASSERT_NEAR(mesh.bbox.min.y, 0.0, 1e-6);
    ASSERT_NEAR(mesh.bbox.max.y, 0.05, 1e-6);
    ASSERT_NEAR(mesh.bbox.min.z, 0.0, 1e-6);
    ASSERT_NEAR(mesh.bbox.max.z, 0.01, 1e-6);

    // Surface area: 2*(0.05*0.05) + 2*(0.05*0.01) + 2*(0.05*0.01) = 0.005 + 0.001 + 0.001 = 0.007 m^2
    ASSERT_NEAR(mesh.surface_area, 0.007, 1e-6);
}

TEST("Phase 4 - parse_stl: rejects invalid header, truncated content, and NaN coordinates") {
    // Missing solid
    ASSERT_THROWS(geometry::parse_stl_string("not_solid"));

    // Truncated facet
    std::string truncated = "solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nendloop\nendsolid";
    ASSERT_THROWS(geometry::parse_stl_string(truncated));

    // Non-finite vertex
    std::string nan_stl = "solid test\nfacet normal 0 0 1\nouter loop\nvertex NaN 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid";
    ASSERT_THROWS(geometry::parse_stl_string(nan_stl));
}

TEST("Phase 4 - parse_ansys_csv: parses sample_ansys.csv and extracts 65 thermal nodes") {
    auto pc = geometry::parse_ansys_csv_file("data/sample_ansys.csv");
    ASSERT(pc.nodes.size() == 65);
    ASSERT(geometry::validate_pointcloud(pc));

    ASSERT_NEAR(pc.min_T, 28.2, 0.1);
    ASSERT_NEAR(pc.max_T, 94.8, 0.1);

    ASSERT(pc.nodes[0].id == 1);
    ASSERT_NEAR(pc.nodes[0].temperature_C, 32.4, 1e-4);

    ASSERT_NEAR(pc.bbox.min.x, 0.0, 1e-6);
    ASSERT_NEAR(pc.bbox.max.x, 0.05, 1e-6);
    ASSERT_NEAR(pc.bbox.max.z, 0.01, 1e-6);
}

TEST("Phase 4 - parse_ansys_csv: rejects bad header, missing columns, and non-numeric values") {
    // Bad header
    std::string bad_hdr = "a,b,c,d,e\n1,0,0,0,25";
    ASSERT_THROWS(geometry::parse_ansys_csv_string(bad_hdr));

    // Missing column
    std::string missing_col = "node_id,x_m,y_m,z_m,temperature_C\n1,0,0,25";
    ASSERT_THROWS(geometry::parse_ansys_csv_string(missing_col));

    // Non-numeric
    std::string non_num = "node_id,x_m,y_m,z_m,temperature_C\n1,0,ABC,0,25";
    ASSERT_THROWS(geometry::parse_ansys_csv_string(non_num));
}

TEST("Phase 4 - serializers: stl_to_json and pointcloud_to_json produce valid schemas") {
    auto mesh = geometry::parse_stl_file("data/sample.stl");
    std::string mesh_json = geometry::stl_to_json(mesh);
    ASSERT(!mesh_json.empty());
    ASSERT(mesh_json.find("\"triangle_count\":12") != std::string::npos);
    ASSERT(mesh_json.find("\"name\":\"heatsink_block\"") != std::string::npos);

    auto pc = geometry::parse_ansys_csv_file("data/sample_ansys.csv");
    std::string pc_json = geometry::pointcloud_to_json(pc);
    ASSERT(!pc_json.empty());
    ASSERT(pc_json.find("\"node_count\":65") != std::string::npos);
    ASSERT(pc_json.find("\"max_temperature_C\":") != std::string::npos);
}

} // namespace
