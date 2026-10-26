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

// Simulates ballistic trajectory, ground bounce, friction and tumble rotation for a severed head
inline void SimulateHeadPhysics(float t, const float p0[3], const float v0[3],
                                float floorY, const float spinAxis[3], float spinSpeed,
                                float outPos[3], float outRot[9]) {
    constexpr float g = 9.81f;
    constexpr float r = 0.12f; // approximate head radius
    const float groundY = floorY + r;

    // Time to first ground impact: 1/2 g t^2 - v0_y t - h0 = 0
    const float h0 = std::max(0.01f, p0[1] - groundY);
    const float disc1 = v0[1] * v0[1] + 2.0f * g * h0;
    const float t1 = (v0[1] + std::sqrt(std::max(0.0f, disc1))) / g;

    // First bounce: restitution e = 0.35
    constexpr float e = 0.35f;
    const float v1 = -(v0[1] - g * t1); // downward speed at ground
    const float vBounce1 = std::max(0.0f, v1 * e);
    const float dtBounce1 = (2.0f * vBounce1) / g;
    const float t2 = t1 + dtBounce1;

    // Second bounce:
    const float vBounce2 = vBounce1 * e;
    const float dtBounce2 = (2.0f * vBounce2) / g;
    const float t3 = t2 + dtBounce2;

    float py = groundY;
    if (t < t1) {
        py = p0[1] + v0[1] * t - 0.5f * g * t * t;
    } else if (t < t2) {
        const float tb = t - t1;
        py = groundY + vBounce1 * tb - 0.5f * g * tb * tb;
    } else if (t < t3) {
        const float tb = t - t2;
        py = groundY + vBounce2 * tb - 0.5f * g * tb * tb;
    } else {
        py = groundY;
    }
    py = std::max(groundY, py);

    // Horizontal travel with exponential damping / ground friction
    constexpr float mu = 2.5f;
    const float hFactor = (1.0f - std::exp(-mu * t)) / mu;
    const float px = p0[0] + v0[0] * hFactor;
    const float pz = p0[2] + v0[2] * hFactor;

    outPos[0] = px;
    outPos[1] = py;
    outPos[2] = pz;

    // Angular tumble rotation around spinAxis:
    const float angle = spinSpeed * hFactor;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float k = 1.0f - c;
    const float x = spinAxis[0], y = spinAxis[1], z = spinAxis[2];

    // Rodrigues' rotation matrix (row-major)
    outRot[0] = c + x * x * k;
    outRot[1] = x * y * k - z * s;
    outRot[2] = x * z * k + y * s;

    outRot[3] = y * x * k + z * s;
    outRot[4] = c + y * y * k;
    outRot[5] = y * z * k - x * s;

    outRot[6] = z * x * k - y * s;
    outRot[7] = z * y * k + x * s;
    outRot[8] = c + z * z * k;
}

