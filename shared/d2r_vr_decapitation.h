#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace framevr::decap {

// Checks if a bone name represents a head joint across common D2R and Biped rigs
inline bool IsHeadBoneName(std::string_view name) {
    if (name.empty()) return false;
    std::string lo;
    lo.reserve(name.size());
    for (char c : name) lo.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));

    // Exclusions for sub-parts or unrelated equipment
    if (lo.find("overhead") != std::string::npos ||
        lo.find("arrowhead") != std::string::npos ||
        lo.find("headband") != std::string::npos ||
        lo.find("necklace") != std::string::npos) {
        return false;
    }

    // High-priority joint names
    if (lo == "head_m_jnt" || lo == "head_bind_jnt" || lo == "c_neckhead_head" ||
        lo == "bip01 head" || lo == "bip01_head" || lo == "bip_head" ||
        lo == "head" || lo == "def_head" || lo == "j_head") {
        return true;
    }

    if (lo.find("head_") != std::string::npos ||
        lo.find("_head") != std::string::npos ||
        lo.find("bip01 head") != std::string::npos) {
        return true;
    }

    return false;
}

// Checks if a bone name represents a neck joint
inline bool IsNeckBoneName(std::string_view name) {
    if (name.empty()) return false;
    std::string lo;
    lo.reserve(name.size());
    for (char c : name) lo.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));

    if (lo.find("necklace") != std::string::npos) return false;

    if (lo == "neck" || lo == "neck_m_jnt" || lo == "neck_bind_jnt" ||
        lo == "bip01 neck" || lo == "bip01_neck" || lo == "bip_neck" ||
        lo == "c_neck_neck1" || lo == "c_neck_neck" || lo == "neck_01" ||
        lo == "def_neck" || lo == "j_neck") {
        return true;
    }

    if (lo.find("neck") != std::string::npos) return true;

    return false;
}

// Finds the primary head bone index in a bone hierarchy
inline int FindHeadBone(const std::vector<std::string>& names, const std::vector<int32_t>& parents) {
    (void)parents;
    const int count = static_cast<int>(names.size());

    // Pass 1: exact high-priority joint matches (prioritize parent joint over leaf dummies)
    for (int i = 0; i < count; ++i) {
        std::string lo;
        lo.reserve(names[i].size());
        for (char c : names[i]) lo.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));
        if (lo == "head_m_jnt" || lo == "head_bind_jnt" || lo == "c_neckhead_head" ||
            lo == "bip01 head" || lo == "bip01_head" || lo == "bip_head") {
            return i;
        }
    }

    // Pass 2: exact "head"
    for (int i = 0; i < count; ++i) {
        std::string lo;
        lo.reserve(names[i].size());
        for (char c : names[i]) lo.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));
        if (lo == "head") return i;
    }

    // Pass 3: general head names
    for (int i = 0; i < count; ++i) {
        if (IsHeadBoneName(names[i])) return i;
    }

    return -1;
}

// Finds the neck bone index supporting the given head bone
inline int FindNeckBone(const std::vector<std::string>& names, const std::vector<int32_t>& parents, int headBone) {
    const int count = static_cast<int>(names.size());
    if (headBone < 0 || headBone >= count) return -1;

    // Check immediate ancestors of headBone first (the actual supporting neck)
    for (int p = parents[headBone]; p >= 0 && p < count; p = parents[p]) {
        if (IsNeckBoneName(names[p])) return p;
    }

    // Pass 2: any bone containing neck that is not the head itself
    for (int i = 0; i < count; ++i) {
        if (i != headBone && IsNeckBoneName(names[i])) return i;
    }

    // Fallback: parent of headBone (spine/chest/clavicle)
    if (headBone >= 0 && headBone < count) {
        return parents[headBone];
    }

    return -1;
}

// Generates a boolean mask marking the root bone and all its descendants.
// In Granny 3D hierarchies, parent bones always precede their children (parent[i] < i).
inline std::vector<uint8_t> BuildDescendantMask(int count, const std::vector<int32_t>& parents, int rootBone) {
    std::vector<uint8_t> mask(count, 0);
    if (rootBone >= 0 && rootBone < count) {
        mask[rootBone] = 1;
        for (int i = rootBone + 1; i < count; ++i) {
            const int p = parents[i];
            if (p >= 0 && p < count && mask[p]) {
                mask[i] = 1;
            }
        }
    }
    return mask;
}

