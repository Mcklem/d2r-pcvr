#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "d2r_vr_gore.h"
#include "d2r_vr_decapitation.h"

namespace {

struct MockMatrix {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    static MockMatrix Translation(float x, float y, float z) {
        MockMatrix mat;
        mat.m[12] = x;
        mat.m[13] = y;
        mat.m[14] = z;
        return mat;
    }
};

void TestBoneIdentification() {
    printf("[granny-decapitation-test] Suite 1: Bone Identification & Hierarchy Parsing\n");

    // 1. Act I Fallen / Fallen Shaman rig
    {
        std::vector<std::string> names = {
            "root", "spine", "chest", "neck_M_JNT", "head_M_JNT", "jaw", "horn_L", "horn_R"
        };
        std::vector<int32_t> parents = {-1, 0, 1, 2, 3, 4, 4, 4};
        int head = framevr::decap::FindHeadBone(names, parents);
        int neck = framevr::decap::FindNeckBone(names, parents, head);
        assert(head == 4);
        assert(neck == 3);
        assert(names[head] == "head_M_JNT");
        assert(names[neck] == "neck_M_JNT");
    }

    // 2. Act I Zombie / Ghoul Biped rig
    {
        std::vector<std::string> names = {
            "Bip01", "Bip01 Pelvis", "Bip01 Spine", "Bip01 Neck", "Bip01 Head", "Head_End"
        };
        std::vector<int32_t> parents = {-1, 0, 1, 2, 3, 4};
        int head = framevr::decap::FindHeadBone(names, parents);
        int neck = framevr::decap::FindNeckBone(names, parents, head);
        assert(head == 4);
        assert(neck == 3);
        assert(names[head] == "Bip01 Head");
        assert(names[neck] == "Bip01 Neck");
    }

    // 3. Act I Skeleton warrior rig
    {
        std::vector<std::string> names = {
            "root", "spine_01", "neck", "head", "teeth"
        };
        std::vector<int32_t> parents = {-1, 0, 1, 2, 3};
        int head = framevr::decap::FindHeadBone(names, parents);
        int neck = framevr::decap::FindNeckBone(names, parents, head);
        assert(head == 3);
        assert(neck == 2);
        assert(names[head] == "head");
        assert(names[neck] == "neck");
    }

    // 4. Act I Corrupt Rogue rig (Hero-style Amazon bind joints)
    {
        std::vector<std::string> names = {
            "root", "spine_01_bind_jnt", "neck_bind_jnt", "head_bind_jnt", "hair_01", "hair_02"
        };
        std::vector<int32_t> parents = {-1, 0, 1, 2, 3, 4};
        int head = framevr::decap::FindHeadBone(names, parents);
        int neck = framevr::decap::FindNeckBone(names, parents, head);
        assert(head == 3);
        assert(neck == 2);
        assert(names[head] == "head_bind_jnt");
        assert(names[neck] == "neck_bind_jnt");
    }

    // 5. Werewolf / Werebear enemy rig
    {
        std::vector<std::string> names = {
            "root", "C_spine1", "C_Neck_neck1", "C_NeckHead_head", "snout"
        };
        std::vector<int32_t> parents = {-1, 0, 1, 2, 3};
        int head = framevr::decap::FindHeadBone(names, parents);
        int neck = framevr::decap::FindNeckBone(names, parents, head);
        assert(head == 3);
        assert(neck == 2);
        assert(names[head] == "C_NeckHead_head");
        assert(names[neck] == "C_Neck_neck1");
    }

    // 6. Assassin rig edge-case: joint "head_M_JNT" with a leaf dummy "head"
    {
        std::vector<std::string> names = {
            "root", "spine", "neck_main_00_M_JNT", "head_M_JNT", "head"
        };
        std::vector<int32_t> parents = {-1, 0, 1, 2, 3};
        int head = framevr::decap::FindHeadBone(names, parents);
        int neck = framevr::decap::FindNeckBone(names, parents, head);
        // Must select the main joint head_M_JNT (index 3), not the leaf dummy (index 4)
        assert(head == 3);
        assert(neck == 2);
    }

    // 7. Non-humanoid / headless entity (e.g. object or spider)
    {
        std::vector<std::string> names = {"root", "body", "leg1", "leg2"};
        std::vector<int32_t> parents = {-1, 0, 1, 1};
        int head = framevr::decap::FindHeadBone(names, parents);
        assert(head == -1);
    }
}

void TestDescendantHierarchyMask() {
    printf("[granny-decapitation-test] Suite 2: Descendant Hierarchy Mask Generation\n");

    // Full humanoid rig hierarchy:
    // 0: root
    // 1: spine (parent 0)
    // 2: chest (parent 1)
    // 3: neck (parent 2)
    // 4: head (parent 3)
    // 5: jaw (parent 4)
    // 6: horn_L (parent 4)
    // 7: horn_R (parent 4)
    // 8: eye_L (parent 4)
    // 9: shoulder_L (parent 2)
    // 10: arm_L (parent 9)
    // 11: hand_L (parent 10)
    // 12: shoulder_R (parent 2)
    // 13: arm_R (parent 12)
    // 14: hand_R (parent 13)
    // 15: pelvis (parent 0)
    // 16: leg_L (parent 15)
    // 17: leg_R (parent 15)
    const int count = 18;
    std::vector<int32_t> parents = {
        -1, 0, 1, 2, 3, 4, 4, 4, 4, 2, 9, 10, 2, 12, 13, 0, 15, 15
    };

    auto mask = framevr::decap::BuildDescendantMask(count, parents, 4); // root at head (4)

    assert(mask.size() == count);
    // Head and descendants must be 1
    assert(mask[4] == 1); // head
    assert(mask[5] == 1); // jaw
    assert(mask[6] == 1); // horn_L
    assert(mask[7] == 1); // horn_R
    assert(mask[8] == 1); // eye_L

    // Non-head bones must be 0
    assert(mask[0] == 0);  // root
    assert(mask[1] == 0);  // spine
    assert(mask[2] == 0);  // chest
    assert(mask[3] == 0);  // neck
    assert(mask[9] == 0);  // shoulder_L
    assert(mask[10] == 0); // arm_L
    assert(mask[11] == 0); // hand_L
    assert(mask[12] == 0); // shoulder_R
    assert(mask[13] == 0); // arm_R
    assert(mask[14] == 0); // hand_R
    assert(mask[15] == 0); // pelvis
    assert(mask[16] == 0); // leg_L
    assert(mask[17] == 0); // leg_R
}

void TestMatrixCollapseMath() {
    printf("[granny-decapitation-test] Suite 3: Matrix Collapse Math\n");

    constexpr int count = 6;
    std::vector<MockMatrix> poses(count);

    // Bone 0: Root at (10, 0, 20)
    poses[0] = MockMatrix::Translation(10.0f, 0.0f, 20.0f);
    // Bone 1: Spine at (10, 1, 20)
    poses[1] = MockMatrix::Translation(10.0f, 1.0f, 20.0f);
    // Bone 2: Neck at (10, 1.5, 20)
    poses[2] = MockMatrix::Translation(10.0f, 1.5f, 20.0f);
    // Bone 3: Head at (10, 1.8, 20)
    poses[3] = MockMatrix::Translation(10.0f, 1.8f, 20.0f);
    // Bone 4: Horn at (10.2, 2.1, 20.1)
    poses[4] = MockMatrix::Translation(10.2f, 2.1f, 20.1f);
    // Bone 5: Arm at (10.5, 1.2, 20.0)
    poses[5] = MockMatrix::Translation(10.5f, 1.2f, 20.0f);

    std::vector<uint8_t> inHead(count, 0);
    inHead[3] = 1; // head
    inHead[4] = 1; // horn

    float outPivot[3] = {};
    framevr::decap::CollapseHeadBones(poses.data(), count, inHead, 3, 2, outPivot);

    // 1. Pivot must match neck translation
    assert(std::fabs(outPivot[0] - 10.0f) < 1e-4f);
    assert(std::fabs(outPivot[1] - 1.5f) < 1e-4f);
    assert(std::fabs(outPivot[2] - 20.0f) < 1e-4f);

    // 2. Neck bone must be completely untouched
    assert(std::fabs(poses[2].m[12] - 10.0f) < 1e-4f);
    assert(std::fabs(poses[2].m[13] - 1.5f) < 1e-4f);
    assert(std::fabs(poses[2].m[14] - 20.0f) < 1e-4f);
    assert(std::fabs(poses[2].m[0] - 1.0f) < 1e-4f);
    assert(std::fabs(poses[2].m[5] - 1.0f) < 1e-4f);
    assert(std::fabs(poses[2].m[10] - 1.0f) < 1e-4f);

    // 3. Head bone must have translation pinned to neck pivot and 3x3 scaled down to 0.0001
    assert(std::fabs(poses[3].m[12] - 10.0f) < 1e-4f);
    assert(std::fabs(poses[3].m[13] - 1.5f) < 1e-4f);
    assert(std::fabs(poses[3].m[14] - 20.0f) < 1e-4f);
    assert(std::fabs(poses[3].m[0] - 0.0001f) < 1e-6f);
    assert(std::fabs(poses[3].m[5] - 0.0001f) < 1e-6f);
    assert(std::fabs(poses[3].m[10] - 0.0001f) < 1e-6f);

    // 4. Horn bone must also be collapsed to neck pivot and scaled down
    assert(std::fabs(poses[4].m[12] - 10.0f) < 1e-4f);
    assert(std::fabs(poses[4].m[13] - 1.5f) < 1e-4f);
    assert(std::fabs(poses[4].m[14] - 20.0f) < 1e-4f);
    assert(std::fabs(poses[4].m[0] - 0.0001f) < 1e-6f);

    // 5. Spine and Arm must be completely untouched
    assert(std::fabs(poses[1].m[13] - 1.0f) < 1e-4f);
    assert(std::fabs(poses[1].m[0] - 1.0f) < 1e-4f);
    assert(std::fabs(poses[5].m[12] - 10.5f) < 1e-4f);
    assert(std::fabs(poses[5].m[0] - 1.0f) < 1e-4f);
}

void TestDecapitationManagerLifecycle() {
    printf("[granny-decapitation-test] Suite 4: Decapitation Manager Lifecycle & TTL\n");

    auto& mgr = framevr::decap::Manager();
    mgr.Clear();
    assert(mgr.Count() == 0);

    const uintptr_t skelA = 0x1000A0;
    const uintptr_t skelB = 0x1000B0;
    const uint64_t t0 = 10000;

    std::vector<uint8_t> inHead = {0, 0, 0, 1, 1}; // 5 bones, 3 & 4 in head

    // Register instance A
    bool reg = mgr.RegisterDecapitation(skelA, 42, 3, 2, inHead, t0, 60000);
    assert(reg);
    assert(mgr.Count() == 1);

    // At t0 + 1000ms: active
    assert(mgr.IsDecapitated(skelA, t0 + 1000));
    assert(!mgr.IsDecapitated(skelB, t0 + 1000));

    // Posing collapse test
    std::vector<MockMatrix> poses = {
        MockMatrix::Translation(0, 0, 0),
        MockMatrix::Translation(0, 1, 0),
        MockMatrix::Translation(0, 1.5f, 0),
        MockMatrix::Translation(0, 1.8f, 0),
        MockMatrix::Translation(0, 2.0f, 0)
    };
    float pivot[3] = {};
    bool applied = mgr.ApplyCollapse(skelA, poses.data(), 5, t0 + 5000, pivot);
    assert(applied);
    assert(std::fabs(pivot[1] - 1.5f) < 1e-4f);
    assert(std::fabs(poses[3].m[13] - 1.5f) < 1e-4f);
    assert(std::fabs(poses[3].m[0] - 0.0001f) < 1e-6f);

    // At t0 + 30000ms: still active
    assert(mgr.IsDecapitated(skelA, t0 + 30000));

    // At t0 + 60001ms: expired
    assert(!mgr.IsDecapitated(skelA, t0 + 60001));

    // Prune
    mgr.PruneExpired(t0 + 60001);
    assert(mgr.Count() == 0);

    mgr.Clear();
}

void TestEventQueueDecapitationMatching() {
    printf("[granny-decapitation-test] Suite 5: Event Queue Spatial Decapitation Matching\n");

    framevr::gore::EventQueue queue;
    queue.Clear();

    const uint64_t t0 = 5000;

    framevr::gore::GoreEvent ev{};
    ev.timestamp = t0;
    ev.monsterId = 101;
    ev.type = framevr::gore::GoreType::SlashDecapitation;
    ev.worldPos[0] = 20.0f;
    ev.worldPos[1] = 0.0f;
    ev.worldPos[2] = 30.0f;
    queue.Publish(ev);

    assert(queue.Count() == 1);

    // 1. Exact / nearby match (0.5m away) within 500ms
    framevr::gore::GoreEvent matched{};
    bool found = queue.FindRecentDecapitation(t0 + 500, 1500, 20.3f, 30.4f, 2.5f, matched);
    assert(found);
    assert(matched.monsterId == 101);

    // 2. Distant entity (10m away) must not match
    framevr::gore::GoreEvent distant{};
    bool foundDistant = queue.FindRecentDecapitation(t0 + 500, 1500, 30.0f, 30.0f, 2.5f, distant);
    assert(!foundDistant);

    // 3. Stale event (> 1500ms) must not match
    framevr::gore::GoreEvent stale{};
    bool foundStale = queue.FindRecentDecapitation(t0 + 2000, 1500, 20.1f, 30.1f, 2.5f, stale);
    assert(!foundStale);
}


void TestSeveredHeadPhysicsTrajectory() {
    printf("[granny-decapitation-test] Suite 6: Severed Head Physics Trajectory & Floor Bounce\n");

    const float p0[3] = {10.0f, 1.5f, 20.0f};
    const float v0[3] = {2.0f, 1.8f, 0.0f};
    const float floorY = 0.0f;
    const float spinAxis[3] = {0.0f, 1.0f, 0.0f};
    const float spinSpeed = 8.0f;

    // At t = 0: exactly initial pivot
    {
        float pos[3] = {};
        float rot[9] = {};
        framevr::decap::SimulateHeadPhysics(0.0f, p0, v0, floorY, spinAxis, spinSpeed, pos, rot);
        assert(std::fabs(pos[0] - 10.0f) < 1e-4f);
        assert(std::fabs(pos[1] - 1.5f) < 1e-4f);
        assert(std::fabs(pos[2] - 20.0f) < 1e-4f);
    }

    // At t = 0.15s: in air, rising toward apex
    {
        float pos[3] = {};
        float rot[9] = {};
        framevr::decap::SimulateHeadPhysics(0.15f, p0, v0, floorY, spinAxis, spinSpeed, pos, rot);
        assert(pos[1] > 1.5f); // rose into the air
        assert(pos[0] > 10.0f); // moved forward
    }

    // At t = 1.0s: bouncing above ground
    {
        float pos[3] = {};
        float rot[9] = {};
        framevr::decap::SimulateHeadPhysics(1.0f, p0, v0, floorY, spinAxis, spinSpeed, pos, rot);
        assert(pos[1] >= floorY + 0.12f); // never below ground plane
    }

    // At t = 5.0s: completely settled and resting on the ground
    {
        float pos[3] = {};
        float rot[9] = {};
        framevr::decap::SimulateHeadPhysics(5.0f, p0, v0, floorY, spinAxis, spinSpeed, pos, rot);
        assert(std::fabs(pos[1] - (floorY + 0.12f)) < 1e-3f); // resting at floor + radius
        assert(pos[0] > 10.5f && pos[0] < 12.0f); // realistic horizontal slide
        // Check rotation matrix orthogonality (row 0 length ~ 1)
        float lenSq = rot[0]*rot[0] + rot[1]*rot[1] + rot[2]*rot[2];
        assert(std::fabs(lenSq - 1.0f) < 1e-3f);
    }
}

void TestDecapitationPhysicsModes() {
    printf("[granny-decapitation-test] Suite 7: Severed Head Physics Toggle (Enabled vs Disabled)\n");

    auto& mgr = framevr::decap::Manager();
    mgr.Clear();

    const uintptr_t skelPhys = 0x2000A0;
    const uintptr_t skelVanish = 0x2000B0;
    const uint64_t t0 = 10000;
    const std::vector<uint8_t> inHead = {0, 0, 0, 1, 1}; // 5 bones: 0=root, 1=spine, 2=neck, 3=head, 4=horn

    const float impulse[3] = {1.5f, 1.8f, 0.0f};

    // 1. Instance with head physics ENABLED
    mgr.RegisterDecapitation(skelPhys, 501, 3, 2, inHead, t0, 0.0f, impulse, true);

    // 2. Instance with head physics DISABLED (clean vanish)
    mgr.RegisterDecapitation(skelVanish, 502, 3, 2, inHead, t0, 0.0f, impulse, false);

    std::vector<MockMatrix> posesA = {
        MockMatrix::Translation(0, 0, 0),
        MockMatrix::Translation(0, 1, 0),
        MockMatrix::Translation(0, 1.5f, 0),
        MockMatrix::Translation(0, 1.8f, 0),
        MockMatrix::Translation(0.2f, 2.1f, 0)
    };
    std::vector<MockMatrix> posesB = posesA;

    // Apply at t0 + 2000ms
    float pivotA[3] = {}, pivotB[3] = {};
    mgr.ApplyCollapse(skelPhys, posesA.data(), 5, t0 + 2000, pivotA);
    mgr.ApplyCollapse(skelVanish, posesB.data(), 5, t0 + 2000, pivotB);

    // Instance A (physics): head must NOT be collapsed to 0.0001f scale, but displaced by physics
    const float scaleSqA = posesA[3].m[0] * posesA[3].m[0] + posesA[3].m[1] * posesA[3].m[1] + posesA[3].m[2] * posesA[3].m[2];
    assert(std::fabs(scaleSqA - 1.0f) < 1e-3f); // full scale maintained (unit orthogonal rotation)
    assert(posesA[3].m[12] > 0.2f); // moved horizontally by physics
    assert(posesA[4].m[12] > 0.2f); // child bone horn followed head

    // Instance B (vanish): head must be collapsed to 0.0001f scale and pinned to neck pivot
    assert(std::fabs(posesB[3].m[0] - 0.0001f) < 1e-6f);
    assert(std::fabs(posesB[3].m[13] - 1.5f) < 1e-4f);
    assert(std::fabs(posesB[4].m[0] - 0.0001f) < 1e-6f);

    mgr.Clear();
}

} // namespace

int main() {
    printf("=== D2R VR Extra Gore - Granny 3D Decapitation Test Suite ===\n");
    TestBoneIdentification();
    TestDescendantHierarchyMask();
    TestMatrixCollapseMath();
    TestDecapitationManagerLifecycle();
    TestEventQueueDecapitationMatching();
    TestSeveredHeadPhysicsTrajectory();
    TestDecapitationPhysicsModes();
    printf("=== ALL GRANNY DECAPITATION TESTS PASSED (7/7 SUITES) ===\n");
    return 0;
}

