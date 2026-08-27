#ifndef slic3r_SSWCPProtocol_hpp_
#define slic3r_SSWCPProtocol_hpp_

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace Slic3r {

/// Pure protocol-conversion helpers shared by SSWCP handlers and tests.
/// No wxWidgets dependency — safe to unit-test without a GUI event loop.
namespace SSWCPProtocol {

/// Parse sw_SkipObject params: "name" (single string) or "names" (array of strings).
/// "name" wins when both are present. Every entry must be a non-empty string;
/// anything else (missing key, wrong type, empty name, empty array) rejects the
/// whole request — a half-applied skip list must never reach the printer.
/// *out* is always cleared first; returns true only when at least one name was parsed.
bool parse_skip_object_names(const nlohmann::json &params, std::vector<std::string> &out);

/// Build one Klipper skip-object directive per name: "EXCLUDE_OBJECT NAME=<name>".
std::vector<std::string> build_exclude_object_scripts(const std::vector<std::string> &names);

} // namespace SSWCPProtocol
} // namespace Slic3r

#endif // slic3r_SSWCPProtocol_hpp_
