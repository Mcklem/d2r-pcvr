#include "companion_skills.h"
#include <cassert>
#include <cmath>
#include <map>
#include <tuple>
#include <vector>

namespace {
using namespace framevr::companionai;

struct MockItem {
    uintptr_t id = 0;
    // Map of (statId, param) -> value
    std::map<std::pair<int, int>, int> stats;

    void SetStat(int statId, int param, int value) {
        stats[{statId, param}] = value;
    }

    int GetStat(int statId, int param) const {
        auto it = stats.find({statId, param});
        return it != stats.end() ? it->second : 0;
    }
};

} // namespace

int main() {
    // 1. Virtual Class Mapping Tests
    assert(GetVirtualPlayerClass(MonStatsHireling::Rogue) == VirtualPlayerClass::Amazon);
    assert(GetVirtualPlayerClass(MonStatsHireling::Desert) == VirtualPlayerClass::Paladin);
    assert(GetVirtualPlayerClass(MonStatsHireling::IronWolf) == VirtualPlayerClass::Sorceress);
    assert(GetVirtualPlayerClass(MonStatsHireling::Barbarian1) == VirtualPlayerClass::Barbarian);
    assert(GetVirtualPlayerClass(MonStatsHireling::Barbarian2) == VirtualPlayerClass::Barbarian);
    assert(GetVirtualPlayerClass(1) == VirtualPlayerClass::None);    // Fallen
    assert(GetVirtualPlayerClass(267) == VirtualPlayerClass::None);  // Blood Raven
    assert(GetVirtualPlayerClass(544) == VirtualPlayerClass::None);  // Baal

    // 2. Skill Class and Tab Identification Tests
    // Amazon: Class 0
    assert(GetSkillClass(6) == 0);  // Magic Arrow
    assert(GetSkillClass(7) == 0);  // Fire Arrow
    assert(GetSkillClass(9) == 0);  // Critical Strike
    assert(GetSkillClass(10) == 0); // Jab
    assert(GetSkillClass(22) == 0); // Guided Arrow
    assert(GetSkillClass(26) == 0); // Strafe
    assert(GetSkillClass(33) == 0); // Pierce

    assert(GetSkillTab(6) == 0);  // Magic Arrow -> Bow (tab 0)
    assert(GetSkillTab(7) == 0);  // Fire Arrow -> Bow (tab 0)
    assert(GetSkillTab(12) == 0); // Multiple Shot -> Bow (tab 0)
    assert(GetSkillTab(22) == 0); // Guided Arrow -> Bow (tab 0)
    assert(GetSkillTab(26) == 0); // Strafe -> Bow (tab 0)

    assert(GetSkillTab(8) == 1);  // Inner Sight -> Passive (tab 1)
    assert(GetSkillTab(9) == 1);  // Critical Strike -> Passive (tab 1)
    assert(GetSkillTab(23) == 1); // Penetrate -> Passive (tab 1)
    assert(GetSkillTab(33) == 1); // Pierce -> Passive (tab 1)

    assert(GetSkillTab(10) == 2); // Jab -> Javelin & Spear (tab 2)

    // Sorceress: Class 1 (tabs 8, 9, 10: 1*8 + page)
    assert(GetSkillClass(36) == 1); // Fire Bolt
    assert(GetSkillClass(37) == 1); // Warmth
    assert(GetSkillClass(47) == 1); // Fire Ball
    assert(GetSkillClass(55) == 1); // Glacial Spike
    assert(GetSkillClass(59) == 1); // Blizzard
    assert(GetSkillClass(61) == 1); // Fire Mastery
    assert(GetSkillClass(65) == 1); // Cold Mastery

    assert(GetSkillTab(36) == 8); // Fire Bolt -> Fire (tab 8)
    assert(GetSkillTab(37) == 8); // Warmth -> Fire (tab 8)
    assert(GetSkillTab(47) == 8); // Fire Ball -> Fire (tab 8)
    assert(GetSkillTab(61) == 8); // Fire Mastery -> Fire (tab 8)

    assert(GetSkillTab(38) == 9); // Charged Bolt -> Lightning (tab 9)
    assert(GetSkillTab(48) == 9); // Nova -> Lightning (tab 9)
    assert(GetSkillTab(49) == 9); // Lightning -> Lightning (tab 9)
    assert(GetSkillTab(53) == 9); // Chain Lightning -> Lightning (tab 9)
    assert(GetSkillTab(54) == 9); // Teleport -> Lightning (tab 9)

    assert(GetSkillTab(40) == 10); // Frozen Armor -> Cold (tab 10)
    assert(GetSkillTab(45) == 10); // Ice Blast -> Cold (tab 10)
    assert(GetSkillTab(55) == 10); // Glacial Spike -> Cold (tab 10)
    assert(GetSkillTab(59) == 10); // Blizzard -> Cold (tab 10)
    assert(GetSkillTab(65) == 10); // Cold Mastery -> Cold (tab 10)

    // Necromancer: Class 2 (tabs 16, 17, 18: 2*8 + page)
    assert(GetSkillClass(66) == 2); // Amplify Damage
    assert(GetSkillClass(84) == 2); // Bone Spear
    assert(GetSkillClass(75) == 2); // Clay Golem
    assert(GetSkillTab(66) == 16);  // Amplify Damage -> Curses (tab 16)
    assert(GetSkillTab(84) == 17);  // Bone Spear -> Poison & Bone (tab 17)
    assert(GetSkillTab(75) == 18);  // Clay Golem -> Summoning (tab 18)

    // Paladin: Class 3 (tabs 24, 25, 26: 3*8 + page)
    assert(GetSkillClass(98) == 3);  // Might
    assert(GetSkillClass(99) == 3);  // Prayer
    assert(GetSkillClass(104) == 3); // Defiance
    assert(GetSkillClass(121) == 3); // Fist of the Heavens
    assert(GetSkillTab(121) == 24);  // Fist of the Heavens -> Combat (tab 24)
    assert(GetSkillTab(98) == 25);   // Might -> Offensive Auras (tab 25)
    assert(GetSkillTab(99) == 26);   // Prayer -> Defensive Auras (tab 26)
    assert(GetSkillTab(104) == 26);  // Defiance -> Defensive Auras (tab 26)

    // Barbarian: Class 4 (tabs 32, 33, 34: 4*8 + page)
    assert(GetSkillClass(126) == 4); // Bash
    assert(GetSkillClass(138) == 4); // Shout
    assert(GetSkillClass(145) == 4); // Iron Skin
    assert(GetSkillClass(147) == 4); // Frenzy
    assert(GetSkillTab(126) == 32);  // Bash -> Combat (tab 32)
    assert(GetSkillTab(147) == 32);  // Frenzy -> Combat (tab 32)
    assert(GetSkillTab(145) == 33);  // Iron Skin -> Masteries (tab 33)
    assert(GetSkillTab(138) == 34);  // Shout -> Warcries (tab 34)

    // Druid: Class 5 (tabs 40, 41, 42: 5*8 + page)
    assert(GetSkillClass(221) == 5); // Raven
    assert(GetSkillClass(223) == 5); // Werewolf
    assert(GetSkillClass(225) == 5); // Firestorm
    assert(GetSkillTab(221) == 40);  // Raven -> Summoning (tab 40)
    assert(GetSkillTab(223) == 41);  // Werewolf -> Shape Shifting (tab 41)
    assert(GetSkillTab(225) == 42);  // Firestorm -> Elemental (tab 42)

    // Assassin: Class 6 (tabs 48, 49, 50: 6*8 + page)
    assert(GetSkillClass(251) == 6); // Fire Blast
    assert(GetSkillClass(258) == 6); // Burst of Speed
    assert(GetSkillClass(254) == 6); // Tiger Strike
    assert(GetSkillTab(251) == 48);  // Fire Blast -> Traps (tab 48)
    assert(GetSkillTab(258) == 49);  // Burst of Speed -> Shadow Disciplines (tab 49)
    assert(GetSkillTab(254) == 50);  // Tiger Strike -> Martial Arts (tab 50)

    // 3. Skill Matching Virtual Class
    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Amazon, 7));   // Fire Arrow
    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Amazon, 9));   // Critical Strike
    assert(!IsSkillMatchingVirtualClass(VirtualPlayerClass::Amazon, 47)); // Fire Ball
    assert(!IsSkillMatchingVirtualClass(VirtualPlayerClass::Amazon, 126));// Bash

    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Sorceress, 47)); // Fire Ball
    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Sorceress, 55)); // Glacial Spike
    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Sorceress, 37)); // Warmth
    assert(!IsSkillMatchingVirtualClass(VirtualPlayerClass::Sorceress, 7)); // Fire Arrow

    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Paladin, 98));  // Might
    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Paladin, 10));  // Jab (special case for Act 2 merc)
    assert(!IsSkillMatchingVirtualClass(VirtualPlayerClass::Paladin, 47)); // Fire Ball

    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Barbarian, 126)); // Bash
    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Barbarian, 147)); // Frenzy
    assert(IsSkillMatchingVirtualClass(VirtualPlayerClass::Barbarian, 138)); // Iron Skin
    assert(!IsSkillMatchingVirtualClass(VirtualPlayerClass::Barbarian, 98));  // Might

    // 4. Equipment Skill Bonus Calculation Tests
    std::vector<MockItem> mockItems(4);
    for (size_t i = 0; i < mockItems.size(); ++i) {
        mockItems[i].id = 0x1000 + i;
    }

    auto getStatLambda = [&](uintptr_t itemPtr, int statId, int param) -> int {
        for (const auto& item : mockItems) {
            if (item.id == itemPtr) {
                return item.GetStat(statId, param);
            }
        }
        return 0;
    };

    uintptr_t itemPtrs[4] = { mockItems[0].id, mockItems[1].id, mockItems[2].id, mockItems[3].id };
    std::span<const uintptr_t> equipSpan(itemPtrs, 4);

    // Scenario A: Rogue (Amazon) with Amazon Gear
    // Item 0 (Bow): +2 to Amazon Skills (Stat 83, param 0), +3 to Bow & Crossbow (Stat 188, param 0), +1 to Fire Arrow (Stat 107, param 7)
    mockItems[0].SetStat(ItemStat::ClassSkills, 0, 2);
    mockItems[0].SetStat(ItemStat::TabSkills, 0, 3);
    mockItems[0].SetStat(ItemStat::SingleSkill, 7, 1);

    // Item 1 (Circlet): +1 to Amazon Skills (Stat 83, param 0)
    mockItems[1].SetStat(ItemStat::ClassSkills, 0, 1);

    // Test Fire Arrow (ID 7, base level 15):
    // Native monster bonus-level calculation already incorporates Stat 107 / Stat 97.
    // Detour bonus adds Class (+2 from bow +1 from circlet = 3) + Tab (+3 from bow) = +6!
    const int rogueFireBonus = ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Amazon, 7, equipSpan, getStatLambda);
    assert(rogueFireBonus == 6);
    assert(ComputeEffectiveCompanionSkillLevel(15, VirtualPlayerClass::Amazon, 7, equipSpan, getStatLambda, true) == 21);

    // Test Critical Strike (ID 9, Passive tab 1, base level 4):
    // Bow has tab 0 (Bow & Crossbow), so tab bonus does NOT apply. Single bonus is for skill 7, so it does NOT apply.
    // Class bonus (+3 from bow and circlet) DOES apply!
    const int rogueCritBonus = ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Amazon, 9, equipSpan, getStatLambda);
    assert(rogueCritBonus == 3);
    assert(ComputeEffectiveCompanionSkillLevel(4, VirtualPlayerClass::Amazon, 9, equipSpan, getStatLambda, true) == 7);

    // Scenario B: Feature Disabled Check
    // When feature is disabled, effective level must equal base level strictly
    assert(ComputeEffectiveCompanionSkillLevel(15, VirtualPlayerClass::Amazon, 7, equipSpan, getStatLambda, false) == 15);
    assert(ComputeEffectiveCompanionSkillLevel(4, VirtualPlayerClass::Amazon, 9, equipSpan, getStatLambda, false) == 4);

    // Scenario C: Iron Wolf (Sorceress) with Sorceress Gear
    for (auto& item : mockItems) item.stats.clear();

    // Item 0 (Sword): +2 to All Sorceress Skills (Stat 83, param 1)
    mockItems[0].SetStat(ItemStat::ClassSkills, 1, 2);
    // Item 1 (Shield): +1 to Fire Skills (Stat 188, param 8: 1*8 + 0)
    mockItems[1].SetStat(ItemStat::TabSkills, 8, 1);
    // Item 2 (Armor): Ormus' Robes with +3 to Fire Ball (Stat 107, param 47)
    mockItems[2].SetStat(ItemStat::SingleSkill, 47, 3);
    // Item 3 (Helm): +1 to Cold Skills (Stat 188, param 10: 1*8 + 2)
    mockItems[3].SetStat(ItemStat::TabSkills, 10, 1);

    // Fire Ball (47, Fire tab 8):
    // Bonus = +2 (Sorc class) + 1 (Fire tab) = +3! (Stat 107 is native base)
    const int wolfFireBonus = ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Sorceress, 47, equipSpan, getStatLambda);
    assert(wolfFireBonus == 3);
    assert(ComputeEffectiveCompanionSkillLevel(12, VirtualPlayerClass::Sorceress, 47, equipSpan, getStatLambda, true) == 15);

    // Glacial Spike (55, Cold tab 10):
    // Bonus = +2 (Sorc class) + 1 (Cold tab from helm) + 0 (Fire tab does not apply) = +3!
    const int wolfColdBonus = ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Sorceress, 55, equipSpan, getStatLambda);
    assert(wolfColdBonus == 3);
    assert(ComputeEffectiveCompanionSkillLevel(8, VirtualPlayerClass::Sorceress, 55, equipSpan, getStatLambda, true) == 11);

    // Warmth (37, Fire tab 8, passive):
    // Bonus = +2 (Sorc class) + 1 (Fire tab) = +3!
    const int wolfWarmthBonus = ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Sorceress, 37, equipSpan, getStatLambda);
    assert(wolfWarmthBonus == 3);

    // Scenario D: Cross-class gear mismatch
    // If Iron Wolf equips Rogue's Amazon bow (+2 Amazon, +3 Bow, +1 Fire Arrow),
    // bonuses to Sorceress skills (Fire Ball, Glacial Spike) must be 0!
    for (auto& item : mockItems) item.stats.clear();
    mockItems[0].SetStat(ItemStat::ClassSkills, 0, 2);
    mockItems[0].SetStat(ItemStat::TabSkills, 0, 3);
    mockItems[0].SetStat(ItemStat::SingleSkill, 7, 1);

    assert(ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Sorceress, 47, equipSpan, getStatLambda) == 0);
    assert(ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Sorceress, 55, equipSpan, getStatLambda) == 0);
    assert(ComputeEffectiveCompanionSkillLevel(12, VirtualPlayerClass::Sorceress, 47, equipSpan, getStatLambda, true) == 12);

    // Scenario E: Desert Guard (Paladin) using Jab (skill 10) with Amazon and Paladin gear
    for (auto& item : mockItems) item.stats.clear();
    mockItems[0].SetStat(ItemStat::ClassSkills, 3, 2);  // +2 Paladin Skills
    mockItems[0].SetStat(ItemStat::TabSkills, 25, 3);  // +3 Offensive Auras (Might tab 25: 3*8 + 1)
    mockItems[1].SetStat(ItemStat::SingleSkill, 10, 2); // +2 to Jab
    mockItems[2].SetStat(ItemStat::ClassSkills, 0, 1);  // +1 to Amazon Skills

    // Might (98, Paladin tab 25):
    // Bonus = +2 (Paladin class) + 3 (Offensive Auras tab) = +5!
    assert(ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Paladin, 98, equipSpan, getStatLambda) == 5);
    assert(ComputeEffectiveCompanionSkillLevel(10, VirtualPlayerClass::Paladin, 98, equipSpan, getStatLambda, true) == 15);

    // Jab (10):
    // Bonus = +1 (Amazon class gear on Desert Guard special case) = +1! (Single skill Stat 107 is in baseLevel)
    assert(ComputeCompanionEquipmentSkillBonus(VirtualPlayerClass::Paladin, 10, equipSpan, getStatLambda) == 1);
    assert(ComputeEffectiveCompanionSkillLevel(8, VirtualPlayerClass::Paladin, 10, equipSpan, getStatLambda, true) == 9);

    // Scenario F: Max Skill Level Clamp (99)
    assert(ComputeEffectiveCompanionSkillLevel(95, VirtualPlayerClass::Paladin, 98, equipSpan, getStatLambda, true) == 99);

    return 0;
}

