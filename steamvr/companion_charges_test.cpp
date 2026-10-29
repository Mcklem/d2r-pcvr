#include "companion_charges.h"
#include <cassert>
#include <cstdio>
#include <vector>

using namespace framevr::companionai;

namespace {

void TestStat204Packing() {
    // 1. Standard skill (< 256): Teleport (54, level 1, 20/20 charges)
    {
        const uint16_t param = PackChargedSkillParam(54, 1);
        const int32_t val = PackChargedSkillValue(20, 20);
        const auto skill = UnpackChargedSkill(101, 4, param, val);

        assert(skill.itemId == 101);
        assert(skill.bodyLocation == 4);
        assert(skill.skillId == 54);
        assert(skill.level == 1);
        assert(skill.currentCharges == 20);
        assert(skill.maxCharges == 20);
    }

    // 2. Curse: Life Tap (82, level 5, 90/90 charges)
    {
        const uint16_t param = PackChargedSkillParam(82, 5);
        const int32_t val = PackChargedSkillValue(90, 90);
        const auto skill = UnpackChargedSkill(102, 4, param, val);

        assert(skill.skillId == 82);
        assert(skill.level == 5);
        assert(skill.currentCharges == 90);
        assert(skill.maxCharges == 90);
    }

    // 3. High skill ID (> 255): Cloak of Shadows (264, level 3, 30/30 charges)
    {
        const uint16_t param = PackChargedSkillParam(264, 3);
        const int32_t val = PackChargedSkillValue(30, 30);
        const auto skill = UnpackChargedSkill(103, 1, param, val);

        assert(skill.skillId == 264);
        assert(skill.level == 3);
        assert(skill.currentCharges == 30);
        assert(skill.maxCharges == 30);
    }

    // 4. Extreme bounds: Skill 1023, Level 63, Charges 255/255
    {
        const uint16_t param = PackChargedSkillParam(1023, 63);
        const int32_t val = PackChargedSkillValue(255, 255);
        const auto skill = UnpackChargedSkill(104, 3, param, val);

        assert(skill.skillId == 1023);
        assert(skill.level == 63);
        assert(skill.currentCharges == 255);
        assert(skill.maxCharges == 255);
    }

    // 5. Depleted charges: 0 current, 45 max
    {
        const uint16_t param = PackChargedSkillParam(91, 3); // Lower Resist
        const int32_t val = PackChargedSkillValue(0, 45);
        const auto skill = UnpackChargedSkill(105, 4, param, val);

        assert(skill.skillId == 91);
        assert(skill.level == 3);
        assert(skill.currentCharges == 0);
        assert(skill.maxCharges == 45);
    }

    std::puts("TestStat204Packing: OK");
}

void TestTacticalSkillKindClassification() {
    assert(GetTacticalSkillKind(54) == TacticalSkillKind::Teleport);
    assert(GetTacticalSkillKind(82) == TacticalSkillKind::Curse);        // Life Tap
    assert(GetTacticalSkillKind(87) == TacticalSkillKind::Curse);        // Decrepify
    assert(GetTacticalSkillKind(91) == TacticalSkillKind::Curse);        // Lower Resist
    assert(GetTacticalSkillKind(9)  == TacticalSkillKind::Suppression);  // Slow Missiles
    assert(GetTacticalSkillKind(264) == TacticalSkillKind::Suppression); // Cloak of Shadows
    assert(GetTacticalSkillKind(44) == TacticalSkillKind::CrowdControl); // Frost Nova
    assert(GetTacticalSkillKind(55) == TacticalSkillKind::CrowdControl); // Glacial Spike
    assert(GetTacticalSkillKind(246) == TacticalSkillKind::Summon);      // Heart of Wolverine
    assert(GetTacticalSkillKind(226) == TacticalSkillKind::Summon);      // Oak Sage
    assert(GetTacticalSkillKind(47) == TacticalSkillKind::DamageSpell);  // Fire Ball
    assert(GetTacticalSkillKind(56) == TacticalSkillKind::DamageSpell);  // Meteor
    assert(GetTacticalSkillKind(52) == TacticalSkillKind::Buff);         // Enchant
    assert(GetTacticalSkillKind(999) == TacticalSkillKind::None);

    std::puts("TestTacticalSkillKindClassification: OK");
}

void TestCompanionSkillUsability() {
    // Bow skills only usable by Rogue (Amazon)
    for (uint16_t bowSkill : {6, 11, 12, 16, 21, 22, 26, 27, 31, 35}) {
        assert(IsSkillUsableByCompanion(VirtualPlayerClass::Amazon, bowSkill));
        assert(!IsSkillUsableByCompanion(VirtualPlayerClass::Paladin, bowSkill));
        assert(!IsSkillUsableByCompanion(VirtualPlayerClass::Sorceress, bowSkill));
        assert(!IsSkillUsableByCompanion(VirtualPlayerClass::Barbarian, bowSkill));
        assert(!IsSkillUsableByCompanion(VirtualPlayerClass::None, bowSkill));
    }

    // Incompatible player-locked animations rejected for ALL
    for (uint16_t bannedSkill : {97, 100, 106, 132, 144, 151, 255, 260, 270, 275}) {
        assert(!IsSkillUsableByCompanion(VirtualPlayerClass::Amazon, bannedSkill));
        assert(!IsSkillUsableByCompanion(VirtualPlayerClass::Paladin, bannedSkill));
        assert(!IsSkillUsableByCompanion(VirtualPlayerClass::Sorceress, bannedSkill));
        assert(!IsSkillUsableByCompanion(VirtualPlayerClass::Barbarian, bannedSkill));
    }

    // Standard spells/curses/summons usable by ALL
    for (uint16_t universalSkill : {54 /*Teleport*/, 82 /*Life Tap*/, 9 /*Slow Missiles*/, 44 /*Frost Nova*/, 246 /*Wolverine*/}) {
        assert(IsSkillUsableByCompanion(VirtualPlayerClass::Amazon, universalSkill));
        assert(IsSkillUsableByCompanion(VirtualPlayerClass::Paladin, universalSkill));
        assert(IsSkillUsableByCompanion(VirtualPlayerClass::Sorceress, universalSkill));
        assert(IsSkillUsableByCompanion(VirtualPlayerClass::Barbarian, universalSkill));
    }

    std::puts("TestCompanionSkillUsability: OK");
}

void TestEquipmentScanning() {
    struct MockItemStat {
        uint16_t statId;
        uint16_t param;
        int32_t value;
    };

    // Simulate equipped items:
    // Slot 1 (Head): Blackhorn's Face -> Slow Missiles (9, level 10, 20/20) + stat 72 durability
    // Slot 4 (Mainhand): Spellsteel -> Teleport (54, level 1, 20/20) + Decrepify (87, level 3, 30/30)
    // Slot 5 (Offhand): Shield with Bow skill (Multiple Shot 12, level 4, 15/15)
    std::vector<MockItemStat> headStats = {
        {72, 0, 20}, // Durability
        {ItemStat::ChargedSkill, PackChargedSkillParam(9, 10), PackChargedSkillValue(20, 20)}
    };

    std::vector<MockItemStat> weaponStats = {
        {ItemStat::ChargedSkill, PackChargedSkillParam(54, 1), PackChargedSkillValue(20, 20)},
        {ItemStat::ChargedSkill, PackChargedSkillParam(87, 3), PackChargedSkillValue(30, 30)},
        {ItemStat::AllSkills, 0, 1}
    };

    std::vector<MockItemStat> offhandStats = {
        {ItemStat::ChargedSkill, PackChargedSkillParam(12, 4), PackChargedSkillValue(15, 15)} // Multiple Shot
    };

    std::array<EquippedItemSlot, 3> slots = {{
        {1001, 1, 0x1000},
        {1002, 4, 0x2000},
        {1003, 5, 0x3000}
    }};

    auto extractor = [&](const EquippedItemSlot& slot, auto&& cb) {
        const std::vector<MockItemStat>* list = nullptr;
        if (slot.bodyLocation == 1) list = &headStats;
        else if (slot.bodyLocation == 4) list = &weaponStats;
        else if (slot.bodyLocation == 5) list = &offhandStats;

        if (list) {
            for (const auto& s : *list) {
                cb(s.statId, s.param, s.value);
            }
        }
    };

    // Paladin companion (Desert Guard): Multiple Shot must be filtered out!
    {
        auto charges = ScanCompanionChargedSkills(VirtualPlayerClass::Paladin, slots, extractor);
        assert(charges.size() == 3);
        assert(charges[0].skillId == 9);  // Slow Missiles
        assert(charges[1].skillId == 54); // Teleport
        assert(charges[2].skillId == 87); // Decrepify
    }

    // Amazon companion (Rogue): Multiple Shot is allowed!
    {
        auto charges = ScanCompanionChargedSkills(VirtualPlayerClass::Amazon, slots, extractor);
        assert(charges.size() == 4);
        assert(charges[0].skillId == 9);  // Slow Missiles
        assert(charges[1].skillId == 54); // Teleport
        assert(charges[2].skillId == 87); // Decrepify
        assert(charges[3].skillId == 12); // Multiple Shot
    }

    std::puts("TestEquipmentScanning: OK");
}

void TestTacticalPriorityArbitration() {
    std::vector<CompanionChargedSkill> skills;

    // Equip companion with:
    // 1. Teleport (54)
    // 2. Slow Missiles (9)
    // 3. Life Tap (82)
    // 4. Frost Nova (44)
    // 5. Heart of Wolverine (246)
    // 6. Fire Ball (47)
    skills.push_back({1001, 4, 54, 1, 20, 20, 0, TacticalSkillKind::Teleport});
    skills.push_back({1002, 1, 9, 10, 20, 20, 0, TacticalSkillKind::Suppression});
    skills.push_back({1003, 4, 82, 5, 30, 30, 0, TacticalSkillKind::Curse});
    skills.push_back({1004, 3, 44, 3, 25, 25, 0, TacticalSkillKind::CrowdControl});
    skills.push_back({1005, 1, 246, 5, 13, 13, 0, TacticalSkillKind::Summon});
    skills.push_back({1006, 4, 47, 8, 40, 40, 0, TacticalSkillKind::DamageSpell});

    TacticalCombatSensors sensors{};
    sensors.nowMs = 100000;

    // Test P1: Critical health & surrounded -> MUST select Teleport
    {
        sensors.companionHealthPct = 0.35f; // < 40%
        sensors.meleeAttackersCount = 3;    // surrounded
        sensors.nearbyHostilesCount = 4;
        sensors.hasActiveBoss = true;       // Boss present too!
        sensors.targetHasCurse = false;

        auto decision = SelectBestTacticalCharge(skills, sensors, true, true);
        assert(decision.shouldCast);
        assert(decision.kind == TacticalSkillKind::Teleport);
        assert(decision.skillId == 54);
    }

    // Test P2: Safe health, distant ranged shooters targeting squad -> MUST select Slow Missiles
    {
        sensors.companionHealthPct = 0.85f;
        sensors.meleeAttackersCount = 0;
        sensors.nearbyHostilesCount = 1;
        sensors.rangedShootersCount = 3; // Gloams/archers
        sensors.hasActiveBoss = false;

        auto decision = SelectBestTacticalCharge(skills, sensors, true, true);
        assert(decision.shouldCast);
        assert(decision.kind == TacticalSkillKind::Suppression);
        assert(decision.skillId == 9);
    }

    // Test P3: Boss present, no distant shooters, safe health -> MUST select Life Tap
    {
        sensors.companionHealthPct = 0.90f;
        sensors.meleeAttackersCount = 1;
        sensors.nearbyHostilesCount = 2;
        sensors.rangedShootersCount = 0;
        sensors.hasActiveBoss = true;
        sensors.targetHasCurse = false;

        auto decision = SelectBestTacticalCharge(skills, sensors, true, true);
        assert(decision.shouldCast);
        assert(decision.kind == TacticalSkillKind::Curse);
        assert(decision.skillId == 82);
    }

    // Test P3 Target already cursed -> should skip Curse and select Frost Nova (P4) or Wolverine (P5)
    {
        sensors.companionHealthPct = 0.90f;
        sensors.meleeAttackersCount = 1;
        sensors.nearbyHostilesCount = 3; // Dense pack
        sensors.rangedShootersCount = 0;
        sensors.hasActiveBoss = true;
        sensors.targetHasCurse = true; // ALREADY CURSED!
        sensors.isCombatActive = true;
        sensors.hasAlliedSummon = false;

        auto decision = SelectBestTacticalCharge(skills, sensors, true, true);
        assert(decision.shouldCast);
        assert(decision.kind == TacticalSkillKind::CrowdControl);
        assert(decision.skillId == 44);
    }

    // Test P5: In combat, target cursed, no dense pack, summon missing -> MUST select Wolverine
    {
        sensors.companionHealthPct = 0.90f;
        sensors.meleeAttackersCount = 1;
        sensors.nearbyHostilesCount = 1;
        sensors.rangedShootersCount = 0;
        sensors.hasActiveBoss = false;
        sensors.targetHasCurse = true;
        sensors.isCombatActive = true;
        sensors.hasAlliedSummon = false; // Missing summon

        auto decision = SelectBestTacticalCharge(skills, sensors, true, true);
        assert(decision.shouldCast);
        assert(decision.kind == TacticalSkillKind::Summon);
        assert(decision.skillId == 246);
    }

    // Test Master Toggle Disabled: `isMasterEnabled == false`
    {
        auto decision = SelectBestTacticalCharge(skills, sensors, false, true);
        assert(!decision.shouldCast);
    }

    // Test Cooldown Gating
    {
        // Set lastCastMs to nowMs - 5000 (only 5s ago, while Teleport requires 12s)
        skills[0].lastCastMs = sensors.nowMs - 5000;
        sensors.companionHealthPct = 0.25f;
        sensors.meleeAttackersCount = 3;
        sensors.nearbyHostilesCount = 3;

        // Teleport is on cooldown! The next best trigger is evaluated
        auto decision = SelectBestTacticalCharge(skills, sensors, true, true);
        assert(decision.shouldCast);
        assert(decision.skillId != 54); // Cannot be Teleport!
    }

    std::puts("TestTacticalPriorityArbitration: OK");
}

void TestChargeAccounting() {
    CompanionChargedSkill skill{};
    skill.skillId = 54;
    skill.currentCharges = 2;
    skill.maxCharges = 20;

    // Zero-Wear Dev Mode: `consumeCharges == false`
    assert(DecrementChargedSkill(skill, false));
    assert(skill.currentCharges == 2); // Charges remain intact!

    // Realistic Mode: `consumeCharges == true`
    assert(DecrementChargedSkill(skill, true));
    assert(skill.currentCharges == 1);

    assert(DecrementChargedSkill(skill, true));
    assert(skill.currentCharges == 0);

    // Exhausted: Cannot decrement further!
    assert(!DecrementChargedSkill(skill, true));
    assert(skill.currentCharges == 0);

    std::puts("TestChargeAccounting: OK");
}

} // namespace

int main() {
    std::puts("Running companion_charges_test...");
    TestStat204Packing();
    TestTacticalSkillKindClassification();
    TestCompanionSkillUsability();
    TestEquipmentScanning();
    TestTacticalPriorityArbitration();
    TestChargeAccounting();
    std::puts("All companion_charges_test checks passed successfully!");
    return 0;
}

