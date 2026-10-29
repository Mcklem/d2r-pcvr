#pragma once
#include "frame_math.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Paladin Charge punch (Cal, 2026-10-27). Value-only: the transport supplies
// the head, the weapon hand and its grip, and pulses the slot Charge is bound
// to, so mana, the rush, its target and the hit stay native (the aim rules
// hand Charge the monster the weapon hand points at, else its spot).
//
// The punch: make a fist with the weapon hand (grip held, trigger free) drawn
// back near the body, then punch it straight out ahead, extending the arm.
// One punch casts once; to punch again the fist comes back in first.
namespace framevr::chargepunch {
struct Tuning {
    // Head-local: head position, head yaw only (+X right, +Y up, -Z forward).
    float gripOn=.6f,gripOff=.4f;   // the fist closes above gripOn, opens below gripOff
    float drawnBack=.30f;           // a punch starts from a fist at most this far ahead of the head
    float lowest=-.70f,highest=.10f;// ...between these heights (hip to just over the eyes)
    float punchSpeed=2.0f;          // forward speed (m/s) that starts the punch
    float straight=1.5f;            // forward speed at least this times the vertical and the sideways speed
    float extendShare=.28f;         // extended: fist ahead of the head by this share of the arm span
    float extendDefault=.45f;       // ...or by this when no body calibration is stored
    float extendMin=.38f,extendMax=.60f;
    float minTravel=.20f;           // the fist moved at least this far ahead from where the punch began
    float followSpeed=.5f;          // while loaded and moving forward slower than this, the punch's start follows the fist
    float window=.30f;              // seconds from reaching punchSpeed to full extension
    float stallSpeed=.2f;           // forward speed under this before extension abandons the punch
    float Extended(float armSpan)const{return armSpan>0?std::clamp(armSpan*extendShare,extendMin,extendMax):extendDefault;}
};
struct Input {
    double time=0;
    uint64_t session=0,epoch=0;
    bool active=false;                      // Charge on a slot, gestures on, no menus
    math::Vec head{};math::Quat headRotation{0,0,0,1};
    math::Vec hand{},handVelocity{};        // the weapon hand, world frame
    float grip=0,armSpan=0;                 // armSpan: calibrated grip-to-grip span, 0 if none
    bool tracked=false,busy=false,velocityKnown=false;
};
struct Output {
    bool cast=false;
    const char* dropped=nullptr;            // a punch in progress abandoned this step (for the log)
    float travel=0,reach=0;                 // the dropped punch's travel from where it began, and how far out it got (m)
};
class Engine {
    enum class Phase { Open, Out, Loaded, Punching };
    Phase phase=Phase::Open;
    uint64_t session=0,epoch=0;
    double last=0,startAt=0;
    float startReach=0;
    math::Vec previous{};
    bool havePrevious=false;
    static bool Finite(math::Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
    void Drop(Output& out,const char* why,float reach=0){if(phase==Phase::Punching&&!out.dropped){out.dropped=why;out.travel=reach-startReach;out.reach=reach;}phase=Phase::Open;}
    // Loaded: startReach is where the punch begins, the fist's reach whenever
    // it is not yet moving out fast, so it settles at the most drawn-back
    // point. A real punch accelerates: by the time it reaches punchSpeed it has
    // already covered part of its travel (and may have left the drawn-back
    // zone), which counts toward minTravel.
    void Load(float reach){phase=Phase::Loaded;startReach=reach;}
public:
    Tuning tuning;
    void Reset(){const auto keep=tuning;*this=Engine{};tuning=keep;}
    bool Punching()const{return phase==Phase::Punching;}
    Output Step(const Input& in) {
        Output out;
        if(!std::isfinite(in.time)||!Finite(in.head)){Reset();return out;}
        if(in.session!=session||in.epoch!=epoch){Reset();session=in.session;epoch=in.epoch;}
        const double dt=in.time-last;
        const bool continuous=last>0&&dt>0&&dt<=.15;
        last=in.time;
        const bool ready=in.active&&in.tracked&&!in.busy&&Finite(in.hand)&&std::isfinite(in.grip);
        if(!continuous||!ready){Drop(out,!continuous?"frame gap":"hand busy, untracked or input inactive");havePrevious=false;return out;}
        const auto velocityWorld=in.velocityKnown?in.handVelocity:havePrevious?(in.hand-previous)*float(1/dt):math::Vec{};
        const bool velocityOk=in.velocityKnown||havePrevious;
        previous=in.hand;havePrevious=true;
        const auto inverse=math::Conjugate(math::Yaw(math::Heading(in.headRotation)));
        const auto local=math::Rotate(inverse,in.hand-in.head);
        const auto velocity=math::Rotate(inverse,velocityWorld);
        const float reach=-local.z,forward=-velocity.z;
        const bool fist=phase==Phase::Open?in.grip>=tuning.gripOn:in.grip>tuning.gripOff;
        if(!fist){Drop(out,"fist opened before the arm was out");return out;}
        const bool drawn=reach<=tuning.drawnBack&&local.y>=tuning.lowest&&local.y<=tuning.highest;
        switch(phase) {
        case Phase::Open:
            // A fist made with the arm already out waits for it to come back in.
            if(drawn)Load(reach);else phase=Phase::Out;
            break;
        case Phase::Out:
            if(drawn)Load(reach);
            break;
        case Phase::Loaded: {
            const bool slow=!velocityOk||forward<tuning.followSpeed;
            if(slow)startReach=reach;
            if(velocityOk&&forward>=tuning.punchSpeed&&forward>=tuning.straight*std::fabs(velocity.y)&&forward>=tuning.straight*std::fabs(velocity.x)) {
                phase=Phase::Punching;startAt=in.time;
            }
            else if(!drawn&&slow)phase=Phase::Out;   // drifted out slowly: not a punch
            break;
        }
        case Phase::Punching:
        {
            // Casts once the arm is out AND the fist has travelled far enough;
            // a punch from near the line keeps going until both hold.
            const bool out_=reach>=tuning.Extended(in.armSpan),travelled=reach-startReach>=tuning.minTravel;
            if(out_&&travelled){out.cast=true;phase=Phase::Out;break;}   // spent: draw the fist back in for the next
            const char* why=out_?"punch too short (started too far out)":nullptr;
            if(in.time-startAt>tuning.window){Drop(out,why?why:"arm not extended in time",reach);if(drawn)Load(reach);else phase=Phase::Out;}
            else if(forward<tuning.stallSpeed){Drop(out,why?why:"punch stalled before the arm was out",reach);if(drawn)Load(reach);else phase=Phase::Out;}
            break;
        }
        }
        return out;
    }
};
}
