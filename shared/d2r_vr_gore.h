#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include "d2r_vr_state.h"

namespace framevr::gore {

enum class GoreType : uint8_t {
    None              = 0,
    SlashDecapitation = 1, // Sharp weapon lethal slash / decapitation
    BluntBoneCrush    = 2, // Blunt weapon heavy skull / bone crushing impact
    PiercingArterial  = 3, // Puncture / arrow / javelin deep strike
    CorpseExplosion   = 4, // Necromancer corpse detonation
    ColdShatter       = 5, // Frozen enemy shattered into crystalline chunks
    PoisonBurst       = 6, // Acidic / poison bile explosion
    GenericLethal     = 7  // Other fatal blow
};

enum class BloodTaxonomy : uint8_t {
    DemonIchor   = 0, // Dark black / tar-like demonic ichor
    UndeadRot    = 1, // Grey / yellowish decayed rot and bone dust
    BeastCrimson = 2, // Vivid red animal / beast blood
    InsectBile   = 3  // Acidic green / lime bile
};

struct GoreEvent {
    uint64_t timestamp = 0;       // System timestamp (GetTickCount64)
    uint32_t monsterId = 0;       // D2 monster unit ID (dwUnitId)
    uint32_t classId = 0;         // D2 monster class ID (MonStats row)
    GoreType type = GoreType::None;
    BloodTaxonomy taxonomy = BloodTaxonomy::DemonIchor;
    int hand = 0;                 // 0 = right hand, 1 = left hand
    bool lethal = false;          // True if the blow was fatal
    bool critical = false;        // True if critical strike / crushing blow
    float distanceSubTiles = 0.f; // Distance in game sub-tiles (5 sub-tiles = 1 tile)
    float worldPos[3] = {0,0,0};  // Game world coordinates
    float impactNormal[3] = {0,0,0}; // Strike impulse direction vector
};

inline bool IsCuttingWeapon(uint32_t weaponType) {
    switch (weaponType) {
        case D2RVR_TYPE_SWORD:
        case D2RVR_TYPE_AXE:
        case D2RVR_TYPE_DAGGER:
        case D2RVR_TYPE_POLEARM:
        case D2RVR_TYPE_CLAW:
            return true;
        default:
            return false;
    }
}

inline bool IsBluntWeapon(uint32_t weaponType) {
    switch (weaponType) {
        case D2RVR_TYPE_MACE:
        case D2RVR_TYPE_SCEPTER:
        case D2RVR_TYPE_STAFF:
        case D2RVR_TYPE_WAND:
        case D2RVR_TYPE_UNARMED:
            return true;
        default:
            return false;
    }
}

inline BloodTaxonomy TaxonomyForMonster(uint32_t classId) {
    // Skeletons, Zombies, Wraiths, Mummies, Vampires, Knights
    if ((classId <= 3) || (classId >= 16 && classId <= 20) || (classId >= 64 && classId <= 74)) {
        return BloodTaxonomy::UndeadRot;
    }
    // Sand Maggots, Giant Spiders, Mosquitoes, Swarms
    if ((classId >= 58 && classId <= 63) || (classId >= 150 && classId <= 165)) {
        return BloodTaxonomy::InsectBile;
    }
    // Beasts, Animals (Quill Rats, Yetis, Brutes, Blood Hawks)
    if ((classId >= 21 && classId <= 35) || (classId >= 40 && classId <= 45)) {
        return BloodTaxonomy::BeastCrimson;
    }
    // Default to Demonic Ichor for Fallens, Megademons, Overlords, Bosses
    return BloodTaxonomy::DemonIchor;
}

inline GoreType ClassifyMeleeGore(uint32_t weaponType, bool lethal, bool critical) {
    if (!lethal && !critical) return GoreType::None;
    if (IsCuttingWeapon(weaponType)) {
        return GoreType::SlashDecapitation;
    }
    if (IsBluntWeapon(weaponType)) {
        return GoreType::BluntBoneCrush;
    }
    if (weaponType == D2RVR_TYPE_BOW || weaponType == D2RVR_TYPE_CROSSBOW ||
        weaponType == D2RVR_TYPE_SPEAR || weaponType == D2RVR_TYPE_JAVELIN) {
        return GoreType::PiercingArterial;
    }
    return lethal ? GoreType::GenericLethal : GoreType::None;
}

// Thread-safe lock-free circular queue for IPC / inter-thread event handoff
class EventQueue {
public:
    static constexpr size_t Capacity = 16;

    void Publish(const GoreEvent& ev) {
        const uint32_t h = head.load(std::memory_order_relaxed);
        buffer[h % Capacity] = ev;
        head.store(h + 1, std::memory_order_release);
    }

