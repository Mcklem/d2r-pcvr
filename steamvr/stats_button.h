#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>
#include "leather_menu.h"

// Advanced Stats from the tabbed menu (owner, 2026-10-29). The game opens it
// with the pad's View (Back) button - Start (Menu) closes the tabbed menu
// (headset test 2026-10-29: ClosePanel UISwitcher 23 ms after each Start) -
// and Quest and other VR controllers have no View button free. A stamped
// button the laser clicks sends the game that one View press instead: on the leather, beside the menu, below its hint row;
// without the leather, on the room panel, at the same spot below the hints.
// The same icon both ways, so it reads as the same button (owner's choice).
// A controller whose menu button reaches the game still works as before.
namespace framevr::statsbutton {

// While the tabbed menu (or the Advanced Stats it opened) is the only menu
// open: not over the stash, a merchant or other world panel, not with a cube,
// where View is no Advanced Stats.
template<class Names> inline bool Shown(const Names& open) {
    bool menu=false;
    for(const auto& name:open) {
        const std::string_view n(name);
        if(leather::RoomPanel(n)||n.starts_with("HoradricCube"))return false;
        menu=menu||n==panels::TabbedMenu||n.starts_with("AdvancedStats");
    }
    return menu;
}

// Where it sits on the surface showing the menu: centred under the art, in the
// menu's 16:9 band (the top of a taller surface, as the room panel and the
// leather show the tabbed menu). The art ends at 86% of the band (Cal's
// captures, 2026-10-28/29: y 80-1489 of a 1728-high band), its hint row last;
// the button sits just below, inside the band (its top 1% of the band under the
// art). Size: 6.2% of the band, about 2 cm on the leather (owner 2026-10-29: the
// first 11% made 25% smaller (8.25%), then half that (4.125%) was "too small";
// this is between the two). Metres from the surface's centre, y up.
constexpr float Size=.062f,Center=.873f+Size*.5f;
struct Spot {float x=0,y=0,size=0;};
inline Spot Place(float width,float height) {
    if(!(width>0)||!(height>0))return {};
    const float band=std::min(height,width*9.f/16.f);
    return {0.f,height*.5f-Center*band,Size*band};
}
// The laser's hit on the surface (u, v from its top left, 0-1), inside the button.
inline bool Hit(Spot s,float width,float height,float u,float v) {
    if(!(s.size>0))return false;
    const float x=(u-.5f)*width,y=(.5f-v)*height;
    return std::fabs(x-s.x)<=s.size*.5f&&std::fabs(y-s.y)<=s.size*.5f;
}

// One View press per pull of the trigger begun on the button: held at least MinMs so
// the game's poll sees it, and while the trigger stays down, at most MaxMs. A
// trigger already down when the laser reaches the button does nothing; letting
// go anywhere rearms it.
constexpr uint64_t MinMs=80,MaxMs=400;
class Press {
    bool was=true,held=false;uint64_t since=0;
public:
    void Reset(){was=true;held=false;}
    bool Held() const{return held;}
    // True on the frame a press starts (to log it).
    bool Step(bool on,bool trigger,uint64_t now) {
        bool started=false;
        if(trigger&&!was&&on){held=true;since=now;started=true;}
        else if(held&&(now-since>=MaxMs||(!trigger&&now-since>=MinMs)))held=false;
        was=trigger;
        return started;
    }
};

// The icon: a stone badge like the native menu's panels (grey, grained, a
// bevelled rim) with three rising gold bars, the same on the hide and the room
// panel (owner 2026-10-29: "grey with that stone texture of the native menu").
// Premultiplied RGBA, size x size; `lit` is the laser's hover (brighter bars,
// a gold rim).
inline float StoneNoise(float x,float y) {   // value noise, 0-1, repeatable
    const auto hash=[](int i,int j){uint32_t h=uint32_t(i)*374761393u+uint32_t(j)*668265263u;h=(h^(h>>13))*1274126177u;return float((h^(h>>16))&0xffff)/65535.f;};
    const int i=int(std::floor(x)),j=int(std::floor(y));
    const float fx=x-i,fy=y-j,ux=fx*fx*(3-2*fx),uy=fy*fy*(3-2*fy);
    const float a=hash(i,j),b=hash(i+1,j),c=hash(i,j+1),d=hash(i+1,j+1);
    return a+(b-a)*ux+(c-a)*uy+(a-b-c+d)*ux*uy;
}
inline std::vector<uint8_t> Paint(int size,bool lit) {
    std::vector<uint8_t> px(size_t(size)*size*4,0);
    if(size<=0)return px;
    const auto box=[](float x,float y,float cx,float cy,float hx,float hy,float r) {
        const float qx=std::fabs(x-cx)-hx+r,qy=std::fabs(y-cy)-hy+r;
        return std::hypot(std::max(qx,0.f),std::max(qy,0.f))+std::min(std::max(qx,qy),0.f)-r;
    };
    const float gold=lit?1.f:.8f;
    const float bar[3]={236*gold/255.f,196*gold/255.f,112*gold/255.f};
    constexpr int S=4;   // supersamples per axis
    for(int py=0;py<size;++py)for(int px_=0;px_<size;++px_) {
        float acc[4]{};
        for(int sy=0;sy<S;++sy)for(int sx=0;sx<S;++sx) {
            const float x=(px_+(sx+.5f)/S)/size,y=(py+(sy+.5f)/S)/size;
            const float badge=box(x,y,.5f,.5f,.46f,.46f,.10f);
            if(badge>0)continue;
            // Stone: grey, grained at three scales, a little speckle; the face sunk inside a rim.
            const float grain=.5f*StoneNoise(x*7,y*7)+.3f*StoneNoise(x*19+3,y*19+5)+.2f*StoneNoise(x*53+7,y*53+1);
            float g=.30f+.16f*(grain-.5f)*2.f;
            const bool rim=badge>-.07f,lip=!rim&&badge>-.085f;
            if(rim)g+=.10f+.10f*(.5f-y);          // the rim raised, lit from above
            if(lip)g*=.45f;                       // the groove between rim and face
            if(!rim)g*=.78f+.22f*(1-y);           // the face sunk, darker low
            float c[3]={g,g*.99f,g*.96f};
            if(rim&&lit)for(int i=0;i<3;++i)c[i]=bar[i]*(1.f-.2f*y);
            bool glyph=box(x,y,.5f,.775f,.27f,.018f,.01f)<=0;   // the base line
            const float bars[3][2]={{.33f,.56f},{.5f,.43f},{.67f,.29f}};   // centre x, top
            for(const auto& b:bars)glyph=glyph||box(x,y,b[0],(b[1]+.74f)*.5f,.06f,(.74f-b[1])*.5f,.02f)<=0;
            if(glyph)for(int i=0;i<3;++i)c[i]=bar[i];
            for(int i=0;i<3;++i)acc[i]+=std::clamp(c[i],0.f,1.f);
            acc[3]+=1;
        }
        auto* p=&px[(size_t(py)*size+px_)*4];
        const float n=float(S*S);
        for(int i=0;i<3;++i)p[i]=uint8_t(std::clamp(acc[i]/n,0.f,1.f)*255.f+.5f);   // premultiplied: colour sums over covered samples only
        p[3]=uint8_t(acc[3]/n*255.f+.5f);
    }
    return px;
}
}
