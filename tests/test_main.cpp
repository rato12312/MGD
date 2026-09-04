#include <iostream>
#include <string>
#include <vector>

bool run_types_tests();
bool run_entity_tests();
bool run_mental_map_tests();
bool run_camera_tests();
bool run_collision_tests();
bool run_visibility_tests();
bool run_framebuffer_tests();
bool run_rasterizer_tests();
bool run_painter_tests();
bool run_polygon_query_tests();
bool run_dna_pipeline_tests();

struct TestResult {
    std::string name;
    bool passed;
};

int main() {
    std::vector<TestResult> results;
    results.push_back({"Types", run_types_tests()});
    results.push_back({"Entity", run_entity_tests()});
    results.push_back({"MentalMap", run_mental_map_tests()});
    results.push_back({"Camera", run_camera_tests()});
    results.push_back({"Collision", run_collision_tests()});
    results.push_back({"Visibility", run_visibility_tests()});
    results.push_back({"Framebuffer", run_framebuffer_tests()});
    results.push_back({"Rasterizer", run_rasterizer_tests()});
    results.push_back({"Painter", run_painter_tests()});
    results.push_back({"PolygonQuery", run_polygon_query_tests()});
    results.push_back({"DnaPipeline", run_dna_pipeline_tests()});

    int passed = 0, failed = 0;
    for (auto& r : results) {
        if (r.passed) { passed++; std::cout << "  PASS: " << r.name << std::endl; }
        else { failed++; std::cout << "  FAIL: " << r.name << std::endl; }
    }
    std::cout << "\nResults: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed;
}
