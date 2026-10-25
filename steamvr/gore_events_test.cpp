#include "../shared/d2r_vr_gore.h"
#include "haptic_strength.h"
#include <cassert>
#include <cstdio>
#include <vector>

namespace {

void TestWeaponClassification() {
    using namespace framevr::gore;

    // Cutting weapons
    assert(IsCuttingWeapon(D2RVR_TYPE_SWORD));
    assert(IsCuttingWeapon(D2RVR_TYPE_AXE));
    assert(IsCuttingWeapon(D2RVR_TYPE_DAGGER));
    assert(IsCuttingWeapon(D2RVR_TYPE_POLEARM));
    assert(IsCuttingWeapon(D2RVR_TYPE_CLAW));
    assert(!IsCuttingWeapon(D2RVR_TYPE_MACE));
    assert(!IsCuttingWeapon(D2RVR_TYPE_BOW));
    assert(!IsCuttingWeapon(D2RVR_TYPE_UNARMED));

    // Blunt weapons
    assert(IsBluntWeapon(D2RVR_TYPE_MACE));
    assert(IsBluntWeapon(D2RVR_TYPE_SCEPTER));
    assert(IsBluntWeapon(D2RVR_TYPE_STAFF));
    assert(IsBluntWeapon(D2RVR_TYPE_WAND));
    assert(IsBluntWeapon(D2RVR_TYPE_UNARMED));
    assert(!IsBluntWeapon(D2RVR_TYPE_SWORD));
    assert(!IsBluntWeapon(D2RVR_TYPE_AXE));
    assert(!IsBluntWeapon(D2RVR_TYPE_CROSSBOW));

    std::puts("TestWeaponClassification: PASS");
}

void TestMonsterTaxonomy() {
    using namespace framevr::gore;

    // Undead
    assert(TaxonomyForMonster(0) == BloodTaxonomy::UndeadRot);  // Skeleton
    assert(TaxonomyForMonster(1) == BloodTaxonomy::UndeadRot);
    assert(TaxonomyForMonster(18) == BloodTaxonomy::UndeadRot); // Zombie
    assert(TaxonomyForMonster(70) == BloodTaxonomy::UndeadRot);

    // Insects / Vermin
    assert(TaxonomyForMonster(58) == BloodTaxonomy::InsectBile); // Sand Maggot
    assert(TaxonomyForMonster(62) == BloodTaxonomy::InsectBile); // Spider
    assert(TaxonomyForMonster(155) == BloodTaxonomy::InsectBile);

    // Animals / Beasts
    assert(TaxonomyForMonster(25) == BloodTaxonomy::BeastCrimson); // Quill Rat / Brute
    assert(TaxonomyForMonster(41) == BloodTaxonomy::BeastCrimson); // Blood Hawk

    // Demons
    assert(TaxonomyForMonster(4) == BloodTaxonomy::DemonIchor);   // Fallen
    assert(TaxonomyForMonster(13) == BloodTaxonomy::DemonIchor);  // Fallen Shaman
    assert(TaxonomyForMonster(999) == BloodTaxonomy::DemonIchor); // Default

    std::puts("TestMonsterTaxonomy: PASS");
}

void TestGoreClassification() {
    using namespace framevr::gore;

    // Non-lethal and non-critical should yield None
    assert(ClassifyMeleeGore(D2RVR_TYPE_SWORD, false, false) == GoreType::None);
    assert(ClassifyMeleeGore(D2RVR_TYPE_MACE, false, false) == GoreType::None);

    // Lethal slash with cutting weapons
    assert(ClassifyMeleeGore(D2RVR_TYPE_SWORD, true, false) == GoreType::SlashDecapitation);
    assert(ClassifyMeleeGore(D2RVR_TYPE_AXE, true, false) == GoreType::SlashDecapitation);
    assert(ClassifyMeleeGore(D2RVR_TYPE_DAGGER, true, false) == GoreType::SlashDecapitation);

    // Lethal crush with blunt weapons
    assert(ClassifyMeleeGore(D2RVR_TYPE_MACE, true, false) == GoreType::BluntBoneCrush);
    assert(ClassifyMeleeGore(D2RVR_TYPE_STAFF, true, false) == GoreType::BluntBoneCrush);
    assert(ClassifyMeleeGore(D2RVR_TYPE_UNARMED, true, false) == GoreType::BluntBoneCrush);

    // Piercing weapons
    assert(ClassifyMeleeGore(D2RVR_TYPE_SPEAR, true, false) == GoreType::PiercingArterial);
    assert(ClassifyMeleeGore(D2RVR_TYPE_JAVELIN, true, false) == GoreType::PiercingArterial);

    // Other lethal
    assert(ClassifyMeleeGore(D2RVR_TYPE_ORB, true, false) == GoreType::GenericLethal);

    // Critical non-lethal strike
    assert(ClassifyMeleeGore(D2RVR_TYPE_SWORD, false, true) == GoreType::SlashDecapitation);
    assert(ClassifyMeleeGore(D2RVR_TYPE_MACE, false, true) == GoreType::BluntBoneCrush);

    std::puts("TestGoreClassification: PASS");
}

void TestEventQueue() {
    using namespace framevr::gore;

    EventQueue q;
    assert(q.Count() == 0);

    GoreEvent ev1{};
    ev1.timestamp = 1000;
    ev1.monsterId = 42;
    ev1.hand = 0;
    ev1.type = GoreType::SlashDecapitation;
    ev1.taxonomy = BloodTaxonomy::DemonIchor;
    ev1.lethal = true;

    q.Publish(ev1);
    assert(q.Count() == 1);

    GoreEvent peeked{};
    assert(q.Peek(peeked));
    assert(peeked.monsterId == 42);
    assert(peeked.type == GoreType::SlashDecapitation);
    assert(q.Count() == 1);

    GoreEvent taken{};
    assert(q.Take(taken));
    assert(taken.monsterId == 42);
    assert(q.Count() == 0);
    assert(!q.Take(taken));

    // Test ring buffer wrap-around
    for (uint32_t i = 0; i < 25; ++i) {
        GoreEvent ev{};
        ev.timestamp = 2000 + i;
        ev.monsterId = i;
        ev.hand = int(i % 2);
        ev.type = (i % 2 == 0) ? GoreType::SlashDecapitation : GoreType::BluntBoneCrush;
        q.Publish(ev);
    }

    assert(q.Count() == EventQueue::Capacity);

    // LatestForHand checks
    GoreEvent latestRight{};
    assert(q.LatestForHand(0, 2024, 150, latestRight));
    assert(latestRight.hand == 0);
    assert(latestRight.monsterId == 24);
    assert(latestRight.type == GoreType::SlashDecapitation);

    GoreEvent latestLeft{};
    assert(q.LatestForHand(1, 2024, 150, latestLeft));
    assert(latestLeft.hand == 1);
    assert(latestLeft.monsterId == 23);
    assert(latestLeft.type == GoreType::BluntBoneCrush);

    // Stale check
    GoreEvent stale{};
    assert(!q.LatestForHand(0, 3000, 100, stale)); // Timestamp 2024 is older than 3000 - 100

    q.Clear();
    assert(q.Count() == 0);

    std::puts("TestEventQueue: PASS");
}

void TestGoreHaptics() {
    using namespace framevr::gore;
    using namespace framevr::haptics;

    const auto slash = GoreSampleFor(GoreType::SlashDecapitation);
    assert(slash.amplitude == 1.0f);
    assert(slash.milliseconds == 35);
    assert(slash.frequency == 160.0f);

    const auto crush = GoreSampleFor(GoreType::BluntBoneCrush);
    assert(crush.amplitude == 1.0f);
    assert(crush.milliseconds == 95);
    assert(crush.frequency == 50.0f);

    const auto explosion = GoreSampleFor(GoreType::CorpseExplosion);
    assert(explosion.amplitude == 1.0f);
    assert(explosion.milliseconds == 140);
    assert(explosion.frequency == 60.0f);

    const auto cold = GoreSampleFor(GoreType::ColdShatter);
    assert(cold.amplitude == 0.85f);
    assert(cold.milliseconds == 45);
    assert(cold.frequency == 200.0f);

    std::puts("TestGoreHaptics: PASS");
}

} // namespace

int main() {
    std::puts("Running gore-events-test...");
    TestWeaponClassification();
    TestMonsterTaxonomy();
    TestGoreClassification();
    TestEventQueue();
    TestGoreHaptics();
    std::puts("All gore-events-test checks PASSED.");
    return 0;
}

