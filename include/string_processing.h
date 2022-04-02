#pragma once

#include <set>
#include <string>
#include <vector>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(StringContainer strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    for (auto str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(std::string(str));
        }
    }
    return non_empty_strings;
}
