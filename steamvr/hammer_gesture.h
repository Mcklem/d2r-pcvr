#pragma once
#include "frame_math.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Paladin Blessed Hammer fist pump (Cal, 2026-10-27). Value-only: the
// transport supplies head and hand poses plus each hand's grip, and pulses the
// slot Blessed Hammer is bound to, so mana, cast rate (FCR), the hammer and its
// spiral stay native.
//
// The pump: make a fist (grip held, trigger free) with either hand below the
// head, then punch it up to the sky, ending above the head. One pump casts one
// hammer. To pump again the fist comes back down below the head first, so a
// fist held high never repeats. A pump faster than the game's cast rate is a
// press the game ignores, as with the button.
namespace framevr::hammerpump {
struct Tuning {
    // Head-local heights: head position, head yaw only (+Y up).
    float gripOn=.6f,gripOff=.4f; // a fist closes above gripOn and opens below gripOff
    float loadBelow=-.10f;        // a pump starts from a fist below the head by this (chin, shoulder or lower)
    float punchSpeed=1.6f;        // upward speed (m/s, world) that starts the punch
    float steep=1.0f;             // ...and at least this times the horizontal speed (within 45 deg of straight up)
    float topHeight=.08f;         // the fist ends above the head by this
    float minTravel=.25f;         // and rose at least this far from where the punch began
    float followSpeed=.5f;        // while loaded and rising slower than this, the punch's start follows the fist
    float window=.35f;            // seconds from reaching punchSpeed to clearing the head
    float stallSpeed=.2f;         // upward speed under this below the top abandons the punch
};
struct Input {
    double time=0;
    uint64_t session=0,epoch=0;
    bool active=false;                        // gestures allowed this frame (Blessed Hammer on a slot, no menus)
    math::Vec head{};math::Quat headRotation{0,0,0,1};
    math::Vec hand[2]{},handVelocity[2]{};    // world frame; 0 right, 1 left
    float grip[2]{};
    bool tracked[2]{},busy[2]{},velocityKnown[2]{}; // busy: trigger squeezed, bottle or other hand owner, bow drawn, face wipe
};
struct Output {
    int cast=-1;                 // the hand whose pump fires this step
    int droppedHand=-1;          // a punch in progress abandoned this step (for the log)
    const char* dropped=nullptr;
    float travel=0;              // the dropped punch's rise from where it began (m), for the log
};
class Engine {
    enum class Phase { Open, High, Loaded, Rising };
    struct Hand {Phase phase=Phase::Open;double startAt=0;float startY=0;math::Vec previous{};bool havePrevious=false;};
    Hand hands[2]{};
    uint64_t session=0,epoch=0;
    double last=0;
    static bool Finite(math::Vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
    void Drop(Output& out,int h,const char* why,float y=0){if(hands[h].phase==Phase::Rising&&!out.dropped){out.dropped=why;out.droppedHand=h;out.travel=y-hands[h].startY;}hands[h].phase=Phase::Open;}
    // Loaded: startY is where the punch begins, the fist's height whenever it
    // is not yet rising fast, so it settles at the bottom of a dip or wind-up.
    // A real punch accelerates: by the time it reaches punchSpeed it has
    // already covered part of its rise, which counts toward minTravel.
    void Load(Hand& s,float y){s.phase=Phase::Loaded;s.startY=y;}
public:
    Tuning tuning;
    void Reset(){const auto keep=tuning;*this=Engine{};tuning=keep;}
    bool Rising(int h)const{return h>=0&&h<2&&hands[h].phase==Phase::Rising;}
    Output Step(const Input& in) {
        Output out;
        if(!std::isfinite(in.time)||!Finite(in.head)){Reset();return out;}
        if(in.session!=session||in.epoch!=epoch){Reset();session=in.session;epoch=in.epoch;}
        const double dt=in.time-last;
        const bool continuous=last>0&&dt>0&&dt<=.15;
        last=in.time;
        const auto inverse=math::Conjugate(math::Yaw(math::Heading(in.headRotation)));
        for(int h=0;h<2;++h) {
            auto& s=hands[h];
            const bool ready=in.active&&in.tracked[h]&&!in.busy[h]&&Finite(in.hand[h])&&std::isfinite(in.grip[h]);
            if(!continuous||!ready){Drop(out,h,!continuous?"frame gap":"hand busy, untracked or input inactive");s.havePrevious=false;continue;}
            const auto velocity=in.velocityKnown[h]?in.handVelocity[h]:s.havePrevious?(in.hand[h]-s.previous)*float(1/dt):math::Vec{};
            const bool velocityOk=in.velocityKnown[h]||s.havePrevious;
            s.previous=in.hand[h];s.havePrevious=true;
            const float y=math::Rotate(inverse,in.hand[h]-in.head).y;
            const bool fist=s.phase==Phase::Open?in.grip[h]>=tuning.gripOn:in.grip[h]>tuning.gripOff;
            if(!fist){Drop(out,h,"fist opened before clearing the head");continue;}
            switch(s.phase) {
            case Phase::Open:
                // A fist made up high waits for the hand to come down first.
                if(y<=tuning.loadBelow)Load(s,y);else s.phase=Phase::High;
                break;
            case Phase::High:
                if(y<=tuning.loadBelow)Load(s,y);
                break;
            case Phase::Loaded: {
                const bool slow=!velocityOk||velocity.y<tuning.followSpeed;
                if(slow)s.startY=y;
                const float horizontal=std::hypot(velocity.x,velocity.z);
                if(velocityOk&&velocity.y>=tuning.punchSpeed&&velocity.y>=tuning.steep*horizontal){s.phase=Phase::Rising;s.startAt=in.time;}
                else if(slow&&y>tuning.loadBelow)s.phase=Phase::High;   // drifted up slowly: not a punch
                break;
            }
            case Phase::Rising:
            {
                // Casts once the fist is over the head AND has risen far
                // enough; a short punch keeps going until both hold.
                const bool over=y>=tuning.topHeight,risen=y-s.startY>=tuning.minTravel;
                if(over&&risen){s.phase=Phase::High;if(out.cast<0)out.cast=h;break;}   // spent: comes back down to load the next pump
                const char* why=over?"punch too short (started too close to the head)":nullptr;
                if(in.time-s.startAt>tuning.window){Drop(out,h,why?why:"too slow to clear the head",y);if(y<=tuning.loadBelow)Load(s,y);else s.phase=Phase::High;}
                else if(velocity.y<tuning.stallSpeed){Drop(out,h,why?why:"punch stalled below the head",y);if(y<=tuning.loadBelow)Load(s,y);else s.phase=Phase::High;}
                break;
            }
            }
        }
        return out;
    }
};
}
