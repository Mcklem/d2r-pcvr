#pragma once
#include "limb_rig.h"
#include "limb_tuning.h"
#include <array>
#include <memory>

// Bodies that give way (limb_rig.h): each body a powered ragdoll in its own
// Box3D world. Its limbs are capsules joined at the bones' joints; a spring
// of limited torque at each joint holds the animated pose between limbs (a
// light touch is held, a hard hit wins), and a spring holds the root limb to
// where the animation has it. Each weapon or fist is a body of its own held
// to its hand by a joint of limited strength, so a light limb gives way at
// once and a heavy body resists it; one caught behind its hand slips through.
// A landed hit adds an impulse along the swing that grows with the square
// root of the struck limb's mass, so a heavy limb moves less. The bodies are
// simulated in the animation's frame: every new pose carries each limb with
// it, keeping only its offset, so a monster that runs or swings does not lag
// behind its own animation; only pushes move a limb off it.
namespace framevr::limbs {
// A held weapon, shield or fist: base to tip in world metres. Slots as
// bottles::Collider: right hand, left hand, shield.
struct Weapon {bool active=false;Vec base{},tip{};float radius=0;};
inline constexpr int Weapons=3;
// A weapon that pushed a body: for the hand's tick and the log.
struct Touch {uint32_t entity=0;Kind kind=Kind::None;int slot=-1;float speed=0;};
inline constexpr unsigned MaxTouches=8;
inline constexpr double Step=1/120.;
inline constexpr float WakeReach=.6f;   // metres: a weapon this near a body's limbs brings it to life
inline constexpr double Unseen=.4;      // seconds a body may go without a pose before it goes
inline constexpr double RestTime=1.5;   // seconds back on its animation, out of reach, before it goes
inline constexpr double StillLife=1;    // seconds a body's animated pose may stay unchanged before it counts as lifeless (a corpse); one first seen is lifeless until it moves
inline constexpr float StillMove=.003f,StillAngle=.02f;   // metres, radians: what counts as a change
inline constexpr float Recentre=20;     // metres from the physics origin before it moves
class World {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    World();
    ~World();
    World(const World&)=delete;
    World& operator=(const World&)=delete;
    bool Ready() const;
    // The values that shape the reactions (limb_tuning.h). A change lets
    // every body go; each is made again with them as a weapon nears it.
    void Tune(const Tuning&);
    // The bodies near the player as posed now (Capture). A simulated body is
    // carried to its new pose; one unseen for Unseen seconds goes.
    void Bodies(const Rig* rigs,unsigned count,double time);
    // Where the weapons are now; the next Advance moves them there.
    void Hands(const std::array<Weapon,Weapons>&);
    // Fixed steps to time. A body comes to life when a weapon nears it and
    // goes once it is back on its animation with no weapon near.
    void Advance(double time,Vec player);
    // A hit landed by this slot's weapon: an impulse on the limb nearest it,
    // along its swing. False when no simulated limb is within reach of it.
    bool Strike(int slot);
    // Every simulated body's offsets from its animation, at the latest step.
    unsigned Reactions(std::array<Reaction,MaxBodies>& out,uint64_t now) const;
    // The pushes since the last call, cleared.
    unsigned Touches(std::array<Touch,MaxTouches>& out);
    unsigned Slips();          // weapons that slipped through a body since the last call
    unsigned Simulated() const;
    unsigned Lifeless() const;    // bodies posed now whose animation has held still (corpses)
    float Mass(uint32_t entity) const;   // kg, 0 when not simulated
    void Reset();
};
}
