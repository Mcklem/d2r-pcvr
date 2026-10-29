#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

// Bodies that give way: the values that shape how bodies react, for tuning.
// Each is [bodies] <key> in the INI (read live; any value in its range), and
// the developer wheel's BODIES page steps it through its steps and saves it.
// The defaults are the owner's, tuned in the headset (2026-10-27): quick,
// stiff springs, a light push, small bends and shoves, a short ease back.
namespace framevr::limbs {
enum Knob {LimbSpring,BodySpring,Bounce,PushStrength,JointStrength,Steadiness,HitKick,MaxBend,MaxShove,EaseBack,Knobs};
struct KnobInfo {
    const char* name;const char* detail;const wchar_t* key;const char* unit;
    float standard,low,high;      // the default and the range the INI may set
    std::array<float,6> steps;int count;
};
inline constexpr std::array<KnobInfo,Knobs> KnobTable{{
    {"Limb spring back","How fast a bent limb swings back to the animation",L"limb_hertz","Hz",6,.3f,10,{1,1.5f,2.5f,4,6},5},
    {"Body spring back","How fast a shoved body returns to its place and lean",L"body_hertz","Hz",4,.3f,10,{1,1.5f,2,3,4},5},
    {"Bounce","Damping: low wobbles past the pose, 1 settles without overshoot",L"damping","",.4f,.05f,2,{.15f,.25f,.4f,.7f,1},5},
    {"Push strength","The most a weapon or fist pushes with",L"push_newtons","N",300,50,5000,{300,600,1200,2400},4},
    {"Joint strength","How hard each joint holds the animated pose, per kg it holds up",L"joint_torque","Nm/kg",40,1,200,{5,10,20,40,80},5},
    {"Body steadiness","How hard the hips hold their place, per kg of the body",L"body_force","N/kg",40,5,400,{10,20,40,80,160},5},
    {"Hit kick","A landed hit's push on the struck limb (0 = none)",L"hit_kick","Ns",1.5f,0,30,{0,1.5f,3,6,10},5},
    {"Most bend","How far a limb may swing off the animation",L"max_bend","deg",30,10,150,{30,45,69,90,120},5},
    {"Most shove","How far the whole body may be pushed",L"max_shove","cm",10,0,100,{10,20,30,50},4},
    {"Ease back","When the physics lets a body go, how long it eases back",L"ease_ms","ms",100,0,3000,{100,300,600,1000},4},
}};
struct Tuning {
    std::array<float,Knobs> value{};
    static Tuning Standard() {Tuning t;for(int k=0;k<Knobs;++k)t.value[size_t(k)]=KnobTable[size_t(k)].standard;return t;}
    float operator[](Knob k) const {return value[size_t(k)];}
    bool operator==(const Tuning&) const=default;
};
// A value within its knob's range; anything unreadable is its default.
inline float Clamped(Knob k,float v) {
    const auto& info=KnobTable[size_t(k)];
    return std::isfinite(v)?std::clamp(v,info.low,info.high):info.standard;
}
// The step after the one nearest v, wrapping round.
inline float NextStep(Knob k,float v) {
    const auto& info=KnobTable[size_t(k)];
    int nearest=0;
    for(int i=1;i<info.count;++i)if(std::fabs(info.steps[size_t(i)]-v)<std::fabs(info.steps[size_t(nearest)]-v))nearest=i;
    return info.steps[size_t((nearest+1)%info.count)];
}
// The live values: set by the INI reload and the developer wheel, read by
// the physics and the skeleton hook's threads without a lock.
class Tunings {
    std::array<std::atomic<float>,Knobs> value;
public:
    Tunings() {Set(Tuning::Standard());}
    void Set(const Tuning& t) {for(int k=0;k<Knobs;++k)value[size_t(k)].store(Clamped(Knob(k),t.value[size_t(k)]),std::memory_order_relaxed);}
    void Set(Knob k,float v) {value[size_t(k)].store(Clamped(k,v),std::memory_order_relaxed);}
    float Get(Knob k) const {return value[size_t(k)].load(std::memory_order_relaxed);}
    Tuning Get() const {Tuning t;for(int k=0;k<Knobs;++k)t.value[size_t(k)]=Get(Knob(k));return t;}
};
inline Tunings& Tuned() {static Tunings tunings;return tunings;}
}
