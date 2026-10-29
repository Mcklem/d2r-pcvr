#pragma once
#include "frame_math.h"
#include "interactions.h"
#include <array>
#include <cmath>
#include <cstdint>

// Necromancer Bone Armor as bone pieces grafted onto both forearms, drawn by
// the mod's model renderer (potion_models) with the controller pose, so they
// stay on the arm (Cal, 2026-10-26). Native Bone Armor is three orbiting
// layers, one gone each time the absorb pool (stat 132, maximum 133) falls
// below a third (developer capture 2026-10-26: below 2/3 -> 2 layers, below
// 1/3 -> 1, 0 -> none). Each forearm carries three pieces on its back, the
// side facing the eyes (the renderer draws over the game, so a piece behind
// the arm would show through the sleeve); one comes off each arm per third.
namespace framevr::bonegraft {
using math::Vec;using math::Quat;
inline constexpr int Pieces=3;
// The native layers left for a pool (both 8.8 fixed point, as the stats hold).
inline int Layers(int pool,int maximum) {
    if(pool<=0||maximum<=0)return 0;
    const int64_t p=int64_t(pool)*3,m=maximum;
    return p<m?1:p<2*m?2:3;
}
// Which pieces show with this many layers: the centre piece last.
inline bool Shown(int piece,int layers) {
    constexpr int order[Pieces]={1,0,2};   // the centre, then one side, then the other
    for(int i=0;i<layers&&i<Pieces;++i)if(order[i]==piece)return true;
    return false;
}
struct Settings {
    float size=1.f;      // x the piece length (LengthShare of the forearm: wrist to elbow)
    float out=.05f;      // metres from the forearm's axis to a piece's centre
    float spread=40.f;   // degrees between neighbouring pieces around the arm
    float along=.5f;     // where a piece's middle sits: 0 the wrist .. 1 the elbow
    // Its thickness, x its length: 0.5 keeps the bones as thick as at half the
    // forearm's length (Cal 2026-10-26: wrist to elbow, the first look was too small).
    float width=.5f;
};
inline constexpr float LengthShare=1.f;   // a piece's length: the drawn forearm, wrist to elbow, at size 1
inline Settings Clamp(Settings s) {
    auto keep=[](float v,float lo,float hi,float d){return std::isfinite(v)&&v>=lo&&v<=hi?v:d;};
    const Settings d{};
    return {keep(s.size,.2f,3.f,d.size),keep(s.out,0.f,.2f,d.out),keep(s.spread,0.f,90.f,d.spread),keep(s.along,0.f,1.f,d.along),
            keep(s.width,.1f,2.f,d.width)};
}
// The drawn forearm in its controller's own frame (metres): wrist, elbow and
// the way the palm faces (skeleton: skel::ForearmOnController).
struct Forearm {Vec wrist{},elbow{},palm{};};
// arm: the forearm as a capsule (wrist, elbow, radius) that hides the piece's
// pixels behind it, the renderer having no game depth (potion_models).
// width: its thickness in metres (the model's cross-section scale).
struct Piece {interaction::Pose pose{};float length=0,width=0;Vec armA{},armB{};float armR=0;};
inline constexpr float ArmShare=.8f;   // the arm's radius: this share of out (pieces lie just outside it)
// Piece i's pose in the room from the controller's current room pose; the
// piece model's +Y lies along the forearm (toward the elbow), its +Z faces
// out from the arm. left mirrors the layout round the arm, so each piece sits
// in the same place on both arms (Cal 2026-10-26: the left arm showed another
// bone where the right had its long ones). False when the forearm is degenerate.
inline bool Place(const Forearm& f,interaction::Pose controller,const Settings& s,int i,Piece& out,bool left=false) {
    const Vec wrist=controller.p+math::Rotate(controller.q,f.wrist);
    const Vec elbow=controller.p+math::Rotate(controller.q,f.elbow);
    const Vec palm=math::Rotate(controller.q,f.palm);
    const Vec arm=elbow-wrist;const float length=std::sqrt(math::Dot(arm,arm));
    if(!(length>.05f&&length<1.f)||i<0||i>=Pieces)return false;
    const Vec y=arm*(1/length);
    Vec back=math::Normalized(Vec{0,0,0}-(palm-y*math::Dot(palm,y)));   // the back of the forearm
    if(math::Dot(back,back)<.5f)return false;
    const Vec side=math::Normalized(math::Cross(y,back));
    const float a=(left?-1.f:1.f)*(i-1)*s.spread*3.14159265f/180.f;
    const Vec z=back*std::cos(a)+side*std::sin(a);
    const Vec x=math::Normalized(math::Cross(y,z));
    const Quat q=math::FromAxes(x,y,z);
    const float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    out.pose={wrist+arm*s.along+z*s.out,{q.x/n,q.y/n,q.z/n,q.w/n}};
    out.length=length*LengthShare*s.size;out.width=out.length*s.width;
    out.armA=wrist;out.armB=elbow;out.armR=s.out*ArmShare;
    return interaction::Valid(out.pose)&&std::isfinite(out.length+out.width);
}
// A piece knocked off: thrown out from the arm, falling and shrinking away.
struct Falling {interaction::Pose pose{};Vec velocity{};Vec spin{};float length=0,width=0,age=0;int code=0;bool live=false;};
inline constexpr float FallSeconds=.9f,Gravity=9.8f;
inline Falling Knock(const Piece& p,int code,Vec outward) {
    Falling f;f.pose=p.pose;f.length=p.length;f.width=p.width;f.code=code;f.live=true;
    f.velocity=outward*.9f+Vec{0,.6f,0};
    f.spin=Vec{7.f,2.f,4.f};
    return f;
}
// Advance; false once it has gone.
inline bool Step(Falling& f,float dt) {
    if(!f.live)return false;
    if(!(dt>0)||dt>.25f)dt=.011f;
    f.age+=dt;if(f.age>=FallSeconds){f.live=false;return false;}
    f.velocity.y-=Gravity*dt;f.pose.p=f.pose.p+f.velocity*dt;
    const Vec w=f.spin*dt;const float angle=std::sqrt(math::Dot(w,w));
    if(angle>1e-6f) {
        const Vec axis=w*(1/angle);const float s=std::sin(angle*.5f);
        f.pose.q=math::Multiply(Quat{axis.x*s,axis.y*s,axis.z*s,std::cos(angle*.5f)},f.pose.q);
        const float n=std::sqrt(f.pose.q.x*f.pose.q.x+f.pose.q.y*f.pose.q.y+f.pose.q.z*f.pose.q.z+f.pose.q.w*f.pose.q.w);
        f.pose.q={f.pose.q.x/n,f.pose.q.y/n,f.pose.q.z/n,f.pose.q.w/n};
    }
    return true;
}
// The drawn length while falling: whole, then shrinking over the last third.
inline float FallShare(const Falling& f) {
    const float t=f.age/FallSeconds;return t<.66f?1.f:std::max(0.f,(1-t)/(1-.66f));
}
inline float FallLength(const Falling& f) {return f.length*FallShare(f);}
inline float FallWidth(const Falling& f) {return f.width*FallShare(f);}
// Per-arm layer tracking: which pieces broke since the last update.
struct Arms {
    int layers=0;bool known=false;
    // Returns the pieces that just came off (bit i), 0 on the first update or a recast.
    uint32_t Update(int now) {
        uint32_t broken=0;
        if(known&&now<layers)for(int i=0;i<Pieces;++i)if(Shown(i,layers)&&!Shown(i,now))broken|=1u<<i;
        layers=now;known=true;return broken;
    }
    void Reset(){layers=0;known=false;}
};
}
