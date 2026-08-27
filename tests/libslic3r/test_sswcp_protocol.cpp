#include <catch2/catch_test_macros.hpp>

#include "libslic3r/SSWCPProtocol.hpp"

#include <nlohmann/json.hpp>

#include <vector>

using json = nlohmann::json;
using namespace Slic3r;

TEST_CASE("parse_skip_object_names accepts name or names and rejects invalid input", "[SSWCPProtocol]")
{
    std::vector<std::string> names;

    // Single "name" string.
    CHECK(SSWCPProtocol::parse_skip_object_names(json{{"name", "CUBE_ID_0"}}, names));
    CHECK(names == std::vector<std::string>{"CUBE_ID_0"});

    // "names" array.
    CHECK(SSWCPProtocol::parse_skip_object_names(json{{"names", {"CUBE_ID_0", "CUBE_ID_1"}}}, names));
    CHECK(names == std::vector<std::string>{"CUBE_ID_0", "CUBE_ID_1"});

    // "name" wins when both are present.
    CHECK(SSWCPProtocol::parse_skip_object_names(
        json{{"name", "A"}, {"names", {"B", "C"}}}, names));
    CHECK(names == std::vector<std::string>{"A"});

    // Missing / wrong type / empty values must reject — a half-applied skip
    // list must never reach the printer.
    for (const json &invalid : std::vector<json>{
             json::object(),
             json{{"name", ""}},
             json{{"name", 42}},
             json{{"names", json::array()}},
             json{{"names", "CUBE_ID_0"}},
             json{{"names", {"A", 7}}},
             json{{"names", {"A", ""}}}}) {
        CHECK_FALSE(SSWCPProtocol::parse_skip_object_names(invalid, names));
        CHECK(names.empty());
    }
}

TEST_CASE("build_exclude_object_scripts emits one directive per object", "[SSWCPProtocol]")
{
    CHECK(SSWCPProtocol::build_exclude_object_scripts({}) == std::vector<std::string>{});
    CHECK(SSWCPProtocol::build_exclude_object_scripts({"CUBE_ID_0"}) ==
          std::vector<std::string>{"EXCLUDE_OBJECT NAME=CUBE_ID_0"});
    CHECK(SSWCPProtocol::build_exclude_object_scripts({"CUBE_ID_0", "CUBE_ID_1"}) ==
          std::vector<std::string>{"EXCLUDE_OBJECT NAME=CUBE_ID_0", "EXCLUDE_OBJECT NAME=CUBE_ID_1"});
}