// 4x4 matrix helpers for rigid skeletal manipulation in model space
struct DecapMat {
    float m[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

inline DecapMat DecapMul(const DecapMat& a, const DecapMat& b) {
    DecapMat r{};
    for (int row = 0; row < 4; ++row) {
        for (int k = 0; k < 4; ++k) {
            const float x = a.m[row * 4 + k];
            for (int col = 0; col < 4; ++col) {
                r.m[row * 4 + col] += x * b.m[k * 4 + col];
            }
        }
    }
    return r;
}

inline DecapMat DecapInvAffine(const DecapMat& a) {
    const float* m = a.m;
    const float c00 = m[5]*m[10] - m[6]*m[9], c01 = m[6]*m[8] - m[4]*m[10], c02 = m[4]*m[9] - m[5]*m[8];
    const float det = m[0]*c00 + m[1]*c01 + m[2]*c02;
    const float id = std::fabs(det) > 1e-12f ? 1.0f / det : 0.0f;
    DecapMat r{};
    r.m[0] = c00*id;                       r.m[1] = (m[2]*m[9] - m[1]*m[10])*id; r.m[2] = (m[1]*m[6] - m[2]*m[5])*id;
    r.m[4] = c01*id;                       r.m[5] = (m[0]*m[10] - m[2]*m[8])*id; r.m[6] = (m[2]*m[4] - m[0]*m[6])*id;
    r.m[8] = c02*id;                       r.m[9] = (m[1]*m[8] - m[0]*m[9])*id;  r.m[10] = (m[0]*m[5] - m[1]*m[4])*id;
    for (int j = 0; j < 3; ++j) r.m[12+j] = -(m[12]*r.m[j] + m[13]*r.m[4+j] + m[14]*r.m[8+j]);
    r.m[15] = 1.0f;
    return r;
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
    float neckPivot[3] = {};
    float headInitialPos[3] = {};
    DecapMat initialHeadMat{};
    std::vector<DecapMat> initialRelMats;
    float velocity[3] = {};
    float spinAxis[3] = {0.0f, 1.0f, 0.0f};
    float spinSpeed = 0.0f;
    float floorY = 0.0f;
    bool headPhysics = true;
    bool initialized = false;
};

// Thread-safe manager for tracked decapitations in Granny 3D
class DecapitationManager {
public:
    static constexpr uint64_t kDefaultTtlMs = 60000; // 60 seconds (covers death anim + corpse persistence)
    static constexpr size_t kMaxInstances = 512;

    bool RegisterDecapitation(uintptr_t self, uint32_t monsterId, int headBone, int neckBone,
                              const std::vector<uint8_t>& inHead, uint64_t now,
                              float floorY, const float impulse[3],
                              bool headPhysics, uint64_t ttlMs = kDefaultTtlMs) {
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
        inst.floorY = floorY;
        inst.headPhysics = headPhysics;

        if (impulse && (std::fabs(impulse[0]) > 1e-4f || std::fabs(impulse[2]) > 1e-4f)) {
            float hx = impulse[0], hz = impulse[2];
            float hlen = std::hypot(hx, hz);
            if (hlen > 1e-4f) { hx /= hlen; hz /= hlen; }
            inst.velocity[0] = hx * 0.75f;
            inst.velocity[1] = 0.90f; // realistic pop
            inst.velocity[2] = hz * 0.75f;
        } else {
            inst.velocity[0] = 0.5f;
            inst.velocity[1] = 0.90f;
            inst.velocity[2] = 0.5f;
        }

        // Compute spin axis perpendicular to trajectory
        float ax = -inst.velocity[2], ay = 0.5f, az = inst.velocity[0];
        float alen = std::hypot(ax, ay, az);
        if (alen > 1e-4f) {
            inst.spinAxis[0] = ax / alen;
            inst.spinAxis[1] = ay / alen;
            inst.spinAxis[2] = az / alen;
        } else {
            inst.spinAxis[0] = 0.0f; inst.spinAxis[1] = 1.0f; inst.spinAxis[2] = 0.0f;
        }
        inst.spinSpeed = 5.5f;

        instances_[self] = std::move(inst);
        return true;
    }

    bool RegisterDecapitation(uintptr_t self, uint32_t monsterId, int headBone, int neckBone,
                              const std::vector<uint8_t>& inHead, uint64_t now,
                              uint64_t ttlMs = kDefaultTtlMs) {
        return RegisterDecapitation(self, monsterId, headBone, neckBone, inHead, now, 0.0f, nullptr, false, ttlMs);
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

        std::unique_lock lock(mutex_);
        auto it = instances_.find(self);
        if (it == instances_.end() || now >= it->second.expiresAt) {
            return false;
        }

        auto& inst = it->second;

        // Initialize initial pivot and capture relative child poses on first frame
        if (!inst.initialized) {
            const int neck = (inst.neckBone >= 0 && inst.neckBone < boneCount) ? inst.neckBone : inst.headBone;
            if (neck >= 0 && neck < boneCount) {
                inst.neckPivot[0] = worldPoses[neck].m[12];
                inst.neckPivot[1] = worldPoses[neck].m[13];
                inst.neckPivot[2] = worldPoses[neck].m[14];
            }

            const int head = (inst.headBone >= 0 && inst.headBone < boneCount) ? inst.headBone : inst.neckBone;
            if (head >= 0 && head < boneCount) {
                inst.headInitialPos[0] = worldPoses[head].m[12];
                inst.headInitialPos[1] = worldPoses[head].m[13];
                inst.headInitialPos[2] = worldPoses[head].m[14];

                for (int k = 0; k < 16; ++k) {
                    inst.initialHeadMat.m[k] = worldPoses[head].m[k];
                }

                const DecapMat invHead = DecapInvAffine(inst.initialHeadMat);
                inst.initialRelMats.resize(boneCount);
                const size_t n = std::min<size_t>(static_cast<size_t>(boneCount), inst.inHead.size());
                for (size_t i = 0; i < n; ++i) {
                    if (inst.inHead[i]) {
                        DecapMat cur{};
                        for (int k = 0; k < 16; ++k) cur.m[k] = worldPoses[i].m[k];
                        inst.initialRelMats[i] = DecapMul(cur, invHead);
                    }
                }
            }
            // In model space, ground floor is always Y = 0.0f
            inst.floorY = 0.0f;
            inst.initialized = true;
        }

        if (outPivot) {
            outPivot[0] = inst.neckPivot[0];
            outPivot[1] = inst.neckPivot[1];
            outPivot[2] = inst.neckPivot[2];
        }

        if (!inst.headPhysics) {
            CollapseHeadBones(worldPoses, boneCount, inst.inHead, inst.headBone, inst.neckBone, nullptr);
            return true;
        }

        // Severed head physics simulation in model space
        const float t = (now > inst.decapitatedAt) ? float(now - inst.decapitatedAt) / 1000.0f : 0.0f;
        float headPos[3] = {};
        float headRot[9] = {};
        SimulateHeadPhysics(t, inst.headInitialPos, inst.velocity, inst.floorY,
                            inst.spinAxis, inst.spinSpeed, headPos, headRot);

        if (inst.headBone >= 0 && inst.headBone < boneCount) {
            // Compute full 4x4 matrix for head bone: tumbled rotation applied to initial orientation
            DecapMat newHeadMat{};
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    newHeadMat.m[r * 4 + c] = headRot[r * 3 + 0] * inst.initialHeadMat.m[0 * 4 + c] +
                                              headRot[r * 3 + 1] * inst.initialHeadMat.m[1 * 4 + c] +
                                              headRot[r * 3 + 2] * inst.initialHeadMat.m[2 * 4 + c];
                }
            }
            newHeadMat.m[12] = headPos[0];
            newHeadMat.m[13] = headPos[1];
            newHeadMat.m[14] = headPos[2];
            newHeadMat.m[15] = 1.0f;

            for (int k = 0; k < 16; ++k) {
                worldPoses[inst.headBone].m[k] = newHeadMat.m[k];
            }

            // Child bones (jaw, eyes, horns, teeth, snout, ears) rigidly follow the head bone
            // using their exact rest relative transform - preventing any mesh stretching or deformation
            const size_t n = std::min<size_t>(static_cast<size_t>(boneCount), inst.inHead.size());
            for (size_t i = 0; i < n; ++i) {
                if (int(i) != inst.headBone && inst.inHead[i] && i < inst.initialRelMats.size()) {
                    const DecapMat childPosed = DecapMul(inst.initialRelMats[i], newHeadMat);
                    for (int k = 0; k < 16; ++k) {
                        worldPoses[i].m[k] = childPosed.m[k];
                    }
                }
            }
        }

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