    bool Take(GoreEvent& out) {
        uint32_t t = tail.load(std::memory_order_relaxed);
        const uint32_t h = head.load(std::memory_order_acquire);
        if (t == h) return false;
        // In case of ring overflow, advance tail to latest available window
        if (h - t > Capacity) {
            t = h - Capacity;
        }
        out = buffer[t % Capacity];
        tail.store(t + 1, std::memory_order_release);
        return true;
    }

    bool Peek(GoreEvent& out) const {
        uint32_t t = tail.load(std::memory_order_relaxed);
        const uint32_t h = head.load(std::memory_order_acquire);
        if (t == h) return false;
        if (h - t > Capacity) t = h - Capacity;
        out = buffer[t % Capacity];
        return true;
    }

    // Finds the most recent event for a given hand within maxAgeMs
    bool LatestForHand(int hand, uint64_t now, uint64_t maxAgeMs, GoreEvent& out) {
        const uint32_t h = head.load(std::memory_order_acquire);
        uint32_t t = tail.load(std::memory_order_relaxed);
        if (t == h) return false;
        if (h - t > Capacity) t = h - Capacity;

        for (uint32_t i = h; i > t; --i) {
            const auto& candidate = buffer[(i - 1) % Capacity];
            if (candidate.hand == hand && candidate.timestamp <= now && (now - candidate.timestamp) <= maxAgeMs) {
                out = candidate;
                return true;
            }
        }
        return false;
    }

    // Finds the most recent decapitation event within maxAgeMs and maxDist of (x, z)
    bool FindRecentDecapitation(uint64_t now, uint64_t maxAgeMs, float x, float z, float maxDist, GoreEvent& out) const {
        const uint32_t h = head.load(std::memory_order_acquire);
        uint32_t t = tail.load(std::memory_order_relaxed);
        if (t == h) return false;
        if (h - t > Capacity) t = h - Capacity;

        for (uint32_t i = h; i > t; --i) {
            const auto& candidate = buffer[(i - 1) % Capacity];
            if (candidate.type == GoreType::SlashDecapitation &&
                candidate.timestamp <= now && (now - candidate.timestamp) <= maxAgeMs) {
                const float dx = candidate.worldPos[0] - x;
                const float dz = candidate.worldPos[2] - z;
                if (std::hypot(dx, dz) <= maxDist) {
                    out = candidate;
                    return true;
                }
            }
        }
        return false;
    }

    // Finds and consumes the most recent decapitation event within maxAgeMs and maxDist of (x, z).
    // Consuming ensures only ONE monster skeleton claims the decapitation, preventing nearby living monsters from losing heads.
    bool ConsumeRecentDecapitation(uint64_t now, uint64_t maxAgeMs, float x, float z, float maxDist, GoreEvent& out) {
        const uint32_t h = head.load(std::memory_order_acquire);
        uint32_t t = tail.load(std::memory_order_relaxed);
        if (t == h) return false;
        if (h - t > Capacity) t = h - Capacity;

        for (uint32_t i = h; i > t; --i) {
            auto& candidate = buffer[(i - 1) % Capacity];
            if (candidate.type == GoreType::SlashDecapitation &&
                candidate.timestamp <= now && (now - candidate.timestamp) <= maxAgeMs) {
                const float dx = candidate.worldPos[0] - x;
                const float dz = candidate.worldPos[2] - z;
                if (std::hypot(dx, dz) <= maxDist) {
                    out = candidate;
                    candidate.type = GoreType::None; // Atomically consumed
                    return true;
                }
            }
        }
        return false;
    }

    uint32_t Count() const {
        const uint32_t h = head.load(std::memory_order_acquire);
        const uint32_t t = tail.load(std::memory_order_relaxed);
        if (h <= t) return 0;
        return std::min<uint32_t>(h - t, Capacity);
    }

    void Clear() {
        const uint32_t h = head.load(std::memory_order_relaxed);
        tail.store(h, std::memory_order_release);
    }

private:
    std::array<GoreEvent, Capacity> buffer{};
    std::atomic<uint32_t> head{0};
    std::atomic<uint32_t> tail{0};
};

inline EventQueue& Queue() {
    static EventQueue s_queue;
    return s_queue;
}

inline void Publish(const GoreEvent& ev) {
    Queue().Publish(ev);
}

inline bool Take(GoreEvent& out) {
    return Queue().Take(out);
}

inline bool LatestForHand(int hand, uint64_t now, uint64_t maxAgeMs, GoreEvent& out) {
    return Queue().LatestForHand(hand, now, maxAgeMs, out);
}

inline void Reset() {
    Queue().Clear();
}

} // namespace framevr::gore

