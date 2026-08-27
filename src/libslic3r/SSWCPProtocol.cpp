#include "SSWCPProtocol.hpp"

#include <utility>

namespace Slic3r { namespace SSWCPProtocol {

bool parse_skip_object_names(const nlohmann::json &params, std::vector<std::string> &out)
{
    out.clear();

    if (const auto name = params.find("name"); name != params.end()) {
        if (!name->is_string())
            return false;
        const std::string text = name->get<std::string>();
        if (text.empty())
            return false;
        out.push_back(text);
        return true;
    }

    const auto names = params.find("names");
    if (names == params.end() || !names->is_array() || names->empty())
        return false;

    std::vector<std::string> parsed;
    parsed.reserve(names->size());
    for (const nlohmann::json &entry : *names) {
        if (!entry.is_string())
            return false;
        const std::string text = entry.get<std::string>();
        if (text.empty())
            return false;
        parsed.push_back(text);
    }
    out = std::move(parsed);
    return true;
}

std::vector<std::string> build_exclude_object_scripts(const std::vector<std::string> &names)
{
    std::vector<std::string> scripts;
    scripts.reserve(names.size());
    for (const std::string &name : names)
        scripts.push_back("EXCLUDE_OBJECT NAME=" + name);
    return scripts;
}

}} // namespace Slic3r::SSWCPProtocol
