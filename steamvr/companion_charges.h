#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include <array>
#include "companion_skills.h"

namespace framevr::companionai {

// High-level tactical classification of skills for companion AI decision making.
enum class TacticalSkillKind : uint8_t {
    None = 0,
    Teleport,       // Emergency mobility & repositions
    Suppression,    // Defensive anti-missile / blinding (Slow Missiles, Cloak of Shadows)
    Curse,          // Life Tap, Lower Resist, Decrepify, Weaken, Dim Vision...
    CrowdControl,   // Frost Nova, Glacial Spike, Blizzard, Bone Prison...
    Summon,         // Heart of Wolverine, Oak Sage, Spirit Wolf, Golems...
    DamageSpell,    // Fireball, Meteor, Chain Lightning, Nova, Bone Spear...
    Buff            // Enchant, Cyclone Armor, Bone Armor...
};

// Represents a verified charged skill discovered on companion equipment.
struct CompanionChargedSkill {
    uint32_t itemId = 0;
    int bodyLocation = 0;          // 1 = Head, 3 = Torso, 4 = Mainhand, 5 = Offhand
    uint16_t skillId = 0;
    uint8_t level = 0;
    uint8_t currentCharges = 0;
    uint8_t maxCharges = 0;
    uint64_t lastCastMs = 0;
    TacticalSkillKind kind = TacticalSkillKind::None;
};

// Tactical sensors provided by Layer 1 & Layer 2 to evaluate casting eligibility.
struct TacticalCombatSensors {
    float companionHealthPct = 1.0f; // 0.0f to 1.0f
    int nearbyHostilesCount = 0;     // Hostiles within 8 sub-tiles
    int meleeAttackersCount = 0;     // Hostiles within 2.5 sub-tiles
    int rangedShootersCount = 0;     // Missile shooters (gloams/archers) > 12 sub-tiles
    bool hasActiveBoss = false;       // Target is Boss/Champion/Unique
    bool targetHasCurse = false;     // Target already afflicted by curse
    bool hasAlliedSummon = false;    // Squad already has active Oak Sage / Wolverine
    bool isCombatActive = false;     // True if currently engaged in combat
    uint64_t nowMs = 0;              // Current timestamp in milliseconds
};

// Decision output from the tactical charge evaluation.
struct TacticalCastDecision {
    bool shouldCast = false;
    uint16_t skillId = 0;
    uint8_t level = 0;
    uint32_t itemId = 0;
    int bodyLocation = 0;
    TacticalSkillKind kind = TacticalSkillKind::None;
    const char* reason = "";
};

// Slot metadata for inventory item traversal.
struct EquippedItemSlot {
    uint32_t itemId = 0;
    int bodyLocation = 0;
    uintptr_t itemUnit = 0;
};

// Stat 204 (item_charged_skill) packing and unpacking helpers.
// In Diablo II / D2R (Encode = 3):
// Parameter (16 bits): aabbbbbbcccccccc
//   - cccccccc (bits 0..7): Skill ID low 8 bits
//   - bbbbbb   (bits 8..13): Skill Level (6 bits, 0..63)
//   - aa       (bits 14..15): Skill ID high 2 bits (bits 8..9)
// Value (32 bits):
//   - bits 0..7: Current Charges (0..255)
//   - bits 8..15: Max Charges (0..255)
// Value integer = (MaxCharges << 8) | CurrentCharges

inline constexpr uint16_t PackChargedSkillParam(uint16_t skillId, uint8_t level) noexcept {
    return static_cast<uint16_t>(
        (skillId & 0xFF) |
        ((static_cast<uint16_t>(level) & 0x3F) << 8) |
        (((static_cast<uint16_t>(skillId) >> 8) & 0x03) << 14)
    );
}

inline constexpr int32_t PackChargedSkillValue(uint8_t currentCharges, uint8_t maxCharges) noexcept {
    return static_cast<int32_t>(
        (static_cast<uint32_t>(maxCharges) << 8) | static_cast<uint32_t>(currentCharges)
    );
}

inline CompanionChargedSkill UnpackChargedSkill(
    uint32_t itemId,
    int bodyLocation,
    uint16_t param,
    int32_t value) noexcept {
    CompanionChargedSkill skill{};
    skill.itemId = itemId;
    skill.bodyLocation = bodyLocation;
    skill.skillId = static_cast<uint16_t>((param & 0xFF) | (((param >> 14) & 0x03) << 8));
    skill.level = static_cast<uint8_t>((param >> 8) & 0x3F);
    skill.currentCharges = static_cast<uint8_t>(value & 0xFF);
    skill.maxCharges = static_cast<uint8_t>((value >> 8) & 0xFF);
    return skill;
}

// Maps a skill ID to its tactical combat role.
inline TacticalSkillKind GetTacticalSkillKind(uint16_t skillId) noexcept {
    switch (skillId) {
        // Mobility / Emergency Escape
        case 54: // Teleport
            return TacticalSkillKind::Teleport;

        // Projectile Suppression & Vision
        case 7:   // Inner Sight
        case 9:   // Slow Missiles
        case 264: // Cloak of Shadows
            return TacticalSkillKind::Suppression;

        // Curses
        case 66: // Weaken
        case 69: // Amplify Damage
        case 71: // Dim Vision
        case 72: // Iron Maiden
        case 76: // Terror
        case 77: // Confuse
        case 82: // Life Tap
        case 86: // Attract
        case 87: // Decrepify
        case 91: // Lower Resist
            return TacticalSkillKind::Curse;

        // Crowd Control
        case 44:  // Frost Nova
        case 55:  // Glacial Spike
        case 59:  // Blizzard
        case 78:  // Bone Wall
        case 89:  // Bone Prison
        case 240: // Twister
            return TacticalSkillKind::CrowdControl;

        // Summons
        case 75:  // Clay Golem
        case 85:  // Blood Golem
        case 90:  // Iron Golem
        case 94:  // Fire Golem
        case 95:  // Revive
        case 221: // Raven
        case 222: // Poison Creeper
        case 226: // Oak Sage
        case 227: // Spirit Wolf
        case 232: // Carrion Vine
        case 236: // Spirit of Barbs
        case 237: // Dire Wolf
        case 242: // Solar Creeper
        case 246: // Heart of Wolverine
        case 247: // Grizzly
            return TacticalSkillKind::Summon;

        // Targeted Damage Spells & AoE
        case 36:  // Fire Bolt
        case 38:  // Charged Bolt
        case 47:  // Fire Ball
        case 48:  // Nova
        case 49:  // Lightning
        case 53:  // Chain Lightning
        case 56:  // Meteor
        case 62:  // Hydra
        case 64:  // Frozen Orb
        case 67:  // Teeth
        case 74:  // Corpse Explosion
        case 84:  // Bone Spear
        case 92:  // Poison Nova
        case 93:  // Bone Spirit
        case 101: // Holy Bolt
        case 121: // Fist of the Heavens
        case 225: // Firestorm
        case 229: // Molten Boulder
        case 234: // Fissure
        case 244: // Volcano
        case 245: // Tornado
        case 249: // Armageddon
        case 250: // Hurricane
            return TacticalSkillKind::DamageSpell;

        // Buffs
        case 52:  // Enchant
        case 68:  // Bone Armor
        case 235: // Cyclone Armor
            return TacticalSkillKind::Buff;

        default:
            return TacticalSkillKind::None;
    }
}

// Minimum cooldown in milliseconds required between tactical casts of each kind.
inline uint64_t GetTacticalCooldownMs(TacticalSkillKind kind) noexcept {
    switch (kind) {
        case TacticalSkillKind::Teleport:     return 12000; // 12.0s
        case TacticalSkillKind::Suppression:  return 15000; // 15.0s
        case TacticalSkillKind::Curse:        return 20000; // 20.0s
        case TacticalSkillKind::CrowdControl: return  8000; //  8.0s
        case TacticalSkillKind::Summon:       return 45000; // 45.0s
        case TacticalSkillKind::DamageSpell:  return  4000; //  4.0s
        case TacticalSkillKind::Buff:         return 60000; // 60.0s
        default:                              return  5000;
    }
}

// Verifies that the companion model and animation system can execute the skill safely
// without missing animation tokens or skeletal glitches.
inline bool IsSkillUsableByCompanion(VirtualPlayerClass vclass, uint16_t skillId) noexcept {
    if (vclass == VirtualPlayerClass::None) {
        return false;
    }

    // Hardcoded player-locked animations and complex state machines that hireling skeletons lack:
    switch (skillId) {
        case 97:  // Smite
        case 100: // Charge
        case 106: // Zeal
        case 132: // Leap
        case 144: // Leap Attack
        case 151: // Whirlwind
        case 255: // Dragon Talon
        case 260: // Dragon Claw
        case 270: // Dragon Tail
        case 275: // Dragon Flight
            return false;
    }

    // Amazon bow/crossbow exclusive skills:
    switch (skillId) {
        case 6:  // Magic Arrow
        case 11: // Cold Arrow
        case 12: // Multiple Shot
        case 16: // Fire Arrow
        case 21: // Ice Arrow
        case 22: // Guided Arrow
        case 26: // Exploding Arrow
        case 27: // Strafe
        case 31: // Immolation Arrow
        case 35: // Freezing Arrow
            // Only Rogue archers (Amazon virtual class) have bow casting tokens
            return vclass == VirtualPlayerClass::Amazon;
    }

    // All standard spells, curses, summons, buffs, and warcries are supported across
    // all hireling models (they use special cast SC or standard attack A1/A2).
    return true;
}

// Scans equipped items and extracts usable charged skills matching the companion archetype.
template <typename StatExtractor>
std::vector<CompanionChargedSkill> ScanCompanionChargedSkills(
    VirtualPlayerClass vclass,
    std::span<const EquippedItemSlot> items,
    StatExtractor&& extractStats) {
    std::vector<CompanionChargedSkill> result;

    for (const auto& item : items) {
        if (!item.itemUnit) continue;

        extractStats(item, [&](uint16_t statId, uint16_t param, int32_t value) {
            if (statId != ItemStat::ChargedSkill) {
                return;
            }

            CompanionChargedSkill skill = UnpackChargedSkill(item.itemId, item.bodyLocation, param, value);
            if (skill.skillId == 0 || skill.maxCharges == 0) {
                return;
            }

            // Usability validation by virtual class
            if (!IsSkillUsableByCompanion(vclass, skill.skillId)) {
                return;
            }

            skill.kind = GetTacticalSkillKind(skill.skillId);
            result.push_back(skill);
        });
    }

    return result;
}

// Accounting helper: decrements 1 charge if realistic consumption is active.
inline bool DecrementChargedSkill(
    CompanionChargedSkill& skill,
    bool consumeCharges) noexcept {
    if (!consumeCharges) {
        // Zero-Wear Dev Mode: charges remain intact
        return true;
    }

    if (skill.currentCharges > 0) {
        skill.currentCharges--;
        return true;
    }

    return false; // Charges exhausted
}

// Evaluates a single candidate charged skill against tactical triggers and cooldowns.
inline bool EvaluateTacticalCharge(
    const CompanionChargedSkill& skill,
    const TacticalCombatSensors& sensors,
    bool consumeCharges,
    const char*& outReason) noexcept {
    outReason = "";

    // Charge check
    if (consumeCharges && skill.currentCharges == 0) {
        return false;
    }

    // Cooldown check
    const uint64_t cooldown = GetTacticalCooldownMs(skill.kind);
    if (skill.lastCastMs > 0 && sensors.nowMs < skill.lastCastMs + cooldown) {
        return false;
    }

    // Tactical trigger evaluation by priority category
    switch (skill.kind) {
        case TacticalSkillKind::Teleport:
            // Emergency escape: Health < 40% and surrounded by melee or dense pack
            if (sensors.companionHealthPct < 0.40f &&
                (sensors.meleeAttackersCount >= 2 || sensors.nearbyHostilesCount >= 3)) {
                outReason = "P1 Emergency Teleport retreat";
                return true;
            }
            return false;

        case TacticalSkillKind::Suppression:
            // Threat suppression: Ranged shooters threatening squad
            if (sensors.rangedShootersCount >= 1) {
                outReason = "P2 Projectile suppression (Slow Missiles / Cloak)";
                return true;
            }
            return false;

        case TacticalSkillKind::Curse:
            // High-value debuff: Target Elite/Boss or dense pack lacking active curse
            if ((sensors.hasActiveBoss || sensors.nearbyHostilesCount >= 3) && !sensors.targetHasCurse) {
                outReason = "P3 Assistance Curse (Life Tap / Lower Resist / Decrepify)";
                return true;
            }
            return false;

        case TacticalSkillKind::CrowdControl:
            // Area crowd control: Dense pack within range
            if (sensors.nearbyHostilesCount >= 3) {
                outReason = "P4 Area Crowd Control (Frost Nova / Glacial Spike)";
                return true;
            }
            return false;

        case TacticalSkillKind::Summon:
            // Squad enhancement: Active combat and allied summon is missing
            if (sensors.isCombatActive && !sensors.hasAlliedSummon) {
                outReason = "P5 Supportive Summon (Heart of Wolverine / Oak Sage)";
                return true;
            }
            return false;

        case TacticalSkillKind::DamageSpell:
            // Tactical offense: Engaging Boss or group
            if (sensors.isCombatActive && (sensors.hasActiveBoss || sensors.nearbyHostilesCount >= 2)) {
                outReason = "P6 Elemental Damage Burst";
                return true;
            }
            return false;

        case TacticalSkillKind::Buff:
            // Squad buff: In combat without active buff
            if (sensors.isCombatActive) {
                outReason = "P7 Squad Buff (Enchant / Armor)";
                return true;
            }
            return false;

        default:
            return false;
    }
}

// Selects the highest-priority tactical charged skill ready for execution.
inline TacticalCastDecision SelectBestTacticalCharge(
    std::span<CompanionChargedSkill> skills,
    const TacticalCombatSensors& sensors,
    bool isMasterEnabled,
    bool consumeCharges) noexcept {
    TacticalCastDecision decision{};

    // Master feature toggle check
    if (!isMasterEnabled || skills.empty()) {
        return decision;
    }

    // Evaluation in strict priority hierarchy (P1 to P7)
    constexpr std::array<TacticalSkillKind, 7> PriorityOrder = {
        TacticalSkillKind::Teleport,
        TacticalSkillKind::Suppression,
        TacticalSkillKind::Curse,
        TacticalSkillKind::CrowdControl,
        TacticalSkillKind::Summon,
        TacticalSkillKind::DamageSpell,
        TacticalSkillKind::Buff
    };

    for (TacticalSkillKind kind : PriorityOrder) {
        for (auto& skill : skills) {
            if (skill.kind != kind) continue;

            const char* reason = "";
            if (EvaluateTacticalCharge(skill, sensors, consumeCharges, reason)) {
                decision.shouldCast = true;
                decision.skillId = skill.skillId;
                decision.level = skill.level;
                decision.itemId = skill.itemId;
                decision.bodyLocation = skill.bodyLocation;
                decision.kind = skill.kind;
                decision.reason = reason;
                return decision;
            }
        }
    }

    return decision;
}

} // namespace framevr::companionai

