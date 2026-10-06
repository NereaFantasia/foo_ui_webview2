#pragma once

#include <nlohmann/json.hpp>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace metadata_tags {

enum class Action { Ignore, Remove, Set, Invalid };

struct ParsedValue {
    Action action = Action::Ignore;
    std::vector<std::string> values;
    nlohmann::json appliedValue = nullptr;
    std::optional<std::size_t> invalidIndex;
};

inline ParsedValue ParseTagValue(const nlohmann::json& value) {
    ParsedValue parsed;
    if (value.is_null() || (value.is_string() && value.get_ref<const std::string&>().empty())) {
        parsed.action = Action::Remove;
    } else if (value.is_string()) {
        parsed.action = Action::Set;
        parsed.values.push_back(value.get<std::string>());
        parsed.appliedValue = value;
    } else if (value.is_number_integer()) {
        parsed.action = Action::Set;
        // dump() 给出精确的十进制文本；get<int>() 会把超出 int 的值截断。
        parsed.values.push_back(value.dump());
        parsed.appliedValue = value;
    } else if (value.is_number_float()) {
        parsed.action = Action::Set;
        parsed.values.push_back(std::to_string(value.get<double>()));
        parsed.appliedValue = value.get<double>();
    } else if (value.is_array()) {
        if (value.empty()) {
            parsed.action = Action::Remove;
            return parsed;
        }
        for (std::size_t index = 0; index < value.size(); ++index) {
            const auto& element = value[index];
            if (!element.is_string() || element.get_ref<const std::string&>().empty() ||
                element.get_ref<const std::string&>().find('\0') != std::string::npos) {
                parsed.action = Action::Invalid;
                parsed.invalidIndex = index;
                return parsed;
            }
        }
        parsed.action = Action::Set;
        parsed.values = value.get<std::vector<std::string>>();
        parsed.appliedValue = value;
    }
    return parsed;
}

} // namespace metadata_tags