// Collapses head and descendant bones into the neck pivot point.
// Matrix16 requires float m[16] row-vector layout (standard Granny 3D world pose).
template <typename Matrix16>
inline void CollapseHeadBones(Matrix16* worldPoses, int boneCount,
                              const std::vector<uint8_t>& inHead,
                              int headBone, int neckBone,
                              float outPivot[3] = nullptr) {
    if (!worldPoses || headBone < 0 || headBone >= boneCount) return;

    // Determine pivot point: neck bone position if valid, else head bone position
    const int pivotBone = (neckBone >= 0 && neckBone < boneCount) ? neckBone : headBone;
    const float px = worldPoses[pivotBone].m[12];
    const float py = worldPoses[pivotBone].m[13];
    const float pz = worldPoses[pivotBone].m[14];

    if (outPivot) {
        outPivot[0] = px;
        outPivot[1] = py;
        outPivot[2] = pz;
    }

    const size_t n = std::min<size_t>(static_cast<size_t>(boneCount), inHead.size());
    for (size_t i = 0; i < n; ++i) {
        if (inHead[i]) {
            auto& mat = worldPoses[i];
            // Collapse rotation and scale to 0.0001f
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    mat.m[r * 4 + c] *= 0.0001f;
                }
            }
            // Pin translation to neck cut-off pivot
            mat.m[12] = px;
            mat.m[13] = py;
            mat.m[14] = pz;
        }
    }
}

// State of an active decapitated monster instance
struct DecapInstance {
    uintptr_t skeletonInstance = 0;
    uint32_t monsterId = 0;
    uint64_t decapitatedAt = 0;
    uint64_t expiresAt = 0;
    int headBone = -1;
    int neckBone = -1;
    std::vector<uint8_t> inHead;
    float lastPivot[3] = {};
};

// Thread-safe manager for tracked decapitations in Granny 3D
class DecapitationManager {
public:
    static constexpr uint64_t kDefaultTtlMs = 60000; // 60 seconds (covers death anim + corpse persistence)
    static constexpr size_t kMaxInstances = 512;

    bool RegisterDecapitation(uintptr_t self, uint32_t monsterId, int headBone, int neckBone,
                              const std::vector<uint8_t>& inHead, uint64_t now, uint64_t ttlMs = kDefaultTtlMs) {
        if (!self || headBone < 0) return false;
        std::unique_lock lock(mutex_);

        if (instances_.size() >= kMaxInstances) {
            PruneExpiredLocked(now);
            if (instances_.size() >= kMaxInstances) {
                instances_.clear();
            }
        }

        DecapInstance inst{};
        inst.skeletonInstance = self;
        inst.monsterId = monsterId;
        inst.decapitatedAt = now;
        inst.expiresAt = now + ttlMs;
        inst.headBone = headBone;
        inst.neckBone = neckBone;
        inst.inHead = inHead;

        instances_[self] = std::move(inst);
        return true;
    }

    bool IsDecapitated(uintptr_t self, uint64_t now) const {
        if (!self) return false;
        std::shared_lock lock(mutex_);
        auto it = instances_.find(self);
        if (it != instances_.end()) {
            return now < it->second.expiresAt;
        }
        return false;
    }

    template <typename Matrix16>
    bool ApplyCollapse(uintptr_t self, Matrix16* worldPoses, int boneCount, uint64_t now, float outPivot[3] = nullptr) {
        if (!self || !worldPoses || boneCount <= 0) return false;

        std::shared_lock lock(mutex_);
        auto it = instances_.find(self);
        if (it == instances_.end() || now >= it->second.expiresAt) {
            return false;
        }

        const auto& inst = it->second;
        CollapseHeadBones(worldPoses, boneCount, inst.inHead, inst.headBone, inst.neckBone, outPivot);
        return true;
    }

    void PruneExpired(uint64_t now) {
        std::unique_lock lock(mutex_);
        PruneExpiredLocked(now);
    }

    void Clear() {
        std::unique_lock lock(mutex_);
        instances_.clear();
    }

    size_t Count() const {
        std::shared_lock lock(mutex_);
        return instances_.size();
    }

    bool GetInstance(uintptr_t self, DecapInstance& out) const {
        std::shared_lock lock(mutex_);
        auto it = instances_.find(self);
        if (it != instances_.end()) {
            out = it->second;
            return true;
        }
        return false;
    }

private:
    void PruneExpiredLocked(uint64_t now) {
        for (auto it = instances_.begin(); it != instances_.end();) {
            if (now >= it->second.expiresAt) {
                it = instances_.erase(it);
            } else {
                ++it;
            }
        }
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<uintptr_t, DecapInstance> instances_;
};

inline DecapitationManager& Manager() {
    static DecapitationManager s_manager;
    return s_manager;
}

} // namespace framevr::decap

