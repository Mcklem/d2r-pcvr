#include "limb_physics.h"
#include <cassert>
#include <cstdio>

using namespace framevr;
using namespace framevr::limbs;
using math::Vec;using math::Quat;
namespace {
constexpr float Pi=3.14159265f;
Quat About(Vec axis,float angle) {const Vec n=math::Normalized(axis);const float s=std::sin(angle*.5f);return {n.x*s,n.y*s,n.z*s,std::cos(angle*.5f)};}
float Angle(Quat q) {return 2*std::acos(std::min(1.f,std::fabs(q.w)));}
// A torso standing at x (its joint at the hips) and an arm hanging from its
// shoulder; the arm's own frame turned (its x along the arm, pointing down),
// so the joint's animated turn is not the identity. turn: the whole body faced.
Rig Body(uint64_t at,Vec where={0,0,2},Quat turn={0,0,0,1},float girth=.15f,uint32_t entity=0x100042) {
    Rig r;r.entity=entity;r.kind=Kind::Monster;r.layout=7;r.at=at;r.count=2;
    auto place=[&](Vec p){return where+math::Rotate(turn,p);};
    auto& torso=r.limbs[0];
    torso.joint=place({0,1,0});torso.q=turn;torso.a=place({0,1.1f,0});torso.b=place({0,1.5f,0});torso.radius=girth;
    auto& arm=r.limbs[1];
    arm.joint=place({.25f,1.5f,0});arm.q=math::Multiply(turn,About({0,0,1},-Pi/2));
    arm.a=place({.25f,1.45f,0});arm.b=place({.25f,.95f,0});arm.radius=.05f;
    r.parent[0]=-1;r.parent[1]=0;
    return r;
}
std::array<Weapon,Weapons> Blade(Vec base,Vec tip) {std::array<Weapon,Weapons> w{};w[0]={true,base,tip,.03f};return w;}
Reaction Answer(const World& world) {std::array<Reaction,MaxBodies> r{};assert(world.Reactions(r,1)==1);return r[0];}
float Bent(const World& world) {const auto r=Answer(world);return std::max(Angle(r.turn[0]),Angle(r.turn[1]));}
// The values these checks were written for (the first defaults: livelier
// than the owner's, so a push shows clearly).
Tuning Lively() {Tuning t;t.value={2.5f,2,.4f,600,20,40,3,69,30,300};return t;}
constexpr float LivelyTurn=69*Pi/180,LivelyShove=.3f;
// One display frame at 90 Hz: the body posed, the weapons placed, the world
// stepped. A living body's animation never holds quite still: each frame's
// pose is new and its hips rise or sink 4 mm (StillMove is 3).
int ticks=0;
double Frame(World& world,double t,const Rig& body,const std::array<Weapon,Weapons>& hands) {
    Rig r=body;r.at=body.at*100000+uint64_t(++ticks);
    const Vec breath{0,ticks&1?.004f:0,0};
    for(int i=0;i<r.count;++i){auto& l=r.limbs[size_t(i)];l.joint=l.joint+breath;l.a=l.a+breath;l.b=l.b+breath;}
    t+=1/90.;world.Bodies(&r,1,t);world.Hands(hands);world.Advance(t,{0,0,0});return t;
}
// The same, the pose held exactly: a corpse.
double Still(World& world,double t,const Rig& body,const std::array<Weapon,Weapons>& hands) {
    t+=1/90.;world.Bodies(&body,1,t);world.Hands(hands);world.Advance(t,{0,0,0});return t;
}
}
int main() {
    World world;
    assert(world.Ready());
    world.Tune(Lively());
    double t=0;
    // No weapon near, no ragdoll.
    for(int i=0;i<10;++i)t=Frame(world,t,Body(1),{});
    assert(world.Simulated()==0);
    t=Frame(world,t,Body(1),Blade({3,1.2f,2},{3,1.2f,3}));
    assert(world.Simulated()==0);
    // A blade within reach makes it one; untouched, it holds the animated
    // pose (the joint's spring aims at the animated turn between the limbs).
    const auto still=Blade({.25f,1.2f,2.4f},{.25f,1.2f,3.2f});
    for(int i=0;i<90;++i)t=Frame(world,t,Body(1),still);
    assert(world.Simulated()==1);
    assert(Bent(world)<.02f&&Length(Answer(world).shift)<.005f);
    // The body runs and turns: carried with its animation, not left behind.
    for(int i=0;i<45;++i) {
        const float s=float(i+1)/45;
        const auto body=Body(uint64_t(2+i),{.03f*float(i+1),0,2},About({0,1,0},.5f*s));
        t=Frame(world,t,body,Blade(body.limbs[1].joint+Vec{0,-.3f,.4f},body.limbs[1].joint+Vec{0,-.3f,1.2f}));
        assert(Bent(world)<.02f&&Length(Answer(world).shift)<.01f);
    }
    // A blade swept front to back through the arm, clear of the torso,
    // pushes it aside: the arm bends at the shoulder, more than the torso
    // leans; the hand's tick is reported.
    const Vec where{1.35f,0,2};const Quat facing=About({0,1,0},.5f);
    const Rig body=Body(100,where,facing);
    auto across=[&](float z){return Blade(where+math::Rotate(facing,{.2f,1.2f,z}),where+math::Rotate(facing,{.5f,1.2f,z}));};
    for(int i=0;i<30;++i)t=Frame(world,t,body,across(-.6f));
    float most=0,lean=0;std::array<Touch,MaxTouches> touches{};unsigned touched=0;
    for(int i=0;i<36;++i) {
        t=Frame(world,t,body,across(-.6f+1.2f*float(i)/35));   // 3 m/s
        const auto r=Answer(world);
        most=std::max(most,Angle(r.turn[1]));lean=std::max(lean,Angle(r.turn[0]));
        assert(Angle(r.turn[1])<=LivelyTurn+.05f&&Length(r.shift)<=LivelyShove+.01f);
        if(const unsigned n=world.Touches(touches)){touched+=n;assert(touches[0].entity==body.entity&&touches[0].slot==0&&touches[0].kind==Kind::Monster);}
    }
    std::printf("limb physics: arm bent %.2f rad, torso leaned %.2f rad, %u touches\n",most,lean,touched);
    assert(most>.15f&&most>lean&&touched>0);
    // The blade gone: the springs bring it back to the animation, and once
    // still and out of reach the ragdoll goes.
    for(int i=0;i<180;++i)t=Frame(world,t,body,{});
    assert(world.Simulated()==0||Bent(world)<.05f);
    for(int i=0;i<180;++i)t=Frame(world,t,body,{});
    assert(world.Simulated()==0);
    // A body no longer posed goes after Unseen.
    for(int i=0;i<30;++i)t=Frame(world,t,Body(200),still);
    assert(world.Simulated()==1);
    for(int i=0;i<60;++i){t+=1/90.;world.Bodies(nullptr,0,t);world.Hands(still);world.Advance(t,{0,0,0});}
    assert(world.Simulated()==0);
    world.Reset();assert(world.Simulated()==0);
    // Weight: the same blade swept through the middle of a slight body and a
    // heavy one shoves the slight one far more; the heavy one stops the blade
    // until it slips through and goes on with the hand.
    auto shove=[&](float girth,unsigned& slips,float farthest=30) {
        World w;double s=0;Tuning tuning=Lively();tuning.value[MaxShove]=farthest;w.Tune(tuning);const Rig b=Body(300,{0,0,2},{0,0,0,1},girth,0x200000u+unsigned(girth*1000));
        auto at=[&](float z){return Blade({-.4f,1.3f,2+z},{.4f,1.3f,2+z});};
        for(int i=0;i<20;++i)s=Frame(w,s,b,at(-.7f));
        float most=0;slips=0;
        for(int i=0;i<47;++i) {
            s=Frame(w,s,b,at(-.7f+1.4f*float(i)/46));   // 2.7 m/s through the torso
            most=std::max(most,Length(Answer(w).shift));slips+=w.Slips();
        }
        std::printf("limb physics: a torso %.0f cm thick (%.0f kg) shoved %.1f cm, %u slips\n",girth*200,w.Mass(b.entity),most*100,slips);
        return most;
    };
    unsigned slight=0,heavy=0;
    const float light=shove(.08f,slight),stout=shove(.3f,heavy);
    assert(light>2*stout&&heavy>0);
    unsigned ignored=0;assert(shove(.08f,ignored,10)<=.105f);   // Most shove, tuned down
    // A landed hit kicks the struck limb along the swing; nothing near, no kick.
    {
        World w;w.Tune(Lively());double s=0;const Rig b=Body(400);
        const auto beside=Blade({.25f,1.2f,2.17f},{.25f,1.2f,2.9f});   // 10 cm off the arm
        for(int i=0;i<20;++i)s=Frame(w,s,b,beside);
        assert(Bent(w)<.02f);
        assert(w.Strike(0)&&!w.Strike(1));
        float kicked=0;for(int i=0;i<20;++i){s=Frame(w,s,b,beside);kicked=std::max(kicked,Bent(w));}
        std::printf("limb physics: a landed hit bent the arm %.2f rad\n",kicked);
        assert(kicked>.1f);
        // Hit kick tuned to none: no kick.
        World calm;Tuning none=Lively();none.value[HitKick]=0;calm.Tune(none);s=0;
        for(int i=0;i<20;++i)s=Frame(calm,s,b,beside);
        assert(calm.Simulated()==1&&!calm.Strike(0));
    }
    // A corpse (its pose held exactly) is lifeless after StillLife: a body
    // pushed as it died is let go, and a blade swept through it moves nothing.
    {
        World w;w.Tune(Lively());double s=0;const Rig b=Body(500);
        const auto beside=Blade({.25f,1.2f,2.17f},{.25f,1.2f,2.9f});
        for(int i=0;i<30;++i)s=Frame(w,s,b,beside);
        assert(w.Simulated()==1&&w.Lifeless()==0);
        for(int i=0;i<int(StillLife*90)+5;++i)s=Still(w,s,b,beside);
        assert(w.Lifeless()==1);
        s=Still(w,s,b,beside);
        assert(w.Simulated()==0);
        for(int i=0;i<36;++i){s=Still(w,s,b,Blade({.25f-.6f+1.2f*float(i)/35,1.2f,1.7f},{.25f-.6f+1.2f*float(i)/35,1.2f,2.3f}));assert(w.Simulated()==0);}
        std::array<Touch,MaxTouches> none{};assert(w.Touches(none)==0);
        // Moving again (it was a living body holding still), it is pushed again.
        for(int i=0;i<3;++i)s=Frame(w,s,b,beside);
        assert(w.Lifeless()==0&&w.Simulated()==1);
    }
    // A body first seen holding still - a corpse come upon, or come back to - is lifeless at once: a
    // blade beside it from its first frame wakes nothing. Once its animation moves it is alive.
    {
        World w;w.Tune(Lively());double s=0;const Rig b=Body(600);
        const auto beside=Blade({.25f,1.2f,2.17f},{.25f,1.2f,2.9f});
        for(int i=0;i<20;++i){s=Still(w,s,b,beside);assert(w.Simulated()==0&&w.Lifeless()==1);}
        for(int i=0;i<3;++i)s=Frame(w,s,b,beside);
        assert(w.Lifeless()==0&&w.Simulated()==1);
    }
    std::puts("limb physics: all checks passed");
    return 0;
}
