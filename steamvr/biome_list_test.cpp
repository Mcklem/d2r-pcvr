// The INI's biome lists (biome_list.h): [sky] outdoor and [fog] no_fog_areas, the River of Flame's
// act4_lava by default.
#include "biome_list.h"
#include <cassert>
#include <cstdio>

int main() {
    using namespace framevr::biomes;
    const auto list = Parse(L" act1_outdoors, ACT4_Lava,,\tact4_diab ,");
    assert(list.size() == 3 && list[0] == "act1_outdoors" && list[1] == "act4_lava" && list[2] == "act4_diab");
    assert(Has(list, "act4_lava") && !Has(list, "act4_mesa") && !Has(list, "act4") && !Has(list, ""));
    assert(Parse(L"").empty() && Parse(L" , ,").empty());
    assert(Has(Parse(L"act4_lava"), "act4_lava"));   // the shipped default: the River of Flame
    std::puts("biome list: all checks passed");
    return 0;
}
