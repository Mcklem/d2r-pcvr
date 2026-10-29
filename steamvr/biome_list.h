#pragma once
// A list of the game's biome names from the INI ([sky] outdoor, [fog] no_fog_areas): comma
// separated, matched whole, lower case. Spaces and tabs are dropped; a biome name is ASCII.
#include <cwctype>
#include <string>
#include <vector>

namespace framevr::biomes {

inline std::vector<std::string> Parse(const wchar_t* text) {
    std::vector<std::string> list;
    std::string cur;
    for (const wchar_t* p = text;; ++p) {
        if (*p == L',' || *p == 0) {
            if (!cur.empty()) list.push_back(cur);
            cur.clear();
            if (!*p) break;
        } else if (*p != L' ' && *p != L'\t' && *p < 128) cur += (char)towlower(*p);
    }
    return list;
}

inline bool Has(const std::vector<std::string>& list, const std::string& biome) {
    for (const std::string& b : list) if (b == biome) return true;
    return false;
}

}   // namespace framevr::biomes
